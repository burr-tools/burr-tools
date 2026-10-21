# Split on demand (assembler_0)

Status: implemented for assembler_0 (DLX and SIMD workers). Huang and
A1-DLX follow separately.

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
- A donation is an ordinary `push_tasks`. Nothing in the pool protocol
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
  the serial baseline as a multiset and finish at progress 1. Mutation
  checks: dropping the DLX cut, or recording SIMD stop branches past the
  loop end, both fail it.
- The whole solver suite also passes with `BURRTOOLS_DONATE_EVERY` set to
  1, 5 and 300.
- "chunked splitting ..." pins the pop-time splitter, so it runs with
  `BURRTOOLS_NO_DONATE=1`.

## 8. Not covered here

- **Huang** (A1 default) has no runtime splitting at all. Its progress
  counts tasks as equal units, so a donation would need fractional task
  units first.
- **A1-DLX** keeps five explicit stacks. A donation there means handing
  over the remaining rows of a state-3/4 frame, which needs its own
  write-back-style task.
