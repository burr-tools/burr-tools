# Std Container Replacements — Plan

**Date:** 2026-09-17
**Status:** Implemented (Phases 0–4)
**Scope:** `src/lib`, `src/tools`, `src/halfedge` (excludes `src/lua`, `subprojects`, `src/gui` widgets)

## Problem

Several BurrTools containers predate usable C++ standard equivalents
(hand-rolled bit vectors, hash tables, thread wrapper). The question is
which can now be replaced with `std` without behaviour or performance
regressions. Project is on `cpp_std=c++20` (`meson.build`).

## Inventory

| # | Internal class | Location | Std candidate | Used by |
|---|---|---|---|---|
| 1 | `bitfield_c<bits>` | `src/lib/bitfield.h` | `std::bitset<N>`, `<bit>` popcount | `symmetries_2.cpp`, `tabs_2/generator_2.cpp` (N=240) |
| 2 | `voxelTable_c` + `hashNode` | `src/lib/voxeltable.h/.cpp` | `std::unordered_map/multimap` | shape dedup |
| 3 | `nodeHash`, `countingNodeHash` | `src/lib/disassemblerhashes.h/.cpp` | `std::unordered_set` + order vector | disassembler visited sets |
| 4 | `movementCache_c::moEntry` table | `src/lib/movementcache.h/.cpp` | `std::unordered_map<Key, vector<uint>>` | movement cache |
| 5 | `thread_c` | `src/lib/thread.h/.cpp` | `std::thread` / `std::jthread` + `stop_token` | `solveThread_c` |
| 6 | `Vector3D<T>` | `src/halfedge/vector3.h` | `std::array<T,3>` (+ glue) | mesh code |
| 7 | `vertexList_c` | `src/halfedge/polyhedron.h` | already `std::map`+`vector` | mesh builders |
| 8 | `xmlParser_c`/`xmlWriter_c`, `gzstream` | `src/tools/` | none (`std` has no XML/gzip) | persistence |
| 9 | voxel/assembler/disassembler/symmetries/`Polyhedron`, `grouping_c` structs | `src/lib`, `src/halfedge` | none (domain types) | core |

Most of the codebase is already `std`-based (`vector/map/set/unordered_map/memory/mutex/atomic`).

## Decisions

* **1 bitfield: modernize in place, do NOT wholesale replace with `std::bitset`.**
  `std::bitset<240>` covers `test/set/reset/any/count/&/|/==`, but has no
  hex-literal constructor and no hex printer. The symmetry tables are
  `static const bitfield_c<240> symmetries[] = { #include "tabs_2/symmetries.inc" }`
  with hex strings (`"0000...0001"`, etc.). A `std::bitset` swap forces
  either runtime init of those tables or a wrapper of about the same size
  as the current class, plus call-site churn for `notNull()`/`countbits()`/
  `print()`. Not worth it. The obviously-good subset is C++20 hygiene
  inside the existing class (Phase 0, done — see below).
* **2–4 hash tables: replaced with `std::unordered_*`.**
  All three were `vector<*>` buckets + intrusive `next` + manual rehash +
  manual `new/delete`. Each is now backed by `std::unordered_map/set` with
  the original hash functions and equality semantics kept:
  - `voxelTable_c` (`src/lib/voxeltable.h/.cpp`): `unordered_multimap<hash,
    {index, transformation}>`. Note: this table is GUI-only
    (`statuswindow.cpp`, `mainwindow.cpp`), not on the solver hot path.
  - `nodeHash` / `countingNodeHash` (`src/lib/disassemblerhashes.h/.cpp`):
    `unordered_set<node*, hash-by-value, equal-by-value>`; refcount
    discipline (`incRefCount` on insert, `decRefCount`+delete on clear)
    and `replaceNode`-on-shorter-way preserved. `countingNodeHash` keeps
    reverse-insertion scan order via a side `vector` (replaces the `link`
    list); the per-entry `hashNode` wrapper allocations are gone. The now
    unused intrusive `disassemblerNode_c::next` hook was removed.
  - `movementCache_c` (`src/lib/movementcache.h/.cpp`): `unordered_map<moKey,
    vector<uint>>` with the original `moHashValue` as the hasher; manual
    `moRehash`/`new/delete` deleted.
* **5 thread: keep.** `thread_c` already wraps `std::thread` + `atomic<bool>`.
  `std::jthread` migration changes shutdown/join semantics observed by the
  GUI polling loop. Needs a concurrency design of its own.
* **6–9 keep.** `std::array` is storage, not a geometry vector (no
  dot/cross/normalize/rotate/spheric); XML/gzip and domain types have no
  `std` counterpart.

## Phase 0 — implemented (obviously good, behaviour-preserving)

File: `src/lib/bitfield.h`. No call-site changes. Covered by
`test/test_bitfield.cpp` (`[bitfield]`: zero-init, set/reset, `notNull`,
word-boundary seams incl. bit 63, hex ctor, copy).

1. Missing includes: `<cstdint>` (`uint64_t`), `<cstring>`
   (`memcpy/memset/strlen`), `<cstdio>` (`printf/snprintf`). Previously
   relied on transitive includes.
2. Signed-shift UB: `1ll << (pos & 63)` -> `1ULL << ...` in
   `get/set/reset` (bit 63 shifted a signed `long long` into the sign bit).
   Popcount masks `0x...ll` -> `0x...ULL`.
3. Hand-rolled parallel popcount -> `std::popcount` (`<bit>`, C++20).
   Removes the bithack, uses the compiler intrinsic.
4. `notNull()` is now `const` (was non-`const`, blocked use on const refs).
5. `static_assert(bits > 0)` + explicitly-defaulted copy assignment
   (class already had a user copy ctor; assignment was implicit).
6. `print()` casts to `unsigned long long` for `%016llx` so `-Wformat`
   stays clean where `uint64_t` is `unsigned long` (LP64).

Explicitly out of scope: renaming `notNull/countbits` to `any/count`,
`[[nodiscard]]` rollout, `constexpr` conversion, `std::bitset` migration.

## Future work (not started)

* Re-evaluate `thread_c` -> `std::jthread` together with solver
  cancellation semantics (AGENTS.md rule 3: atomics/mutexes stay).

## Benchmarks (example puzzles, `burrTxt -d -q -o 0`)

Baseline measured on the parent commit via `git stash`, same machine.

| Puzzle | Baseline | With Phases 2–4 | Result |
|---|---|---|---|
| SolidSixPieceBurrs (heavy: 588 asm / 179 sol / 4302868 iter, counts identical) | 10.09s | 9.85s / 10.01s | parity (noise dominates) |
| PelikanBurr | 0.24s | 0.19–0.24s | parity |
| DraculasDentalDesaster | 0.22s | 0.16–0.19s | parity |
| Prisgon / MirrorParadox / CubeInCage / Bermuda / Stellation | 0.004–0.02s | same ballpark | parity (too fast to discriminate) |

No benchmark regressed; the heavy disassembler workload is within run-to-run
noise. Correctness is pinned by `test_solver.cpp` exact assembly/solution
counts plus the `[disasm][hash]`, `[voxeltable]` and `[movementcache]` cases.

## Benchmark corpus (BTFiles sweep, 2026-09-17)

`puzzles/BTFiles/` (gitignored, personal-use license) holds 399 `.xmpuzzle`
files from https://brettkuehner.com/btfiles/, mirrored with directory
structure. Full problem-0 sweep (`burrTxt -d -q -o 0`, 90s timeout, raw data
in `/tmp/opencode/btfiles_sweep.csv`):

* 384 exit 0; **85 with solutions > 0**; 379 with assemblies > 0
* 13 over the timeout (e.g. Bruce Patterson `cube90`, several Jack Krijnen /
  James Fortune / Tyler Hudson files) — unknown whether slow or unsolvable
* 2 abort (`Andrew Crowell/ARCparent_Cubes/CoverUp2_NoSolution`,
  `CoverUp_PiecesOnly`: `p < problems.size()` / `problem.resultValid()`);
  as expected, Andrew's files are the least tractable

Recommended perf corpus (problem 0, counts deterministic across runs,
each run 3x; covers assembler-heavy and disassembly-heavy profiles):

| Puzzle | Assemblies / Solutions / Iterations | Time |
|---|---|---|
| Tyler Hudson/Third_Times_the_Charm | 71 / 1 / 439905 | ~8–13s |
| James Fortune/elephant burr | 4251 / 30 / 159734 | ~5–12s |
| Jack Krijnen/Burrly Sane for Professionals | 895 / 1 / 205481 | ~5–6.5s |
| Jack Krijnen/Simplicity | 188 / 1 / 5105717 | ~4.3s, assembler-search heavy |
| James Fortune/kangaroo | 9831 / 2 / 1137000 | ~4.3s, stable |
| James Fortune/detonator | 7562 / 1 / 96435 | ~3.8s |
| James Fortune/hippo burr | 13464 / 1 / 302289 | ~2.4–3.8s |
| James Fortune/Hog Wild 2 | 5743 / 1 / 2435643 | ~2.3–3.4s |
| Jack Krijnen/Excelsior | 7 / 1 / 112 | ~2.7s, disassembly-heavy |
| Tom Messina/CD_Pack | 2 / 2 / 3087443 | ~1.2s |

Heavier options if needed: Tyler Hudson/Alpaca (~37s, 675/1),
James Fortune/iceburrg (~64s, 425202/6).

## Verification

* `just build`
* `just test` (fast loop; `just test-all` before merge, per AGENTS.md)
* `just check` (cppcheck)
* `./build/test_burrtools "[bitfield]"` for the touched unit
