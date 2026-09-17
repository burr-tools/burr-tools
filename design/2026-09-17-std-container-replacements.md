# Std Container Replacements — Plan

**Date:** 2026-09-17
**Status:** Planned (Phase 0 implemented)
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
* **2–4 hash tables: keep custom for now, revisit with benchmarks.**
  All three are `vector<*>` buckets + intrusive `next` + manual rehash +
  manual `new/delete`, so they *look* replaceable. But each carries
  semantics `unordered_map/set` does not give for free: `voxelTable_c`
  equality is transform + `identicalInBB`, not key equality;
  `nodeHash` is intrusive (`disassemblerNode_c::next`) with refcount +
  `replaceNode`-on-shorter-way; `countingNodeHash` adds reverse-insertion
  scan order; `movementCache_c` keys 7 fields. All sit on solver hot paths.
  Replacement changes allocation and rehash behaviour and needs perf
  measurement before/after. Documented as future work, not done here.
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

* Prototype 2–4 on `unordered_map/set` behind a flag, compare solver
  timings (`just test-all`) and memory on a standard puzzle corpus before
  committing.
* Re-evaluate `thread_c` -> `std::jthread` together with solver
  cancellation semantics (AGENTS.md rule 3: atomics/mutexes stay).

## Verification

* `just build`
* `just test` (fast loop; `just test-all` before merge, per AGENTS.md)
* `just check` (cppcheck)
* `./build/test_burrtools "[bitfield]"` for the touched unit
