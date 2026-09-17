#!/usr/bin/env python3
"""Solver benchmark: time burrTxt assemble+disassemble runs over a puzzle corpus.

Single-binary mode (timings CSV to stdout):
    bench/bench_solve.py --binary build/burrTxt [--runs 5] [puzzle ...]

A/B mode (interleaved, shared core, for before/after comparisons):
    bench/bench_solve.py --ab old/burrTxt new/burrTxt [--runs 8] [--cpu 7] [puzzle ...]

If no puzzles are given, a built-in corpus of example + BTFiles puzzles is
used (BTFiles lives in puzzles/, which is gitignored). All runs use
`burrTxt -d -q -o 0` (assemble problem 0 and disassemble). Stats lines
(assemblies/solutions/iterations) are captured so correctness can be checked
alongside speed. Not wired into meson/CI; scratch tooling.
"""
import argparse
import subprocess
import sys
import time

CORPUS = [
    "examples/SolidSixPieceBurrs.xmpuzzle",
    "examples/PelikanBurr.xmpuzzle",
    "puzzles/BTFiles/Tyler Hudson/Third_Times_the_Charm.xmpuzzle",
    "puzzles/BTFiles/James Fortune/kangaroo.xmpuzzle",
    "puzzles/BTFiles/Jack Krijnen/Simplicity.xmpuzzle",
    "puzzles/BTFiles/Jack Krijnen/Excelsior.xmpuzzle",
    "puzzles/BTFiles/Tom Messina/CD_Pack.xmpuzzle",
]


def run_once(cmd, puzzle):
    t0 = time.monotonic()
    p = subprocess.run(
        cmd + ["-d", "-q", "-o", "0", puzzle],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
    )
    el = time.monotonic() - t0
    text = p.stdout.decode("utf-8", "replace")
    stats = next((l.strip() for l in text.splitlines() if "assemblies and" in l),
                 text.strip().splitlines()[-1] if text.strip() else "")
    return el, p.returncode, stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary", help="burrTxt binary for single mode")
    ap.add_argument("--ab", nargs=2, metavar=("BEFORE", "AFTER"),
                    help="two burrTxt binaries for interleaved A/B mode")
    ap.add_argument("--runs", type=int, default=5)
    ap.add_argument("--cpu", default=None, help="pin runs via taskset -c CPU")
    ap.add_argument("puzzles", nargs="*", default=CORPUS)
    args = ap.parse_args()

    def cmd(binary):
        return (["taskset", "-c", args.cpu] if args.cpu else []) + [binary]

    print("tag,puzzle,run,elapsed_s,exit,stats", flush=True)
    if args.ab:
        tags = (("before", args.ab[0]), ("after", args.ab[1]))
        for i in range(args.runs):
            for tag, binary in tags:
                for puzzle in args.puzzles:
                    el, ec, stats = run_once(cmd(binary), puzzle)
                    print(f"{tag},{puzzle},{i},{el:.2f},{ec},{stats}", flush=True)
    else:
        if not args.binary:
            ap.error("need --binary or --ab")
        for puzzle in args.puzzles:
            for i in range(args.runs):
                el, ec, stats = run_once(cmd(args.binary), puzzle)
                print(f"single,{puzzle},{i},{el:.2f},{ec},{stats}", flush=True)


if __name__ == "__main__":
    sys.exit(main())
