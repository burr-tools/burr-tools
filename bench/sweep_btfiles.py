"""Screen BTFiles puzzles: solve problem 0 (assemble+disassemble), record stats.

Runs build/burrTxt -d -q -o 0 with a per-puzzle timeout, in parallel.
Writes CSV: relpath, exit, elapsed_s, assemblies, solutions, iterations, note

The BTFiles corpus itself lives in puzzles/BTFiles/ (gitignored,
personal-use license; source https://brettkuehner.com/btfiles/) and is not
part of the repo, so a fresh clone can only run the examples/ subset until
those files are fetched.
"""
import csv
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "build", "burrTxt")
TIMEOUT = 90
JOBS = 6
OUT = os.path.join(ROOT, "btfiles_sweep.csv")

STAT = re.compile(r"(\d+) assemblies and (\d+) solutions found with (\d+) iterations")


def all_puzzles():
    base = os.path.join(ROOT, "puzzles", "BTFiles")
    out = []
    for dirpath, _dirs, files in os.walk(base):
        for fn in sorted(files):
            if fn.endswith(".xmpuzzle"):
                full = os.path.join(dirpath, fn)
                out.append(os.path.relpath(full, ROOT))
    return sorted(out)


def run_one(rel):
    t0 = time.monotonic()
    try:
        p = subprocess.run(
            [BIN, "-d", "-q", "-o", "0", rel],
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=TIMEOUT,
        )
        el = time.monotonic() - t0
        text = p.stdout.decode("utf-8", "replace")
        m = STAT.search(text)
        if m:
            return (rel, p.returncode, round(el, 2), int(m.group(1)), int(m.group(2)), int(m.group(3)), "")
        last = text.strip().splitlines()[-1] if text.strip() else ""
        return (rel, p.returncode, round(el, 2), "", "", "", last[:100])
    except subprocess.TimeoutExpired:
        return (rel, "timeout", TIMEOUT, "", "", "", "")


def main():
    puzzles = all_puzzles()
    print(f"{len(puzzles)} puzzles, {JOBS} parallel, {TIMEOUT}s timeout", flush=True)
    rows = []
    with ThreadPoolExecutor(max_workers=JOBS) as ex:
        for i, row in enumerate(ex.map(run_one, puzzles), 1):
            rows.append(row)
            if i % 25 == 0:
                print(f"{i}/{len(puzzles)}", flush=True)
    with open(OUT, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["relpath", "exit", "elapsed_s", "assemblies", "solutions", "iterations", "note"])
        w.writerows(rows)
    print(f"wrote {OUT}")


if __name__ == "__main__":
    sys.exit(main())
