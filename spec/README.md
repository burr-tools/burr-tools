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

## Deliberate gaps (not modeled)

C++ locks (atomic steps here; token-free/lock-free waits by construction),
the C++ memory model (atomics are sequentially consistent),
`abort()`/`drain()`, the null-budget path (budget = N ≈ uncapped),
task bodies (exact cover), voxel caches, disassembler pool, GUI.

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
