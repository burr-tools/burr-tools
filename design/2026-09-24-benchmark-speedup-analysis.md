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

## 8. Low-hanging-fruit triage (2026-09-24, same branch)

Question: after the build-flags fix, is there cheap serial performance left?
Method: release-binary `perf record` on kangaroo (asm_0) and SolidSix
(asm_1), both `-t 1`. Result: zero SIMD symbols — the heaviest puzzles run
fully classical paths. Findings, ranked:

1. **Done — `getPieceInformation` linear scan → binary search** (both
   assemblers, committed): `piecePositions[].row` is strictly ascending
   (`piecenode == left.size()`, append-only), so the reverse linear scan per
   call became `upper_bound` logic, same result (last entry with
   `row <= node`). Called once per piece of every found assembly; kangaroo
   (9831 assemblies) spent ~8% of instructions there. Measured, release
   binary, kangaroo `-t 1` asm-only: 6.73B → 6.21B instructions (−7.8%),
   4.11B → 3.91B cycles (−4.7%). Verified by `test-all` (4/4) and
   `test-regression` (20/20 vs 0.7.1).
2. **SIMD is gated off on exactly the heavy puzzles — structural, not
   low-hanging.** Temporary `canUseSimd` diagnostics (since reverted) show:
   kangaroo blocked by `holes=65` *and* `res_vari=160` (asm_0 requires
   neither); SolidSix/Lomino:3 pass asm_1's early gates (`headerNodes` 131 /
   136 ≤ 32768) and fall into the `hasRange`/variable-count exclusion (both
   print "range optimisation used"). Extending the bit-parallel solvers to
   holes / variable voxels / range puzzles is an algorithmic project (cf.
   #87, #82), not a tweak. `BURRTOOLS_NO_SIMD=1` A/B confirms: 0.97–1.13x,
   i.e. no measurable SIMD contribution on these puzzles today.
3. **Thread count already optimal.** Release binary, with `-d`:
   SolidSix `t1/t2/t4/t8` = 7.98/4.95/3.79/2.46s; kangaroo =
   1.70/0.98/0.70/0.67s. Eight workers on 4 cores beats four (HT helps
   +54% on SolidSix — latency-hiding in pointer-chasing DLX); keep the
   `hardware_concurrency` default.
4. **Ruled out:** `iterativeMultiSearch` 11.6% self-share is the loop itself
   (MRV scan is algorithmic); per-node `stop_token` load + atomic
   `iterations` RMW are a few instructions against thousands in
   cover/uncover; disassembly `movementCache` is mutex-free (per-worker
   instances) and its 13% share only matters for the Amdahl-capped §4
   puzzles anyway.

## 9. Range puzzles go SIMD (2026-09-24, branch `perf/simd-huang-range`)

§8.2 left SIMD coverage for gated puzzles as the next big lever. The
`hasRange` gate in `assembler_1_c::canUseSimd` turned out to be stale
review conservatism (#82: "duplicate-assembly concern for is_range pivots",
gate as the safe resolution), not a missing feature: `SimdHuangCover`
already models the range column end to end (`is_range` bounds,
per-row `range_weight`, max pruning in scalar/AVX2/AVX512/NEON
`filterRows` kernels, min/max goal check, MRV/dead-end pruning in
`search()`, bound checks in `solveSubtree()`), and `createSimdSolver()`
already feeds it. The change is deletion of the gate plus tests.

Correctness evidence (all differential DLX-vs-SIMD, `BURRTOOLS_NO_SIMD=1`
as the DLX side):
- SolidSix: 588/588 assemblies, sorted `-s` placement output identical
  (only the stats line differs: 4.30M → 0.60M iterations).
- Lomino:3: 5/5 assemblies, placement output identical (37.7M → 4.3M).
- Disassembly parity (`-d`): SolidSix 588/179, Lomino:3 5/5, both engines,
  serial and `-t 8`.
- Small range puzzles agree too (DemoMirrorParadox 1/1, DemoPieceGenerator
  928/928, PiecesOfEight 6/6) — PiecesOfEight and DemoPieceGenerator added
  to the `[solver][simd][dlx][equivalence]` test (DemoMirrorParadox was
  already listed and now actually exercises the SIMD path).
- `test-all` 4/4, `test-regression` 20/20 vs 0.7.1, `just check` clean.

Speedup (release binary, same-day): SolidSix assembly 7.9s → 0.54s
(**14.6x** serial, 2.4s → 0.23s parallel); Lomino:3 ≈63s → 13.6s serial,
≈18s → 4.4s parallel. Corpus totals vs 0.7.1: **SolidSix 38.3x, Lomino:3
14.0x** (`results_20260924_201717_…`); all other puzzles unchanged within
noise. Only 2 of 16 corpus puzzles use range columns (both min==max);
`examples/` adds DemoMirrorParadox, DemoPieceGenerator, PiecesOfEight.

Still gated (unchanged, structural): variable voxels with min<max shapes,
>32768 columns, and all of assembler_0's holes/variables exclusions
(kangaroo: `holes=65, res_vari=160`).

## 10. Holes/variables go SIMD in assembler_0 (2026-09-24, branch `perf/simd-exact-holes`)

The remaining structural gap from §8.2: `SimdExactCover` knew only required
columns, so `canUseSimd()` refused every puzzle with holes or variable
voxels — i.e. all of assembler_0 except the Lomino:0/1/2-type pure cases
(which already ran SIMD). A corpus probe showed only kangaroo (holes=65,
vari=160, 1.13M iterations) has enough search for this to matter; the rest
is trivial or disassembly-bound.

DLX semantics ported: variable columns live on their own ring (never
pivoted); covering is at-most-once; uncovered-unfillable columns beyond the
`holes` budget prune at column-selection time, with no check at the goal
(`solution()` fires as soon as ring 0 is empty). The SIMD port mirrors this
exactly: new `setOptionalColumn`/`setHoleBudget` interface, optional bits in
the disjointness masks (conflict-free), never in `active_column_list` (never
pivoted, never dead-end on empty), hole prune at `search()` entry over
uncovered optionals with zero covering rows among the active set. Deliberate
parity decision: **no budget check at the goal**, matching DLX — including
its quirk that an over-budget excess appearing after the last selection
check still reports (hole count is monotonic along a path, so pruning is
safe, but the goal itself is unchecked in both engines; "fixing" it would
break 0.7.1 parity, so it stays as-is). `solveSubtree` needs no change
(prefix flows into the checking `search()`); none of the four `filterRows`
kernels change (full-mask disjointness already covers optional bits).
`createSimdSolver` walks both rings, sizes tiers over the total width
(kangaroo: 366 → 512 tier), and the 2048 cap now applies to the total.

Correctness: kangaroo 9831/9831 assemblies with sorted `-s` placement
output identical; solutions 2/2 serial and `-t 8`; all other holes puzzles
agree (Excelsior 7/7 incl. `-d`, Pelikan 12/12, Supernova 10/10, Dracula
84/84); no-vari path bit-identical (Lomino:1 `-t 8` SIMD iterations exactly
383910 as before). New `[simd][exact_cover][holes]` unit test pins the
machinery (incl. the no-goal-check parity); Pelikan/Dracula in the
equivalence test now exercise the SIMD path. `test-all` 4/4,
`test-regression` 20/20, `check` clean.

Speedup: kangaroo assembly 1.13M → 0.58M iterations; with disassembly
t1 1.72s → 1.04s (**1.65x**), t8 0.66s → 0.47s (**1.4x**), corpus total
2.9x → **3.2x** (`results_20260924_203315_…`). Modest — disassembly owns
the rest — but it closes the last structural SIMD gap for assembler_0.

Follow-up fixed on the same branch: SIMD iteration batching (publish every
256 nodes, remainder at end) left sub-256 searches reporting live 0 after
their first solution, flaking `test_iterator_iterations_live_and_finished`
under load (reproduced 2/15). Both solvers now flush remainders at each
reported solution (exact: batches re-based on `flushed_iterations`, DLX
worker idiom) and both parallel paths drain per-task counters to shared at
each solution. Totals bit-identical (kangaroo 580114, SolidSix 601901);
20/20 python runs green under full parallel-build load. This also fixes the
open TODO.md item on frozen parallel-SIMD counters.
