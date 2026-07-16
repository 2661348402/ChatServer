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
import json
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


@dataclass
class Stats:
    lock: threading.Lock = field(default_factory=threading.Lock)
    latencies_us: List[int] = field(default_factory=list)
    received: int = 0
    parse_errors: int = 0
    disconnects: int = 0
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


def print_report(
    args: argparse.Namespace,
    stats: Stats,
    sent: int,
    elapsed: float,
    sender_ids: List[int],
) -> None:
    with stats.lock:
        latencies = list(stats.latencies_us)
        received = stats.received
        parse_errors = stats.parse_errors
        disconnects = stats.disconnects
        recv_without_ts = stats.recv_without_ts

    latencies.sort()

    local_receivers_per_message = max(args.local_online - len(sender_ids), 0)
    expected_local_received = sent * local_receivers_per_message
    missing_local_messages = max(expected_local_received - received, 0)

    throughput = received / elapsed if elapsed > 0 else 0
    avg_ms = (sum(latencies) / len(latencies) / 1000) if latencies else 0
    p95_ms = percentile(latencies, 95) / 1000
    p99_ms = percentile(latencies, 99) / 1000
    min_ms = (latencies[0] / 1000) if latencies else 0
    max_ms = (latencies[-1] / 1000) if latencies else 0

    total_errors = parse_errors + disconnects + recv_without_ts + missing_local_messages

    print("\n========== Benchmark Result ==========")
    print(f"duration: {elapsed:.2f} s")
    print(f"group id: {args.group_id}")
    print(f"total group users: {args.total_users}")
    print(f"local online users: {args.local_online}")
    print(f"remote-online users: {args.remote_online}")
    print(f"offline users: {args.total_users - args.local_online - args.remote_online}")
    print(f"sender ids: {','.join(str(uid) for uid in sender_ids)}")
    print(f"target send qps: {args.qps:.2f}")
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
    print(f"messages without timestamp: {recv_without_ts}")
    print(f"missing local messages: {missing_local_messages}")
    print(f"total errors: {total_errors}")


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

    print(f"Logging in users 1-{args.local_online} to {args.server_host}:{args.server_port}...")
    try:
        for uid in range(1, args.local_online + 1):
            sock = login_user(args, uid)
            sockets.append(sock)
            t = threading.Thread(target=reader_loop, args=(uid, sock, stats, stop_event), daemon=True)
            t.start()
            readers.append(t)

        sender_ids = list(range(1, args.senders + 1))
        sender_sockets = {uid: sockets[uid - 1] for uid in sender_ids}
        print("Warmup...")
        time.sleep(args.warmup)

        interval = 1.0 / args.qps if args.qps > 0 else 0
        sent = 0
        next_send_time = time.monotonic()
        start = time.monotonic()
        end = start + args.duration

        print(f"Running benchmark for {args.duration:.2f}s at {args.qps:.2f} msg/s...")
        while time.monotonic() < end:
            now = time.monotonic()
            if interval > 0 and now < next_send_time:
                time.sleep(min(next_send_time - now, 0.001))
                continue

            sender_id = sender_ids[sent % len(sender_ids)]
            seq = sent + 1
            payload = {
                "msgId": GROUP_CHAT_MSG,
                "id": sender_id,
                "groupId": args.group_id,
                "message": f"benchmark-{seq}",
                "seq": seq,
                "send_ts_us": time.monotonic_ns() // 1000,
            }
            send_frame(sender_sockets[sender_id], payload)
            sent += 1
            next_send_time += interval

        elapsed = time.monotonic() - start
        print(f"Send phase finished. Waiting {args.drain_time:.2f}s for pending messages...")
        time.sleep(args.drain_time)
        print_report(args, stats, sent, elapsed, sender_ids)

    finally:
        stop_event.set()
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
    parser.add_argument("--warmup", type=float, default=2.0)
    parser.add_argument("--drain-time", type=float, default=3.0)

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
    if args.duration <= 0:
        parser.error("duration must be positive")
    return args


def main() -> int:
    run_benchmark(parse_args())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
