# Parallel Assembly & Disassembly — Opportunities

**Date:** 2026-09-18
**Status:** Brainstorm / measured survey. Nothing implemented.
**Scope:** `src/lib` solver core (`assembler_0/1`, `disassembler_0/a`,
`movementanalysator`, `movementcache`, `solvethread`). No GUI changes proposed here.

## Why this document

BurrTools runs its whole solve on a single worker thread (`solveThread_c`),
while the GUI thread only polls progress. Every machine the program runs on
today has 4–16 idle cores. This document measures where solver time actually
goes, then ranks the places where concurrency can recover it, with the
prerequisites and the correctness traps for each.

## How the numbers were obtained

A standalone harness (not committed) linked the `src/lib` core and timed the
four phases separately: `createMatrix` (prepare), `reduce`, the DLX search, and
disassembly. Instruction-level profiles came from `valgrind --tool=callgrind`.

Caveats on every number below:

- **4 cores.** All scaling figures are 4-thread figures; they say nothing
  directly about 16-core behaviour.
- The harness stubbed out `minkmesh` (the `manifold` subproject could not be
  fetched in this environment). Mesh code is not on any solver path, so this
  does not affect the measurements, but it does mean these runs did not go
  through `just build`.
- The bundled `examples/` are demo-sized (worst case ~0.5 s). They establish
  the *shape* of the cost, not absolute runtimes of puzzles users actually wait on.

## Where the time goes

Per-phase wall time, `examples/`, problem 0, disassembly enabled (ms):

| Puzzle | prepare | reduce | search | disasm | total | dominant |
|---|---:|---:|---:|---:|---:|---|
| PelikanBurr | 2.5 | 9.2 | 0.2 | **143.7** | 155.6 | disasm 92% |
| DraculasDentalDesaster | 1.6 | 33.4 | 0.5 | **117.8** | 153.3 | disasm 77% |
| CubeInCage | 0.4 | 0.4 | 0.2 | **6.4** | 7.4 | disasm 87% |
| DemoPieceGenerator | 0.1 | 0.1 | 3.5 | **9.9** | 13.6 | disasm 73% |
| SolidSixPieceBurrs | 6.3 | 12.8 | **502.8** | 9.5 | 531.4 | search 95% |
| AlPackino | 0.9 | 8.8 | **28.1** | 0.3 | 38.1 | search 74% |
| Bermuda | 0.6 | **34.9** | 4.6 | 0.3 | 40.3 | reduce 87% |
| Prisgon | 1.0 | **8.1** | 0.3 | 1.5 | 10.9 | reduce 74% |

**There is no single hot phase.** Disassembly, search, and `reduce` each
dominate some puzzles outright. A parallelisation effort that only targets one
of them will look like a no-op on half the corpus. `prepare` is never
dominant and can be ignored.

Instruction profile, disassembly-dominated (`PelikanBurr`, 1.35e9 Ir):

| % | Function |
|---:|---|
| 25.5 | `movementAnalysator_c::checkmovement` |
| 24.1 | `movementAnalysator_c::prepare` |
| 6.5 | `movementCache_0_c::moCalcValues` (cache misses) |
| 6.0 | `movementAnalysator_c::newNodeMerge` |
| 5.3 | `movementAnalysator_c::newNode` |
| 3.7 | `movementCache_c::getMoValue` |
| 3.7 | `movementAnalysator_c::find` |
| 3.2 | `disassemblerNode_c::operator==` |

Instruction profile, search-dominated (`SolidSixPieceBurrs`, 3.0e9 Ir):

| % | Function |
|---:|---|
| 20.6 | `assembler_1_c::hiderow` |
| 17.4 | `assembler_1_c::unhiderow` |
| 14.0 | `assembler_1_c::getAssembly` |
| 5.9 | `assembler_1_c::hiderows` |
| 5.9 | `assembler_1_c::iterative` |
| 5.2 | `assembly_c::transform` |
| 4.7 | `open_column_conditions_fulfillable` |
| 3.6 | `find_best_unclosed_column` |
| 3.3 | `assembly_c::smallerRotationExists` |
| 3.1 | `assembly_c::compare` |
| 1.3 | `assembly_c::sort` |

Note the second cluster: `getAssembly` + `transform` + `smallerRotationExists`
+ `compare` + `sort` ≈ **27%** of a search-dominated run is *per-assembly
post-processing*, not tree search. That work is independent per assembly.

## Prerequisite: three unsynchronised lazy caches

Before any of this, three shared objects mutate through a `const` interface:

| Location | Member |
|---|---|
| `src/lib/voxel.cpp:591` (`selfSymmetries`) | `mutable symmetries_t symmetries` (`voxel.h:98`) |
| `src/lib/voxel.cpp:237-251` (`getHotspot`, `getBoundingBox`) | `mutable std::vector<int> BbHsCache` (`voxel.h:159`) |
| `src/lib/gridtype.cpp:174-176` (`getSymmetries`) | `mutable std::unique_ptr<symmetries_c> sym` (`gridtype.h:81`) |

These are real races, not theoretical. ThreadSanitizer on four threads calling
`selfSymmetries()` and `getHotspot()` on one freshly loaded shared shape reports:

```
WARNING: ThreadSanitizer: data race
  Write of size 1 by thread T1:
    #0 voxel_c::selfSymmetries() const src/lib/voxel.cpp:591
WARNING: ThreadSanitizer: data race
  Write of size 4 by thread T2:
    #0 voxel_c::getHotspot(unsigned char, int*, int*, int*) const src/lib/voxel.cpp:249
```

Today they are latent: the single-threaded `createMatrix`/`reduce` phase warms
every entry before anything else runs, and a TSan run of the parallel
disassembly experiment below is clean for exactly that reason. That is luck,
and it evaporates the moment threads start earlier (parallel `prepare`,
parallel `reduce`) or a shape is reached only from a worker.

Two options, in order of preference:

1. **Prime eagerly, then freeze.** Extend the existing `initHotspot()` warm-up
   into a `primeCaches()` that fills `symmetries` and every `BbHsCache` slot and
   forces `gridType_c::sym`, called once before any worker starts. Cheapest, no
   hot-path cost, and it keeps the caches plain. Needs a debug-build assertion
   that nothing fills a slot afterwards, or the guarantee rots silently.
2. **Synchronise.** `std::once_flag` for `gridType_c::sym`; per-slot atomics for
   `BbHsCache`. Robust but puts an acquire load on genuinely hot paths
   (`getHotspot` is called from `transform`).

Recommend (1), with (2) for `gridType_c::sym` only, since that one is
construct-once and not hot.

## Opportunity A — parallel disassembly pipeline

**Measured. Highest value, lowest risk.**

`solveThread_c::assembly()` (`solvethread.cpp:194`) calls
`disassm->disassemble(a.get())` *inline on the assembler's own thread*. The
assembler search stalls for the entire disassembly. Each assembly's disassembly
is completely independent of every other.

Experiment: collect the assembly set, then disassemble it with N threads, each
owning its own `disassembler_0_c` (and therefore its own `movementCache_c`);
nothing shared but the `const problem_c`.

| Puzzle | jobs | 1 thread | 2 | 3 | 4 |
|---|---:|---:|---:|---:|---:|
| PelikanBurr ×20 | 240 | 2575 ms | 1.90x | 2.80x | **3.59x** |
| DraculasDentalDesaster ×10 | 840 | 1011 ms | 1.91x | 2.78x | **3.60x** |
| CubeInCage ×40 | 3840 | 199 ms | 1.89x | 2.76x | **3.65x** |
| DemoPieceGenerator ×4 | 3712 | 42 ms | 1.77x | 2.63x | **3.45x** |
| SolidSixPieceBurrs ×4 | 2352 | 19 ms | 1.36x | 1.87x | 1.75x |

**~3.6x on 4 cores, ~90% efficiency**, with the solvable-count identical at
every thread count. The `SolidSixPieceBurrs` row is the informative failure:
its 2352 jobs take 19 ms total, so per-job work is below thread-dispatch
overhead — a size threshold is needed, not a fixed pool.

The ~10% loss is explained by the profile: each thread starts with a cold
`movementCache_c` and redundantly recomputes entries (`moCalcValues` is 6.5% of
a serial run). Per-thread caches are the right call anyway — a shared cache
would need a lock on `getMoValue`, which the profile shows at 3.7% of all
instructions, i.e. called constantly.

Design: bounded work queue between the assembler and a pool of disassembly
workers. The assembler pushes and keeps searching; workers pop. Bounded, so a
puzzle with millions of assemblies cannot exhaust memory.

**The hard part is not the threading, it is determinism.** `assembly()` today
does four order-dependent things: sorted insertion into the solution list,
`incNumSolutions`/`incNumAssemblies`, the `solutionLimit`/`dropMultiplicator`
thinning, and `liveSort`. Out-of-order completion changes which solutions
survive thinning. Fix: tag each assembly with a monotonic sequence number at
dispatch and commit results through a reorder buffer, so the committed sequence
is bit-identical to the serial one. Without this, `test/test_solver.cpp`
expectations become unstable and — worse — saved puzzle files change contents
run to run.

## Opportunity B — parallel DLX subtree search

**Highest ceiling, highest effort.** Targets the 95%-search puzzles.

Two facts make this far more tractable than it looks:

1. **All mutable search state is POD vectors.** `assembler_1_c` holds
   `left/right/up/down/colCount/weight/min/max`, `rows`, `hidden_rows`,
   `task_stack`, `next_row_stack`, `column_stack`, `finished_a/b` — every one a
   `std::vector<unsigned int>` — plus scalars and a read-only `const problem_c&`.
   A clone is a memberwise copy with no pointer fixups.
2. **A search position is already a serialisable value with a replay path.**
   `assembler_1_c::save` (`assembler_1.cpp:1989`) writes exactly those seven
   stack vectors, and `setPosition` (`assembler_1.cpp:1921`) rebuilds the matrix
   by *replaying* the cover operations from `task_stack`. `assembler_0_c` has
   the equivalent in `(pos, rows[], columns[])`. This is precisely the
   task-descriptor + replay mechanism a work-stealing scheduler needs, and it
   already exists and is already exercised by `test/test_assembler_config.cpp`.

Design: each worker owns a cloned matrix. When a worker's stack has an
unexplored alternative near the *root* (shallowest open branch — the largest
subtree, and the one least likely to be stolen again), it splits that off as a
position vector; an idle worker replays it onto its own clone and searches.

Open questions, in rough order of difficulty:

- **Work granularity.** Burr search trees are wildly unbalanced. Splitting at a
  fixed depth will produce one worker doing 90% of the work. Steal-on-idle from
  the shallowest open branch is the standard answer, but needs a minimum-subtree
  heuristic so stealing does not cost more than the subtree is worth.
- **Memory.** Per-worker matrix clone is O(placements × voxels-per-piece)
  `unsigned int`s. Fine at 4 threads; measure before promising 16.
- **`getFinished()` and progress.** `assembler_0_c::getFinished`
  (`assembler_0.cpp:1413`) computes a fraction from the single `rows`/`columns`
  stack; `assembler_1_c` uses `finished_a`/`finished_b` under `finishedMutex`.
  Neither has meaning across N divergent stacks. Progress becomes a sum over
  workers of their subtree's share of the root — a real design task, not a detail.
- **`avoidTransformedAssemblies` dedup.** `solution()` (`assembler_1.cpp:1079`)
  calls `smallerRotationExists`, which is `const` and reads only the problem, so
  it is safe to run concurrently. Good news: no shared dedup set to lock.
- **`stop()` / save-and-resume.** `stop()` currently sets one `abbort` flag;
  saving a paused N-worker search means persisting N positions. The on-disk
  assembler format would need to grow a worker list, with backward compatibility
  for single-position files.
- **Iteration counts change.** Parallel search visits nodes in a different
  order and a different number of them. `CLAUDE.md` already forbids asserting
  exact iteration counts in regression tests — that rule is what makes this
  safe to attempt, and it must hold.

## Opportunity C — parallel `reduce()`

Targets the `reduce`-dominated puzzles (Bermuda 87%, Prisgon 74%,
DraculasDentalDesaster 22%).

`assembler_0_c::reduce` (`assembler_0.cpp:1024`) has two loops with different
characters:

- The **opening column scan** builds per-column fill counts read-only, then
  removes rows. The counting is independent per column; only the removal
  mutates. Split counting across threads, collect a row-removal set, apply
  serially.
- The **deep per-piece check** covers piece `p`, then for each placement `r`
  does `try_cover_row(r, …)` + `checkmatrix()` + `uncover_row(r)`. Within one
  piece, each placement is tested and then fully undone, so the placements are
  **independent given a per-thread matrix copy**. This is the bigger win and it
  parallelises cleanly: each thread takes a slice of the placements, returns
  its rejects, and the merged reject set is applied once on the main copy.

The outer `do { … } while (rem_sth)` fixpoint stays serial; only the inside of
each round parallelises. Note `reduce()` ends with a bare
`fprintf(stderr, "removed %u rows and %u columns\n", …)` — that should become
conditional before it gets interleaved from several threads.

## Opportunity D — offload per-assembly post-processing

The cheapest of the search-side wins, and independent of Opportunity B. ~27% of
a search-dominated run is `getAssembly` + `transform` + `smallerRotationExists`
+ `compare` + `sort`, all per-assembly and all reading immutable problem state.

Hand each found solution to the same pool as Opportunity A and let the search
thread return immediately to the matrix. Amdahl bound is ~1.37x on its own —
modest, but it shares all its plumbing (reorder buffer, sequence numbers,
bounded queue) with Opportunity A, so the marginal cost is close to zero once A
is done. It also happens to be the *only* search-side speedup that does not
require cloning the matrix.

## Opportunity E — parallel placement enumeration in `prepare()`

`assembler_0_c::prepare` (`assembler_0.cpp:428`) loops over parts, then over the
24/48 rotations, calling `placementFinder_c::find`. The finds are independent;
only `AddPieceNode`/`AddVoxelNode` append to shared arrays.

Worth listing for completeness, but `prepare` never exceeded 6.3 ms in the
corpus and is never the dominant phase. **Do not spend effort here** unless a
large real-world puzzle shows otherwise — and note it is the phase most exposed
to the lazy-cache races above, since it runs before anything is warm.

## Opportunity F — intra-node parallelism in the analysator

`movementAnalysator_c::prepare` is 24% of disassembly time and its outer loop
`for (d = 0; d < dirs; d++) { do { … } while (again); }`
(`movementanalysator.cpp:79`) is **fully independent per direction** — each
direction relaxes its own slice of the matrix.

But `dirs` is 3 for brick grids, and the matrices are small. Threading this is
almost certainly a loss to synchronisation overhead. The realistic win here is
**SIMD/layout**, not threads: the inner relaxation is a strided
min-plus product over `unsigned int`, currently walking with stride `dirs`.
Restructuring to direction-major contiguous slices would make it vectorisable.
Listed as a *serial* optimisation that happens to compose with Opportunity A.

Similarly `disassembler_a_c::checkSubproblems` (`disassembler_a.cpp:103`) solves
two subproblems, and currently short-circuits: `left` is only attempted when
`remove_ok`. Running both speculatively would waste the `left` work whenever
`remove` fails. More importantly `groups` (`grouping_c`) is **shared mutable
state** across the recursion — `subProbGrouping` calls `groups->newSet()`. Any
parallel recursion here needs per-branch grouping state first. Low priority.

## Opportunity G — coarse-grained: whole problems

`burrTxt2.cpp` loops `for (pr = firstProblem; pr < lastProblem; pr++)` solving
each problem sequentially, and `burrgrower.cpp` evaluates a stream of candidate
puzzles the same way. Both are embarrassingly parallel at the top level, need no
solver changes at all, and would speed up exactly the batch workloads people
leave running overnight.

This is the best effort-to-payoff ratio in the document *for batch use*, and it
does nothing for a user waiting on one problem in the GUI. Both statements are
true and they target different users.

## Suggested sequencing

1. **Fix the lazy caches** (prerequisite for everything; small, self-contained).
2. **Opportunity A** — parallel disassembly. Measured 3.6x/4 cores, and the
   reorder-buffer work it forces is the foundation for D.
3. **Opportunity D** — post-processing offload. Nearly free once A exists.
4. **Opportunity G** — batch-level parallelism in `burrTxt2`/`burrgrower`.
   Independent of everything else; can proceed in parallel with the above.
5. **Opportunity C** — parallel `reduce`. Self-contained, clear win on its
   puzzles.
6. **Opportunity B** — parallel search. Largest ceiling, but only after the
   determinism and progress-reporting machinery from A exists.

Opportunities E and F are not recommended as threading work.

## Cross-cutting constraints

- **Determinism is the headline risk.** Solution lists are written to
  `.xmpuzzle` files. Any scheme whose output depends on thread timing changes
  saved files run to run. Reorder-buffer-on-commit is non-negotiable.
- **`CLAUDE.md` rule 4 already covers the test side**: assert on
  rotation/permutation-invariant properties (assembly count, disassemblable
  count, move levels), never exact iteration counts. Parallel search changes
  iteration counts legitimately.
- **`CLAUDE.md` rule 3** — never strip synchronisation from shared solver state
  — applies with more force once there are N workers instead of one.
- **`just build-tsan` already exists.** Every item here should land with a TSan
  run in CI, not just a passing `just test-all`; the races in the prerequisite
  section are invisible to a normal test run.
- **Thread count must be configurable**, defaulting to something conservative.
  The GUI polls solver state at interactive rates from the main thread; a pool
  sized to `hardware_concurrency()` will starve it.
