#!/usr/bin/env python3
"""Tests for burrTxt reading its puzzle from standard input.

The puzzle file may be named "-" or left out altogether when it is piped in.
Either way the run must match the one that names the file, whether the data
is gzip compressed (as the example files are) or plain XML.

Every run gets a timeout, so a binary left waiting on stdin fails the test
instead of hanging the suite.
"""

import gzip
import subprocess
import sys
from pathlib import Path

TIMEOUT = 120

PUZZLE = "examples/PelikanBurr.xmpuzzle"

failures = []


def run(binary, args, stdin_data, label):
    """Run burrTxt with stdin_data on its stdin, return (returncode, output)."""
    try:
        proc = subprocess.run(
            [binary] + args,
            input=stdin_data,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=TIMEOUT,
        )
    except subprocess.TimeoutExpired:
        failures.append(f"{label}: did not terminate within {TIMEOUT}s")
        return None, ""
    return proc.returncode, proc.stdout.decode(errors="replace")


def check(label, condition, detail):
    if not condition:
        failures.append(f"{label}: {detail}")


def main():
    if len(sys.argv) != 3:
        print("usage: cli_stdin.py <path to burrTxt> <source root>", file=sys.stderr)
        return 2

    binary, source_root = sys.argv[1], sys.argv[2]
    # absolute: Windows runs a bare relative path ("build/burrTxt.exe")
    # only while NoDefaultCurrentDirectoryInExePath is unset
    binary = str(Path(binary).resolve())
    puzzle = Path(source_root) / PUZZLE

    compressed = puzzle.read_bytes()
    plain = gzip.decompress(compressed)
    args = ["-q", "-d"]

    rc, expected = run(binary, args + [str(puzzle)], b"", "named file")
    check("named file", rc == 0 and "solutions found" in expected,
          f"exited {rc}\n{expected}")

    for label, extra, data in [
        ("compressed, no file argument", [], compressed),
        ("compressed, file \"-\"", ["-"], compressed),
        ("plain XML, no file argument", [], plain),
        ("plain XML, file \"-\"", ["-"], plain),
    ]:
        rc, out = run(binary, args + extra, data, label)
        if rc is not None:
            check(label, rc == 0, f"exited {rc}\n{out}")
            check(label, out == expected,
                  f"output differs from the named file's\n{out}\nexpected\n{expected}")

    # a named file wins over whatever stdin holds
    label = "named file with data on stdin"
    rc, out = run(binary, args + [str(puzzle)], b"not a puzzle", label)
    if rc is not None:
        check(label, rc == 0 and out == expected, f"exited {rc}\n{out}")

    # nothing piped in is a usage error, not a parser crash
    label = "empty stdin"
    rc, out = run(binary, args, b"", label)
    if rc is not None:
        check(label, rc == 1, f"exited {rc}\n{out}")
        check(label, "nothing on standard input" in out,
              f"did not say that stdin was empty\n{out}")

    for f in failures:
        print(f"FAILED  {f}")

    if failures:
        return 1

    print("all burrTxt stdin tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
