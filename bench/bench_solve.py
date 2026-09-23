#!/usr/bin/env python3
"""Solver benchmark: time burrTxt assemble+disassemble runs over a puzzle corpus.

Single-binary mode (timings CSV to stdout):
    bench/bench_solve.py --binary build/burrTxt [--runs 5] [puzzle ...]

A/B mode (interleaved, shared core, for before/after comparisons):
    bench/bench_solve.py --ab old/burrTxt new/burrTxt [--runs 8] [--cpu 7] [puzzle ...]

If no puzzles are given, a built-in corpus of example + BTFiles puzzles is
used (BTFiles lives in puzzles/, which is gitignored). Runs assemble
problem 0 and, unless --no-disassemble is given, disassemble (`-d`).
Stats lines (assemblies/solutions/iterations) are captured so correctness
can be checked alongside speed. Not wired into meson/CI; scratch tooling.
"""
import argparse
import os
import selectors
import signal
import socket
import statistics
import subprocess
import sys
import time

CORPUS = [
    "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:1",
    "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:2",
    "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:0",
    "puzzles/BTFiles/James Fortune/kangaroo.xmpuzzle",
    "examples/PelikanBurr.xmpuzzle",
    "examples/SolidSixPieceBurrs.xmpuzzle",
    "puzzles/BTFiles/Tyler Hudson/Third_Times_the_Charm.xmpuzzle",
    "puzzles/BTFiles/Jack Krijnen/Simplicity.xmpuzzle",
    "puzzles/BTFiles/Jack Krijnen/Excelsior.xmpuzzle",
    "puzzles/BTFiles/Tom Messina/CD_Pack.xmpuzzle",
]


def run_cmd_rusage(cmd, timeout=600):
    """Run command measuring elapsed time, CPU time, and peak RSS via os.wait4."""
    r_fd, w_fd = os.pipe()
    t0 = time.monotonic()
    pid = os.fork()
    if pid == 0:
        os.close(r_fd)
        os.dup2(w_fd, 1)
        os.dup2(w_fd, 2)
        os.close(w_fd)
        try:
            os.execvp(cmd[0], cmd)
        except Exception:
            os._exit(127)

    os.close(w_fd)
    os.set_blocking(r_fd, False)

    sel = selectors.DefaultSelector()
    sel.register(r_fd, selectors.EVENT_READ)

    output = bytearray()
    deadline = t0 + timeout
    timed_out = False

    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            timed_out = True
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            break

        events = sel.select(timeout=min(max(remaining, 0.05), 0.5))
        for key, mask in events:
            chunk = os.read(r_fd, 65536)
            if chunk:
                output.extend(chunk)
            else:
                sel.unregister(r_fd)
                os.close(r_fd)
                r_fd = -1
                break
        if r_fd == -1:
            break

    if r_fd != -1:
        sel.unregister(r_fd)
        os.close(r_fd)

    _, status, rusage = os.wait4(pid, 0)
    wall_s = time.monotonic() - t0
    exit_code = 124 if timed_out else os.waitstatus_to_exitcode(status)
    user_s = rusage.ru_utime
    sys_s = rusage.ru_stime
    # On Linux, ru_maxrss is in Kilobytes
    maxrss_mb = rusage.ru_maxrss / 1024.0

    return {
        "wall_s": wall_s,
        "user_s": user_s,
        "sys_s": sys_s,
        "maxrss_mb": maxrss_mb,
        "exit_code": exit_code,
        "stdout": output.decode("utf-8", "replace"),
    }


def run_once(cmd, puzzle, timeout=600):
    prob_idx = "0"
    puzzle_path = puzzle
    if ":" in puzzle:
        parts = puzzle.rsplit(":", 1)
        if parts[1].isdigit() or parts[1] == "all":
            puzzle_path, prob_idx = parts[0], parts[1]

    full_cmd = cmd + ["-q", "-o", prob_idx, puzzle_path]
    res = run_cmd_rusage(full_cmd, timeout=timeout)
    text = res["stdout"]
    stats = next((l.strip() for l in text.splitlines() if "assemblies and" in l),
                 text.strip().splitlines()[-1] if text.strip() else "")
    cpu_pct = ((res["user_s"] + res["sys_s"]) / res["wall_s"] * 100.0) if res["wall_s"] > 0 else 0.0
    return {
        "wall_s": res["wall_s"],
        "user_s": res["user_s"],
        "sys_s": res["sys_s"],
        "cpu_pct": cpu_pct,
        "maxrss_mb": res["maxrss_mb"],
        "exit_code": res["exit_code"],
        "stats": stats,
    }


def git_info():
    """Identify the measured source tree: full hash, subject, dirty flag.

    Returns 'unknown' fields when git is unavailable or cwd is not a repo,
    so metadata collection can never break a benchmark run.
    """
    def run(*git_args):
        try:
            out = subprocess.run(
                ["git"] + list(git_args),
                capture_output=True, text=True, timeout=10)
            return out.stdout.strip() if out.returncode == 0 else "unknown"
        except Exception:
            return "unknown"

    status = run("status", "--porcelain")
    dirty = "unknown" if status == "unknown" else ("dirty" if status else "clean")
    return {
        "commit": run("rev-parse", "HEAD"),
        "subject": run("log", "-1", "--format=%s"),
        "worktree": dirty,
    }


def run_metadata(args):
    """'# key: value' comment lines describing exactly how this run was made."""
    info = git_info()
    if args.ab:
        mode = f"ab before={args.ab[0]} after={args.ab[1]}"
    else:
        mode = f"single binary={args.binary}"
    try:
        host = socket.gethostname()
    except Exception:
        host = "unknown"
    return [
        "# burrtools solver benchmark",
        f"# timestamp_utc: {time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())}",
        f"# mode: {mode}",
        f"# git_commit: {info['commit']}",
        f"# git_subject: {info['subject']}",
        f"# git_worktree: {info['worktree']}",
        f"# host: {host} cpus={os.cpu_count()}",
        f"# runs: {args.runs} timeout_s: {args.timeout} "
        f"threads: {args.threads} disassemble: {not args.no_disassemble} "
        f"cpu_pin: {args.cpu}",
    ]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary", help="burrTxt binary for single mode")
    ap.add_argument("--ab", nargs=2, metavar=("BEFORE", "AFTER"),
                    help="two burrTxt binaries for interleaved A/B mode")
    ap.add_argument("--runs", type=int, default=5)
    ap.add_argument("--timeout", type=int, default=600,
                    help="timeout in seconds per solve (default: 600s = 10min)")

    ap.add_argument("--cpu", default=None, help="pin runs via taskset -c CPU")
    ap.add_argument("--no-disassemble", action="store_true",
                    help="assemble only (omit -d): measures the assembler share")
    ap.add_argument("--threads", type=int, default=None,
                    help="pass -t THREADS to burrTxt (0 = auto)")
    ap.add_argument("--output", "-O", help="output file for CSV results")
    ap.add_argument("puzzles", nargs="*", default=CORPUS)
    args = ap.parse_args()

    def cmd(binary):
        c = (["taskset", "-c", args.cpu] if args.cpu else []) + [binary]
        if args.threads is not None:
            c = c + ["-t", str(args.threads)]
        if not args.no_disassemble:
            c = c + ["-d"]
        return c

    out_file = open(args.output, "w") if args.output else None

    def log(line):
        print(line, flush=True)
        if out_file:
            out_file.write(line + "\n")
            out_file.flush()

    # Provenance header: how exactly this run was made (git commit, host,
    # flags). '#'-prefixed so CSV consumers can skip these lines.
    for line in run_metadata(args):
        log(line)
    log("tag,puzzle,run,wall_s,user_s,sys_s,cpu_pct,max_rss_mb,exit,stats")
    runs_data = {}
    if args.ab:
        tags = (("before", args.ab[0]), ("after", args.ab[1]))
        for i in range(args.runs):
            for tag, binary in tags:
                for puzzle in args.puzzles:
                    r = run_once(cmd(binary), puzzle, timeout=args.timeout)
                    log(f"{tag},{puzzle},{i},{r['wall_s']:.2f},{r['user_s']:.2f},{r['sys_s']:.2f},{r['cpu_pct']:.1f}%,{r['maxrss_mb']:.1f}MB,{r['exit_code']},{r['stats']}")
                    runs_data.setdefault((tag, puzzle), []).append(r)

        # Print Wall Clock & Speedup summary table
        log("\n=== Wall Clock & Speedup (Medians) ===")
        log(f"{'Puzzle':<55} {'Before (s)':>10} {'After (s)':>10} {'Speedup':>10} {'Before CPU%':>12} {'After CPU%':>12}")
        log("-" * 115)
        for puzzle in args.puzzles:
            b_list = runs_data.get(("before", puzzle), [])
            a_list = runs_data.get(("after", puzzle), [])
            if b_list and a_list:
                med_b_wall = statistics.median([x["wall_s"] for x in b_list])
                med_a_wall = statistics.median([x["wall_s"] for x in a_list])
                med_b_cpu = statistics.median([x["cpu_pct"] for x in b_list])
                med_a_cpu = statistics.median([x["cpu_pct"] for x in a_list])
                speedup = med_b_wall / med_a_wall if med_a_wall > 0 else 1.0
                short_name = puzzle.replace("puzzles/BTFiles/", "").replace("examples/", "")
                log(f"{short_name:<55} {med_b_wall:>10.2f} {med_a_wall:>10.2f} {speedup:>9.2f}x {med_b_cpu:>11.1f}% {med_a_cpu:>11.1f}%")

        # Print Peak Resident Memory summary table
        log("\n=== Peak Resident Memory / RSS (Medians) ===")
        log(f"{'Puzzle':<55} {'Before (MB)':>12} {'After (MB)':>12} {'Delta (MB)':>12} {'Delta (%)':>12}")
        log("-" * 107)
        for puzzle in args.puzzles:
            b_list = runs_data.get(("before", puzzle), [])
            a_list = runs_data.get(("after", puzzle), [])
            if b_list and a_list:
                med_b_rss = statistics.median([x["maxrss_mb"] for x in b_list])
                med_a_rss = statistics.median([x["maxrss_mb"] for x in a_list])
                delta_mb = med_a_rss - med_b_rss
                delta_pct = (delta_mb / med_b_rss * 100.0) if med_b_rss > 0 else 0.0
                short_name = puzzle.replace("puzzles/BTFiles/", "").replace("examples/", "")
                log(f"{short_name:<55} {med_b_rss:>11.2f}M {med_a_rss:>11.2f}M {delta_mb:>+11.2f}M {delta_pct:>+11.1f}%")
    else:
        if not args.binary:
            ap.error("need --binary or --ab")
        for puzzle in args.puzzles:
            for i in range(args.runs):
                r = run_once(cmd(args.binary), puzzle, timeout=args.timeout)
                log(f"single,{puzzle},{i},{r['wall_s']:.2f},{r['user_s']:.2f},{r['sys_s']:.2f},{r['cpu_pct']:.1f}%,{r['maxrss_mb']:.1f}MB,{r['exit_code']},{r['stats']}")


    if out_file:
        out_file.close()


if __name__ == "__main__":
    sys.exit(main())
