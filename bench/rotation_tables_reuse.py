#!/usr/bin/env python3
# BurrTools
#
# Copyright (C) 2026 Arne Köhn <arne@chark.eu>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

"""
Solve several puzzles one after another in one process, with rotation
moves, and time each solve: the use case of trying designs of one size in
sequence.

Two modes, interleaved per round from the same build:
  kept     the rotation tables stay alive between solves (the default)
  rebuild  burrtools.release_rotation_tables() before every solve, so each
           solve builds its tables again (the behaviour before 2026-10-28)

Each round solves all puzzles in order in one mode; the first solve of a
kept round starts from released tables, so it pays the build like rebuild.

  PYTHONPATH=build-rel python3 bench/rotation_tables_reuse.py --runs 3 puzzles/TICs/decoded/*.xmpuzzle

Use the release build (build-rel, see AGENTS.md "Benchmarks must use the
release binary").
"""

import argparse
import statistics
import sys
import time

import burrtools


def solve(path, threads):
    """Seconds for one full solve with disassembly and rotation moves, and
    the number of solutions found."""
    start = time.perf_counter()
    solutions = 0
    for problem in burrtools.load(path).problems:
        for sol in problem.solve(disassemble=True, rotation_moves=True, threads=threads):
            solutions += sol.has_disassembly
    return time.perf_counter() - start, solutions


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("puzzles", nargs="+", help=".xmpuzzle files, solved in this order")
    parser.add_argument("--runs", type=int, default=3, help="rounds per mode (default 3)")
    parser.add_argument("--threads", type=int, default=0, help="solver threads (default 0: all cores)")
    args = parser.parse_args()

    modes = ("kept", "rebuild")
    times = {m: {p: [] for p in args.puzzles} for m in modes}
    solutions = {}

    for run in range(args.runs):
        for mode in (modes if run % 2 == 0 else reversed(modes)):
            burrtools.release_rotation_tables()
            for path in args.puzzles:
                if mode == "rebuild":
                    burrtools.release_rotation_tables()
                seconds, found = solve(path, args.threads)
                times[mode][path].append(seconds)
                if solutions.setdefault(path, found) != found:
                    sys.exit(f"{path}: {found} solutions in {mode}, {solutions[path]} before")
            print(f"round {run + 1} {mode}: {sum(times[mode][p][-1] for p in args.puzzles):.2f} s", file=sys.stderr)

    width = max(len(p.rsplit("/", 1)[-1]) for p in args.puzzles)
    print(f"{'puzzle':<{width}}  {'kept':>8}  {'rebuild':>8}  {'saved':>6}  solutions")
    total = {m: 0.0 for m in modes}
    for path in args.puzzles:
        med = {m: statistics.median(times[m][path]) for m in modes}
        for m in modes:
            total[m] += med[m]
        saved = 1 - med["kept"] / med["rebuild"] if med["rebuild"] > 0 else 0
        print(f"{path.rsplit('/', 1)[-1]:<{width}}  {med['kept']:8.3f}  {med['rebuild']:8.3f}  {saved:6.0%}  {solutions[path]}")
    saved = 1 - total["kept"] / total["rebuild"] if total["rebuild"] > 0 else 0
    print(f"{'total (medians)':<{width}}  {total['kept']:8.3f}  {total['rebuild']:8.3f}  {saved:6.0%}")


if __name__ == "__main__":
    main()
