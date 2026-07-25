#!/usr/bin/env python3
"""
Group-chat benchmark client for Cluster Chat Server.

It prepares benchmark data, logs in local users, sends GROUP_CHAT_MSG frames,
receives forwarded messages, and prints throughput plus latency metrics.

Default layout for groupId=1:
- users 1-60: real TCP clients connected to the current ChatServer
- users 61-80: marked online in MySQL but not connected locally, triggering Redis publish
- users 81-100: marked offline, triggering OfflineMessage writes

All benchmark users use password: 123456
"""

import argparse
import csv
import json
import os
import socket
import struct
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Set


PASSWORD = "123456"
PASSWORD_HASH = "8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92"

LOGIN_MSG = 1
GROUP_CHAT_MSG = 9
PING_MSG = 14


@dataclass
class Stats:
    lock: threading.Lock = field(default_factory=threading.Lock)
    latencies_us: List[int] = field(default_factory=list)
    received: int = 0
    parse_errors: int = 0
    disconnects: int = 0
    send_errors: int = 0
    recv_without_ts: int = 0
    seqs: Set[int] = field(default_factory=set)

    def record_message(self, msg: Dict) -> None:
        recv_ts_us = time.monotonic_ns() // 1000
        with self.lock:
            self.received += 1
            seq = msg.get("seq")
            if isinstance(seq, int):
                self.seqs.add(seq)

            send_ts_us = msg.get("send_ts_us")
            if isinstance(send_ts_us, int):
                self.latencies_us.append(recv_ts_us - send_ts_us)
            else:
                self.recv_without_ts += 1

    def record_parse_error(self) -> None:
        with self.lock:
            self.parse_errors += 1

    def record_disconnect(self) -> None:
        with self.lock:
            self.disconnects += 1

    def record_send_error(self) -> None:
        with self.lock:
            self.send_errors += 1


@dataclass
class BenchmarkSummary:
    elapsed: float
    target_qps: float
    actual_send_qps: float
    receive_throughput: float
    sent: int
    received: int
    expected_local_received: int
    missing_local_messages: int
    avg_ms: float
    min_ms: float
    p95_ms: float
    p99_ms: float
    max_ms: float
    parse_errors: int
    disconnects: int
    send_errors: int
    recv_without_ts: int
    total_errors: int


def build_sql(group_id: int, total_users: int, local_online: int, remote_online: int) -> str:
    offline_start = local_online + remote_online + 1

    users = []
    for uid in range(1, total_users + 1):
        state = "online" if local_online < uid < offline_start else "offline"
        users.append(
            f"  ({uid}, 'bench_user_{uid:03d}', '{PASSWORD_HASH}', '{state}')"
        )

    group_users = []
    for uid in range(1, total_users + 1):
        role = "creator" if uid == 1 else "normal"
        group_users.append(f"  ({group_id}, {uid}, '{role}')")

    return f"""CREATE DATABASE IF NOT EXISTS chat
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_0900_ai_ci;

USE chat;

DELETE FROM OfflineMessage WHERE userid BETWEEN 1 AND {total_users};
DELETE FROM GroupUser WHERE groupid = {group_id} OR userid BETWEEN 1 AND {total_users};
DELETE FROM Friend WHERE userid BETWEEN 1 AND {total_users} OR friendid BETWEEN 1 AND {total_users};

INSERT INTO user (id, name, password, state) VALUES
{",\n".join(users)}
ON DUPLICATE KEY UPDATE
  name = VALUES(name),
  password = VALUES(password),
  state = VALUES(state);

INSERT INTO AllGroup (id, groupname, groupdesc) VALUES
  ({group_id}, 'benchmark-room', 'Group chat benchmark room')
ON DUPLICATE KEY UPDATE
  groupname = VALUES(groupname),
  groupdesc = VALUES(groupdesc);

INSERT INTO GroupUser (groupid, userid, grouprole) VALUES
{",\n".join(group_users)}
ON DUPLICATE KEY UPDATE
  grouprole = VALUES(grouprole);
"""


def run_mysql(args: argparse.Namespace, sql: str) -> None:
    cmd = [
        args.mysql_bin,
        "-h",
        args.db_host,
        "-P",
        str(args.db_port),
        "-u",
        args.db_user,
    ]
    if args.db_password:
        cmd.append(f"-p{args.db_password}")

    print("Applying benchmark data through mysql client...")
    proc = subprocess.run(
        cmd,
        input=sql.encode("utf-8"),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr.decode("utf-8", errors="replace"))
        raise RuntimeError("mysql command failed")
    if proc.stdout:
        sys.stdout.write(proc.stdout.decode("utf-8", errors="replace"))


def send_frame(sock: socket.socket, payload: Dict) -> None:
    body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    sock.sendall(struct.pack("!I", len(body)) + body)


def send_frame_locked(sock: socket.socket, lock: threading.Lock, payload: Dict) -> None:
    with lock:
        send_frame(sock, payload)


def recv_exact(sock: socket.socket, size: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < size:
        part = sock.recv(size - len(chunks))
        if not part:
            raise ConnectionError("connection closed")
        chunks.extend(part)
    return bytes(chunks)


def recv_frame(sock: socket.socket) -> Dict:
    header = recv_exact(sock, 4)
    length = struct.unpack("!I", header)[0]
    if length > 1024 * 1024:
        raise ValueError(f"frame too large: {length}")
    body = recv_exact(sock, length)
    return json.loads(body.decode("utf-8"))


def reader_loop(uid: int, sock: socket.socket, stats: Stats, stop_event: threading.Event) -> None:
    while not stop_event.is_set():
        try:
            msg = recv_frame(sock)
            if msg.get("msgId") == GROUP_CHAT_MSG:
                stats.record_message(msg)
        except json.JSONDecodeError:
            stats.record_parse_error()
        except (ConnectionError, OSError, ValueError):
            if not stop_event.is_set():
                stats.record_disconnect()
            return


def login_user(args: argparse.Namespace, uid: int) -> socket.socket:
    sock = socket.create_connection((args.server_host, args.server_port), timeout=args.connect_timeout)
    sock.settimeout(None)
    send_frame(sock, {"msgId": LOGIN_MSG, "id": uid, "password": PASSWORD})
    ack = recv_frame(sock)
    if ack.get("errno") != 0:
        sock.close()
        raise RuntimeError(f"user {uid} login failed: {ack}")
    return sock


def close_sockets(sockets: List[socket.socket]) -> None:
    for sock in sockets:
        try:
            sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        try:
            sock.close()
        except OSError:
            pass


def percentile(sorted_values: List[int], pct: int) -> int:
    if not sorted_values:
        return 0
    index = int(len(sorted_values) * pct / 100)
    index = min(index, len(sorted_values) - 1)
    return sorted_values[index]


def start_heartbeat_thread(
    args: argparse.Namespace,
    user_sockets: Dict[int, socket.socket],
    send_locks: Dict[int, threading.Lock],
    stats: Stats,
    stop_event: threading.Event,
) -> Optional[threading.Thread]:
    if args.heartbeat_interval <= 0:
        return None

    def heartbeat_loop() -> None:
        while not stop_event.wait(args.heartbeat_interval):
            for uid, sock in user_sockets.items():
                if stop_event.is_set():
                    return
                try:
                    send_frame_locked(sock, send_locks[uid], {"msgId": PING_MSG, "id": uid})
                except OSError:
                    stats.record_send_error()

    t = threading.Thread(target=heartbeat_loop, daemon=True)
    t.start()
    return t


def run_send_phase(
    args: argparse.Namespace,
    sender_ids: List[int],
    sender_sockets: Dict[int, socket.socket],
    send_locks: Dict[int, threading.Lock],
    stats: Stats,
    stop_event: threading.Event,
) -> tuple[int, float]:
    total_to_send = max(1, int(args.duration * args.qps))
    interval = 1.0 / args.qps
    sender_workers = args.sender_workers or min(args.senders, 8)

    next_seq = 1
    sent = 0
    seq_lock = threading.Lock()
    sent_lock = threading.Lock()
    start_at = time.monotonic() + 0.1

    def next_message_seq() -> Optional[int]:
        nonlocal next_seq
        with seq_lock:
            if next_seq > total_to_send:
                return None
            seq = next_seq
            next_seq += 1
            return seq

    def sender_loop() -> None:
        nonlocal sent
        while not stop_event.is_set():
            seq = next_message_seq()
            if seq is None:
                return

            scheduled_at = start_at + (seq - 1) * interval
            wait_seconds = scheduled_at - time.monotonic()
            if wait_seconds > 0 and stop_event.wait(wait_seconds):
                return

            sender_id = sender_ids[(seq - 1) % len(sender_ids)]
            payload = {
                "msgId": GROUP_CHAT_MSG,
                "id": sender_id,
                "groupId": args.group_id,
                "message": f"benchmark-{seq}",
                "seq": seq,
                "send_ts_us": time.monotonic_ns() // 1000,
            }

            try:
                send_frame_locked(sender_sockets[sender_id], send_locks[sender_id], payload)
                with sent_lock:
                    sent += 1
            except OSError:
                stats.record_send_error()

    workers = [
        threading.Thread(target=sender_loop, daemon=True)
        for _ in range(sender_workers)
    ]

    for worker in workers:
        worker.start()
    for worker in workers:
        worker.join()

    elapsed = max(time.monotonic() - start_at, 0.0)
    return sent, elapsed


def print_report(
    args: argparse.Namespace,
    stats: Stats,
    sent: int,
    elapsed: float,
    sender_ids: List[int],
) -> BenchmarkSummary:
    with stats.lock:
        latencies = list(stats.latencies_us)
        received = stats.received
        parse_errors = stats.parse_errors
        disconnects = stats.disconnects
        send_errors = stats.send_errors
        recv_without_ts = stats.recv_without_ts

    latencies.sort()

    local_receivers_per_message = max(args.local_online - 1, 0)
    expected_local_received = sent * local_receivers_per_message
    missing_local_messages = max(expected_local_received - received, 0)

    throughput = received / elapsed if elapsed > 0 else 0
    actual_send_qps = sent / elapsed if elapsed > 0 else 0
    avg_ms = (sum(latencies) / len(latencies) / 1000) if latencies else 0
    p95_ms = percentile(latencies, 95) / 1000
    p99_ms = percentile(latencies, 99) / 1000
    min_ms = (latencies[0] / 1000) if latencies else 0
    max_ms = (latencies[-1] / 1000) if latencies else 0

    total_errors = parse_errors + disconnects + send_errors + recv_without_ts + missing_local_messages

    print("\n========== Benchmark Result ==========")
    print(f"duration: {elapsed:.2f} s")
    print(f"group id: {args.group_id}")
    print(f"total group users: {args.total_users}")
    print(f"local online users: {args.local_online}")
    print(f"remote-online users: {args.remote_online}")
    print(f"offline users: {args.total_users - args.local_online - args.remote_online}")
    print(f"sender ids: {','.join(str(uid) for uid in sender_ids)}")
    print(f"target send qps: {args.qps:.2f}")
    print(f"actual send qps: {actual_send_qps:.2f}")
    print(f"sent messages: {sent}")
    print(f"received local messages: {received}")
    print(f"expected local messages: {expected_local_received}")
    print(f"throughput: {throughput:.2f} msg/s")
    print(f"avg latency: {avg_ms:.2f} ms")
    print(f"min latency: {min_ms:.2f} ms")
    print(f"p95 latency: {p95_ms:.2f} ms")
    print(f"p99 latency: {p99_ms:.2f} ms")
    print(f"max latency: {max_ms:.2f} ms")
    print(f"parse errors: {parse_errors}")
    print(f"disconnects: {disconnects}")
    print(f"send errors: {send_errors}")
    print(f"messages without timestamp: {recv_without_ts}")
    print(f"missing local messages: {missing_local_messages}")
    print(f"total errors: {total_errors}")

    return BenchmarkSummary(
        elapsed=elapsed,
        target_qps=args.qps,
        actual_send_qps=actual_send_qps,
        receive_throughput=throughput,
        sent=sent,
        received=received,
        expected_local_received=expected_local_received,
        missing_local_messages=missing_local_messages,
        avg_ms=avg_ms,
        min_ms=min_ms,
        p95_ms=p95_ms,
        p99_ms=p99_ms,
        max_ms=max_ms,
        parse_errors=parse_errors,
        disconnects=disconnects,
        send_errors=send_errors,
        recv_without_ts=recv_without_ts,
        total_errors=total_errors,
    )


def write_csv_report(
    args: argparse.Namespace,
    summary: BenchmarkSummary,
    sender_ids: List[int],
) -> None:
    if not args.csv_out:
        return

    fieldnames = [
        "timestamp",
        "server",
        "group_id",
        "total_users",
        "local_online",
        "remote_online",
        "offline_users",
        "senders",
        "sender_workers",
        "sender_ids",
        "duration_s",
        "target_qps",
        "actual_send_qps",
        "receive_throughput",
        "sent",
        "received",
        "expected_local_received",
        "missing_local_messages",
        "avg_latency_ms",
        "min_latency_ms",
        "p95_latency_ms",
        "p99_latency_ms",
        "max_latency_ms",
        "parse_errors",
        "disconnects",
        "send_errors",
        "messages_without_timestamp",
        "total_errors",
    ]

    row = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "server": f"{args.server_host}:{args.server_port}",
        "group_id": args.group_id,
        "total_users": args.total_users,
        "local_online": args.local_online,
        "remote_online": args.remote_online,
        "offline_users": args.total_users - args.local_online - args.remote_online,
        "senders": args.senders,
        "sender_workers": args.sender_workers or min(args.senders, 8),
        "sender_ids": ",".join(str(uid) for uid in sender_ids),
        "duration_s": f"{summary.elapsed:.6f}",
        "target_qps": f"{summary.target_qps:.6f}",
        "actual_send_qps": f"{summary.actual_send_qps:.6f}",
        "receive_throughput": f"{summary.receive_throughput:.6f}",
        "sent": summary.sent,
        "received": summary.received,
        "expected_local_received": summary.expected_local_received,
        "missing_local_messages": summary.missing_local_messages,
        "avg_latency_ms": f"{summary.avg_ms:.6f}",
        "min_latency_ms": f"{summary.min_ms:.6f}",
        "p95_latency_ms": f"{summary.p95_ms:.6f}",
        "p99_latency_ms": f"{summary.p99_ms:.6f}",
        "max_latency_ms": f"{summary.max_ms:.6f}",
        "parse_errors": summary.parse_errors,
        "disconnects": summary.disconnects,
        "send_errors": summary.send_errors,
        "messages_without_timestamp": summary.recv_without_ts,
        "total_errors": summary.total_errors,
    }

    needs_header = not os.path.exists(args.csv_out) or os.path.getsize(args.csv_out) == 0
    with open(args.csv_out, "a", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        if needs_header:
            writer.writeheader()
        writer.writerow(row)
    print(f"Wrote CSV result to {args.csv_out}")


def run_benchmark(args: argparse.Namespace) -> None:
    sql = build_sql(args.group_id, args.total_users, args.local_online, args.remote_online)
    if args.sql_out:
        with open(args.sql_out, "w", encoding="utf-8", newline="\n") as f:
            f.write(sql)
        print(f"Wrote SQL to {args.sql_out}")
        return

    if not args.skip_db:
        run_mysql(args, sql)

    stats = Stats()
    stop_event = threading.Event()
    sockets: List[socket.socket] = []
    readers: List[threading.Thread] = []
    user_sockets: Dict[int, socket.socket] = {}
    send_locks: Dict[int, threading.Lock] = {}

    print(f"Logging in users 1-{args.local_online} to {args.server_host}:{args.server_port}...")
    try:
        for uid in range(1, args.local_online + 1):
            sock = login_user(args, uid)
            sockets.append(sock)
            user_sockets[uid] = sock
            send_locks[uid] = threading.Lock()
            t = threading.Thread(target=reader_loop, args=(uid, sock, stats, stop_event), daemon=True)
            t.start()
            readers.append(t)

        sender_ids = list(range(1, args.senders + 1))
        sender_sockets = {uid: user_sockets[uid] for uid in sender_ids}
        heartbeat_thread = start_heartbeat_thread(args, user_sockets, send_locks, stats, stop_event)
        print("Warmup...")
        time.sleep(args.warmup)

        print(
            f"Running benchmark for {args.duration:.2f}s at {args.qps:.2f} msg/s "
            f"with {args.sender_workers or min(args.senders, 8)} sender workers..."
        )
        sent, elapsed = run_send_phase(
            args,
            sender_ids,
            sender_sockets,
            send_locks,
            stats,
            stop_event,
        )
        print(f"Send phase finished. Waiting {args.drain_time:.2f}s for pending messages...")
        time.sleep(args.drain_time)
        summary = print_report(args, stats, sent, elapsed, sender_ids)
        write_csv_report(args, summary, sender_ids)

    finally:
        stop_event.set()
        if "heartbeat_thread" in locals() and heartbeat_thread is not None:
            heartbeat_thread.join(timeout=1.0)
        close_sockets(sockets)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run a group-chat benchmark.")
    parser.add_argument("--server-host", default="127.0.0.1")
    parser.add_argument("--server-port", type=int, default=12345)
    parser.add_argument("--connect-timeout", type=float, default=5.0)

    parser.add_argument("--group-id", type=int, default=1)
    parser.add_argument("--total-users", type=int, default=100)
    parser.add_argument("--local-online", type=int, default=60)
    parser.add_argument("--remote-online", type=int, default=20)
    parser.add_argument("--senders", type=int, default=1)

    parser.add_argument("--duration", type=float, default=60.0)
    parser.add_argument("--qps", type=float, default=100.0)
    parser.add_argument(
        "--qps-list",
        help="Comma-separated target QPS values. Example: 100,200,500,800,1000.",
    )
    parser.add_argument(
        "--sender-workers",
        type=int,
        default=0,
        help="Concurrent sending threads. 0 means min(senders, 8).",
    )
    parser.add_argument("--warmup", type=float, default=2.0)
    parser.add_argument("--drain-time", type=float, default=3.0)
    parser.add_argument(
        "--heartbeat-interval",
        type=float,
        default=30.0,
        help="Seconds between benchmark client heartbeats. Use 0 to disable.",
    )
    parser.add_argument("--csv-out", help="Append benchmark result to this CSV file.")

    parser.add_argument("--mysql-bin", default="mysql")
    parser.add_argument("--db-host", default="127.0.0.1")
    parser.add_argument("--db-port", type=int, default=3306)
    parser.add_argument("--db-user", default="root")
    parser.add_argument("--db-password", default="123456")
    parser.add_argument("--skip-db", action="store_true", help="Do not apply MySQL seed data.")
    parser.add_argument("--sql-out", help="Write generated SQL to this file and exit.")
    args = parser.parse_args()

    if args.local_online + args.remote_online > args.total_users:
        parser.error("local-online + remote-online must be <= total-users")
    if args.senders <= 0:
        parser.error("senders must be positive")
    if args.senders > args.local_online:
        parser.error("senders must be <= local-online")
    if args.qps <= 0:
        parser.error("qps must be positive")
    if args.qps_list:
        try:
            args.qps_values = [float(item.strip()) for item in args.qps_list.split(",") if item.strip()]
        except ValueError:
            parser.error("qps-list must contain only numbers")
        if not args.qps_values:
            parser.error("qps-list must contain at least one value")
        if any(value <= 0 for value in args.qps_values):
            parser.error("qps-list values must be positive")
    else:
        args.qps_values = [args.qps]
    if args.duration <= 0:
        parser.error("duration must be positive")
    if args.sender_workers < 0:
        parser.error("sender-workers must be >= 0")
    if args.heartbeat_interval < 0:
        parser.error("heartbeat-interval must be >= 0")
    return args


def main() -> int:
    args = parse_args()
    for index, qps in enumerate(args.qps_values, start=1):
        args.qps = qps
        if len(args.qps_values) > 1:
            print(f"\n========== QPS Run {index}/{len(args.qps_values)}: {qps:.2f} ==========")
        run_benchmark(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
