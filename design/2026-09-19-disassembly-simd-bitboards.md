# Disassembly Vectorization & Bitboard Optimization Design

**Date:** 2026-09-19  
**Scope:** `src/lib/movementanalysator.{h,cpp}`, `src/lib/movementcache.{h,cpp}`  
**Branch:** `assembler-simd`  

---

## 1. Executive Summary & Profiling Analysis

Disassembly analysis (`movementAnalysator_c` & `disassembler_0_c`) determines whether assembled 3D interlocking burr structures can be separated into sub-assemblies and individual pieces. On puzzles with complex, deep sliding sequences (such as `Excelsior.xmpuzzle`), disassembly accounts for virtually **100%** of the entire solve time.

### 1.1 Instruction Profile on `Excelsior` (Callgrind Analysis)

A full instruction profile (`callgrind-3.27.1`) of `./build/burrTxt -d "puzzles/BTFiles/Jack Krijnen/Excelsior.xmpuzzle"` reveals the exact CPU bottleneck:

| Function | Instructions (`Ir`) | Share (%) | Description |
| :--- | :--- | :--- | :--- |
| `movementAnalysator_c::closureFull()` | 12,918,388,279 | **51.11%** | All-pairs transitive movement closure (Cutler matrix multiplication) |
| `movementAnalysator_c::checkmovement()` | 3,868,468,241 | **15.31%** | Collision chain-reaction test (inner loop) |
| `stl_vector.h:checkmovement()` | 3,483,660,528 | **13.78%** | Inline `std::vector` indexing & per-call zero-fill |
| **Combined Hotspot** | **20,270,517,048** | **80.20%** | **Over 80% of all CPU cycles in disassembly** |

All other operations (node allocation, hash tables, cache lookups) combined represent less than 20% of total runtime.

---

## 2. Part 1: `checkmovement()` Bitmask & Bitboard Vectorization

### 2.1 Problem in Existing Implementation
`movementAnalysator_c::checkmovement(unsigned int maxPieces, unsigned int nextstep)` is called **13.5 million times** on `Excelsior`.

In the current code:
1. **Per-call `std::vector` zeroing (13.78% of all instructions):**
   ```cpp
   for (int i = 0; i < next_pn; i++) {
     movement[i] = 0;
     check[i] = false;
   }
   ```
   On every single one of the 13.5M calls, it writes to heap-allocated `std::vector` buffers, even when the move fails on the very first piece test.
2. **Quadratic re-scanning (`do ... while (!finished)`):**
   When a piece $j$ is pushed by piece $i$, `finished` is set to `false`. The algorithm restarts the outer loop `for (int i = 0; i < next_pn; i++)`, re-scanning every piece $i$ and checking `if (check[i])`, and inside checking all $j$ with `if ((i != j) && (movement[j] == 0))`. For $N = 18$, this executes up to $18 \times 18 = 324$ checks per pass across multiple passes.

### 2.2 Proposed Bitmask Architecture ($N \le 64$)

For all practical puzzles, the number of pieces in any subproblem $N \le 64$ (for `Excelsior` $N = 18$, 6-piece burrs $N = 6$, `kangaroo` $N = 36$).

We represent the search state using two 64-bit integer bitmasks:
- `moved_mask`: bit $k$ is 1 iff piece $k$ is moving with velocity `nextstep`.
- `check_mask`: bit $k$ is 1 iff piece $k$ has been pushed and its outgoing collision constraints must be propagated.

#### Algorithmic Flow:
1. **Zero-overhead initialization:**
   ```cpp
   uint64_t moved_mask = 1ULL << nextpiece;
   uint64_t check_mask = moved_mask;
   ```
   Zero memory writes, zero vector clearing.

2. **Single-pass worklist via `std::countr_zero` (`tzcnt`):**
   ```cpp
   uint64_t all_pieces_mask = (next_pn == 64) ? ~0ULL : ((1ULL << next_pn) - 1);
   while (check_mask) {
     int i = std::countr_zero(check_mask);
     check_mask &= check_mask - 1; // pop piece i

     // Pieces that are still stationary
     uint64_t candidates = all_pieces_mask & ~moved_mask;
     while (candidates) {
       int j = std::countr_zero(candidates);
       candidates &= candidates - 1;

       // Check if piece i pushes piece j
       if (nextstep > get_matrix_val(i, j, nextdir, nd)) {
         moved_mask |= (1ULL << j);
         check_mask |= (1ULL << j);
         if (std::popcount(moved_mask) > maxPieces)
           return false; // Early exit, 0 cleanup
       }
     }
   }
   ```
3. **Lazy `movement` population:**
   Only when `checkmovement` succeeds (returns `true`), populate `movement[i]` for downstream `newNode()` consumers:
   ```cpp
   for (int i = 0; i < next_pn; i++) {
     movement[i] = (moved_mask & (1ULL << i)) ? nextstep : 0;
   }
   ```
   Since >95% of calls fail early, `movement` is almost never written to.

4. **Fallback:** If $N > 64$, fall back to the dynamic vector path.

---

## 3. Part 2: `closureFull()` Roy-Floyd-Warshall Single-Pass Vectorization

### 3.1 Problem in Existing Implementation
In `movementAnalysator_c::closureFull()`, the transitive closure of piece movement constraints is computed.

The original implementation uses Bill Cutler's $(\min, +)$ matrix multiplication formulation with intermediate node $k$ on the **inside**:
```cpp
for (y = 0; y < N; y++)
  for (x = 0; x < N; x++) {
    min = dist[y][0] + dist[0][x];
    for (i = 1; i < N; i++) {
      l = dist[y][i] + dist[i][x];
      if (l < min) min = l;
    }
    if (min < dist[y][x]) {
      dist[y][x] = min;
      // if change leads to other changes, again = true
    }
  }
} while (again > 0);
```

#### Why This is Inefficient:
- Placing intermediate node $i$ on the inside computes $M' = M \otimes M$. Each pass only propagates paths of length 2.
- Finding paths of length up to $N$ requires looping `do { ... } while (again > 0)`, taking $O(N^4)$ worst-case instructions!
- Memory access in `dist[y][i] + dist[i][x]` jumps across memory strides simultaneously in both rows and columns.

### 3.2 Proposed Roy-Floyd-Warshall Formulation

By placing intermediate node $k$ on the **outside** (classical Roy-Floyd-Warshall algorithm):
$$D^{(k)}[y][x] = \min\left(D^{(k-1)}[y][x], D^{(k-1)}[y][k] + D^{(k-1)}[k][x]\right)$$

#### Key Architectural Advantages:
1. **Guaranteed single pass ($O(N^3)$ operations):**
   - No `do ... while (again > 0)` loop!
   - Exactly $N$ outer steps for all-pairs shortest paths.
2. **Embarrassingly SIMD-Vectorizable Inner Loop:**
   - In the inner loop, for a fixed $k$ and $y$:
     $$c = D[y][k] \quad \text{(constant broadcast scalar)}$$
   - The inner loop across $x$ becomes:
     $$D[y][x] = \min\left(D[y][x], c + D[k][x]\right)$$
   - Notice that $D[y][x]$ and $D[k][x]$ are both row vectors!
   - This translates directly into SIMD vector operations:
     - AVX2: `_mm256_min_epu32(v_yx, _mm256_add_epi32(v_c, v_kx))`
     - ARM NEON: `vminq_u32(v_yx, vaddq_u32(v_c, v_kx))`
   - For $N = 18$ (as in `Excelsior`), 18 elements is just **three 256-bit AVX2 vectors**!
   - The entire inner loop executes in a handful of vector cycles without branch mispredictions.

---

## 4. Verification & Testing Strategy

1. **Exact Regression Parity:**
   - All disassembly move levels (`movesText()`, e.g. Pelikan `"101.2.4.2"`, Excelsior `"156.6.4.1.1.2.2.4.1.3.3.1.1.1.2.2"`) must remain 100% bit-identical.
   - Solution counts and assembly counts must match exactly.
2. **Unit Tests:**
   - Verify `checkmovement` bitmask logic on edge cases ($N = 1, N = 64, N > 64$, all-moving, none-moving, chained pushes).
   - Verify `closureFull` Roy-Floyd-Warshall equivalence against the original matrix multiplication.
3. **Quality Gates:**
   - `just test-all` (fast + slow regression suites).
   - `just check` (cppcheck static analysis).
4. **Interleaved A/B Benchmarking:**
   - Use `bench/bench_solve.py --ab` across the full 10-puzzle corpus.
   - Verify speedup specifically on disassembly-dominated puzzles (`Excelsior`, `Simplicity`, `kangaroo`).
