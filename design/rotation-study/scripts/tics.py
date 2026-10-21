#!/usr/bin/env python3
"""Run the decoded TICs with several burrTxt binaries: default model, printed
model, and bisect the smallest play (sharp cubes) and the smallest bevel (at
the printed play) at which each puzzle comes apart."""
import json, os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
RESULTS = os.path.join(HERE, "..", "results")
# the puzzles are not in the repository; point PUZZLES at their directory
PUZZLES = os.environ.get("PUZZLES", os.path.join(HERE, "..", "..", "..", "puzzles"))
DEC = os.path.join(PUZZLES, "TICs", "decoded")
BINS = dict(a.split("=") for a in sys.argv[1:])
MODELS = dict(l.split() for l in open(os.path.join(RESULTS, "tic_models.txt")))


def run(binary, puzzle, model):
    t = time.time()
    try:
        out = subprocess.run([binary, "-d", "-R", "-m", model, f"{DEC}/{puzzle}.xmpuzzle"],
                             capture_output=True, text=True, timeout=300).stdout
    except subprocess.TimeoutExpired:
        return {"solved": None, "t": 300}
    m = re.search(r"(\d+) assemblies and (\d+) solutions", out)
    lv = re.findall(r"level: ([0-9.]+), (\d+) rotations?", out)
    return {"solved": bool(m and int(m.group(2)) > 0), "level": lv[0][0] if lv else None,
            "rot": int(lv[0][1]) if lv else None, "t": round(time.time() - t, 2),
            "skipped": "may have been missed" in out}


def bisect(ok, lo, hi, steps=8):
    """smallest x in [lo, hi] with ok(x), assuming monotone; None if not even at hi"""
    if not ok(hi):
        return None
    for _ in range(steps):
        mid = 0.5 * (lo + hi)
        if ok(mid):
            hi = mid
        else:
            lo = mid
    return round(hi, 4)


res = {}
for name, binary in BINS.items():
    for p, model in MODELS.items():
        play = model.split(",")[0]
        r = {"default": run(binary, p, "0.081,0,0"), "printed": run(binary, p, model)}
        r["minPlay"] = bisect(lambda x: run(binary, p, f"{x:.5f},0,0")["solved"], 0.002, 0.2)
        r["minBevel"] = bisect(lambda x: run(binary, p, f"{play},0,{x:.5f}")["solved"], 0.0, 0.4)
        res.setdefault(p, {})[name] = r
        print(name, p, json.dumps(r), flush=True)
json.dump(res, open(os.path.join(RESULTS, "tics.json"), "w"), indent=1)
