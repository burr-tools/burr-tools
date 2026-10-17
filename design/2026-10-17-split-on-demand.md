# Split on demand

Status: implemented for every engine: assembler_0 (DLX and SIMD
workers, sections 3-4) and assembler_1 (Huang and DLX, sections 8-10).

## 1. Problem

The task pool used to be refined at pop time by heuristics: every task
with a prefix shallower than 3 was split before it ran, and deeper ones
were split when a sibling was already waiting and the queue was short.
Both guess at the future. A task that turned out large after it started
could no longer be split, so one big subtree could leave the other
workers idle at the end of a run (the "long tail"). A task that was split
eagerly but was small anyway just cost queue traffic.

## 2. Idea

Split a task only when someone needs the work, and split the task that
actually holds it: a **running** task. When a worker is waiting, a busy
worker hands over the untried part of the shallowest level of its own
search that still has untried alternatives, and stops before those
alternatives itself.

- The pool keeps a relaxed atomic flag, `wants_work()`: more workers wait
  than tasks are queued. It is re-derived under the pool mutex on every
  change of the waiting count or the queue (`assembler_pool.h`), but
  stored only when its value changes, and it sits on its own cache line,
  so pops and pushes do not keep invalidating the line the workers poll.
- Busy workers poll that flag every 128 (DLX) or 256 (SIMD) nodes, a
  relaxed load next to the iteration flush that already happens at that
  rate. Benchmarks show no measurable cost (section 6).
- A worker that sees the flag and has something to give first claims it
  (`claim_work()`, a CAS from true to false), and only the winner donates.
  Without the claim, every busy worker whose poll lands before the first
  push would donate for the same waiter. The donor's push re-derives the
  flag, so a second waiter raises it again. A worker claims only once it
  has found an open level, so a claim is always followed by a push.
- A donation is an ordinary `push_tasks`, made from inside the donor's
  search: the claim's CAS and the push's mutex and notify are the only
  pool operations the node loop performs, at most once per waiter. The
  push never refuses (the pool has no stop flag), so nothing donated is
  dropped and every claim re-derives the flag. Nothing in the pool protocol
  changes: the spec already models pushes by active workers, so the
  TLA+ model is unaffected.
- At pop time only cursor tasks are split, because a cursor cannot be
  searched itself. All other refinement happens by donation.

`BURRTOOLS_NO_DONATE=1` restores the old pop-time heuristics (A/B toggle).
`BURRTOOLS_DONATE_EVERY=N` forces a donation every N nodes, whether or not
anyone waits; tests use it to make donation deterministic, even with one
thread.

## 3. DLX workers

The donor scans the placed levels from the task's prefix down. Level d is
open if it is not cut yet, its current row has a successor in the column,
and at least two levels remain below it (deeper pieces are about as cheap
as the handoff). For the shallowest open level the donor:

1. pushes one **cursor** task: prefix = the task's prefix plus the live
   path above d, cursor column = the column of d, skip = the index of the
   current row from the column head plus one. The popping worker splits
   the cursor into at most K singletons plus a rest, like any other cursor
   (`splitPrefix`).
2. sets `cut[d]`. The advance at level d then treats the level as
   exhausted after the current row's subtree. The flag clears when the
   level is dropped, so a fresh pick at d starts uncut.

**Why the cursor names exactly the donated rows.** The cursor's resume
state is the root matrix plus the same prefix covers, which rebuilds the
column list identically. A placed level's column list does not change
while deeper levels search: `cover(col)` unlinked its rows from every
other column, so no deeper cover reaches them. Counting from the column
head therefore agrees on both sides.

**Stop (divide-at-stop).** `divideRemainder` emits no "following rows" or
rest pieces for a cut level, because those rows already belong to a
donated task. Everything else is unchanged. An untouched task never
donated (a donation needs a placed level, and that level stays placed
until the task is exhausted), so `requeueWhole` stays exact; it is
asserted.

## 4. SIMD workers (`SimdExactCover`)

The recursion keeps a small frame registry in the search context, but
only when a donor is attached: per depth, the branch column, the
candidate count, the first untried index and the loop end. The branch
loop reads its end from the registry after every child returns. A
donation from a deeper node finds the shallowest frame with untried
candidates, pushes a **SIMD rest** for it (path to that frame, restSkip =
first untried index, never 0) and lowers the frame's end to that index.
Rests already exist for divide-at-stop. They re-derive the frame's
active-row list identically from the replayed prefix, so the index names
the same branch. The stop path (`recordFrame`) only records branches
below the loop end, so donated branches are not recorded twice.

A rest's root frame owns only the candidates from its restSkip on, and
its share stands for those, so the registry counts only those there.
Counting all of them would credit every donated piece too small a share.

Finding the open frame scans the candidate masks of each frame's untried
range. A frame's untried range only shrinks while it lives, so a frame
found closed stays closed until it is re-registered. The context keeps
the depth above which all live frames are known closed and starts the
scan there. Without it, a long tail with hungry siblings and nothing left
to give would rescan every live frame at every poll.

Donated rests are searched, never split at pop. When they become large
they donate in turn, so they need no special case.

## 5. Progress

Both engines publish per-task shares; `completedShare` sums the shares
of finished tasks.

- The piece's share is a structural estimate: the weight of the path
  above the level (the product of 1/branching) times the remaining
  branches divided by the branching at the level. That is the nested
  estimate `localFraction()` walks. The donor's `task.share` (what
  `finishTask()` folds in) drops by the same amount, clamped to what it
  still holds.
- The donor's published slot keeps the task's original share. Slot share
  times `localFraction()` is the absolute share explored so far, and a
  donation does not change that. So the bar does not dip while the piece
  is queued. And because the explored part plus the donated estimates
  never exceed the level weight, the donor's in-flight term stays below
  its reduced share, and the handoff at finish never over-reports. A
  placed level's `colCount` is stable while it is placed, so the
  estimate and later walks agree.
- `totalTasks` grows by one per donation and the piece completes like any
  task, so `drainedPoolComplete` stays consistent.

## 6. Measurements

`bench/bench_solve.py --ab` (`BURRTOOLS_NO_DONATE=1` wrapper vs default),
release build, `--no-disassemble`, 3 runs, medians, 4-core / 8-thread
laptop:

| puzzle | no donate | donate | ratio |
| :-- | --: | --: | --: |
| LominoSquare 9-15:1 | 0.24 s | 0.19 s | 1.32 |
| LominoSquare 9-15:2 | 0.23 s | 0.19 s | 1.23 |
| LominoSquare 9-15:0 | 0.06 s | 0.05 s | 1.32 |
| Third_Times_the_Charm | 0.66 s | 0.72 s | 0.92 |
| LominoSquare 9-15:3 | 3.79 s | 3.79 s | 1.00 |
| LominoSquare 9-15:4 | 14.54 s | 14.58 s | 1.00 |
| LominoSquare 9-15:5 | 72.17 s | 72.28 s | 1.00 |
| TheCube | 15.57 s | 15.67 s | 0.99 |
| JiminyJack | 13.00 s | 13.00 s | 1.00 |
| Tipperary | 23.52 s | 23.65 s | 0.99 |

The other puzzles in the corpus run under 0.3 s and stay within noise.
Results are identical everywhere, and peak RSS is unchanged.

Every run over 3 s is within ±1%: the poll has no measurable cost in the
hot loops. The short wide runs gain from better load balance; CPU use
rises from about 670% to 740%. The corpus has no puzzle whose tail is
dominated by one huge task, the case donation exists for. The point of
the change is that refinement is driven by actual demand instead of a
prefix-depth guess, with no cost where it does not help.

## 7. Tests

- "donated work is searched exactly once" (`test_solver.cpp`): line
  puzzle, DLX and SIMD, 1 to 4 threads, forced donation every 1 to 7
  nodes. Each run, both uninterrupted and stopped + restored, must equal
  the serial baseline as a multiset, and the finished tasks' shares
  (`completedShare`) must add up to 1. `getFinished()` cannot stand in for
  the share check: it reports 1 for any completed search. Mutation checks:
  dropping the DLX cut, recording SIMD stop branches past the loop end,
  or not taking a donated piece's share off the donor (DLX or SIMD) all
  fail it.
- The whole solver suite also passes with `BURRTOOLS_DONATE_EVERY` set to
  1, 5 and 300.
- "chunked splitting ..." pins the pop-time splitter, so it runs with
  `BURRTOOLS_NO_DONATE=1`.

## 8. assembler_1: progress units

assembler_1's progress counts tasks: completed tasks plus the in-flight
fraction of each running one, over the task total. A donation that added
a task would make the bar step back. So the counters (`totalTasks`,
`completedTasks`, and the Huang solver's pair) count **units**: a
generated or restored task holds `TaskUnits::kWhole` (2^20), a donation
moves units from the donor to the piece, and a stop divides a task's
units among its pieces. The total only grows at a stop that cuts a task
into more pieces than it has units.

**Invariant: every outstanding task holds at least one unit.** Completed
units reach the total only when no work is left, and `solvethread`
treats a full bar as a finished search. A 0-unit piece would let it
finish with that piece unsearched. An earlier draft that rounded small
shares down to 0 units did exactly that: the chained-stop Huang test lost
two assemblies on SolidSixPieceBurrs. A donor therefore gives units only
while it holds two or more and keeps at least one (`TaskUnits::give`).

Units are bookkeeping, not territory: they are not saved (a restored
task is a whole one), and `HuangSeed::operator==` ignores them.

## 9. Huang

Huang's recursion is the SIMD exact cover shape, so donation is too. A
per-depth frame registry (pivot column, candidate count, first untried
index, loop end) lets a deeper node cut the shallowest frame with untried
pivot candidates. The frame's remaining branches become a rest seed:
path, pivot bits and restSkip = the first untried index. `taskFromSeed`
replays the seed into a pool task.

- **Pivot bits.** Steps inside the task's prefix take the task's own bits;
  a step chosen by a live frame takes that frame's pivot kind. With a
  wrong bit the replay drops the monotonic shape filter and the rest
  re-enumerates permutations of identical pieces. The unit test
  "donated rests replay the monotonic shape filter" (one shape needed six
  times, C(10, 6) = 210) reports 350 with the bits zeroed.
- **Existence check.** It uses `coversPivot` only. `fitsBounds` would see
  the deeper placements' weights, and with identical pieces those fill
  the very shape the frame branches on, so it would hide every sibling.
  A candidate that does not fit at the frame is skipped when the rest
  replays the frame's own state, so overcounting costs at most an empty
  rest.
- **Units.** The piece's units are the structural estimate (product of
  1/branching above the frame, times remaining/branching at it) of the
  donor's units at pop, clamped by `TaskUnits::give`. Huang publishes no
  in-flight fraction, so nothing else depends on the estimate.

## 10. A1-DLX

The DLX walk keeps five explicit stacks. A frame in state 5 has placed a
row and descended; when its child returns, case 5 undoes the row's hides
and case 6 moves on to the next row.

- **The piece.** The donated task is the stacks truncated just after that
  frame, top 5:
  - task_stack and next_row_stack up to the frame;
  - column_stack holds one entry per state-1 frame;
  - rows holds one entry per state-5 frame;
  - hidden_rows keeps everything before the next frame's first segment
    marker, which is two 0-led segments per state-5 frame (asserted).

  `restoreMatrix()` replays a top 5 whole, so the receiver runs cases 5
  and 6 itself on the frame's own matrix. baseDepth = frame + 1 ends the
  task when the frame pops.
- **The cut.** The donor sets `cut[frame]`, and its case 6 then ends the
  frame after the current row. The cut is part of the territory, so
  `SubtreeTask_1::cuts` is written back and saved, as part of format 2.2
  (unreleased, so no version bump; only the saving version resumes).
- **Progress.** The tracker follows every frame of the territory. For a
  written-back task, `seedTracker()` gives the restored ancestor frames
  their levels, with their current child as the active slot. The piece
  receives floor(tracker share x units at pop) of the frame's
  unattributed share, and the tracker gives that share away. The donor's
  published slot (done x units at pop) therefore never exceeds the units
  it still holds, so the handoff at finish never over-reports. A share
  too small for one unit is not donated.

## 11. Tests (assembler_1)

- "assembler_1 donated work is searched exactly once": Huang and DLX
  (forced), CubeInCage (identical pieces), PiecesOfEight and
  SolidSixPieceBurrs (range), 1 to 4 threads, forced donation every 1 to
  61 nodes. Uninterrupted runs and stopped + restored runs must equal the
  baseline as a multiset and finish at progress 1, and donations must
  occur. Mutation checks:
  - Huang: dropping the cut (runaway duplication), or recording stop
    branches past the loop end, fails it.
  - A1-DLX: dropping the cut, not writing the cuts back, or keeping
    deeper frames' hidden-row segments in the piece fails it.
- The solver suite passes with forced donation (every 1/7/200 on the
  default engines, every 1/9/300 with DLX forced). The failures it shows
  are exactly those without donation, i.e. env-forced mismatches.
## 12. Limits

- A1-DLX donates only state-5 frames: the rows after a placed row. A
  state-2 frame (the empty-row-set child in progress, rows still ahead)
  is not donated; its rows become donatable once the frame reaches them.
- No engine donates within two levels of the bottom (A0: two pieces
  before the last; Huang: two placements before the minimum piece
  count, or the depth bound when no shape has a minimum).
