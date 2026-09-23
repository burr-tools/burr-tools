# BurrTools Solver Benchmark Suite

This directory contains automated, noise-resistant benchmarking tools to compare BurrTools solver performance across versions, architectures, and threading models.

---

## 1. Overview & Methodology

Accurately measuring puzzle solver performance requires strict experimental controls:

1. **Interleaved A/B Execution:**
   Rather than running all baseline tests followed by all new tests, runs alternate (`Base -> Parallel -> Base -> Parallel...`) across $N$ iterations (default: 3). This eliminates bias caused by CPU thermal throttling, dynamic frequency scaling (turbo boost), and OS page-cache warming.
2. **Dual Metrics: Time & Memory:**
   - **Wall-Clock Time (s):** Real-world elapsed solve time.
   - **CPU Utilization (%):** Calculated as `(User CPU + System CPU) / Wall Time * 100%`. Single-core workloads report ~100%; multi-core scaling reaches 400%–800%+ depending on CPU thread count.
   - **Peak Resident Memory / RSS (MB):** Measured via `os.wait4()` querying the Linux kernel's `ru_maxrss` structure upon process termination. This captures the true peak memory consumption of the solver process including thread stacks and DLX matrix clones.
3. **Statistical Robustness (Medians):**
   Reports the median across runs for both time and peak RSS to eliminate outliers from background OS tasks or context switches.
4. **Correctness Invariants:**
   Every run logs assemblies found, solutions found, and iteration counts to verify that parallelization or optimizations strictly preserve solver invariants.
5. **Per-Puzzle Timeout:**
   Enforces a strict per-solve timeout (default: 600 seconds = 10 minutes) to prevent runaway search branches from hanging the benchmark suite.

---

## 2. Curated Benchmark Corpus

The benchmark suite defines 14 representative puzzles spanning distinct problem characteristics:

| Puzzle / Path | Problem | Solver Backend | Pieces / Characteristics | Approx. Baseline Time |
| :--- | :--- | :--- | :--- | :--- |
| **George Bell / LominoSquareProblems9-15.xmpuzzle** | Prob 1 (10x10) | `assembler_0_c` (Knuth DLX) | 11 unique pieces, pure exact-cover assembly | ~1.9s (scaled: ~0.7s) |
| **George Bell / LominoSquareProblems9-15.xmpuzzle** | Prob 2 (10x10 Alt) | `assembler_0_c` (Knuth DLX) | 11 unique pieces, pure exact-cover assembly | ~1.9s (scaled: ~0.7s) |
| **George Bell / LominoSquareProblems9-15.xmpuzzle** | Prob 0 (9x9) | `assembler_0_c` (Knuth DLX) | 10 unique pieces, pure exact-cover assembly | ~0.4s (scaled: ~0.18s) |
| **James Fortune / kangaroo.xmpuzzle** | Prob 0 | `assembler_0_c` + Disassembler | 6 unique pieces, 9,831 assemblies + level 10 disassembly | ~1.7s (scaled: ~1.2s) |
| **James Fortune / unlucky block.xmpuzzle** | Prob 0 | `assembler_0_c` + Disassembler | 7 unique pieces, interlocking assembly & disassembly | ~1-3s |
| **Jack Krijnen / Excelsior.xmpuzzle** | Prob 0 | `assembler_0_c` + Disassembler | 6 unique pieces, 7 assemblies + level 14 disassembly | ~1.8s |
| **George Bell / LominoSquareProblems9-15.xmpuzzle** | Prob 3 (11x11) | `assembler_1_c` (Huang) | Pieces with ranges/choices, pure assembly | ~1–5 min |
| **Tyler Hudson / Third_Times_the_Charm.xmpuzzle** | Prob 0 | `assembler_1_c` (Huang) | Pieces with shapes/ranges, 71 assemblies | ~16s |
| **examples/SolidSixPieceBurrs.xmpuzzle** | Prob 0 | `assembler_1_c` (Huang) | 6 pieces with duplicate stick shapes, 588 assemblies, 179 solutions | ~7.5s |
| **Jack Krijnen / Simplicity.xmpuzzle** | Prob 0 | `assembler_1_c` (Huang) | Duplicate pieces, 188 assemblies + level 10 disassembly | ~3.2s |
| **Jack Krijnen / BottomLine.xmpuzzle** | Prob 0 | `assembler_1_c` (Huang) | Duplicate pieces, 76 assemblies + level 11 disassembly | ~1.1s |
| **Jack Krijnen / Tippy.xmpuzzle** | Prob 0 | `assembler_1_c` (Huang) | Duplicate pieces, 460 assemblies, 111 solutions | ~0.6s |
| **examples/PelikanBurr.xmpuzzle** | Prob 0 | `assembler_0_c` (Knuth DLX) | 6 unique pieces, micro-search (~5ms assembly, level 4 disassembly) | ~0.15s |
| **examples/DraculasDentalDesaster.xmpuzzle** | Prob 0 | `assembler_0_c` (Knuth DLX) | 6 unique pieces, 84 assemblies (~8ms assembly, level 8 disassembly) | ~0.14s |

> [!NOTE]
> Puzzles located in `puzzles/BTFiles/` come from the Brett Kuehner BTFiles archive. If a puzzle file is not found on disk, the benchmark runner automatically notes it and skips it gracefully. The `examples/*.xmpuzzle` subset is always available in the repository.

---

## 3. How to Run

### 3.1 Step 1: Prepare the Base Binary (Single-Threaded Reference)

Check out the base reference commit or branch (e.g. `disassembler-optimizations`), compile `burrTxt`, and save it as `build/burrTxt-base`:

```bash
git checkout disassembler-optimizations
ninja -C build burrTxt
cp build/burrTxt build/burrTxt-base
git checkout assembler-parallel
ninja -C build burrTxt
```

### 3.2 Step 2: Run the Benchmark Suite

Run the suite script directly:

```bash
./bench/run_suite.sh
```

To run outside an interactive shell session (e.g. for long-running benchmarks lasting up to an hour), use `nohup`:

```bash
nohup ./bench/run_suite.sh --runs 3 --timeout 600 > bench_run.log 2>&1 &
```

Monitor progress in real time with:

```bash
tail -f bench_run.log
```

### 3.3 Command-Line Options

```
Usage: ./bench/run_suite.sh [options] [puzzle:problem ...]

Options:
  --runs N          Number of interleaved runs per binary (default: 3)
  --timeout SECS    Max execution time per solve (default: 600s = 10 min)
  --output FILE     Output CSV file (default: bench/results/results_<timestamp>.csv)
  --base PATH       Path to base binary (default: build/burrTxt-base)
  --new PATH        Path to current binary (default: build/burrTxt)
  --list            List all puzzles in the curated suite and their availability
  -h, --help        Show help
```

### 3.4 Benchmarking Specific Puzzles

You can pass specific puzzle files or problem indices:

```bash
# Benchmark only George Bell Lomino problems
./bench/run_suite.sh \
  "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:0" \
  "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:1" \
  "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:2"

# Benchmark with a 1-minute timeout
./bench/run_suite.sh --timeout 60 examples/SolidSixPieceBurrs.xmpuzzle
```

---

## 4. Output Format & Summary Tables

The benchmark logs raw CSV data to both stdout and the specified `--output` file:

```csv
tag,puzzle,run,wall_s,user_s,sys_s,cpu_pct,max_rss_mb,exit,stats
before,puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:1,0,1.92,1.92,0.00,99.9%,13.8MB,0,5 assemblies and 5 solutions found with 617942 iterations
after,puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:1,0,0.73,3.95,0.01,542.5%,18.2MB,0,5 assemblies and 5 solutions found with 617899 iterations
```

Upon completion, two summary tables are generated:

### 1. Wall Clock & Speedup Summary
```
=== Wall Clock & Speedup (Medians) ===
Puzzle                                                  Before (s)  After (s)    Speedup  Before CPU%   After CPU%
-------------------------------------------------------------------------------------------------------------------
George Bell/LominoSquareProblems9-15.xmpuzzle:1               1.92       0.73      2.63x        99.9%       542.5%
George Bell/LominoSquareProblems9-15.xmpuzzle:2               1.90       0.75      2.53x        99.8%       540.1%
George Bell/LominoSquareProblems9-15.xmpuzzle:0               0.45       0.17      2.65x        99.9%       543.0%
James Fortune/kangaroo.xmpuzzle                               1.73       1.24      1.40x        99.9%       231.4%
SolidSixPieceBurrs.xmpuzzle                                   7.47       7.95      0.94x        99.8%        99.9%
PelikanBurr.xmpuzzle                                          0.14       0.15      0.93x        99.5%       109.4%
```

### 2. Peak Resident Memory / RSS Summary
```
=== Peak Resident Memory / RSS (Medians) ===
Puzzle                                                   Before (MB)   After (MB)   Delta (MB)    Delta (%)
-----------------------------------------------------------------------------------------------------------
George Bell/LominoSquareProblems9-15.xmpuzzle:1               13.80M       18.25M       +4.45M       +32.2%
George Bell/LominoSquareProblems9-15.xmpuzzle:2               13.81M       18.24M       +4.43M       +32.1%
George Bell/LominoSquareProblems9-15.xmpuzzle:0               13.74M       13.74M       +0.00M        +0.0%
James Fortune/kangaroo.xmpuzzle                               15.20M       19.65M       +4.45M       +29.3%
SolidSixPieceBurrs.xmpuzzle                                   14.10M       14.10M       +0.00M        +0.0%
PelikanBurr.xmpuzzle                                          13.73M       17.78M       +4.05M       +29.5%
```
*(Notice: The threaded version allocates ~4 MB for thread stacks and private worker DLX matrix clones across 8 worker threads—a very modest and completely bounded memory overhead).*
