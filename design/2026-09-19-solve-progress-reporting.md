# Solve progress reporting

Date: 2026-09-19
Status: design approved, not yet implemented

## Problem

A user reported that the solve progress indicator in the GUI is no longer
functional on the parallel solver stack (#78 → #79 → #81 → #82 → #83 → #84).

The indicator does still move, so "frozen" is not quite the symptom. What broke
is that it moves in coarse steps with long stalls, and the estimated-time-
remaining field derived from it is badly and persistently wrong.

### Measured evidence

`examples/Burr-Glar.xmpuzzle`, `assembler_0_c`, 4 threads, sampling
`getFinished()` every 250 ms. Total runtime 315 s, 3,126,313 assemblies.

Values are quantised to ~1/106 (the run generated 106 subtree tasks), and the
curve stalls repeatedly:

| plateau value | held for |
| :--- | :--- |
| 0.9906 | **32.0 s** |
| 0.7075 | 25.5 s |
| 0.6792 | 20.5 s |
| 0.2358 | 17.5 s |

Roughly the final 10% of wall-clock is spent pinned at `99.0566%`, waiting on a
single task out of 106. This is the tail-imbalance signature already noted in
the #81 review (704% CPU for a 2.54× speedup).

The time estimate follows directly. At 285 s elapsed with `finished = 0.9906`,
the GUI computes:

```
TimeEst = ut/finished - ut = 285/0.9906 - 285 ≈ 2.7 s remaining
actual                                        = 30.3 s remaining
```

It predicts ~2.7 s, and then keeps predicting ~2.7 s for the next thirty
seconds. An 11× underestimate that never corrects.

## Root cause

`getFinished()` changed meaning, and its consumers did not.

The original implementation (`assembler_0.cpp:1925-1937`) walks the current DFS
path and accumulates a nested fraction — a smooth, fine-grained estimate of the
proportion of the search tree explored. The parallel path replaced it with a
task count (`assembler_0.cpp:1908`):

```cpp
return static_cast<float>(completedTasks.load(...)) / static_cast<float>(total);
```

Subtree tasks are wildly uneven in size, so task count is not proportional to
work, and a task contributes nothing until it completes.

Three consumers assume the original contract:

- `mainwindow.cpp:2622` — `SolvingProgress->value(100*finished)`
- `mainwindow.cpp:2627` — `snprintf(tmp, 100, "%.4f%%", 100*finished)`, four
  decimal places, which only makes sense for a smooth fraction
- `mainwindow.cpp:2927-2928` — `TimeEst->value(timeToString(ut/finished-ut))`,
  which requires `finished` to be proportional to work done

`solvethread.cpp:108` additionally uses `getFinished() >= 1` to decide between
`ACT_FINISHED` and `ACT_PAUSING`.

### Related defects to fix at the same time

These make the new behaviour unverifiable if left in place, so they are in
scope rather than deferred:

- `assembler_1_c` never resets `totalTasks`/`completedTasks`. `assembler_0_c`
  zeroes them in `createMatrix` (`assembler_0.cpp:755-756`); `assembler_1_c`
  only ever sets them during parallel task generation
  (`assembler_1.cpp:2592-2593`), so a later serial run reports a stale fraction.
- Both engines shortcut `!running && !abbort → 1.0f`
  (`assembler_0.cpp:1906`, `assembler_1.cpp:2786`, and again at
  `assembler_1.cpp:2791` keyed on `iterations > 0`). "Not running" is not the
  same as "finished": a restored-but-unstarted assembler reports 100%.

### Explicitly checked and *not* a defect

The single-threaded SIMD path was suspected of reporting 0 forever and never
reaching `ACT_FINISHED`. Measured: it reaches 1.0 correctly. No change needed.

## Goals

1. The progress bar reflects whole-solve progress, covering assembly and the
   concurrent disassembly pool.
2. The estimated time remaining is meaningful — this is the field users
   actually rely on, and it is the one that broke worst.
3. The behaviour is verifiable without driving the GUI, so regressions are
   caught by CI rather than by user reports.

## Non-goals

- Changing the decomposition or scheduling of the parallel search. Tail
  imbalance is a real performance issue but is out of scope here; this work
  makes it *visible* rather than fixing it.
- Adding work-stealing.
- Changing the GUI layout. The existing bar, percentage and time fields stay as
  they are; only the value feeding them changes.

## Design

### 1. Progress ownership

The assembler cannot see the disassembly pool, so whole-solve progress cannot
live there. The contract splits:

- **`assembler_c::getFinished()`** retains its original meaning — fraction of
  the assembly search tree explored — restored to being smooth. Still needed on
  its own for saved or paused puzzles with no live thread.
- **`solveThread_c::getProgress()`** (new) returns the blended whole-solve
  fraction. `solveThread_c` is the only component that sees both phases.

The GUI uses the thread's value when a thread exists and falls back to the
assembler's otherwise. Both are fractions in `[0,1]` with the same meaning to a
reader, so the two sources are not observably inconsistent.

### 2. A smooth assembly fraction under parallelism

Task generation runs single-threaded, so `colCount` is stable at that moment.
Each task records:

```
share = Π (1 / colCount[column]) over the task's prefix levels
```

Shares sum to 1 by construction. Progress is then:

```
getFinished() = completedShare                       // atomic accumulator
              + Σ over in-flight workers:
                    share(task_w) × localFraction_w
```

`localFraction_w` is the existing nested-path walk restricted to levels *below*
the prefix depth, computed by each worker over its own private matrix and
published to a per-worker atomic slot. Workers publish every N nodes rather
than every node, because the walk is O(depth × colCount).

The GUI never reads another thread's matrix — it only reads the published
atomics. This is what removes the plateau: a long-running task accrues progress
inside its own subtree instead of contributing nothing until it completes.

`assembler_1_c` uses the same structure, with each worker maintaining its own
`finished_a`/`finished_b` pair. That is the mechanism the serial path already
uses and the parallel path currently leaves empty.

### 3. The whole-solve blend

Measuring cost in seconds makes the algebra collapse:

```
progress = totalCostSoFar / projectedTotalCost
         = (asmCost + disCost) / (asmCost/a + disCost/d)

where  a  = assembly tree fraction (§2)
       d  = disassembled / N̂
       N̂  = assembliesFound / a        (projected total assemblies)
```

Because both weights are measured in seconds, progress is time-proportional by
construction — exactly the property `ut/finished - ut` requires. Disassembly
cost is measured precisely (pool workers sum their own task durations);
assembly cost is accumulated worker-seconds.

### 4. Degenerate cases and monotonicity

The blend falls back to `a` alone when:

- `a` is below a trust threshold (`N̂` is unstable while `a` is tiny),
- there is no disassembly pool, or
- nothing has been disassembled yet.

So the bar is never worse than assembly-only, and improves once `N̂` stabilises.

Projections revise as evidence accumulates, which can move the raw value
backwards. `solveThread_c` applies a monotone guard: the reported value never
decreases, and is capped strictly below 1.0 until the search actually
completes. Reaching exactly 1.0 remains the signal `solvethread.cpp:108` uses
to choose `ACT_FINISHED`, so that check keeps working unchanged.

### 5. Components

| Component | Responsibility | Depends on |
| :--- | :--- | :--- |
| `progressModel_c` (new) | Pure blend arithmetic and remaining-time projection | nothing |
| `assembler_0_c` / `assembler_1_c` | Per-task `share`, per-worker published local fraction, `getFinished()` | — |
| `disassemblerPool_c` | Submitted / completed counts, accumulated task cost | — |
| `solveThread_c` | Owns `progressModel_c`, feeds it both phases, applies monotone guard, exposes `getProgress()` | the three above |
| `mainwindow.cpp` | Reads thread value when live, assembler value otherwise | `solveThread_c` |

`progressModel_c` is deliberately free of threading, puzzles and GUI so the
interesting arithmetic is directly testable. Its interface:

```
progress(a, assembliesFound, disassembled, asmCostSeconds, disCostSeconds)
    -> { fraction, projectedRemainingSeconds }
```

### 6. Testing

The point of this section is that none of it requires the GUI.

**Unit tests on `progressModel_c`** — fast, deterministic, no puzzle:

- monotonicity across a synthesised input sequence
- bounds `[0,1]`; never exactly 1.0 until complete
- each degenerate fallback (`a` ≈ 0, no disassembly, unstable `N̂`)
- time-estimate accuracy: projected remaining within tolerance of actual, at
  50% and 90% through a synthesised solve

**Integration tests** over a real solve, sampling `getFinished()` from a second
thread:

- non-decreasing, and ends at exactly 1.0, for serial and parallel
- plateau bound: the longest constant run stays under a set fraction of total
  runtime — this encodes the reported regression directly and fails today
- granularity: the count of distinct observed values exceeds a floor — fails
  today at ~106
- re-running on the same assembler restarts near 0 (the `assembler_1` reset)
- a restored-but-unstarted assembler reports < 1.0

**Puzzle sizing.** Timing every bundled example showed there is no mid-range
puzzle: `Burr-Glar.xmpuzzle` runs 315 s and the next longest,
`HexSticks.xmpuzzle`, is ~750 ms. Nothing sits in the 2-10 s band.

Rather than add a five-minute CI job or synthesise a puzzle, the plateau and
granularity assertions are expressed **scale-invariantly**, so a short run is
sufficient:

- plateau bound as a *fraction of total runtime*, not an absolute duration
- granularity as distinct values observed *relative to sample count*

Both are properties of the curve's shape, not its length, and both still fail
on today's code. The sampler must then run at a fine interval (single-digit
milliseconds) rather than the 250 ms used for the diagnostic above, or a
sub-second run yields too few samples to characterise.

Burr-Glar remains useful as a manual check and is referenced in the test
comment, but no CI job depends on it.

The new cross-thread publication in §2 is covered by the `tsan-parallel` CI job
added in the parent commit.

## Risks

- **`N̂` is only as good as `a`.** If the assembly fraction is itself skewed,
  the projected assembly count and therefore the phase weighting inherit the
  skew. Mitigated by the trust threshold and by falling back to assembly-only.
- **Publication frequency is a tuning parameter.** Too frequent costs search
  throughput; too sparse reintroduces visible stepping. Needs measurement
  against the benchmark corpus rather than a guessed constant.
- **Monotone guard can mask a genuine regression.** If the raw value wants to
  fall sharply, clamping hides it. Accepted: a bar that goes backwards reads as
  a bug to users, and the unit tests assert the raw model's behaviour directly,
  unclamped.

## Constants to determine during implementation

Three values are deliberately left unspecified here because they need
measurement against the benchmark corpus rather than a guess:

- the worker publication interval in §2 (throughput cost vs. visible stepping),
- the `a` trust threshold in §4 below which the blend falls back to
  assembly-only,
- the plateau and granularity thresholds in §6.

Each should be justified by a measurement in the implementing PR, not chosen by
taste.
