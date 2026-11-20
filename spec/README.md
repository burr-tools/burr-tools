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
| `DisasmPool.tla` | Disassembler-pool protocol: bounded queue, worker pickup, direct delivery by the worker (in any order), gap-free numbering, finish/abort/requestStop lifecycle with salvage. |
| `DisasmPool.cfg` | TLC config: 2 workers, 4 assemblies, queue 2. |
| `DisasmPoolTight.cfg` | 3 workers, queue 1: forces the queue-full backpressure path nearly every step. |
| `SolverSession.tla` | Session protocol (`solverSession_c`): `Idle`/`Running`/`Paused`/`Finished`/`Stopped`/`Error` lifecycle, the end-of-phase decision (read outside the lock, published under it) racing `abort()` from another thread, `start()`'s Idle re-check, a failing phase, bounded token-free pull queue with soft-cap lifting on `requestPause()`/`pause()` (the host may keep popping or block in `join()`), salvage/pending re-submission on `resume()`. |
| `SolverSession.cfg` | TLC config: 2 producers, budget 2, 3 items, pull-queue cap 1, up to 2 pause/resume cycles. |
| `SolverSessionTight.cfg` | Same with budget 1: budget contention and soft-cap lifting on nearly every step. |
| `Pipeline.tla` | Both tiers over one shared `ThreadBudget`: assembly pop/search/submit with token yield, disassembly jobs delivered directly by their workers. The only spec that can state the global N-active-thread bound. |
| `Pipeline.cfg` | 1 assembler + 1 disassembler over 1 token, 2 tasks. |
| `PipelineWide.cfg` | 2 assemblers + 1 disassembler over 2 tokens, 3 tasks: genuine contention both ways (~25s). |

## Run

```bash
just spec-check   # translate + lint + check all configs (about a minute)
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

## How to read these specs (for TLA+ non-experts)

Each `.tla` file is one self-contained protocol model plus its checked
properties. Read in this order:

1. The header comment: scope, deliberate gaps, SPEC-ID list.
2. `CONSTANTS` plus the `.cfg` file: the checked instance sizes. The
   code uses 64/256; TLC checks 1–4. Small instances find protocol
   bugs; bigger ones mostly add states.
3. The `--algorithm` block (PlusCal, reads like pseudocode):
   - `process` ≈ thread. `fair` = must eventually run (workers);
     unfair = may never run (environment stop/abort choices — TLC
     covers both).
   - `label:` = atomicity boundary. Everything between two labels is ONE
     indivisible step. New labels = new interleavings; each must be
     checked against the C++ locking.
   - `await cond;` = condition-variable wait (re-evaluated automatically;
     no notify needed in the model). Each non-trivial await carries a
     comment naming the protocol decision it encodes.
   - `either {A} or {B};` = scheduler/environment nondeterminism — TLC
     explores both.
   - `assert P;` = checked at that point in every behavior.
4. After `END TRANSLATION` (generated, never edited): the invariants
   (`SPEC-*` IDs, also quoted in `src/lib` comments) and the liveness
   property. `just spec-check` verifies all of them plus deadlock-freedom.

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
| `stopper` process | Terminal stop at an arbitrary point (C++: run/jthread stop tokens; the assembly pool has no stop flag of its own, so pushes stay accepted). Unfair single step, so TLC covers stop and never-stop |
| `BudgetConservation` | **SPEC-BUDGET-1**: takes/returns pair up |
| `ActiveBounded` | N-active-thread bound: every in-flight task holds a token |
| `Pairing` | **SPEC-POOL-1**: each `pop_task` pairs with exactly one `finishTask` |
| `TaskConservation` | **SPEC-POOL-2**: no task lost/duplicated across splits (pool inventory only; unseeded tasks sit in `genList`) |
| `GenShape` | Generation emits the 1..k prefix in order |
| `NoPartialResume` | **SPEC-POOL-4**: stopped generation ⇒ no list, no seeding, nothing searched (C++ side: PRs #116-#118) |
| `master` + `GenDrop`/`GenSeed` | `generateTasksAtDepth`: one subtask per step; stop discards the partial list and marks `parallelInterrupted` (ditto: PRs #116-#118) |
| `CleanExit` | **SPEC-POOL-3**: exits leak no tasks/tokens |
| `AllTerminate` | Liveness: generate, drain-to-quiescence, or stop always ends the search |

## C++ ↔ spec mapping (`DisasmPool.tla`)

| Spec element | C++ counterpart (`disassemblerpool.cpp`) |
| :--- | :--- |
| `submitter` + `SSpace` await | `submit()`: terminal check, then `hasSpace()` wait (queue room), seqNo on enqueue, salvage + `false` (no seqNo) when terminal |
| `worker` + `WWait` await | `worker_loop()`: pop under `queue_mutex`; predicate deliberately omits `stop_requested` (requestStop re-wake is harmless) |
| `WDeliver` | `worker_loop()` calling `on_result` itself after returning its token; jobs finish, and are delivered, in any order |
| `stopper` + `Salv` | `requestStop()`: set the flag, then salvage the whole queue in one step (C++ holds `queue_mutex` across the drain). The atomic salvage is load-bearing for `GapFree`: an item-by-item salvage interleaved with worker pops violates it (mutation-checked). Fair with a skip branch: skipping covers never-stop |
| `aborter` | `abort()`: discard queue and in-flight jobs; workers exit on the flag (also covers the worker-exception path's observable protocol) |
| `finisher` | `finish()`: set `finished` once every offer resolved; workers drain and join |
| `NoDuplicateDelivery` / `SalvageSuffix` / `GapFree` | **SPEC-DIS-1**: delivered `seqNo`s are unique and gap-free (the salvage is a suffix of the numbering); delivery order is unspecified |
| `QueueBounded` | **SPEC-DIS-2** |
| — | **SPEC-DIS-3** / **SPEC-DIS-4** retired 2026-11-20 with the ordered merger (reorder buffer bound, submit window) |
| `OfferConservation` / `SeqConservation` | **SPEC-DIS-5**: enqueued ∨ salvaged ∨ delivered ∨ discarded, never lost |
| `SalvageOrdered` | **SPEC-DIS-6**: salvage keeps submit order for re-submission |
| `CleanShutdown` | **SPEC-DIS-7**: no stranded jobs |
| `AllTerminate` | Liveness over all four stop/abort combinations |

## C++ ↔ spec mapping (`Pipeline.tla`)

| Spec element | C++ counterpart |
| :--- | :--- |
| `aworker` pop/work/submit | `AssemblyTaskPool::pop_task` + task body + `disassemblerPool_c::submit()` (token yield across the space wait, reacquire before enqueue) |
| `SubmitLoop` + `ARetryRel` | `submit()`'s `while (!hasSpace())`: the slot is re-verified after every lock-free budget wait (removing the recheck overflows the queue — mutation-checked) |
| `ARequeue` | Terminal submit salvages the task back (assembler retry / disassembler salvage paths) |
| `dworker` + `DPark` | `worker_loop()` pickup plus the per-job budget gate |
| `DRel` / `DDeliver` | `worker_loop()` returning the token, then calling `on_result` directly |
| `BudgetConservation` | **SPEC-PIPE-1**: takes/returns pair up globally |
| `WorkersHoldTokens` | **SPEC-PIPE-1** (bound half): every working thread holds a token. Deliberately NOT open-tasks-plus-jobs: TLC refuted that reading — an assembler yielded in submit-wait while a disassembler works is designed overlap |
| `AssemblyConservation` / `DisasmConservation` / `SubmittedEqualsCompleted` / `NoDuplicateDelivery` | **SPEC-PIPE-2**: end-to-end no-loss, task → assembly → delivery, nothing delivered twice |
| — | **SPEC-PIPE-3** retired 2026-11-20 (ordered delivery; result order is unspecified) |
| `QueueBounded` | **SPEC-PIPE-4** |
| `CleanShutdown` | **SPEC-PIPE-5** |

## C++ ↔ spec mapping (`SolverSession.tla`)

| Spec element | C++ counterpart (`src/lib/solver/solversession.cpp`) |
| :--- | :--- |
| `producer` + `PTake` | The threads that push results: assembler threads (assembly only) or the pool (assembler submits, budgeted workers disassemble and deliver), collapsed into take + deliver; pending items first (the driver's re-submission). What is abstracted and why: the spec header |
| `PProduce` salvage vs in-flight | `disassemblerPool_c::requestStop()` salvaging queued work into `pending_` vs in-flight items pushing into `queue_`; the token goes back before delivery (the worker releases before `on_result`) |
| `PEnqLoop` / `PEnqWait` | `enqueueResult()`: token-free, holds `queueMutex_` across cap check + push, lifts the soft cap on `pauseRequested_` so `pause()` never deadlocks on a full queue |
| `driver` + `DMaybeFail` | `runPhase()`'s catch blocks: `teardownAfterFailure()` lifts the cap, aborts the pool, ends in `Error` |
| `driver` + `DWait`/`DPublish` | `runPhase()` step 6 reading `abortRequested_` outside the lock, then `finishPhase()` publishing under `queueMutex_`, turning `Paused`/`Finished` into `Stopped` after an `abort()` |
| `host` + `HValidate`/`HCommit` | `start()`: resume validation while `Idle`, then the Idle re-check under `queueMutex_` before `Running` |
| `host` + `HStep`/`HJoin` | `pop()`/`drain()`, `requestPause()` then more pops, `pause()` blocking in `join()` without popping, `state()` + `resume()` |
| `aborter` + `ABegin`/`AJoin` | `abort()` from any thread at any time: flags and queue under `queueMutex_`, finalising `Idle`/`Paused` itself, then `join()` |
| `StateMachineOK` | **SPEC-SESS-1**: lifecycle state machine & LIFECYCLE-1/2 invariants |
| `QueueSoftBounded` | **SPEC-SESS-2**: `queue_` obeys `queueCapacity` while running and bounded `O(P)` overshoot per pause (or failing phase) when cap is lifted |
| `ItemConservation` | **SPEC-SESS-3**: every item is unsearched, pending, in-flight, queued, popped, or discarded — never lost or duplicated across pause/resume/abort/error |
| `NoDuplicateDelivery` | **SPEC-SESS-4**: no duplicate `pop()` deliveries; `Finished` + empty queue delivers all `K` items |
| `BudgetConservation` / `CleanShutdown` | **SPEC-SESS-5**: `ThreadBudget` conservation and clean worker/queue teardown |
| `AbortTotal` + `AJoin` assert | **SPEC-SESS-6**: after `abort()` never `Paused` again and `Running` only while winding down; `abort()` returns with the session `Stopped` (or `Error`, or already `Finished`). Mutation-checked: publishing the decision unconverted, or `start()` without the Idle re-check, violates it; dropping the cap lift on pause violates `AllTerminate` |
| `AllTerminate` | Liveness: even when the host pauses on a full queue without popping, the session always ends `Finished` / `Stopped` / `Error` |

## Verification input from code review (PRs 113–118)

Review findings that shape what the specs must cover:

- **Stop during generation must discard partial state** (`assembler_1.cpp:3035`): a stop inside `generateTasksAtDepth` left a partial `parallelTasks` list with `interrupted=0`, silently losing assemblies across save/load. The same shape recurred for Huang entry-stop seeds (`simd_huang_cover.cpp:687`; #116). Covered by `NoPartialResume` (SPEC-POOL-4); the C++ side is PRs #116-#118.
- **Salvage-before-return** (#118, #116): every terminal path must preserve requeueable state (seeds, queued assemblies). Covered for the disassembler pool by `SeqConservation`; the assembly-side `drain()` is the wired stop path (not an abstraction), while the stop-time retry re-queue stays deliberately unmodeled — SPEC-POOL-2 is really about that path, so it is listed as a gap, not a claim.
- **Tests must fail without the fix** (#116 `test_solver.cpp:854`, #114 verified-by-revert): the same standard applies here — every spec is mutation-checked (salvaging item by item instead of atomically violates `GapFree`; dropping token releases violates `CleanExit`).
- **NDEBUG-vanishing checks gate nothing** (#113): CI gates must throw via `bt_te()`, never `bt_assert`. Applies to the planned debug accounting asserts: they stay debug-only tripwires inside `#ifndef NDEBUG` tests, never release gates.

## Deliberate gaps (not modeled)

C++ locks (atomic steps here; token-free/lock-free waits by construction),
the C++ memory model (atomics are sequentially consistent),
`AssemblyTaskPool` stop plumbing beyond the run/jthread tokens (it
deliberately has no `requestStop()`/`abort()`: a pool-level stop flag
would refuse the requeue pushes the `drain()`-based resume depends on;
`drain()` itself is live and covered implicitly by task conservation),
the null-budget path (budget = N ≈ uncapped), task bodies (exact cover
search, mid-task requeue), voxel caches, disassembly payloads, the
result callback body, GUI.

## Editing rules (anti-divergence)

1. Edit only the `--algorithm` PlusCal and the property definitions;
   never hand-edit the generated translation — re-run `pcal.trans`.
2. Invariant names are stable IDs (`SPEC-BUDGET-1`, …) quoted in
   `src/lib` comments. Renaming one means updating both sides.
3. Keep the modeled surface to stable interfaces
   (`acquire`/`release`, `pop_task`/`finishTask`, `submit`/`finish`/`abort`/`requestStop`);
   internal refactors then don't touch the spec.
4. Any PR touching `thread_budget.h`, `assembler_pool.h`,
   `disassemblerpool.h`, `assembler.h` or `solver/solversession.h`
   states "spec unaffected" or updates the spec
   — `just spec-check` must pass.
5. TLC already caught two model bugs during construction (a TOCTOU on a
   bound counter split across an atomicity label; an unguarded `finishTask`
   after a skipped pop). New labels/interleavings deserve the same
   suspicion: re-check each split against the C++ locking before assuming
   a red run is a false positive.
6. `just spec-lint` (inside `spec-check`) verifies every declared PlusCal
   process survived translation. `pcal.trans` silently truncates the
   algorithm at a brace imbalance: a stray `};` once dropped master,
   stopper and splitter while TLC stayed green on the worker-only
   remainder. Never trust a green run without the lint passing.
7. Refactoring awaits is high-risk editing: renaming a predicate is
   safe, but changing what it says can make the model vacuous while
   staying green (e.g. dropping the `genDone` gate lets workers
   quiescence-terminate before generation, after which the search phase
   is never exercised yet every invariant holds). After touching an
   await, check that the distinct-state count did not collapse AND that
   a targeted mutant still fails. Current rough counts: AssemblyPool
   ~20k/21k, DisasmPool ~69k/60k (normal/tight),
   SolverSession ~606k/398k (normal/tight), Pipeline ~1k/69k/528k states
   (the last is PipelineWideND2, the only config with two disassembly
   workers -- the one that exercises D-tier holder identity).

## Tooling quirks found the hard way

- TLC `.cfg` files reject `<<…>>` sequence literals — pass a count
  (`NumTasks`) and build `[i \in 1..NumTasks |-> i]` in-spec.
- Process ids compared against `1..N` must be integers (`0`, `N+1`):
  a string id makes TLC throw on `"stop" \in 1..2`. Negative ids are
  fine but need `EXTENDS Integers` (unary minus is not in `Naturals`).
- The PlusCal translator demands labels at control-flow joins; each new
  label is a new interleaving point — verify it against the C++ locks.
- Brace balance is load-bearing and unchecked: one extra `}` ends the
  algorithm early with "Translation completed" and no error. Count
  braces per region when processes go missing from `ProcSet`.
- TLA+ has no forward references: a helper operator over PlusCal
  variables cannot be defined before the `--algorithm` block (the
  variables only materialize in the generated translation below it).
  Keep shared predicate logic in comments at the awaits, not in
  operators above the algorithm.
