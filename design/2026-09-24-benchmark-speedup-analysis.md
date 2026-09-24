# Benchmark Speedup Analysis: Why Some Puzzles Scale Below 4x

**Date:** 2026-09-24
**Scope:** Solver performance of current master vs 0.7.1 across the `bench/run_snapshot.sh` corpus
([`bench/bench_solve.py`](../bench/bench_solve.py), [`bench/compare_snapshots.py`](../bench/compare_snapshots.py)),
solver engines in [`src/lib/assembler_0.cpp`](../src/lib/assembler_0.cpp),
[`src/lib/assembler_1.cpp`](../src/lib/assembler_1.cpp) and
[`src/lib/disassemblerpool.cpp`](../src/lib/disassemblerpool.cpp), CLI driver
[`src/burrTxt.cpp`](../src/burrTxt.cpp)
**Status:** Measurement report (perf + controlled `-t` runs)

---

## 0. Summary

Comparing the 0.7.1 snapshot
(`results_20260923_185154_ab7ebc18-…`, binary `bt071/burrTxt`, ~100% CPU)
against the optimized build
(`results_20260923_194711_d7fab7a7-…`, binary `build/burrTxt`, up to ~750% CPU)
shows speedups scattered from ~1x to ~19x. This is expected, not a defect:

1. The comparison conflates three independent effects — **prep/algorithm**,
   **SIMD**, and **threading** — and each puzzle is dominated by a different
   phase, so each optimization only helps "its" puzzles.
2. Once separated (same binary, `-t 1` vs `-t 8`), **parallel scaling of the
   search is healthy: 3.3–4.0x** on this 4-core/8-thread machine (i7-1185G7),
   i.e. 82–100% efficiency against the physical-core ceiling. Hyperthreading
   adds only ~0–10% on this integer/branchy workload.
3. Two explainable effects pull some *totals* below 4x:
   - a **~10–20% single-threaded per-node regression in assembler_1** vs 0.7.1
     (2.7x more instructions per identical search node), and
   - a **disassembly long-tail** in Excelsior/Supernova where one job is
     6–400x larger than its siblings (Amdahl ceiling ~1.5x, unfixable at pool
     level).

No pool serialization bug was found: per-job instrumentation shows 7
disassembly jobs running on 7 different worker threads.

---

## 1. Method

- Machine: 4 cores / 8 threads (`lscpu`: Core(s) per socket 4, Thread(s) per
  core 2). The threading ceiling for compute-bound search is therefore ~4x,
  not 8x. The new binary defaults to 8 workers (`hardware_concurrency`).
- `bench/compare_snapshots.py` on the two snapshot CSVs gives the *total*
  speedup (algorithm × threading). To separate the factors, each puzzle was
  re-run on current master as: 0.7.1 binary vs new `-t 1` (isolates
  algorithm/SIMD/prep: same thread count) vs new `-t 8` (isolates threading:
  same binary). Phase split via runs with/without `-d` (assembly-only vs
  assemble+disassemble). Hotspots via `perf record -g` / `perf stat`
  (cycles, instructions, branches, cache).

## 2. Decomposition: algorithmic vs parallel speedup

Medians of 2–3 runs on current master (absolute times carry ±10–20% laptop
noise; ratios are stable):

| Puzzle (engine) | 0.7.1 | new `-t 1` (algo factor) | new `-t 8` (par 1→8) | Total |
|---|---|---|---|---|
| Lomino:1 (asm 0) | 2.47s | 0.91s (**2.7x**, fewer iters: 618k→384k) | 0.25s (**3.7x**) | **10.0x** |
| kangaroo (asm 0) | 2.06s | 1.95s (**1.05x**, same work) | 0.59s (**3.3x**) | **3.5x** |
| Lomino:3 (asm 1) | 61.3s | 68.2s (**0.90x**, same work) | 17.9s (**3.8x**) | **3.4x** |
| SolidSix (asm 1) | 8.23s | 9.97s (**0.83x**, same work) | 2.49s (**4.0x**) | **3.3x** |
| Simplicity (asm 1) | 3.68s | 0.61s (**6.1x**, iters 5.1M→19k) | 0.17s (**3.6x**) | **22x** |
| TTTCharm (asm 1) | 6.03s | 2.85s (**2.1x**, iters 440k→20k) | 0.79s (**3.6x**) | **7.7x** |
| Excelsior | 2.42s | 0.97s (**2.5x**) | 0.86s (**1.13x**) | **2.8x** |
| Supernova:0 | 1.68s | 0.69s (**2.4x**) | 0.63s (**1.10x**) | **2.7x** |

Takeaway: wherever there is parallelizable search work, threading delivers
3.3–4.0x. Totals below 4x come from the *other* two columns.

## 3. Class A — search-bound, same work: apparent assembler_1 deficit (resolved in §7 as a build-flags artifact)

kangaroo (assembler_0), Lomino:3 and SolidSix (assembler_1) perform
essentially identical searches old vs new (iteration counts match to 4+
digits), yet new `-t 1` is flat (asm_0: 1.05x) or **10–20% slower**
(asm_1: 0.90x / 0.83x). `perf stat` on SolidSix, assembly-only:

- old: 33.3B instructions / 24.7B cycles (IPC 1.35) → 6.1s
- new `-t 1`: 91.4B instructions / 30.1B cycles (**IPC 3.04**) → 7.5–10s

Same nodes, **2.7x more instructions and 6.7x more branches**
(23.6B vs 3.5B), partly compensated by 2.25x higher IPC. `perf record` puts
86% of new time in `assembler_1_c::hiderow/unhiderow` — the classic engine,
not the SIMD path. Corroborated by `BURRTOOLS_NO_SIMD=1` /
`BURRTOOLS_NO_VECTOR=1` changing nothing on these puzzles, and by the
branch-heavy (not vector-heavy) profile. Likely contributors accumulated
since 0.7.1: parallel scaffolding, per-node stop-token polling, SIMD-tier
dispatch, extra open-column checks (`find_best_unclosed_column`,
`open_column_conditions_fulfillable` visible in profile).

The 3.8–4.0x threading more than repays it, but the *total* vs 0.7.1 lands
at ~2.6–3.4x instead of 4x+.

**Actionable:** reduce per-node instruction/branch cost in the assembler_1
serial search loop (`assembler_1_c::iterative` / `hiderow` / `unhiderow`).
Every assembler_1 puzzle's total improves by whatever is recovered, since
the parallel factor multiplies on top. Assembler_0 shows no such regression.

## 4. Class B — disassembly-bound long-tail: Excelsior, Supernova:0

With/without `-d` runs show assembly costs 0.00–0.01s; ~99% of wall time is
disassembly, and the scaling curve is flat
(`-t 1/2/4/8`: 0.98 / 0.90 / 0.88 / 0.88s; `perf stat`: 1.18 CPUs of 8).
Pool serialization was suspected and ruled out: temporary per-job timing in
`disassemblerPool_c::worker_loop` (instrumentation since reverted) showed
**7 jobs on 7 distinct worker threads** with extreme size skew:

- Excelsior: seq=2 takes **626ms** (the single true solution); siblings take
  0–77ms. Wall ≈ max job ≈ 0.88s.
- Supernova:0: seq=2 takes **441ms** of ~0.65s; siblings ≤36ms.

`perf script` TID breakdown agrees (86% of samples on one thread). Hotspots
are genuine single-disassembly search work —
`movementAnalysator_c::checkmovement` (~21%),
`movementCache_c::getMoValue` (~13%). Only the actually-disassemblable
assembly needs deep search; failures exit fast. The Amdahl ceiling is ~1.5x
with unbounded threads; CPU% ~114% is the correct signature, not a bug.
Fixing this class requires parallelizing *within* one disassembly, not
across assemblies. (Their ~2.7–2.8x totals come purely from faster *serial*
disassembly vs 0.7.1.)

## 5. Why the spread is "all over the place"

Each puzzle is dominated by a different phase, and each optimization only
moves its own phase:

- **Prep/reduce wins** (faster `createMatrix`/placement reduction) collapse
  iteration counts 10–260x → Simplicity 18.7x, BottomLine 6.2x, Tippy 4.2x,
  TTTCharm 5.4x, Lomino:0/1/2 5–7x. Superlinear vs threading; unrelated to
  core count.
- **Same-work search** → ~3.3–4.0x parallel; the apparent §3 serial deficit
  was a build-flags artifact (§7), so release-binary totals should sit at
  ~3–4x (kangaroo, Lomino:3, SolidSix).
- **Disassembly long-tail** → ~1.1x parallel, ~2.8x total from faster serial
  disassembly (Excelsior, Supernova:0).
- **Sub-0.2s puzzles** (Pelikan, Dracula, unlucky block, measured 6–147ms):
  fixed costs — binary load, XML parse, spawning 8 jthreads + merger —
  dominate, so no speedup arithmetic can apply.

## 6. Verdict and follow-ups

- No concurrency defect found. Expectation to reset: **~4x of the
  parallelizable fraction** (4 physical cores), multiplied by that puzzle's
  algorithmic factor — the table then matches theory.
- Follow-up 1 (done, see §7): the §3 "assembler_1 overhead" turned out to be
  a build-configuration artifact, fixed with a release+ndebug bench build.
- Follow-up 2 (only if disassembly-bound puzzles matter more): exploit
  parallelism *inside* a single `disassembler_0_c::disassemble`, e.g. over
  first-move subproblems; pool-level tuning cannot help the long-tail shape.
- Methodology note: medians of 2–3 runs on a frequency-scaling laptop carry
  ±10–20% noise (e.g. Lomino:3 `-t 1` measured 68–72s across runs), and a
  loaded box (Firefox/packagekit at ~50% CPU during this analysis) adds more.
  Deterministic `perf` instruction/branch counts were used wherever wall time
  was inconclusive. A/B claims tighter than ~10% need a quiescent machine,
  pinned runs (`taskset`), fixed governor, more repetitions.

## 7. Assembler_1 overhead investigation: build flags, not code (2026-09-24, branch `perf/asm1-serial-overhead`)

### 7.1 Initial suspicion and how it was ruled out

§3 reported new `-t 1` executing 2.7x more instructions (91.4B vs 33.3B) and
6.7x more branches (23.6B vs 3.5B) than 0.7.1 for identical iteration counts
on SolidSix. Diffs of the hot functions showed `hiderow`/`unhiderow`
themselves are **byte-identical** between v0.7.1 and HEAD, and the profile
attributes 86% to those same two functions — so the extra instructions had
to come from *around* the code, not from changed search logic.

`perf annotate` on the new build settled it: the hottest loop is interleaved
with `__glibcxx_requires_subscript` size-compare + `jae .cold` sequences —
every `vector::operator[]` in `cover`/`uncover`/`hiderow` carries a bounds
check. Root cause: **all local builds had `b_ndebug=false`**, so Meson
injects `-D_GLIBCXX_ASSERTIONS=1` and `NDEBUG` stays undefined, which keeps
every `bt_assert` (e.g. the per-node `bt_assert(ret == …)` in
`assembler_1_c::iterative`, plus several `bt_assert(column_condition_…)` in
debug/step paths) live as well. The 0.7.1 tarball was a true release build
(assertions off); we were comparing apples to oranges.

Decisive experiment (`perf stat`, deterministic counts, SolidSix `-t 1`
assembly-only):

| Build | Instructions | Branches | Cycles |
|---|---|---|---|
| `build/` (-O2, assertions on) | 91.41B | 23.60B | 32.24B |
| fresh `--buildtype=release -Db_ndebug=true` | **33.33B** | **3.53B** | 24.62B |
| 0.7.1 tarball | 33.31B | 3.51B | 24.67B |

The ndebug build reproduces the 0.7.1 instruction/branch stream almost
exactly. **There is no per-node code regression in assembler_1 (or
assembler_0).** Serial parity confirmed on wall time too once cool and
quiescent: kangaroo assembly-only `-t 1` gives 0.90s (0.7.1) vs 0.92s
(ndebug) — inside run-to-run noise.

### 7.2 Secondary finding: -O2 vs -O3 does not matter (without assertions)

An early `-O3` (build-rel, assertions still on) vs `-O2` (build/, assertions
on) comparison showed -O3 ~15% slower on kangaroo, which briefly looked like
a codegen problem. With assertions off, interleaved `-O2` vs `-O3` runs are
identical (kangaroo 0.92 vs 0.92s; SolidSix 6.46 vs 6.49s): the gap was an
-O3×bounds-check interaction (unrolled loops × per-access checks), not a
codegen issue. No `-O` flag change needed.

### 7.3 Fix implemented (this branch)

Code changes were deliberately *not* made — there is nothing to fix in the
solver; the per-node items audited along the way (atomic `iterations`
RMW, per-node `stop_token::stop_requested()` load, `finishedMutex`
push/pop, `emittedSignatures` dedup, stop-token refcount churn at 0.03%)
are all negligible once the instruction stream matches 0.7.1. The fix is
build configuration plus guardrails:

- `justfile`: new `just build-release` recipe
  (`meson setup build-rel --buildtype=release -Db_ndebug=true` + ninja).
- `justfile`: `just bench` now depends on `build-release` and measures
  `build-rel/burrTxt`, so snapshots can never again measure assertion
  overhead by default. `bench/run_snapshot.sh --binary` still overrides.
- `AGENTS.md`: benchmarking section now mandates the release binary for any
  published timing or hand-rolled A/B, with a pointer to this section.

Deliberately *not* changed: the default `build/` dir keeps assertions on —
dev/test builds should keep `bt_assert` and `_GLIBCXX_ASSERTIONS` catching
bugs (the test suite exercises those paths). Correctness of the release
binary was verified by identical stats output
(SolidSix: 588 assemblies / 4302868 iterations in both binaries) and
`just test` passes (3/3).

### 7.5 Release-binary baseline, same day/machine (2026-09-24 evening)

With the box quiet, two same-day snapshots were taken: `bt071/burrTxt`
(`results_20260924_185232_…`) and `build-rel/burrTxt`
(`results_20260924_185054_…`). Totals (0.7.1 → new release, speedup):

| Puzzle | 0.7.1 (s) | New rel (s) | Total |
|---|---|---|---|
| Simplicity | 3.96 | 0.20 | 19.8x |
| BottomLine | 1.63 | 0.21 | 7.8x |
| TTTCharm | 7.12 | 0.95 | 7.5x |
| Lomino:1 | 2.38 | 0.37 | 6.4x |
| Lomino:2 | 2.17 | 0.36 | 6.0x |
| Tippy | 0.79 | 0.14 | 5.6x |
| Lomino:0 | 0.50 | 0.10 | 5.0x |
| SolidSix | 8.80 | 2.42 | 3.6x |
| Lomino:3 | 61.69 | 18.13 | 3.4x |
| Supernova:0 | 1.85 | 0.59 | 3.1x |
| Dracula | 0.21 | 0.07 | 3.0x |
| kangaroo | 2.04 | 0.71 | 2.9x |
| Excelsior | 2.46 | 1.05 | 2.3x |
| Pelikan | 0.21 | 0.11 | 1.9x |

Every row moved up versus the debug-binary snapshots in §0 (e.g. SolidSix
2.62x→3.64x, Lomino:3 2.73x→3.40x), confirming the ~15–30% assertion tax.
Same-work search puzzles now sit at the 4-core ceiling minus ordinary
parallel efficiency; the remainder is the §4 long-tail (Excelsior 2.3x),
the disassembly-phase fraction (kangaroo 2.9x), and fixed costs on tiny
puzzles. Cross-day comparison proved unreliable in the process: the
*identical* 0.7.1 kangaroo binary measured 1.33s (Sep 23) vs 2.04s (Sep 24,
+53%) — same-machine, same-day baselines are mandatory, which is exactly
what the snapshot filenames + provenance lines are for.

### 7.6 Remaining recommendations

The old rec. 1 (re-baseline) is done — §7.5 above. Left open:

1. **CI (optional)**: a job that asserts `build-rel` exists/builds would stop
   the bench recipe from rotting; a stronger variant fails if any bench CSV
   is committed whose provenance `mode` line points at `build/burrTxt`.
2. **If serial search ever needs more**: the remaining per-node costs are, in
   order, `vector::operator[]` double-indirection (`colCount[colCount[rr]]`),
   the atomic `iterations` RMW (batch it: thread-local count, periodic
   publish), and the `stop_token` load per node (already minimal). None is
   justified today — instruction counts equal 0.7.1.
