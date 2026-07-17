#!/usr/bin/env python3
"""
Benchmark the old N+1 group query against the optimized JOIN query.

The script uses the local mysql command-line client and does not require
extra Python packages. It prepares isolated benchmark data with high IDs,
then measures the query path used during login group loading.
"""

import argparse
import csv
import io
import statistics
import subprocess
import sys
import time
from typing import Dict, List, Tuple


PASSWORD_HASH = "8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92"


def mysql_cmd(args: argparse.Namespace) -> List[str]:
    cmd = [
        args.mysql_bin,
        "-h",
        args.db_host,
        "-P",
        str(args.db_port),
        "-u",
        args.db_user,
        "--database",
        args.db_name,
        "--batch",
        "--raw",
    ]
    if args.db_password:
        cmd.append(f"-p{args.db_password}")
    return cmd


def run_mysql(args: argparse.Namespace, sql: str) -> str:
    proc = subprocess.run(
        mysql_cmd(args),
        input=sql.encode("utf-8"),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr.decode("utf-8", errors="replace"))
        raise RuntimeError("mysql command failed")
    return proc.stdout.decode("utf-8", errors="replace")


def sql_string(value: str) -> str:
    return "'" + value.replace("\\", "\\\\").replace("'", "''") + "'"


def build_seed_sql(args: argparse.Namespace) -> str:
    user_start = args.user_start
    user_end = user_start + args.users - 1
    group_start = args.group_start
    group_end = group_start + args.groups - 1
    login_user = user_start

    users = []
    for uid in range(user_start, user_end + 1):
        users.append(
            f"({uid}, 'gqb_user_{uid}', '{PASSWORD_HASH}', 'offline')"
        )

    groups = []
    for offset, gid in enumerate(range(group_start, group_end + 1), start=1):
        groups.append(
            f"({gid}, 'gqb_group_{offset}', 'group query benchmark')"
        )

    group_users = []
    for gid in range(group_start, group_end + 1):
        group_users.append(f"({gid}, {login_user}, 'creator')")
        for n in range(1, args.members_per_group):
            member_uid = user_start + ((gid - group_start) * args.members_per_group + n) % args.users
            if member_uid == login_user:
                member_uid = user_start + ((member_uid - user_start + 1) % args.users)
            group_users.append(f"({gid}, {member_uid}, 'normal')")

    return f"""
CREATE DATABASE IF NOT EXISTS `{args.db_name}`
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_0900_ai_ci;

USE `{args.db_name}`;

DELETE FROM OfflineMessage WHERE userid BETWEEN {user_start} AND {user_end};
DELETE FROM GroupUser
WHERE groupid BETWEEN {group_start} AND {group_end}
   OR userid BETWEEN {user_start} AND {user_end};
DELETE FROM Friend
WHERE userid BETWEEN {user_start} AND {user_end}
   OR friendid BETWEEN {user_start} AND {user_end};
DELETE FROM AllGroup WHERE id BETWEEN {group_start} AND {group_end};
DELETE FROM user WHERE id BETWEEN {user_start} AND {user_end};

INSERT INTO user (id, name, password, state) VALUES
{",\n".join(users)};

INSERT INTO AllGroup (id, groupname, groupdesc) VALUES
{",\n".join(groups)};

INSERT INTO GroupUser (groupid, userid, grouprole) VALUES
{",\n".join(group_users)};
"""


def parse_tsv(output: str) -> List[Dict[str, str]]:
    rows = list(csv.reader(io.StringIO(output), delimiter="\t"))
    if not rows:
        return []
    header = rows[0]
    return [dict(zip(header, row)) for row in rows[1:] if row]


def fetch_group_ids(args: argparse.Namespace, login_user: int) -> List[int]:
    sql = f"""
SELECT a.id
FROM AllGroup a
INNER JOIN GroupUser b ON a.id = b.groupid
WHERE b.userid = {login_user}
ORDER BY a.id;
"""
    out = run_mysql(args, sql)
    return [int(row["id"]) for row in parse_tsv(out)]


def time_query(args: argparse.Namespace, sql: str) -> Tuple[float, int]:
    start = time.perf_counter()
    out = run_mysql(args, sql)
    elapsed_ms = (time.perf_counter() - start) * 1000
    rows = parse_tsv(out)
    return elapsed_ms, len(rows)


def run_old_query_once(args: argparse.Namespace, login_user: int, group_ids: List[int]) -> Tuple[float, int, int]:
    total_ms = 0.0
    total_rows = 0
    query_count = 0

    group_sql = f"""
SELECT a.id,a.groupname,a.groupdesc
FROM AllGroup a
INNER JOIN GroupUser b ON a.id = b.groupid
WHERE b.userid = {login_user}
ORDER BY a.id;
"""
    elapsed_ms, rows = time_query(args, group_sql)
    total_ms += elapsed_ms
    total_rows += rows
    query_count += 1

    for gid in group_ids:
        member_sql = f"""
SELECT a.id,a.name,a.state,b.grouprole
FROM user a
INNER JOIN GroupUser b ON a.id = b.userid
WHERE b.groupid = {gid}
ORDER BY a.id;
"""
        elapsed_ms, rows = time_query(args, member_sql)
        total_ms += elapsed_ms
        total_rows += rows
        query_count += 1

    return total_ms, total_rows, query_count


def run_join_query_once(args: argparse.Namespace, login_user: int) -> Tuple[float, int, int]:
    sql = f"""
SELECT
    g.id,
    g.groupname,
    g.groupdesc,
    u.id AS userid,
    u.name,
    u.state,
    gm.grouprole
FROM GroupUser self
INNER JOIN AllGroup g
    ON self.groupid = g.id
INNER JOIN GroupUser gm
    ON self.groupid = gm.groupid
INNER JOIN user u
    ON gm.userid = u.id
WHERE self.userid = {login_user}
ORDER BY g.id, u.id;
"""
    elapsed_ms, rows = time_query(args, sql)
    return elapsed_ms, rows, 1


def explain_join(args: argparse.Namespace, login_user: int) -> str:
    sql = f"""
EXPLAIN
SELECT
    g.id,
    g.groupname,
    g.groupdesc,
    u.id AS userid,
    u.name,
    u.state,
    gm.grouprole
FROM GroupUser self
INNER JOIN AllGroup g
    ON self.groupid = g.id
INNER JOIN GroupUser gm
    ON self.groupid = gm.groupid
INNER JOIN user u
    ON gm.userid = u.id
WHERE self.userid = {login_user}
ORDER BY g.id, u.id;
"""
    return run_mysql(args, sql)


def summarize(label: str, timings: List[float], rows: int, query_count: int) -> None:
    avg = statistics.mean(timings)
    best = min(timings)
    worst = max(timings)
    print(f"{label}:")
    print(f"  SQL requests per login: {query_count}")
    print(f"  rows returned per login: {rows}")
    print(f"  avg time: {avg:.2f} ms")
    print(f"  best time: {best:.2f} ms")
    print(f"  worst time: {worst:.2f} ms")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare old N+1 group loading with optimized JOIN loading."
    )
    parser.add_argument("--mysql-bin", default="mysql")
    parser.add_argument("--db-host", default="127.0.0.1")
    parser.add_argument("--db-port", type=int, default=3306)
    parser.add_argument("--db-user", default="root")
    parser.add_argument("--db-password", default="12345")
    parser.add_argument("--db-name", default="chat")
    parser.add_argument("--user-start", type=int, default=900001)
    parser.add_argument("--group-start", type=int, default=910001)
    parser.add_argument("--users", type=int, default=2000)
    parser.add_argument("--groups", type=int, default=100)
    parser.add_argument("--members-per-group", type=int, default=50)
    parser.add_argument("--iterations", type=int, default=5)
    parser.add_argument("--skip-seed", action="store_true")
    parser.add_argument("--show-explain", action="store_true")
    args = parser.parse_args()

    if args.users < 2:
        parser.error("--users must be at least 2")
    if args.groups < 1:
        parser.error("--groups must be positive")
    if args.members_per_group < 2:
        parser.error("--members-per-group must be at least 2")
    if args.iterations < 1:
        parser.error("--iterations must be positive")

    login_user = args.user_start

    if not args.skip_seed:
        print("Preparing benchmark data...")
        run_mysql(args, build_seed_sql(args))

    group_ids = fetch_group_ids(args, login_user)
    if not group_ids:
        raise RuntimeError(f"login user {login_user} has no benchmark groups")

    print("\nBenchmark data:")
    print(f"  login user: {login_user}")
    print(f"  groups joined: {len(group_ids)}")
    print(f"  members per group: {args.members_per_group}")
    print(f"  iterations: {args.iterations}")

    old_timings: List[float] = []
    join_timings: List[float] = []
    old_rows = old_queries = join_rows = join_queries = 0

    for _ in range(args.iterations):
        elapsed, rows, queries = run_old_query_once(args, login_user, group_ids)
        old_timings.append(elapsed)
        old_rows = rows
        old_queries = queries

        elapsed, rows, queries = run_join_query_once(args, login_user)
        join_timings.append(elapsed)
        join_rows = rows
        join_queries = queries

    print("\n========== Group Query Benchmark ==========")
    summarize("Old N+1 query", old_timings, old_rows, old_queries)
    summarize("Optimized JOIN query", join_timings, join_rows, join_queries)

    speedup = statistics.mean(old_timings) / statistics.mean(join_timings)
    saved_queries = old_queries - join_queries
    print("\nComparison:")
    print(f"  SQL requests reduced: {old_queries} -> {join_queries} (-{saved_queries})")
    print(f"  average speedup: {speedup:.2f}x")

    if args.show_explain:
        print("\nEXPLAIN for optimized JOIN:")
        print(explain_join(args, login_user))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
