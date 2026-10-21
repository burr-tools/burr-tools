#!/usr/bin/env python3
"""Run every puzzle of Andrew Crowell's collection with several burrTxt
binaries: the default model (play 0.081, sharp) and a bisection of the
smallest play (sharp cubes) at which it comes apart."""
import json, os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
RESULTS = os.path.join(HERE, "..", "results")
# the puzzles are not in the repository; point PUZZLES at their directory
PUZZLES = os.environ.get("PUZZLES", os.path.join(HERE, "..", "..", "..", "puzzles"))
ROOT = os.path.join(PUZZLES, "BTFiles", "Andrew Crowell")
OUT = os.path.join(RESULTS, "crowell.jsonl")
BINS = dict(a.split("=") for a in sys.argv[1:])
FILES = sorted(os.path.relpath(os.path.join(d, f), ROOT)
               for d, _, fs in os.walk(ROOT) for f in fs if f.endswith(".xmpuzzle"))


def run(binary, puzzle, model, timeout=120):
    t = time.time()
    try:
        out = subprocess.run([binary, "-d", "-R", "-m", model, os.path.join(ROOT, puzzle)],
                             capture_output=True, text=True, timeout=timeout).stdout
    except subprocess.TimeoutExpired:
        return {"solved": None, "t": timeout}
    m = re.search(r"(\d+) assemblies and (\d+) solutions", out)
    lv = re.findall(r"level: ([0-9.]+)(?:, (\d+) rotations?)?", out)
    return {"solved": bool(m and int(m.group(2)) > 0), "asm": int(m.group(1)) if m else None,
            "level": lv[0][0] if lv else None, "rot": int(lv[0][1]) if lv and lv[0][1] else 0,
            "t": round(time.time() - t, 2), "skipped": "may have been missed" in out}


def bisect(ok, lo, hi, steps=6):
    if not ok(hi):
        return None
    for _ in range(steps):
        mid = 0.5 * (lo + hi)
        if ok(mid):
            hi = mid
        else:
            lo = mid
    return round(hi, 4)


done = set()
if os.path.exists(OUT):
    done = {(r["bin"], r["puzzle"]) for r in map(json.loads, open(OUT))}
with open(OUT, "a") as out:
    for p in FILES:
        for name, binary in BINS.items():
            if (name, p) in done:
                continue
            r = {"bin": name, "puzzle": p, "default": run(binary, p, "0.081,0,0")}
            # NO_BISECT=1: only the default-model run
            r["minPlay"] = None if r["default"]["solved"] is None or os.environ.get("NO_BISECT") else \
                bisect(lambda x: run(binary, p, f"{x:.5f},0,0", 60)["solved"], 0.002, 0.2)
            out.write(json.dumps(r) + "\n")
            out.flush()
