#!/usr/bin/env python3
import subprocess
import time
import os

puzzles = [
    ("Lomino 9x9 (prob 0)", "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle", "0"),
    ("Lomino 10x10 (prob 1)", "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle", "1"),
    ("Lomino 10x10 Alt (prob 2)", "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle", "2"),
]

configs = [
    ("DLX (1 thread)", {"BURRTOOLS_NO_SIMD": "1"}, ["-t", "1"]),
    ("SIMD (1 thread)", {}, ["-t", "1"]),
    ("SIMD (4 threads)", {}, ["-t", "4"]),
    ("SIMD (8 threads)", {}, ["-t", "8"]),
]

header = f"| {'Puzzle':<26} | {'Config':<18} | {'Time':<8} | {'Speedup':<8} | {'Assemblies':<11} | {'Iterations':<11} |"
sep = f"|{'-'*28}|{'-'*20}|{'-'*10}|{'-'*10}|{'-'*13}|{'-'*13}|"

print(header)
print(sep)

for pname, ppath, prob_idx in puzzles:
    base_time = None
    for cname, env_extra, topts in configs:
        env = os.environ.copy()
        env.update(env_extra)
        cmd = ["./build/burrTxt", "-q", "-o", prob_idx] + topts + [ppath]
        
        times = []
        out_str = ""
        for _ in range(3):
            t0 = time.perf_counter()
            res = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            t1 = time.perf_counter()
            times.append(t1 - t0)
            out_str = res.stdout
            
        best_t = min(times)
        if base_time is None:
            base_time = best_t
        speedup = base_time / best_t
        
        assm = "?"
        iters = "?"
        parts = out_str.split()
        if "assemblies" in parts:
            idx = parts.index("assemblies")
            assm = parts[idx - 1]
        if "iterations" in parts:
            idx = parts.index("iterations")
            iters = parts[idx - 1]
            
        print(f"| {pname:<26} | {cname:<18} | {best_t:6.3f}s | {speedup:6.2f}x | {assm:<11} | {iters:<11} |")
