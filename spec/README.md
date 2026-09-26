# spec/ — Protocol models for BurrTools concurrency

PlusCal/TLA+ models of the coordination protocols in `src/lib/`, checked
with the TLC model checker. They verify **protocol shape** (token
accounting, pairing, quiescence, ordering) over all thread interleavings of
small instances — the defect class behind the lost-wakeup / oversubscription
/ salvage bugs. They do **not** verify the C++ code itself; TSan plus the
`[parallel]` tests remain ground truth for the implementation.

Tooling is pinned so results reproduce: tla2tools **v1.7.4** (TLC 2.19),
fetched by `just spec-tools` into `build-tla/` (gitignored).

## Files

| File | Contents |
| :--- | :--- |
| `AssemblyPool.tla` | PlusCal source (`--algorithm`, human-edited) **plus** the generated TLA+ translation. Commit both; the translator preserves everything outside its markers. |
| `AssemblyPool.cfg` | TLC config: 2 workers, budget 2 (= uncapped), 3 seed tasks, ≤2 splits. |
| `AssemblyPoolLowBudget.cfg` | Same, but budget 1: forces the `acquire`-park + re-check path on (almost) every task. Run with `-config`. |
| `DisasmPool.tla` | Disassembler-pool protocol: bounded queue, worker pickup, bounded reorder buffer, ordered merger, finish/abort/requestStop lifecycle with salvage + dropped-skip. |
| `DisasmPool.cfg` | TLC config: 2 workers, 4 assemblies, queue 2, reorder window 2. |
| `DisasmPoolTight.cfg` | Same with queue 1 / window 1: forces every backpressure path nearly every step. |

## Run

```bash
just spec-check   # translate + check both configs (fast: <2s, a few hundred states)
```

Manually:

```bash
java -cp build-tla/tla2tools.jar pcal.trans spec/AssemblyPool.tla
java -cp build-tla/tla2tools.jar tlc2.TLC -workers 4 -config spec/AssemblyPool.cfg spec/AssemblyPool
java -cp build-tla/tla2tools.jar tlc2.TLC -workers 4 -config spec/AssemblyPoolLowBudget.cfg spec/AssemblyPool
```

Larger instance (ad-hoc confidence, not committed):

```bash
# N=3, BudgetTotal=2, NumTasks=4, MaxPush=3 in a scratch .cfg → ~8k states, ~1s
```

## C++ ↔ spec mapping (`AssemblyPool.tla`)

| Spec element | C++ counterpart |
| :--- | :--- |
| `available`, `holdsToken` | `ThreadBudget::available_`, thread-local `heldBudget` (`thread_budget.h`) |
| `PoolWait` await | `AssemblyTaskPool::pop_task` `cv.wait` predicate (`assembler_pool.h`) |
| `Park` await | `ThreadBudget::acquire` park, pool lock released first |
| `RecheckRel` | Post-park re-check: return token, wait token-free if queue drained |
| `Pop` / `Fin` | `pop_task` success + `finishTask` (`releaseBudget` then `task_done`) |
| `ShedTok` | Model-only shed of a reservation left dangling by a label-split interleave that C++ excludes (reserve is atomic with the nonempty check). Keeps the token-free-wait rule. |
| `splitter` process | `push_tasks` dynamic splits, as environment nondeterminism (splits are optional: the owner searches the whole subtree) |
| `stopper` process | GUI/orchestrator `requestStop()`; unfair, so TLC covers stop and never-stop |
| `BudgetConservation` | **SPEC-BUDGET-1**: takes/returns pair up |
| `ActiveBounded` | N-active-thread bound: every in-flight task holds a token |
| `Pairing` | **SPEC-POOL-1**: each `pop_task` pairs with exactly one `finishTask` |
| `TaskConservation` | **SPEC-POOL-2**: no task lost/duplicated across splits |
| `CleanExit` | **SPEC-POOL-3**: exits leak no tasks/tokens |
| `AllTerminate` | Liveness: drain-to-quiescence or stop always ends the search |

## C++ ↔ spec mapping (`DisasmPool.tla`)

| Spec element | C++ counterpart (`disassemblerpool.cpp`) |
| :--- | :--- |
| `submitter` + `SSpace` await | `submit()`: terminal check, then `hasSpace()` wait (queue + reorder window), seqNo on enqueue, salvage + `false` when terminal |
| `worker` + `WWait` await | `worker_loop()`: pop under `queue_mutex`; predicate deliberately omits `stop_requested` (requestStop re-wake is harmless) |
| `WFile` await | `cv_reorder` wait. TLC proves it never blocks: `WindowBounded` guarantees a free slot whenever a job is in flight |
| `merger` + `MWait` await | `merger_loop()`: next-seq filed, dropped-skip, or finished-and-drained |
| `stopper` + `SalvLoop` | `requestStop()`: salvage queued assemblies, publish `dropped_` |
| `aborter` | `abort()`: discard queue, reorder buffer, drops; workers/merger exit on the flag (also covers the worker-exception path's observable protocol) |
| `finisher` | `finish()`: set `finished` once every offer resolved; workers join, merger drains |
| `OrderedDelivery` | **SPEC-DIS-1**: monotonic `seqNo` merge |
| `QueueBounded` / `ReorderBounded` | **SPEC-DIS-2** / **SPEC-DIS-3** |
| `WindowBounded` | **SPEC-DIS-4**: submit window bounds outstanding work |
| `OfferConservation` / `SeqConservation` | **SPEC-DIS-5**: enqueued ∨ salvaged ∨ delivered ∨ discarded, never lost |
| `SalvageOrdered` | **SPEC-DIS-6**: salvage keeps submit order for re-submission |
| `CleanShutdown` | **SPEC-DIS-7**: no stranded jobs/results |
| `AllTerminate` | Liveness over all four stop/abort combinations |

## Verification input from code review (PRs 113–118)

Review findings that shape what the specs must cover:

- **Stop during generation must discard partial state** (PR #118, `assembler_1.cpp:2727`): a stop inside `generateTasksAtDepth` left a partial `parallelTasks` list with `interrupted=0`, silently losing assemblies across save/load. The same shape recurred for Huang entry-stop seeds (#118, `simd_huang_cover.cpp:677`; #116). `AssemblyPool.tla` currently seeds atomically, so this bug class is **unmodeled** — the planned extension is a generation phase plus `SPEC-POOL-4` (stopped generation ⇒ no partial resumable state).
- **Salvage-before-return** (#118, #116): every terminal path must preserve requeueable state (seeds, queued assemblies). Covered for the disassembler pool by `SeqConservation`; the assembly-side `drain()` path is abstracted with the task body.
- **Tests must fail without the fix** (#116 `test_solver.cpp:854`, #114 verified-by-revert): the same standard applies here — every spec is mutation-checked (removing the submit window violates `WindowBounded`; dropping token releases violates `CleanExit`).
- **NDEBUG-vanishing checks gate nothing** (#113): CI gates must throw via `bt_te()`, never `bt_assert`. Applies to the planned debug accounting asserts: they stay debug-only tripwires inside `#ifndef NDEBUG` tests, never release gates.

## Deliberate gaps (not modeled)

C++ locks (atomic steps here; token-free/lock-free waits by construction),
the C++ memory model (atomics are sequentially consistent),
`AssemblyTaskPool::abort()`/`drain()` (currently unwired: assembly stop
arrives via `runStop` + `notify`), the null-budget path (budget = N ≈
uncapped), task bodies (exact cover search, mid-task requeue), voxel
caches, disassembly payloads, the merger callback body, GUI.

## Editing rules (anti-divergence)

1. Edit only the `--algorithm` PlusCal and the property definitions;
   never hand-edit the generated translation — re-run `pcal.trans`.
2. Invariant names are stable IDs (`SPEC-BUDGET-1`, …) quoted in
   `src/lib` comments. Renaming one means updating both sides.
3. Keep the modeled surface to stable interfaces
   (`acquire`/`release`, `pop_task`/`finishTask`, `submit`/`finish`/`abort`/`requestStop`);
   internal refactors then don't touch the spec.
4. Any PR touching `thread_budget.h`, `assembler_pool.h`,
   `disassemblerpool.h`, `assembler.h` or `solvethread.h` states
   "spec unaffected" or updates the spec — `just spec-check` must pass.
5. TLC already caught two model bugs during construction (a TOCTOU on a
   bound counter split across an atomicity label; an unguarded `finishTask`
   after a skipped pop). New labels/interleavings deserve the same
   suspicion: re-check each split against the C++ locking before assuming
   a red run is a false positive.

## Tooling quirks found the hard way

- TLC `.cfg` files reject `<<…>>` sequence literals — pass a count
  (`NumTasks`) and build `[i \in 1..NumTasks |-> i]` in-spec.
- Process ids compared against `1..N` must be integers (`0`, `N+1`):
  a string id makes TLC throw on `"stop" \in 1..2`.
- The PlusCal translator demands labels at control-flow joins; each new
  label is a new interleaving point — verify it against the C++ locks.
