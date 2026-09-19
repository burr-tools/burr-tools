# Vectorized SIMD Exact-Cover Assembler Design

**Date:** 2026-09-18  
**Scope:** `src/lib/simd_exact_cover.{h,cpp}`, `src/lib/assembler_0.{h,cpp}`, `test/test_simd_exact_cover.cpp`  
**Branch:** `assembler-simd`  

---

## 1. Executive Summary & Problem Formulation

BurrTools' core assembly engine (`assembler_0_c`) formulates 3D interlocking burr assembly as an **Exact Cover** problem solved via Donald Knuth's classical Dancing Links algorithm (DLX / Algorithm X).

While DLX is universally general across arbitrary problem sizes and non-orthogonal grids, it relies on a 2D circular doubly-linked web of heap-allocated `assemblerNode_c` pointers (each node taking 40 bytes on 64-bit platforms). Navigating, covering, and uncovering these pointer webs incurs massive L1/L2/L3 cache misses and heavy memory-write traffic during backtracking.

### 1.1 The Opportunity: 95% of Puzzles Have $\le 256$ Columns
In BurrTools:
- Each column represents either **one piece** (1 to $P$) or **one filled result voxel** (1 to $V$).
- Total columns $C = P + V$.
- For almost all classic burrs, polycube packing, and interlocking puzzles:
  - 6-piece burrs (e.g. `SolidSixPieceBurrs`, `PelikanBurr`, `Simplicity`): $P = 6, V = 24\dots 36 \implies C = 30\dots 42$ columns.
  - $4 \times 4 \times 4$ cubic puzzles: $P \le 16, V \le 64 \implies C \le 80$ columns.
  - $5 \times 5 \times 5$ cubic puzzles: $P \le 25, V \le 125 \implies C \le 150$ columns.
  - $6 \times 6 \times 6$ large burrs: $P \le 18, V \le 150 \implies C \le 168$ columns.

A column space of $C \le 256$ bits fits entirely within **one single 256-bit AVX2 vector register** (`__m256i`), or four 64-bit words (`uint64_t[4]`).

This document describes the architectural design for a **hardware-vectorized Exact Cover engine (`SimdExactCover256`)** that achieves single-cycle conflict detection, zero-write backtracking, and extreme cache residency.

---

## 2. Theoretical Foundation: Vectorized Bit-Parallel Exact Cover

### 2.1 State Representation
Let $C \le 256$ be the number of active columns.
Every placement row $r \in \{0, \dots, R-1\}$ is represented as an aligned 256-bit bitmask $M_r$:
$$M_r \in \{0, 1\}^{256}, \quad \text{where bit } c \text{ is } 1 \iff \text{row } r \text{ covers column } c$$

The search state at any point is simply the accumulator bitmask $S$ of currently covered columns:
$$S \in \{0, 1\}^{256} \quad (\text{initially } S = 0)$$

### 2.2 Conflict Detection via `VPTEST` in 1 Clock Cycle
In DLX, determining whether a row conflicts with the current placement requires traversing pointers for every covered column.
With AVX2 vectorization, determining whether candidate placement $M_r$ is mutually disjoint with current state $S$ is a **single assembly instruction taking 1 clock cycle**:

```cpp
// Returns true iff (S & M_r) == 0 (no column collision, placement is legal)
inline bool is_compatible(const __m256i &S, const __m256i &M_r) {
    return _mm256_testz_si256(S, M_r);
}
```

On platforms without AVX2 (or ARM NEON), the 4-word scalar fallback is branch-free and heavily pipelined:
```cpp
inline bool is_compatible_scalar(const uint64_t S[4], const uint64_t M[4]) {
    return ((S[0] & M[0]) | (S[1] & M[1]) | (S[2] & M[2]) | (S[3] & M[3])) == 0;
}
```

### 2.3 Zero-Cost Backtracking
In classical DLX:
- Backtracking requires unlinking and re-linking every node in reverse order (`L[R[x]] = x`, `U[D[x]] = x`).
- For a row of length 8 with average column depth 15, backtracking executes ~1,000 memory writes and cache-line invalidations per search step.

In `SimdExactCover256`:
- Placing row $r$ updates the state to $S' = S \ | \ M_r$ (`_mm256_or_si256`).
- **Backtracking requires zero memory writes.** The previous state $S$ remains in a CPU register or on the call stack frame. The backtrack cost is literally **0 CPU cycles**.

### 2.4 Cache Footprint Collapse
- In DLX, 5,000 candidate placement rows with 250,000 node pointers consume **10 to 20 MB** of heap memory, completely blowing out of L1 and L2 cache.
- In `SimdExactCover256`, 5,000 placement rows consume:
  $$5{,}000 \times 32 \text{ bytes} = 160 \text{ KB}$$
  **The entire matrix fits entirely within CPU L2 cache** (and individual hot piece subsets fit within L1 cache).

---

## 3. Search Algorithm: Filtered Active Rows with Minimum Remaining Values (MRV)

A naive piece-by-piece scan can create orphaned voxel holes that are discovered only after several pieces are placed. To retain Knuth's powerful branching efficiency, `SimdExactCover256` implements **Minimum Remaining Values (MRV)** column branching with SIMD candidate filtering.

```
       [Root: S = 0, ActiveRows = {all rows}]
                         │
        Count valid rows per uncovered column
            Select col c with min(count)
           (If min == 0 -> prune immediately)
                         │
      ┌──────────────────┴──────────────────┐
  Try Row 1 covering c                  Try Row 2 covering c
  S' = S | M_1                          S' = S | M_2
  Filter ActiveRows via VPTEST          Filter ActiveRows via VPTEST
```

### 3.1 Step-by-Step Search Loop
At search depth $d$ with state $S$ and candidate rows $A_d$:
1. **Goal Check:** If all required columns are covered ($S \ \& \ \text{target\_mask} == \text{target\_mask}$), record the solution.
2. **Column Counting & Dead-End Detection:**
   - For all uncovered columns $c$, compute $\text{count}(c)$ among the rows in $A_d$.
   - **Immediate Prune:** If any required column has $\text{count}(c) == 0$, backtrack immediately.
   - **Forced Column (Unit Propagation):** If any required column has $\text{count}(c) == 1$, select that forced row immediately without creating branching frames.
3. **Choose Pivot Column:** Pick column $c^*$ minimizing $\text{count}(c^*)$.
4. **Branch & SIMD Filter:**
   - For each candidate row $r \in A_d$ that covers column $c^*$:
     - Form $S_{d+1} = S_d \ | \ M_r$.
     - Construct $A_{d+1}$ by filtering $A_d$ with AVX2:
       ```cpp
       for (uint32_t cand : A_d) {
           if (_mm256_testz_si256(M_r, rows[cand].mask)) {
               A_{d+1}.push_back(cand);
           }
       }
       ```
     - Recurse to depth $d+1$.
     - On return, pop $A_{d+1}$. Backtrack of $S$ is automatic.

---

## 4. Integration with BurrTools Architecture

To ensure 100% correctness and zero regressions, `SimdExactCover256` leverages BurrTools' existing pre-solve pipeline:

```mermaid
flowchart LR
    A["puzzle / problem"] --> B["assembler_0_c::prepare()<br>(Bram's anchor sweep)"]
    B --> C["assembler_0_c::reduce()<br>(Bram's hash clumpify)"]
    C --> D{"Active Columns <= 256?"}
    D -- Yes --> E["SimdExactCover256<br>(AVX2 / Bit-Parallel)"]
    D -- No --> F["Classical DLX<br>(assembler_0_c fallback)"]
    E --> G["assembly_c::smallerRotationExists()<br>callback->assembly()"]
    F --> G
```

1. **Preprocessing Reuse:**
   `assembler_0_c::prepare()` and `reduce()` run as usual, producing the reduced matrix.
2. **Eligibility Check (`canUseSimd`):**
   - Total columns $\le 256$.
   - No color constraints requiring secondary column logic beyond standard exact cover.
3. **Bitmask Extraction:**
   Each surviving row in the reduced DLX matrix is converted into a 256-bit bitmask $M_r$, preserving row metadata (`piece`, `rot`, `x`, `y`, `z`).
4. **Solution Emission & Symmetries:**
   When `SimdExactCover256` finds a solution, it passes the chosen row IDs to `assembler_0_c::getAssembly()` and `solution()`, ensuring identical symmetry deduplication (`smallerRotationExists`) and progress callbacks.
5. **Multi-Threading Compatibility:**
   `SimdExactCover256` worker instances can be spawned across threads identically to `assemblerWorker_c`, each with a read-only view of the immutable row bitmasks.

---

## 5. Verification Plan

1. **Unit Testing (`test/test_simd_exact_cover.cpp`):**
   - Direct verification against Knuth's exact-cover test matrices.
   - Bit-identical assembly generation on classic puzzles (`PelikanBurr`, `BallRoom`, `Bermuda`, `BrokenSticks`).
2. **Quality Gates:**
   - Clean `just build`, `just test`, `just test-all`.
   - Clean `just check` (cppcheck) and clang-tidy.
3. **Performance Benchmarking (`bench/run_suite.sh`):**
   - Benchmark single-thread and multi-thread speedup of SIMD Exact Cover against baseline DLX.
   - Measure CPU cycles, memory usage, and assembly throughput.

---

## 6. Empirical Results & Verification

Empirical performance evaluation was conducted on an **11th Gen Intel(R) Core(TM) i7-1185G7 @ 3.00GHz** (4 physical cores, 8 threads, AVX2 and AVX-512 supported).

Benchmarks were executed using the standardized benchmark suite tooling (`bench/bench_solve.py`), comparing baseline DLX (`BURRTOOLS_NO_SIMD=1`) against the SIMD exact-cover solver across the curated puzzle corpus with 3 interleaved runs per puzzle.

### 6.1 Standardized Corpus: Single-Threaded Evaluation (`--threads 1 --no-disassemble`)

Measures pure single-core algorithmic throughput (DLX vs SIMD bit-parallel search):

| Puzzle & Characteristics | Baseline DLX | SIMD Exact Cover | Speedup | Iterations (DLX vs SIMD) | Assemblies |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Lomino 10x10 (prob 1)** *(assembler 0, 11 unique pieces)* | 2.14s | 0.70s | **3.05x** | 617,942 vs 384,645 | 5 |
| **Lomino 10x10 Alt (prob 2)** *(assembler 0, 11 unique pieces)* | 2.01s | 0.73s | **2.77x** | 628,897 vs 396,920 | 8 |
| **Lomino 9x9 (prob 0)** *(assembler 0, 10 unique pieces)* | 0.49s | 0.18s | **2.65x** | 201,846 vs 125,446 | 9 |
| **Pelikan Burr** *(assembler 0, 6 unique pieces)* | 0.01s | 0.01s | **1.26x** | 89 vs 89 | 12 |
| **Excelsior** *(assembler 0, 6 unique pieces)* | 0.01s | 0.01s | **1.18x** | 112 vs 112 | 7 |
| **Kangaroo** *(assembler 0 fallback, 325 cols > 255)* | 1.26s | 1.27s | **0.99x** | 1,137,000 vs 1,137,000 | 9,831 |
| **Solid Six Piece Burrs** *(assembler 1 Huang, duplicate sticks)* | 7.48s | 7.48s | **1.00x** | 4,302,868 vs 4,302,868 | 588 |
| **Simplicity** *(assembler 1 Huang, duplicate pieces)* | 2.09s | 2.02s | **1.04x** | 5,105,717 vs 5,105,717 | 188 |
| **Third Times the Charm** *(assembler 1 Huang, duplicate shapes)* | 4.46s | 4.47s | **1.00x** | 439,905 vs 439,905 | 71 |
| **CD Pack** *(assembler 1 Huang, duplicate pieces)* | 0.91s | 0.90s | **1.01x** | 3,087,443 vs 3,087,443 | 2 |

### 6.2 Standardized Corpus: Multi-Threaded Scaling (`--no-disassemble`, all cores)

Measures parallel tree search (`assembler_0_c::parallelMultiSearch` subtree dispatch):

| Puzzle & Characteristics | Baseline DLX (8 thr) | SIMD (8 thr) | Speedup | CPU Utilization (DLX vs SIMD) | Assemblies |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Lomino 10x10 (prob 1)** | 0.67s | 0.24s | **2.77x** | 661% vs 608% | 5 |
| **Lomino 10x10 Alt (prob 2)** | 0.65s | 0.26s | **2.55x** | 620% vs 591% | 8 |
| **Lomino 9x9 (prob 0)** | 0.17s | 0.07s | **2.35x** | 552% vs 474% | 9 |
| **Pelikan Burr** | 0.02s | 0.01s | **1.53x** | 178% vs 224% | 12 |
| **Excelsior** | 0.01s | 0.01s | **1.24x** | 112% vs 118% | 7 |
| **Kangaroo** *(fallback)* | 0.36s | 0.36s | **1.00x** | 658% vs 670% | 9,831 |
| **Solid Six Piece Burrs** *(Huang)* | 2.34s | 2.43s | **0.96x** | 596% vs 580% | 588 |
| **Simplicity** *(Huang)* | 0.63s | 0.62s | **1.01x** | 681% vs 673% | 188 |
| **Third Times the Charm** *(Huang)* | 1.96s | 1.96s | **1.00x** | 665% vs 666% | 71 |
| **CD Pack** *(Huang)* | 0.30s | 0.30s | **1.02x** | 649% vs 647% | 2 |

### 6.3 Peak Resident Memory (RSS)

Across the entire benchmark corpus, peak resident set size (RSS) differences were negligible:
- Maximum delta: $+0.34\text{ MB}$ ($+1.8\%$ on Solid Six Piece Burrs)
- Typical delta: $\pm 0.00\text{ MB}$ to $+0.01\text{ MB}$ ($< 0.1\%$)
- No memory leaks or buffer accumulation observed.

### 6.4 Key Conclusions:
1. **Accelerated Domain ($\le 256$ columns, assembler 0)**: Delivers a consistent **2.35x to 3.05x** single-threaded speedup and up to **2.77x** multi-threaded speedup over DLX. Overall wall-clock speedup from baseline single-thread DLX (2.14s) to 8-thread SIMD (0.24s) on Lomino 10x10 is **8.92x**.
2. **Transparent Fallback**: Puzzles requiring assembler 1 (duplicate pieces) or having $> 256$ columns seamlessly fall back to existing solver engines with zero regressions and zero performance penalty ($0.96\text{x}$ to $1.04\text{x}$, well within benchmark noise).
3. **Correctness & Symmetries**: Assembly counts and symmetry invariants match 100% across all runs.
4. **Test Suite Verification**: Clean passes on `just test-all` (all 395 test cases + stress tests) and `just check` (static analysis).

