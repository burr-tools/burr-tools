#!/usr/bin/env python3
"""Smoke tests for the burrTxt command line tool.

Like cli_smoke.py for burrTxt2: these drive the real binary, because the
behaviour they pin down lives in main(), namely which runs it refuses to
start. A run that cannot do useful work has to say so and exit, not crash.
"""

import os
import subprocess
import sys

TIMEOUT = 120

failures = []


def run(binary, args, puzzle, label, env=None):
    """Run burrTxt on a puzzle, return (returncode, output); env: variables
    to set on top of the inherited ones (None as a value unsets one)."""
    environ = dict(os.environ)
    for k, v in (env or {}).items():
        if v is None:
            environ.pop(k, None)
        else:
            environ[k] = v
    try:
        proc = subprocess.run([binary] + args + [puzzle], stdin=subprocess.DEVNULL,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, timeout=TIMEOUT, env=environ)
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

    # -R takes an optional mode; the counters show which one ran: fast
    # hands no turn to the planner, balanced and slow do on this puzzle.
    # Without a mode it is balanced, unless BURRTOOLS_ROT_MODE says otherwise.
    stats = {"BURRTOOLS_ROT_STATS": "1", "BURRTOOLS_ROT_MODE": None}
    for mode, planned in (("fast", False), ("balanced", True), ("slow", True), (None, True)):
        args = ["-d", "-R"] + ([mode] if mode else []) + ["-q", "-m", "0.081,0.032"]
        label = f"burrTxt {' '.join(args)}"
        rc, out = run(binary, args, aaa, label, stats)
        if rc is not None:
            check(label, rc == 0 and "assemblies and" in out, f"exited {rc}\n{out}")
            check(label, ("gate 0 " not in out) == planned, f"the planner {'did not run' if planned else 'ran'}\n{out}")
            if mode != "fast":
                check(label, "1 solutions" in out, f"did not come apart\n{out}")

    label = "burrTxt -R with BURRTOOLS_ROT_MODE=fast"
    rc, out = run(binary, ["-d", "-R", "-q", "-m", "0.081,0.032"], aaa, label,
                  {"BURRTOOLS_ROT_STATS": "1", "BURRTOOLS_ROT_MODE": "fast"})
    if rc is not None:
        check(label, rc == 0 and "gate 0 " in out, f"the planner ran\n{out}")

    # a bad mode in the environment is refused with a hint rather than ignored
    for name, value, says in (("BURRTOOLS_ROT_MODE", "quick", "BURRTOOLS_ROT_MODE must be"),):
        label = f"burrTxt -R with {name}={value}"
        rc, out = run(binary, ["-d", "-R", "-q"], aaa, label, {name: value})
        if rc is not None:
            check(label, rc == 1 and says in out, f"was not refused (rc={rc})\n{out}")

    for f in failures:
        print(f"FAILED  {f}")

    if failures:
        return 1

    print("all burrTxt cli smoke tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
