#!/usr/bin/env python3
"""Smoke tests for the burrTxt command line tool.

Like cli_smoke.py for burrTxt2: these drive the real binary, because the
behaviour they pin down lives in main(), namely which runs it refuses to
start. A run that cannot do useful work has to say so and exit, not crash.
"""

import gzip
import os
import subprocess
import sys
import tempfile

TIMEOUT = 120

failures = []


def run(binary, args, puzzle, label):
    """Run burrTxt on a puzzle, return (returncode, output)."""
    try:
        proc = subprocess.run([binary] + args + [puzzle], stdin=subprocess.DEVNULL,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        failures.append(f"{label}: did not terminate within {TIMEOUT}s")
        return None, ""
    return proc.returncode, proc.stdout


def check(label, condition, detail):
    if not condition:
        failures.append(f"{label}: {detail}")


def main():
    if len(sys.argv) != 3:
        print("usage: cli_smoke_burrtxt.py <path to burrTxt> <source root>", file=sys.stderr)
        return 2

    binary, source_root = sys.argv[1], sys.argv[2]
    sticks = f"{source_root}/examples/BrokenSticks.xmpuzzle"

    # A grid without a disassembler must be refused up front: the
    # disassembler fails on the movement cache such a grid does not provide,
    # inside a worker, and took the process down with a segfault.
    for args in (["-d", "-q"], ["-x", "-q"]):
        label = f"burrTxt {' '.join(args)} on a grid that has no disassembler"
        rc, out = run(binary, args, sticks, label)
        if rc is not None:
            check(label, rc == 2, f"exited {rc}\n{out}")
            check(label, "doesn't have a disassembler" in out,
                  f"did not explain that the grid has no disassembler\n{out}")

    # The same grid assembles fine, so without -d the run must still work.
    label = "burrTxt on a grid that has no disassembler, assembling only"
    rc, out = run(binary, ["-q"], sticks, label)
    if rc is not None:
        check(label, rc == 0, f"exited {rc}\n{out}")
        check(label, "assemblies" in out, f"did not finish\n{out}")

    # -m sets the physical model of rotation moves; nonsense is refused.
    aaa = f"{source_root}/examples/AAA_most_rotations.xmpuzzle"
    for model in ("0", "0.1", "0.1,0.5", "0.1,0,0.05", "0.1,0.05x"):
        label = f"burrTxt -m {model}"
        rc, out = run(binary, ["-d", "-R", "-q", "-m", model], aaa, label)
        if rc is not None:
            check(label, rc == 1 and "-m requires" in out, f"was not refused (rc={rc})\n{out}")

    label = "burrTxt -R with beveled cubes"
    rc, out = run(binary, ["-d", "-R", "-q", "-m", "0.081,0.05"], aaa, label)
    if rc is not None:
        check(label, rc == 0 and "1 solutions" in out, f"exited {rc}\n{out}")

    # -M names what it minimizes; anything else is refused.
    for arg in ("0.01,0.2", "play:0,0.2", "bevel:0.2,0.1", "size:0,0.1"):
        label = f"burrTxt -M {arg}"
        rc, out = run(binary, ["-d", "-R", "-q", "-M", arg], aaa, label)
        if rc is not None:
            check(label, rc == 1 and "-M requires" in out, f"was not refused (rc={rc})\n{out}")

    # -w writes the solved puzzle: the model the disassemblies are for and,
    # with -P, the play each one was measured to need; -x -w reads it back.
    with tempfile.TemporaryDirectory() as tmp:
        written = os.path.join(tmp, "solved.xmpuzzle")
        label = "burrTxt -d -R -P -w"
        rc, out = run(binary, ["-d", "-R", "-P", "-q", "-m", "0.07,0.04", "-w", written], aaa, label)
        if rc is not None:
            check(label, rc == 0 and os.path.exists(written), f"exited {rc}\n{out}")
        if os.path.exists(written):
            with gzip.open(written, "rt") as f:
                xml = f.read()
            check(label, 'rotationPlay="0.07" rotationBevel="0.04"' in xml, "the model is not in the file")
            check(label, 'playBevel="0.04"' in xml and "playUpper=" in xml, "the measured play is not in the file")
            check(label, 'state="2"' in xml and 'solutions="1"' in xml, "the problem is not solved in the file")

            again = os.path.join(tmp, "again.xmpuzzle")
            label = "burrTxt -x -R -P -w on the written file"
            rc, out = run(binary, ["-x", "-R", "-P", "-w", again], written, label)
            if rc is not None:
                check(label, rc == 0 and "works at play" in out, f"exited {rc}\n{out}")
                check(label, os.path.exists(again), "nothing written")

            # -m replaces the file's model with -x too
            remodeled = os.path.join(tmp, "remodeled.xmpuzzle")
            label = "burrTxt -x -R -m -w"
            rc, out = run(binary, ["-x", "-R", "-q", "-m", "0.05,0.04", "-w", remodeled], written, label)
            if rc is not None:
                check(label, os.path.exists(remodeled), "nothing written")
            if os.path.exists(remodeled):
                with gzip.open(remodeled, "rt") as f:
                    check(label, 'rotationPlay="0.05"' in f.read(), "-m was not applied")

            # without -R the turns are gone: a disassembly that no longer
            # comes apart is not kept
            plain = os.path.join(tmp, "plain.xmpuzzle")
            label = "burrTxt -x -w without -R"
            rc, out = run(binary, ["-x", "-q", "-w", plain], written, label)
            if rc is not None:
                check(label, os.path.exists(plain), "nothing written")
            if os.path.exists(plain):
                with gzip.open(plain, "rt") as f:
                    xml = f.read()
                check(label, "<separation" not in xml and "playUpper=" not in xml,
                      "the old disassembly was kept")

    # -M bisects up to its upper end: with this bevel no cube is left there,
    # which must be refused rather than crash in the geometry
    label = "burrTxt -M beyond what the bevel allows"
    rc, out = run(binary, ["-d", "-R", "-q", "-m", "0.081,0.3", "-M", "play:0.1,0.45"], aaa, label)
    if rc is not None:
        check(label, rc >= 0 and "does not fit the model" in out, f"was not refused (rc={rc})\n{out}")

    for f in failures:
        print(f"FAILED  {f}")

    if failures:
        return 1

    print("all burrTxt cli smoke tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
