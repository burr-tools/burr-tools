# Bounded resume state for parallel searches

Status: proposal (2026-10-14)
Touches: `assembler_0.{h,cpp}`, `assembler_1.{h,cpp}`, `simd_huang_cover.{h,cpp}`,
`simd_exact_cover` (solveSubtree callback only), `test/test_solver.cpp`.
Spec: protocol unaffected (task requeue/drain/finish pairing does not change).

## 1. Problem

Since #90 (shipped in 0.8.0, formats `assembler_0` 1.6 / `assembler_1` 2.1) a
stopped parallel search saves its task remainder **plus the FNV signature of
every assembly it has reported** (`emittedSignatures`). On continue, an
interrupted task restarts from scratch and the set suppresses the repeats.

That set grows linearly with the number of solutions found, forever:

| | per reported assembly |
| :-- | :-- |
| XML (decimal `uint64` + space) | ~20 bytes |
| RAM (`std::unordered_set<uint64_t>` node + bucket) | ~40 bytes |

A puzzle with 10^8 assemblies writes ~2 GB of digits into the `.xmpuzzle`
and holds ~4 GB of RAM during the solve. `assembler_0_c::handleSolution`
inserts into the set on **every** run, serial and fresh ones included, so the
RAM cost is paid even when resume is never used.

## 2. Key observation

The set is far larger than what dedup actually needs. After a stop, a
reported assembly can only be reported again if the task that found it is
re-run:

- **Completed** tasks are dropped from the pool and never re-run.
- **Queued** tasks never started, so they reported nothing.
- **Interrupted** (in-flight) tasks are requeued *whole* and re-searched.
- Splitting (`assembler_0` work stealing) happens only at `pop_task` time,
  before `beginTask`; a task that has started is never split afterwards, so
  the requeued task is exactly the subtree that ran.

Within one task the search is **deterministic**: the subtree is rebuilt from
the same matrix (node ids are already relied upon for prefixes) by the same
engine, single-threaded, with the same column heuristic. So a re-run finds the
same raw solutions in the same order, and "which assemblies did this task
already report" is fully described by **one integer: how many raw solutions it
had consumed**.

## 3. Design

### 3.1 Per-task replay counter

Every task type gains:

```cpp
uint64_t replaySkip = 0;    // raw solutions an earlier run of this task consumed
uint64_t replayAnchor = 0;  // FNV-1a of the raw node vector of solution #replaySkip
```

Task types: `assembler_0_c::SubtreeTask`, `assembler_1_c::SubtreeTask_1`, and
the Huang prefixes (`std::vector<std::vector<unsigned>>` becomes a small
`HuangSeed { prefix, replaySkip, replayAnchor }`).

The worker keeps a task-local `consumed` counter (no sharing, no atomics):

1. Each raw solution from the engine increments `seen`.
2. If `seen <= replaySkip`: drop it without building the assembly. At
   `seen == replaySkip` compare the raw-node hash with `replayAnchor`
   (`bt_assert` in debug; see §5).
3. Otherwise process normally (symmetry filter, callback). The solution
   counts as **consumed** once it was either filtered out or handed to the
   callback. The existing "stop requested, return before emitting" early-out
   under `callbackMutex` does **not** consume -- that solution is re-found and
   emitted after resume, exactly as today.
4. On interruption the requeued copy gets `replaySkip = consumed`,
   `replayAnchor = hash(last consumed)`.

Counting raw solutions (before the symmetry filter) keeps the skip path
trivially cheap: skipped solutions never build an `assembly_c`.

`handleSolution` / the Huang lambda return a `bool consumed` instead of the
signature bookkeeping; `emittedSignatures`, `callbackMutex`-guarded inserts
and `assemblySignature()` go away (the latter survives only for the legacy
path in §3.5).

### 3.2 Splitting

`shouldSplit(task)` returns false for `replaySkip > 0`. The count describes
the whole subtree's order; children would each need their own share of it.
At most one such task exists per worker per stop, so the lost split
opportunity is negligible.

Fan-out per split is capped at `kSplitFanout` (16): a column fatter than
that yields singletons plus one cursor task (`cursorCol/cursorSkip`) for
the rest instead of one task per row. Without the cap, a depth<3 split on
a breadth-B column enumerates ~B^2 queued tasks, each persisting its full
flat prefix -- RAM, save size and re-parse time all linear in the queue.
Cursors are never searched (pop re-splits or, defensively, drops them
counted with zero share) and persist their position in the 1.7 payload,
so resume continues splitting where it left off. Columns at or below the
cap enumerate exactly as before.

### 3.3 Engine pinning

A skip count is only valid for the engine that produced it (DLX and the SIMD
engines branch in different orders). Today an engine mismatch on continue is
handled by "regenerate everything, dedup against the full set" -- which no
longer exists. Instead the remainder pins its engine:

- `assembler_1`: `parallelTasks` non-empty ⇒ DLX task path; `pendingHuangPrefixes`
  non-empty ⇒ Huang path. Both already exclusive.
- `assembler_0`: new `remainderEngine` (`DLX`/`SIMD`) recorded on drain and saved.
- On continue the pinned engine runs regardless of the benchmark toggles
  (`BURRTOOLS_NO_SIMD`, `BURRTOOLS_HUANG_MEM_MB`) and of thread count. The
  structural eligibility checks (column caps, variable-voxel rule) depend only
  on the matrix, which is identical on reload, so they cannot flip.
- `willRunParallel()` returns true whenever a remainder exists, so a
  single-threaded DLX continue runs the task pool with one worker instead of
  today's "serial re-search from the root with full dedup". This removes
  `resumeDedup` and the serial-path dedup entirely.

### 3.4 What gets saved

Per remaining task: its prefix/snapshot (as now) plus two integers. Size is
O(tasks × prefix length), typically max(16, 4·threads) tasks of a few dozen numbers
-- independent of the number of solutions. RAM likewise.

Format bumps: `assembler_0` 1.6 → **1.7**, `assembler_1` 2.1 → **2.2**.
The signature list is no longer written.

### 3.5 Loading 0.8.0 files (1.6 / 2.1)

Per user decision, interrupted legacy searches (format 1.6 with `interrupted != 0`
or `taskCount > 0`, format 2.1 with `interrupted != 0` or `ptype != 0`) are
refused with `ERR_CAN_NOT_RESTORE_INTERRUPTED`, prompting the user to restart
the search. Uninterrupted 0.8.0 saves (`interrupted == 0`, no tasks) continue
to load and restore normally. This avoids keeping legacy signature dedup sets,
completely eliminating `emittedSignatures` from the codebase.

## 4. Why not the alternatives

| Alternative | Verdict |
| :-- | :-- |
| Keep the set, write it compressed/binary or to a sidecar file | Constant-factor win only; still O(solutions) in disk and RAM, plus a second file to keep in sync with the `.xmpuzzle`. |
| Bloom filter / probabilistic set | False positives silently **drop** assemblies -- unacceptable for a solver. |
| Store only signatures of interrupted tasks | Correct and smaller, but still O(solutions of the biggest task); Huang tasks are never split, so one skewed task can hold most solutions. |
| Save exact mid-task DFS position (like serial save) | Zero replay work, but needs per-engine stack serialisation for DLX, Huang SIMD and `SimdExactCover`. Much larger change; the skip counter gets the same correctness with replay cost bounded by one task's partial subtree. |

The cost of the chosen design: on continue, each previously interrupted task
re-walks the part of its subtree it had already searched (as today), only
without building assemblies for the skipped solutions.

## 5. Risks

- **Determinism is load-bearing.** Any future change that makes in-task
  order depend on threads/time (e.g. in-task stealing for Huang) must either
  disable it for `replaySkip > 0` tasks or bump the format. The anchor hash
  catches divergence in debug builds and tests; documented next to
  `replaySkip`.
- **Engine version drift.** A changed column heuristic changes order. The
  format version already gates restores; heuristic changes to a task engine
  must bump `ASSEMBLER_VERSION`. Noted in both `ASSEMBLER_VERSION` comments.

## 6. Tests

Extend the existing `[resume]` cases in `test/test_solver.cpp`:

1. Stop after N assemblies / after a delay, save, reload, continue, repeated
   chains; compare assembly **multisets** with an uninterrupted run, for
   `assembler_0` (SIMD + DLX), `assembler_1` (Huang + DLX), 1 and N threads,
   including `Bermuda.xmpuzzle` (symmetry breaker -- filtered solutions must
   count as consumed).
2. Payload bound: after a run that reported many assemblies, the saved
   `<assembler>` content length is independent of the assembly count.
3. Engine pinning: save a SIMD remainder, continue with `BURRTOOLS_NO_SIMD=1`
   (and vice versa) -- multiset still exact.
4. Legacy refusal: 1.6 and 2.1 interrupted saves are refused with
   `ERR_CAN_NOT_RESTORE_INTERRUPTED`, while uninterrupted saves restore cleanly.
5. `#ifndef NDEBUG`: a tampered `replayAnchor` trips the assertion.
6. `[parallel]` TSan job unchanged in shape; per-task counters are
   worker-local, so no new shared state.

`just test-regression` must stay identical (fresh solves are unaffected apart
from no longer populating a set).
