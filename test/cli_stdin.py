#!/usr/bin/env python3
"""Tests for burrTxt and burrTxt2 reading their puzzle from standard input.

The puzzle file may be named "-" or left out altogether when it is piped in.
Either way the run must match the one that names the file, whether the data
is gzip compressed (as the example files are) or plain XML. Input that holds
no puzzle has to be reported, not crash the program or leave it waiting.

burrTxt2 needs more than that: stdin is also how a user stops it, and the
name of its output file comes from the name of its input.

Every run gets a timeout, so a binary left waiting on stdin fails the test
instead of hanging the suite.
"""

import gzip
import os
import subprocess
import sys
import tempfile
from pathlib import Path

try:
    import pty
except ImportError:         # Windows
    pty = None

TIMEOUT = 120

PUZZLE = "examples/PelikanBurr.xmpuzzle"
OUTPUT = "stdin.xmpuzzlettt"
SOLVE = ["-R", "-d", "-b", "0"]   # for burrTxt2: from the start, ignoring the stored result

failures = []


def run(binary, args, stdin_data, label, cwd=None):
    """Run a solver with stdin_data on its stdin, return (returncode, output)."""
    try:
        proc = subprocess.run(
            [binary] + args,
            input=stdin_data,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=TIMEOUT,
            cwd=cwd,
        )
    except subprocess.TimeoutExpired:
        failures.append(f"{label}: did not terminate within {TIMEOUT}s")
        return None, ""
    return proc.returncode, proc.stdout.decode(errors="replace")


def check(label, condition, detail):
    if not condition:
        failures.append(f"{label}: {detail}")


def check_reported(binary, args, label):
    """Input that holds no puzzle: a message and an exit status, not a crash."""
    for what, data, status, message in [
        ("empty stdin", b"", 1, "no puzzle on standard input"),
        ("junk on stdin", b"not a puzzle", 2, "could not load the puzzle"),
    ]:
        name = f"{label}, {what}"
        rc, out = run(binary, args, data, name)
        if rc is not None:
            check(name, rc == status, f"exited {rc}\n{out}")
            check(name, message in out, f"did not say \"{message}\"\n{out}")


def check_terminal(binary, label):
    """A terminal on stdin gets an answer at once, it is never waited on."""
    if pty is None:
        return

    for name, args, message in [
        (f"{label}, terminal without a file", [], "-h, --help"),
        (f"{label}, terminal with \"-\"", ["-"], "standard input is a terminal"),
    ]:
        master, slave = pty.openpty()
        try:
            proc = subprocess.run([binary] + args, stdin=slave, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            failures.append(f"{name}: did not terminate within {TIMEOUT}s")
            continue
        finally:
            os.close(slave)
            os.close(master)
        out = proc.stdout.decode(errors="replace")
        check(name, proc.returncode == 1, f"exited {proc.returncode}\n{out}")
        check(name, message in out, f"did not say \"{message}\"\n{out}")


def test_burrtxt(binary, puzzle, compressed, plain):
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

    check_reported(binary, args, "burrTxt")
    check_terminal(binary, "burrTxt")


def solved(text):
    """What a puzzle file says its problem was solved to."""
    return [word for word in text.split() if word.startswith(("assemblies=", "solutions="))]


def test_burrtxt2(binary, compressed, plain):
    # the example is stored solved, which is what a run has to come back with
    expected = solved(plain.decode())
    check("burrTxt2 example", len(expected) == 2, f"found {expected} in the example")

    for label, extra, data in [
        ("burrTxt2 compressed, no file argument", [], compressed),
        ("burrTxt2 compressed, file \"-\"", ["-"], compressed),
        ("burrTxt2 plain XML, no file argument", [], plain),
        ("burrTxt2 plain XML, file \"-\"", ["-"], plain),
    ]:
        with tempfile.TemporaryDirectory() as cwd:
            rc, out = run(binary, SOLVE + extra, data, label, cwd)
            if rc is None:
                continue
            check(label, rc == 0, f"exited {rc}\n{out}")
            # the stdin that brought the puzzle is at its end, which must
            # not read as the user stopping the solver
            check(label, "abborting" not in out, f"stopped by its own input\n{out}")
            check(label, "done" in out, f"did not finish\n{out}")
            check(label, f"outputting into {OUTPUT}" in out,
                  f"did not name its output file\n{out}")
            result = Path(cwd) / OUTPUT
            check(label, result.exists(), f"wrote no {OUTPUT}\n{out}")
            if result.exists():
                found = solved(result.read_text())
                check(label, found == expected, f"found {found}, expected {expected}")

    # continuing it finds nothing to do, as for a named file
    label = "burrTxt2 continuing an already solved puzzle"
    with tempfile.TemporaryDirectory() as cwd:
        rc, out = run(binary, ["-"], compressed, label, cwd)
        if rc is not None:
            check(label, rc == 0, f"exited {rc}\n{out}")
            check(label, "already solved" in out, f"did not say so\n{out}")

    # the checks that fail before the output file is opened leave none behind
    with tempfile.TemporaryDirectory() as cwd:
        for name, data in [("empty", b""), ("junk", b"not a puzzle")]:
            run(binary, ["-"], data, f"burrTxt2 {name} stdin", cwd)
        check("burrTxt2 without a puzzle", not (Path(cwd) / OUTPUT).exists(),
              "wrote an output file all the same")

    check_reported(binary, [], "burrTxt2")
    check_terminal(binary, "burrTxt2")


def main():
    if len(sys.argv) != 4:
        print("usage: cli_stdin.py <path to burrTxt> <path to burrTxt2> <source root>",
              file=sys.stderr)
        return 2

    source_root = sys.argv[3]
    # absolute: Windows runs a bare relative path ("build/burrTxt.exe")
    # only while NoDefaultCurrentDirectoryInExePath is unset
    burrtxt = str(Path(sys.argv[1]).resolve())
    burrtxt2 = str(Path(sys.argv[2]).resolve())
    puzzle = Path(source_root) / PUZZLE

    compressed = puzzle.read_bytes()
    plain = gzip.decompress(compressed)

    test_burrtxt(burrtxt, puzzle, compressed, plain)
    test_burrtxt2(burrtxt2, compressed, plain)

    for f in failures:
        print(f"FAILED  {f}")

    if failures:
        return 1

    print("all stdin tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
