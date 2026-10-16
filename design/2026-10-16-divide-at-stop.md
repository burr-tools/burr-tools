# Divide at stop: resume state holds only unexplored work

Branch stack: `pr/resume-divide-dlx` (DLX engines), `pr/resume-divide-simd`
(SIMD engines, signature removal). Spec: protocol unaffected (see §7).

## 1. Problem

Since #90 a stopped parallel search saves its pool remainder plus the
signature of every assembly it has reported (`emittedSignatures`). On
continue, an interrupted task restarts from scratch and the set suppresses
the repeats. That costs, forever and per reported assembly, ~20 B in the
saved file and ~40 B of RAM -- the set is inserted into on every run, so
even runs that are never resumed pay it -- plus the re-walk of every
interrupted task on resume.

A related report: a puzzle paused after ~6 h with no assemblies found took
very long to save. That one is the frontier, not the signatures:
assembler_0's runtime splitter enumerated one task per live column row, so
a depth<3 split on a breadth-B column queued ~B^2 tasks, each persisting
its full prefix. Fixed separately: the fan-out cap (`kSplitFanout`,
cursor rests) bounds each split, and split children are queued depth-first
(front of the pool) so the queue no longer expands towards every node of
the split depth. Measured on the reported puzzle (Hazmat Cargo 2, paused
after 600 s, no assemblies yet): master saves 351 MB (29 MB gzipped), the
fan-out cap alone 4.4 MB (93k tasks), with depth-first order 39 KB (5 KB
gzipped, 1,005 tasks).

## 2. Idea

A stopped task is not requeued whole. It is divided, on the stopped
worker's live state, into the pieces it has not explored yet; the searched
region is dropped. Then:

- every saved task holds only unexplored territory, so resume re-walks
  nothing and needs no dedup at all -- the signature set can go;
- the remainder stays bounded: O(depth x K) pieces per interrupted task
  (K = 16, the same fan-out cap as runtime splitting).

Partition argument (all engines): an unexplored leaf either diverges from
the live path at exactly one level -- covered by a piece for one of that
level's remaining branches -- or extends the live path, which is covered
by one "continue" piece exactly when the deepest level was reached by a
fresh descent (nothing below it searched yet). Retreat landings (the level
below was just exhausted) get no continue. The sets are disjoint, and the
dropped regions were fully searched with their solutions delivered.

One more case: a solution found but declined because the stop arrived
between discovery and delivery ("missed"). It lies on a dropped path, so
it is kept as its own single-leaf piece.

Engines are converted one commit at a time. Until the last one, the
signature set stays in place: converted engines simply never produce
repeats for it to suppress (which also means intermediate commits' union
tests detect losses but not duplicates; the final commit's do both).

The fan-out cap K is a parameter (`BURRTOOLS_SPLIT_FANOUT`, default 16,
`src/lib/split_fanout.h`), read once per run. Any K >= 1 is exact. At the
default only Burr-Glar among the bundled puzzles has levels wide enough to
produce rests, and it takes minutes; the tests therefore run small puzzles
at K = 1-2, where nearly every divided level leaves a rest.

## 3. assembler_0, DLX

`assemblerWorker_c::divideRemainder(task)` runs on the stopped worker's
private matrix copy (workers are single-use after a stop, so the dirty
state is never reused).

- Stop is only noticed at the loop head, where exactly two states occur: a
  fresh descent (`rows[pos] == 0`, covered levels `[prefix, pos-1]`) or a
  retreat landing (covered `[prefix, pos]`). The covered range is scanned,
  not assumed from `pos`.
- Continue piece over the full live path iff fresh descent
  (`topCovered + 1 == pos`).
- Per covered level, deepest first: uncover the current row *before*
  enumerating (its placement masks valid alternatives), then the rows after
  it become pieces -- up to `kSplitFanout` singletons plus one cursor rest,
  the same convention as `splitPrefix`, so rests resume through the
  existing cursor machinery.
- Missed solutions are recorded by `solution()` (`handleSolution` reports
  `didConsume`) and re-emitted as leaf pieces.
- `searchSubtree` returns whether its own loop exhausted the territory
  (checked before the stop token: an empty-prefix retreat wraps `pos`). An
  exhausted task hands back only its missed pieces (`takeMissed`) -- a stop
  landing inside a task's last deliveries must not drop them, and single-
  leaf tasks (which divide itself creates) are exactly that case. The
  caller never re-reads the token to decide completion.
- Untouched tasks (nothing placed past the prefix) are requeued whole,
  which is exact; `divideRemainder` signals this explicitly
  (`requeueWhole`). The remaining fallbacks are asserted invariant breaches.
- No pieces is a real outcome, not "untouched": a retreat landing on the
  last row of every covered level has nothing left, and the task counts as
  done. Reading an empty result as "requeue whole" re-delivered fully
  searched tasks; the fan-out-1 test below found it.
- The pieces partition the parent's territory, so its unit in `totalTasks`
  transfers to them (`+= n - 1`); shares divide evenly.

Tests: "divided DLX remainders resume exact-once" (a synthetic line
puzzle, `makeLinePuzzle`, at fan-out 1, 4 workers, four stop points; rests
occur), "a
missed solution on an exhausted task survives the stop" (two single-leaf
tasks, two workers, a slow stopping callback so the second solution is
declined as missed; fails without `takeMissed`), "a task stopped right
after its last branch is not requeued" (prefixes two levels short of a
full path, stopped at their last assembly; deterministic, fails 12/12
checks with an empty result read as "requeue whole").

## 4. assembler_1, DLX: live write-back

assembler_1's DLX worker keeps its whole position in five explicit stacks
(`task_stack`, `next_row_stack`, `column_stack`, `rows`, `hidden_rows`),
which fully determine the matrix via `restoreMatrix()`. So no division is
needed: a stopped worker writes its live stacks back into the requeued
task, and resume continues exactly there. Delivered solutions sit behind
the frontier; a missed one (`worker_solution()` returns false) holds it,
so the resumed task re-discovers and delivers it.

Three details make write-back exact:

- **Restore replays the frontier too.** Generated tasks have a cover-free
  top, so `restoreMatrix` used to replay all but the last entry. A stopped
  worker's top can carry live covers (1/2/5); skipping it started the
  resume on a corrupted matrix and silently lost subtrees. Restore now
  replays every entry; cover-clean tops (0/3/4/7) are no-ops.
- **A stopped top-6 drains first.** Case 6 (mid-row bookkeeping, covers
  applied) never delivers or descends, so the loop runs it out even after
  the stop, bounded by the live depth.
- **`baseDepth`, the territory root.** `worker_iterative(size)` used the
  stack size as its floor; after write-back that is the (deeper) live
  size, so a resumed task exited as soon as it popped below the interrupt
  depth, abandoning its ancestors' remaining alternatives. The root is
  persisted per task (set at generation, never touched by write-back), and
  both the resume floor and the completion rule key on it: a worker that
  popped below its root exhausted the task -- also when the stop landed
  after the last pop.

`baseDepth` bumps the assembler_1 format to 2.2; 2.1 (0.8.0) still loads.

Test: "assembler_1 pool write-back resumes exact-once" (a synthetic 1-D
range puzzle A0 refuses, DLX pool, stop after 5, save/restore/continue,
multiset union). Fails 6/6 with the resume floor at the live size.

## 5. assembler_1, Huang: divide while unwinding

The Huang engine (`SimdHuangCover`) is a recursive bit-parallel search, so
there is no explicit stack to divide. None is needed: every frame's
candidate list (`scratch_active_rows[depth]`) is still intact when the stop
unwinds the recursion (deeper frames only write deeper levels), and every
frame returns through a stop check. Frames record their own remainder on
the way out (`StopTrace`), in code that only runs once a stop is requested
-- nothing per node:

- entry check: the node was never explored -> one piece over the full path
  (at the task root: the task itself, including its rest position);
- post-backtrack check at depth d: the current branch is done (the child
  returned), later branches untouched -> one seed per remaining branch
  (same `coversPivot`/`fitsBounds` filters as the loop), up to
  `kRestFanout` (16), then one *rest* seed (`restSkip` = index into the
  frame's active list) for the remainder; rests resume through
  `search(depth, ..., start)`, which re-derives the same pivot and list
  from the replayed state;
- a solution declined by the stop (`consumed == false`) -> one leaf piece.

An empty trace means the task was fully searched: completion is decided by
the trace, not by re-reading the token. The pieces partition the task, so
its unit in `total_tasks` transfers to them (`+= n - 1`).

**Seeds must replay exactly.** `search()` and `generateTasks()` apply a
monotonic shape-row filter below a step chosen on a shape pivot (identical
pieces are enumerated in increasing row order only). Seeds used to hold
only node ids, and `replayPrefix` replayed every step without that filter,
so a replayed seed searched a superset: permutations of identical pieces
reappeared in a sibling seed's territory. The signature set absorbed this;
once it is gone (§7) it would re-report. Seeds therefore carry one
`pivotShape` bit per step; `replayPrefix` applies the filter exactly as the
search did, and `generateTasks` records the bits along its expansion path.
Empty-prefix seeds (the whole search) are no longer dropped.

Format 2.2 Huang seeds: prefix, pivot bits (a vector, one 0/1 per step),
rest start. 2.1 seeds (prefix only) still load, replayed as before.

Tests: "SimdHuangCover: salvaged seeds replay the monotonic shape filter"
(engine level, no dedup: one shape needed twice, 20 placements in optional
voxels; resuming from the 20 salvaged seeds must report C(20,2) = 190 --
380 without the bits), "Huang divided remainders resume exact-once across
chained stops" (PiecesOfEight, CubeInCage, DemoPieceGenerator,
SolidSixPieceBurrs at fan-out 1; stop/save/restore twice, then finish;
every save contains rests).

## 6. assembler_0, SIMD: divide while unwinding

Same scheme as §5 inside `SimdExactCover::search()`
(`ISimdExactCover::SubtreeRemainder`): entry check -> continue piece,
post-backtrack check -> remaining branches (16 singletons + one rest),
declined solution -> leaf piece (added by the A0 worker, which knows about
delivery). No pivot bits: this engine's filter depends only on the chosen
row's mask. An empty remainder means the task is complete.

The A0 worker converts pieces back to ordinary `SubtreeTask`s: steps inside
the task's own prefix are copied verbatim, deeper steps become
`{colCount[node], node}` (a node's DLX column), so divided pieces are normal
splittable tasks. SIMD rests carry `restSkip` (an index into the SIMD
frame's own candidate order): never split (`shouldSplit`), resumed through
`solveSubtree(..., start = restSkip)`.

Rests are only valid for the engine that made them, so the remainder now
pins its engine (`remainderEngine`, saved after the interrupted flag): a
resumed run keeps it regardless of `BURRTOOLS_NO_SIMD` and friends. Parse
rejects a rest under a non-SIMD remainder or combined with a DLX cursor.
Format 1.7 per task: prefix, share, cursor column/skip, rest start.

Tests: "SimdExactCover: stop remainders with rests resume exact-once"
(engine level: 40 branches at the root, stop after every delivery, rests of
rests), "A0-SIMD divided remainders resume exact-once across chained stops"
(PelikanBurr, DraculasDentalDesaster), "A0-SIMD remainders with SIMD rests resume
exact-once" (the synthetic line puzzle at fan-out 1, four stop points;
SIMD rests occur).

## 7. Removing the signature set

With every engine dividing (§3-§6), nothing re-walks searched territory,
so the signature set goes: `emittedSignatures`, `resumeDedup`,
`assemblySignature()`, and the signature lists in both formats. Three
things depended on it and change with it:

- **Single-threaded continue stays on the pool.** A restored remainder used
  to resume single-threaded through the serial search from the root, with
  the signatures suppressing the repeats. `willRunParallel()` now returns
  true whenever a remainder exists, so the pool runs it with one worker.
- **assembler_1 pins its engine too.** A DLX remainder (ptype 1) resumes on
  DLX and Huang seeds (ptype 2) on Huang, regardless of the benchmark
  toggles; a run on the other engine used to regenerate and dedup.
- **Interrupted 0.8.0 saves (1.6 / 2.1) are refused** with
  `ERR_CAN_NOT_RESTORE_INTERRUPTED`: their remainder re-searches in-flight
  tasks whole and relied on the signature list. Uninterrupted ones still
  load. Neither 1.7 nor 2.2 is in a tagged release yet, so both are final
  here.

The disassembly stash (`problem_c::stashAssemblies`) stays required, for a
new reason: assemblies found but never disassembled are not found again,
because resume never re-walks the territory they came from.

Spec (`spec/`): unaffected. Divide and write-back change what a stopped
worker hands back, not the pool protocol: an interrupted DLX/A0-SIMD task
still re-enters the pool (now as one or more pieces) before `finishTask`,
Huang still appends to its salvage list, and the drain/finish pairing is
unchanged.

Tests (now exact in both directions -- duplicates are no longer masked):
the union tests of §3-§6, plus "Saved interrupted search XML size is
bounded and independent of solution count", "Legacy interrupted search
saves are refused on restore", "Interrupted search resumes with pinned
engine across environment overrides", "single-threaded DLX continue reuses
the pool remainder".

## 8. Validation

On the final tree (dev and release `--werror -Db_ndebug=true` builds):
fast and slow suites, `test_examples_regression.py` 20/20 against 0.7.1,
cppcheck clean, `just spec-check` (all seven model configurations, no
errors). The fast suite takes about 26 s on a 4-core laptop; no test needs
Burr-Glar's minutes any more. Mutation checks per engine, each applied
alone: no continue piece,
missed solutions not recorded, rests dropped, rest start ignored on
resume, pivot bits ignored, resume floor at the live size -- every one is
caught by at least one of the tests above.

Performance (indicative, shared dev machine; a full A/B is still to be run
on a quieter machine): identical results everywhere; LominoSquare 3/4
(Huang) at 1.00, :5 at 1.05; Krijnen TheCube/JiminyJack/Tipperary at
1.08-1.14 on one or two samples; a 10x interleaved single-thread check on
CondorsPeeper:1 at +1.3% (noise level). Command:
`bench/bench_solve.py --ab <before> <after> --no-disassemble --runs 3`.
