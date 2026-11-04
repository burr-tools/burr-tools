#!/usr/bin/env python3
"""Tests for burrTxt and burrTxt2 reading their puzzle from standard input.

The puzzle file may be named "-" or left out altogether when it is piped in.
Either way the run must match the one that names the file, whether the data
is gzip compressed (as the example files are) or plain XML. Input that holds
no puzzle has to be reported, not crash the program or leave it waiting.

burrTxt2 needs more than that: stdin is also how a user stops it, and a piped
puzzle has no name to put an output file beside, so its result goes to stdout
and its messages to stderr. A run on a named file keeps writing a file.

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
SOLVE = ["-R", "-d", "-b", "0"]   # for burrTxt2: from the start, ignoring the stored result

failures = []


def execute(binary, args, stdin_data, label, cwd, stderr):
    """Run a solver with stdin_data on its stdin, return the process or None."""
    try:
        return subprocess.run(
            [binary] + args,
            input=stdin_data,
            stdout=subprocess.PIPE,
            stderr=stderr,
            timeout=TIMEOUT,
            cwd=cwd,
        )
    except subprocess.TimeoutExpired:
        failures.append(f"{label}: did not terminate within {TIMEOUT}s")
        return None


def run(binary, args, stdin_data, label, cwd=None):
    """Run a solver, return (returncode, output), stderr and stdout together."""
    proc = execute(binary, args, stdin_data, label, cwd, subprocess.STDOUT)
    if proc is None:
        return None, ""
    return proc.returncode, proc.stdout.decode(errors="replace")


def run_split(binary, args, stdin_data, label, cwd):
    """Run a solver, return (returncode, stdout, stderr)."""
    proc = execute(binary, args, stdin_data, label, cwd, subprocess.PIPE)
    if proc is None:
        return None, "", ""
    return proc.returncode, proc.stdout.decode(errors="replace"), proc.stderr.decode(errors="replace")


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
            rc, out, messages = run_split(binary, SOLVE + extra, data, label, cwd)
            if rc is None:
                continue
            check(label, rc == 0, f"exited {rc}\n{messages}")
            # the stdin that brought the puzzle is at its end, which must
            # not read as the user stopping the solver
            check(label, "abborting" not in messages, f"stopped by its own input\n{messages}")
            check(label, "done" in messages, f"no progress on stderr\n{messages}")
            check(label, out.startswith("<") and out.rstrip().endswith("</puzzle>"),
                  f"stdout is not just the puzzle\n{out[:300]}")
            check(label, solved(out) == expected, f"found {solved(out)}, expected {expected}")
            check(label, not os.listdir(cwd), f"left files behind: {os.listdir(cwd)}")

            # what comes out goes back in: the next program in a pipe
            again = f"{label}, its result piped back in"
            rc, out, messages = run_split(binary, ["-"], out.encode(), again, cwd)
            if rc is not None:
                check(again, rc == 0, f"exited {rc}\n{messages}")
                check(again, "already solved" in messages, f"did not say so\n{messages}")

    # no puzzle, no result: stdout stays empty for whatever it is redirected to
    with tempfile.TemporaryDirectory() as cwd:
        for name, data in [("empty", b""), ("junk", b"not a puzzle")]:
            label = f"burrTxt2 {name} stdin"
            rc, out, _ = run_split(binary, ["-"], data, label, cwd)
            check(label, rc is None or out == "", f"wrote to stdout\n{out}")
        check("burrTxt2 without a puzzle", not os.listdir(cwd),
              f"left files behind: {os.listdir(cwd)}")

    # a named file goes on as it always has: the result beside it, the
    # messages on stdout. A stdin at its end stops the solver at once, which
    # does not matter here
    label = "burrTxt2 named file"
    with tempfile.TemporaryDirectory() as cwd:
        named = Path(cwd) / "named.xmpuzzle"
        named.write_bytes(compressed)
        rc, out, messages = run_split(binary, [str(named)], b"", label, cwd)
        if rc is not None:
            check(label, rc == 0, f"exited {rc}\n{out}\n{messages}")
            check(label, f"outputting into {named}ttt" in out, f"did not say so\n{out}")
            check(label, "<puzzle" not in out, f"wrote the result to stdout\n{out[:300]}")
            check(label, Path(f"{named}ttt").exists(), f"wrote no file\n{out}")

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
