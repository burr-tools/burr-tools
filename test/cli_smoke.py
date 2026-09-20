#!/usr/bin/env python3
"""Smoke tests for the burrTxt2 command line tool.

These drive the real binary, because the behaviour they pin down lives in
main(): which runs it refuses to start, and that every run terminates and
says why. A run that cannot do useful work has to say so and exit; none may
end silently or abort.

Every run gets a timeout, so a wedged binary fails the test instead of
hanging the suite.
"""

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

TIMEOUT = 120

failures = []


def run(binary, args, puzzle, source_root, label):
    """Run burrTxt2 on a scratch copy of a puzzle, return (returncode, output).

    burrTxt2 select()s on stdin and takes readable-stdin as "the user asked to
    abort". /dev/null reads ready at once, and a subprocess.PIPE would be
    closed by communicate() - either way the solver would abort before it
    reached the state under test. So we hand it the read end of a pipe and
    hold the write end open here for as long as the run lasts.
    """
    with tempfile.TemporaryDirectory() as tmp:
        scratch = Path(tmp) / Path(puzzle).name
        shutil.copy(Path(source_root) / puzzle, scratch)

        stdin_r, stdin_w = os.pipe()
        try:
            proc = subprocess.Popen(
                [binary] + args + [str(scratch)],
                stdin=stdin_r,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            os.close(stdin_r)
            stdin_r = None

            try:
                out, _ = proc.communicate(timeout=TIMEOUT)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.communicate()
                failures.append(f"{label}: did not terminate within {TIMEOUT}s")
                return None, ""
        finally:
            if stdin_r is not None:
                os.close(stdin_r)
            os.close(stdin_w)

        return proc.returncode, out


def check(label, condition, detail):
    if not condition:
        failures.append(f"{label}: {detail}")


def main():
    if len(sys.argv) != 3:
        print("usage: cli_smoke.py <path to burrTxt2> <source root>", file=sys.stderr)
        return 2

    binary, source_root = sys.argv[1], sys.argv[2]

    # A grid without a disassembler must be refused up front, before anything
    # builds one: the disassembler asserts on the movement cache such a grid
    # does not provide, on the main thread where nothing catches it.
    label = "disassembly on a grid that has no disassembler"
    rc, out = run(binary, ["-R", "-d", "-b", "0"], "examples/BrokenSticks.xmpuzzle",
                  source_root, label)
    if rc is not None:
        check(label, rc != -6 and "assert_exception" not in out,
              f"aborted through an uncaught assert (rc={rc})\n{out}")
        check(label, "disassembler" in out,
              f"did not explain that the grid has no disassembler\n{out}")

    # The same grid assembles fine, so without -d the run must still work.
    label = "assembly on a grid that has no disassembler"
    rc, out = run(binary, ["-R", "-b", "0"], "examples/BrokenSticks.xmpuzzle",
                  source_root, label)
    if rc is not None:
        check(label, rc == 0, f"exited {rc}\n{out}")
        check(label, "done" in out, f"did not finish\n{out}")

    # Continuing a file that is stored solved has nothing to continue. It used
    # to poll a solver thread that had already died, forever and silently.
    label = "continuing an already solved file"
    rc, out = run(binary, [], "examples/PelikanBurr.xmpuzzle", source_root, label)
    if rc is not None:
        check(label, rc == 0, f"exited {rc}\n{out}")
        check(label, "already solved" in out,
              f"did not report the problem as already solved\n{out}")

    # A problem index the file does not have is a user error, and must be
    # reported as one rather than asserting its way out of getProblem().
    label = "problem index out of range"
    rc, out = run(binary, ["-b", "99"], "examples/PelikanBurr.xmpuzzle", source_root, label)
    if rc is not None:
        check(label, rc != -6 and "assert_exception" not in out,
              f"aborted through an uncaught assert (rc={rc})\n{out}")
        check(label, "no problem 99" in out,
              f"did not report the bad problem index\n{out}")

    # ...and -R still solves that same file.
    label = "restarting an already solved file"
    rc, out = run(binary, ["-R", "-d"], "examples/PelikanBurr.xmpuzzle",
                  source_root, label)
    if rc is not None:
        check(label, rc == 0, f"exited {rc}\n{out}")
        check(label, "done" in out, f"did not finish\n{out}")

    for f in failures:
        print(f"FAILED  {f}")

    if failures:
        return 1

    print("all burrTxt2 cli smoke tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
