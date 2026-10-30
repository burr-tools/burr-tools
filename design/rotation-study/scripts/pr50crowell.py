#!/usr/bin/env python3
"""PR #50's rotation engines (Classic -R, Crowell -C) on Andrew Crowell's collection."""
import json, os, re, subprocess, time

HERE = os.path.dirname(os.path.abspath(__file__))
RESULTS = os.path.join(HERE, "..", "results")
# the puzzles are not in the repository; point PUZZLES at their directory
PUZZLES = os.environ.get("PUZZLES", os.path.join(HERE, "..", "..", "..", "puzzles"))
ROOT = os.path.join(PUZZLES, "BTFiles", "Andrew Crowell")
OUT = os.path.join(RESULTS, "pr50crowell.jsonl")
import sys
ENGINES = sys.argv[1:] or ["-R", "-C"]
OUT = OUT.replace(".jsonl", "".join(e.strip("-") for e in ENGINES) + ".jsonl")
# burrTxt built from branch rotations-pr50 (it has -R/-C)
BIN = os.environ["BURRTXT_PR50"]
FILES = sorted(os.path.relpath(os.path.join(d, f), ROOT)
               for d, _, fs in os.walk(ROOT) for f in fs if f.endswith(".xmpuzzle"))

done = set()
if os.path.exists(OUT):
    done = {(r["engine"], r["puzzle"]) for r in map(json.loads, open(OUT))}
with open(OUT, "a") as out:
    for p in FILES:
        for engine in ENGINES:
            if (engine, p) in done:
                continue
            t = time.time()
            try:
                o = subprocess.run([BIN, "-d", engine, os.path.join(ROOT, p)],
                                   capture_output=True, text=True, timeout=60).stdout
                m = re.search(r"(\d+) assemblies and (\d+) solutions", o)
                lv = re.findall(r"level: (\S+)", o)
                r = {"solved": bool(m and int(m.group(2)) > 0), "level": lv[0] if lv else None}
            except subprocess.TimeoutExpired:
                r = {"solved": None, "level": None}
            r.update(engine=engine, puzzle=p, t=round(time.time() - t, 2))
            out.write(json.dumps(r) + "\n")
            out.flush()
