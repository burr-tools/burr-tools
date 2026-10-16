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
