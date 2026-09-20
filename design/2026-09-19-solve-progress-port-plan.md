# Solve progress reporting — port plan

Date: 2026-09-19
Spec: `design/2026-09-19-solve-progress-reporting.md` (unchanged, still binding)
Supersedes: `design/2026-09-19-solve-progress-implementation-plan.md`

## Why this exists

The original eight-task plan was implemented and reviewed in full on a base that
has since been force-rebased. Upstream restructured the parallel search's task
representation, so the work needs porting rather than replaying. The spec's
goals, the measurements behind them, and the test strategy are unchanged.

Reference implementation: tag `backup/pre-rebase-solve-progress` on branch
`fix/solve-progress-reporting` (22 commits). Every design decision there was
reviewed; consult it, but do not assume its structure still fits.

## What upstream changed, and what it means

`parallelMultiSearch` no longer owns a local `tasks` vector. The task list is now
member state:

- `std::vector<SubtreeTask> parallelTasks`
- `std::vector<uint8_t> taskCompleted` — marked `1` at `assembler_0.cpp:1799`
  and `:1837` as each task finishes
- tasks are generated only `if (parallelTasks.empty())` and a resumed run
  rebuilds `remainingIndices` from `!taskCompleted[i]`
- both vectors are cleared only in `createMatrix` and on completion

Upstream did this to make the parallel search resumable — the defect reported
against PR #79. It is a better foundation than the original port target, and it
removes work rather than adding it.

### Design consequences

1. **`completedShare` no longer needs an accumulator with reset discipline.**
   Seed it once at the top of `parallelMultiSearch`, single-threaded, by summing
   `share` over `taskCompleted[i]`, then accumulate atomically as tasks finish.
   This is race-free (the seeding happens before workers start) and inherently
   correct across a resume.

2. **`assemblyBaseFraction` and `projectAssemblyCost` are dropped entirely.**
   They existed only to compensate for an assembler fraction that restarted at
   the wrong baseline on resume. With (1), the assembler's own fraction is
   correct across a resume, so `solveThread_c` needs no correction. This also
   removes `prepareRunProgress()`, the public base-class virtual added to make
   the compensation ordering work.

3. **Do NOT read `taskCompleted` from the GUI thread.** It is a plain
   `std::vector<uint8_t>` written by workers without synchronisation. Progress
   must be read from the atomic share accumulator, never by walking that vector.

## Unchanged from the original work

These were measured or reviewed and carry over as-is:

- `progressModel_c` — pure, no dependency on any of the restructured code.
- `searchComplete`, checked at the **top** of `getFinished()` before any other
  branch. Placing it inside the `total > 0` branch regresses single-threaded
  SIMD runs to `ACT_PAUSING` on a finished puzzle.
- Publication interval `0xFFF` (4K nodes), justified by measurement: 64K
  publishes only every ~360 ms per worker, slower than the 250 ms GUI refresh.
- The per-worker high-water ratchet in `assembler_0` — the nested-path estimate
  is not monotone because covering mutates the denominators.
- Reader-side mutex around the worker-slot vector; workers publish lock-free.
- Handoff ordering: clear a worker's share before adding to the accumulator, and
  read the accumulator first, so the transient is low rather than high.
- Release/acquire on the accumulator the reader consumes (on `completedShare`
  for `assembler_0`, since that is what its `getFinished()` reads).
- The Huang engine uses **equal task weights** plus in-flight publication.
  Structural share weighting was built, measured, and rejected there: it froze
  the bar at 0.9694 because task generation runs the real search to a cutoff and
  hands instantly-dead subtrees 96.9% of the weight. This is a user decision.
- Pruned-subtree shares must be folded in as completed, or shares sum below 1
  and the bar tops out short.
- Pool instrumentation must cover the **inline** path as well as `worker_loop`,
  or `completedCount()` lies whenever the pool collapses to inline mode.
- The GUI read needs the problem-match guard every sibling block applies, or
  switching the Solve tab mid-solve paints one problem's progress onto another.

## Tasks

| # | Scope | Reference commits |
| :-- | :-- | :-- |
| P1 | `progressmodel.{h,cpp}` + `test_progressmodel.cpp` + meson wiring | `583486cd`, `44ee50de`, parts of `32b949f4` |
| P2 | `searchComplete` + counter resets, both engines | `0facbe16`, `658ed569` |
| P3 | Share weighting on `parallelTasks`/`taskCompleted` + in-flight publication, `assembler_0` | `89e73173`, `bf43f90b`, `82a23369`, `c23d117c`, `f6cb10aa` |
| P4 | In-flight publication, `assembler_1` (equal weights) | `2b68f78c`, `afb270fb`, `dee1bf15` |
| P5 | Pool counts and cost, both paths | `ca6495e6`, `6a5bbde0` |
| P6 | Blend in `solveThread_c` (no base-fraction compensation) + GUI swap | `819d0931`, `39f7d7e8`, `30503000`, `7d167dc4` |

## Known limitations carried forward

- The SIMD path inside `parallelMultiSearch` reports whole-task granularity;
  the SIMD solver interface has no progress hook. Out of scope.
- Assembly and disassembly costs accrue at ~2N worker-seconds per wall-second
  while the phases overlap and ~N in the tail, biasing the time estimate across
  the transition. Documented, not modelled.
- `assembler_0`'s `Π(1/colCount)` share does **not** correlate with actual work
  — no longer an open question. Measured: the subtrees task generation proves
  empty at depth `<= 3` are credited their full structural share before any
  worker starts, which is **0.5 on `DiagonalCube`**, 0.125 on `BallRoom`, and 0
  on every other bundled example including `Burr-Glar`. So a fresh parallel
  solve of `DiagonalCube` opens at `getFinished() == 0.5` and the GUI's
  `ut/finished - ut` under-reports by ~2× early on. The good `Burr-Glar` curve
  is the in-flight term, not the weighting — `Burr-Glar` prunes nothing at this
  depth. This is the same anti-correlation that got structural weighting
  rejected for the Huang engine; it is tolerated here (far better than the 106
  discrete steps it replaced, and share-conserving). Recorded in
  `2026-09-19-solve-progress-reporting.md` §2a and at `assembler_0.h`'s
  `prunedTaskShare`.

## Not ours, still open upstream

`smallerRotationExists` is still called outside `callbackMutex`, so the
`voxel_c::getHotspot` race persists — confirmed present on this base. It causes a
~1-in-30 flake in `test_assembler_config.cpp:562`. Also unfixed: a
`bt_assert(cache)` crash at `movementanalysator.cpp:596` escaping a pool worker.
