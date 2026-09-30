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
| `Pipeline.tla` | Both tiers over one shared `ThreadBudget`: assembly pop/search/submit with token yield, disassembly jobs, ordered merger. The only spec that can state the global N-active-thread bound. |
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
| `stopper` process | Stop path (`runStop` semantics: pushes are still accepted after a stop, which the retry path depends on). Fair with a skip branch: skipping covers never-stop, fairness guarantees a started salvage runs to completion |
| `BudgetConservation` | **SPEC-BUDGET-1**: takes/returns pair up |
| `ActiveBounded` | N-active-thread bound: every in-flight task holds a token |
| `Pairing` | **SPEC-POOL-1**: each `pop_task` pairs with exactly one `finishTask` |
| `TaskConservation` | **SPEC-POOL-2**: no task lost/duplicated across splits (pool inventory only; unseeded tasks sit in `genList`) |
| `GenShape` | Generation emits the 1..k prefix in order |
| `NoPartialResume` | **SPEC-POOL-4**: stopped generation ⇒ no list, no seeding, nothing searched (C++ side lands with PR #118) |
| `master` + `GenDrop`/`GenSeed` | `generateTasksAtDepth`: one subtask per step; stop discards the partial list and marks `parallelInterrupted` (ditto: lands with PR #118) |
| `CleanExit` | **SPEC-POOL-3**: exits leak no tasks/tokens |
| `AllTerminate` | Liveness: generate, drain-to-quiescence, or stop always ends the search |

## C++ ↔ spec mapping (`DisasmPool.tla`)

| Spec element | C++ counterpart (`disassemblerpool.cpp`) |
| :--- | :--- |
| `submitter` + `SSpace` await | `submit()`: terminal check, then `hasSpace()` wait (queue + reorder window), seqNo on enqueue, salvage + `false` when terminal |
| `worker` + `WWait` await | `worker_loop()`: pop under `queue_mutex`; predicate deliberately omits `stop_requested` (requestStop re-wake is harmless) |
| `WFile` await | `cv_reorder` wait. TLC proves it never blocks: `WindowBounded` guarantees a free slot whenever a job is in flight |
| `merger` + `MWait` await | `merger_loop()`: next-seq filed, dropped-skip, or finished-and-drained |
| `stopper` + `SalvLoop`/`DropMove` | `requestStop()`: salvage queued assemblies, then publish `dropped_` in a second step (`pendingDrop` models the queue_mutex/result_mutex window between the two; the merger just waits it out) |
| `aborter` | `abort()`: discard queue, reorder buffer, drops; workers/merger exit on the flag (also covers the worker-exception path's observable protocol) |
| `finisher` | `finish()`: set `finished` once every offer resolved; workers join, merger drains |
| `OrderedDelivery` | **SPEC-DIS-1**: monotonic `seqNo` merge |
| `QueueBounded` / `ReorderBounded` | **SPEC-DIS-2** / **SPEC-DIS-3** |
| `WindowBounded` | **SPEC-DIS-4**: submit window bounds outstanding work |
| `OfferConservation` / `SeqConservation` | **SPEC-DIS-5**: enqueued ∨ salvaged ∨ delivered ∨ discarded, never lost |
| `SalvageOrdered` | **SPEC-DIS-6**: salvage keeps submit order for re-submission |
| `CleanShutdown` | **SPEC-DIS-7**: no stranded jobs/results |
| `AllTerminate` | Liveness over all four stop/abort combinations |

## C++ ↔ spec mapping (`Pipeline.tla`)

| Spec element | C++ counterpart |
| :--- | :--- |
| `aworker` pop/work/submit | `AssemblyTaskPool::pop_task` + task body + `disassemblerPool_c::submit()` (token yield across the space wait, reacquire before enqueue) |
| `SubmitLoop` + `ARetryRel` | `submit()`'s `while (!hasSpace())`: the slot is re-verified after every lock-free budget wait (removing the recheck overflows the queue — mutation-checked) |
| `ARequeue` | Terminal submit salvages the task back (assembler retry / disassembler salvage paths) |
| `dworker` + `DPark` | `worker_loop()` pickup plus the per-job budget gate |
| `BudgetConservation` | **SPEC-PIPE-1**: takes/returns pair up globally |
| `WorkersHoldTokens` | **SPEC-PIPE-1** (bound half): every working thread holds a token. Deliberately NOT open-tasks-plus-jobs: TLC refuted that reading — an assembler yielded in submit-wait while a disassembler works is designed overlap |
| `AssemblyConservation` / `DisasmConservation` / `SubmittedEqualsCompleted` | **SPEC-PIPE-2**: end-to-end no-loss, task → assembly → delivery |
| `OrderedDelivery` | **SPEC-PIPE-3** |
| `QueueBounded` / `ReorderBounded` / `WindowBounded` | **SPEC-PIPE-4** |
| `CleanShutdown` | **SPEC-PIPE-5** |

## Verification input from code review (PRs 113–118)

Review findings that shape what the specs must cover:

- **Stop during generation must discard partial state** (PR #118, `assembler_1.cpp:2727`): a stop inside `generateTasksAtDepth` left a partial `parallelTasks` list with `interrupted=0`, silently losing assemblies across save/load. The same shape recurred for Huang entry-stop seeds (#118, `simd_huang_cover.cpp:677`; #116). Covered by `NoPartialResume` (SPEC-POOL-4); the C++ side is pending PR #118.
- **Salvage-before-return** (#118, #116): every terminal path must preserve requeueable state (seeds, queued assemblies). Covered for the disassembler pool by `SeqConservation`; the assembly-side `drain()` is the wired stop path (not an abstraction), while the stop-time retry re-queue stays deliberately unmodeled — SPEC-POOL-2 is really about that path, so it is listed as a gap, not a claim.
- **Tests must fail without the fix** (#116 `test_solver.cpp:854`, #114 verified-by-revert): the same standard applies here — every spec is mutation-checked (removing the submit window violates `WindowBounded`; dropping token releases violates `CleanExit`).
- **NDEBUG-vanishing checks gate nothing** (#113): CI gates must throw via `bt_te()`, never `bt_assert`. Applies to the planned debug accounting asserts: they stay debug-only tripwires inside `#ifndef NDEBUG` tests, never release gates.

## Deliberate gaps (not modeled)

C++ locks (atomic steps here; token-free/lock-free waits by construction),
the C++ memory model (atomics are sequentially consistent),
`AssemblyTaskPool::abort()` and `requestStop()` (unused; assembly stop
arrives via `runStop` + `notify`, whose `drain()` is wired), the null-budget path (budget = N ≈
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
   ~14k/19k, DisasmPool ~179k/69k, Pipeline ~3k/300k states.

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
