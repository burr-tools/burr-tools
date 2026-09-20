# Disassembler Optimizations — Plan

**Date:** 2026-09-17
**Status:** B2/B3 REVERTED (see below); B1 counters + Part C kept and committed
**Scope:** `src/lib/disassembler_*.{h,cpp}`, `src/lib/movementanalysator.*`,
`src/lib/movementcache.*`, `src/lib/solvethread.*`, `src/burrTxt.cpp`
**Branch:** `disassembler-optimizations`

## Problem

Disassembly dominates solve time on multi-solution puzzles (e.g.
SolidSixPieceBurrs: 588 assemblies → 179 disassemblies in ~9s), and it runs
strictly single-threaded: `solveThread_c::assembly()` calls
`disassm->disassemble(a.get())` inline on the assembler thread
(`src/lib/solvethread.cpp:194`), stalling the assembler while each
disassembly runs. `src/burrTxt.cpp:68` (`-d` mode) has the same inline
pattern. Meanwhile the move generator fans out more than necessary (see
part B). The two efforts are independent and composable: A multiplies
throughput, B divides work per disassembly. Benchmark each separately with
`bench/bench_solve.py --ab`.

## Part A — Thread pool for disassemblies

### Observation

Assemblies are independent: disassembling assembly N never affects assembly
M. The current pipeline is producer (assembler) → inline consumer
(disassembler) on one thread.

### Design: producer–consumer with ordered merge

* The assembler callback (`solveThread_c::assembly`, `burrTxt::asm_cb`)
  becomes a producer: it enqueues `(assembly, seqNo)` and returns
  immediately so the assembler keeps searching.
* N worker threads each own a **private** `disassembler_0_c` (and hence a
  private `movementAnalysator_c`). The analysator is not shareable: scratch
  buffers (`matrix/movement/check`, `movementanalysator.h:55-61`), the
  `init_find`/`find` state machine (`nextpiece/nextdir/nextstep/...`),
  and the per-search `countingNodeHash nodes` are all mutated per call.
* The `movementCache_c` is shared behind **one mutex** covering
  `getMoValue` + `getTransformedShape` (both mutate: cache insert and lazy
  shape creation, `movementcache.cpp`). Coarse first; shard only if the
  profile says so — cache hits are fast, `moCalcValues` under lock is the
  contention risk.
* Merge stays single-threaded (the current solveThread worker): it takes
  finished `(seqNo, assembly, separation)` results and runs the *existing*
  sorted-insert / drop / thin-out logic (`solvethread.cpp:214-331`)
  unchanged. A reorder buffer re-sequences completions by `seqNo`, so
  order-sensitive logic (`solutionDrop`/`dropMultiplicator` index math,
  `SRT_UNSORT` sampling) keeps exact current semantics and output stays
  deterministic.
* New shared class (one implementation for both call sites), e.g.
  `src/lib/disassemblerpool.{h,cpp}` (`disassemblerPool_c`): queue +
  workers + reorder buffer + merge hook. `solveThread_c` and `burrTxt -d`
  both adopt it. Pool size defaults to `hardware_concurrency`, overridable.
* Cancellation: the disassembler currently has **no stop check** (unlike the
  assembler's `stop()`). Add a stop flag plumbed into `disassemble_rec`'s
  open-list loop and the analysator's `find()` loop so `stopInternal()`
  (`solvethread.cpp:336`) also wakes/joins pool workers. Per AGENTS.md
  rule 3, keep all atomics/mutexes; per the `thread_c` destructor lesson
  (`solvethread.cpp:139-151`), join workers before freeing problem state.
* Counters/progress: `incNumAssemblies/incNumSolutions` are bare `++`
  (`problem.h:424-426`) and `addSolution` assumes single-writer; all pool
  → problem mutation goes through the single merger thread and/or
  `lockSolutions()` (`problem.h:480`). GUI polling reads atomics only.
  No FLTK calls off the main thread.

### Verification (A)

* Result equivalence on the BTFiles shortlist + `examples/`: identical
  assembly/solution counts and move levels (`design/2026-09-17-*.md`
  corpus table), repeated runs deterministic.
* `just test-all`, `just check`, plus `just build-tsan` (data-race hunt —
  mandatory for this part) and `just build-asan`.
* Speedup vs workers curve (1/2/4/8) on SolidSixPieceBurrs + kangaroo.

### Risks (A)

* Shared-cache mutex granularity if `moCalcValues`-under-lock serializes
  too much → measure, then consider double-checked insert or sharding.
* Reorder-buffer memory if the assembler far outruns disassembly (assemblies
  are small placement lists; bound the queue and apply back-pressure by
  blocking the producer when full).

## B revert — verdict: complexity without payoff (REVERTED, kept as record below)

The full B2/B3 + memo + `insertOrUpdate` stack was reverted; only B1
(counters, zero behavior impact) and Part C (incremental prepare) remain.
Rationale, agreed on review of the numbers: B bought expansion-count
reductions in slide-heavy searches (CD_Pack 900k→143 nodes at search
level) and a finer move ruler (Pelikan 98→101, same tree) — but no better
solutions anywhere, no wall-clock win (parity except Excelsior +32%), more
peak RSS on deep searches (+66% Excelsior from full closed-set retention),
and ~+450 lines of subtle machinery (dual cost models, stale-skip,
relaxation, retrace soundness). Costs real, gains accounting-only.

Reverted to: 3-front BFS, multi-step edges, plain tree walk, no memo, no
`insertOrUpdate`, goldens back to `98.2.4.2`. What the revert test proved:
Excelsior regression gone (−2.2%), Simplicity +0.6%, kangaroo +7.5%
(small absolute, noise band) — i.e. old search + Part C is parity-or-better
everywhere with identical output.

The B2/B3 analysis below is kept as a record (including the traps and the
LRU negative result) so the next attempt starts informed, not from zero.

## Part B — Unit steps, zero-cost continuation, no inverse moves (REVERTED)

### Observation

`movementAnalysator_c::find()` state 2 tries distances 1, 2, 3, … per
(piece, direction) (`nextstep++` while `checkmovement` succeeds,
`movementanalysator.cpp:557-598`). Each distance becomes a separate BFS
edge costing waylength + 1 (`disassemblernode.cpp:25-32`, `step` defaults
to 1). So one (piece, dir) with reach k costs k `checkmovement` runs and
k successor nodes.

### Proposal

1. Generate **only step-1 moves** per (piece, dir).
2. A move continuing the **same direction** as the node's own move costs
   **0 extra waylength** (the ctor already takes a `step` parameter — pass
   0). A 3-unit slide is then 3 unit edges with total cost 1, same optimum
   as today's direct 3-edge.
3. Never generate the **inverse** of the parent node's move direction
   (`dir ^ 1` in the `nextdir` encoding), except from the root.

Effect: per-node branching drops from O(k·pieces·dirs) to O(pieces·dirs),
and `checkmovement` runs once per (piece, dir) instead of per distance.

### Required changes (the proposal is not a 3-line diff)

1. **0-1 BFS in `disassemble_rec`.** The 3-front scheme
   (`nodeHash closed[3]`, `disassembler_0.cpp:80-87`, fronts == distance)
   assumes uniform edge cost, which zero-cost edges break. Convert to a
   deque: 0-cost continuations `push_front`, cost-1 turns `push_back`,
   distance labels replace fronts. The `replaceNode`-on-shorter-way logic
   in `nodeHash::insert` stays valid and becomes load-bearing — keep it.
2. **Direction bookkeeping.** The no-inverse test must use the node's
   *stored* direction vs the parent's *stored* direction: `newNode` may
   flip direction when the moved pieces outweigh the stationary ones
   (`nd ^= 1`, `movementanalysator.cpp:361-372`), so comparing against the
   *requested* direction is wrong.
3. **Mergers (state 99) stay correct.** `newNodeMerge` requires equal
   amounts in both nodes (`movementanalysator.cpp:443`) — with unit-only
   moves every amount is 1, so merges become amount-compatible by
   construction. No logic change, just re-verify.
4. **Removal moves untouched.** States 0/1 (`amount == 30000`) are not
   distance expansions; leave them.
5. **Soundness of the inverse ban.** A shortest path never immediately
   backtracks: X→Y→X costs ≥ 0 and returns to a visited state, which the
   distance-labelled closed set prunes anyway. The explicit ban therefore
   only saves generating (and `checkmovement`-testing) a doomed node —
   belt and braces on top of the closed set, and measurable via the
   expansion counters from B1.
6. **Reported levels are safe.** `movesText`/`sumMoves`/`getMoves` count
   separation-tree states, not waylength (`disassembly.h:225`), so
   regression levels (Pelikan `"98.2.4.2"` etc.) must come out identical.
   Waylength only steers shortest-path choice via `replaceNode`.
7. **`completeFind` / movementbrowser.** Already runs with `maxstep = 1`
   (`movementanalysator.cpp:721`) — the closest thing to a unit-step
   precedent in the tree; keep its behavior covered by existing tests.

### Phasing (B)

* **B1 — instrument only.** Per-search expansion counters (nodes generated
  / `checkmovement` calls / cache hit rate) behind a debug accessor, no
  behavior change. Baseline the BTFiles shortlist. Per AGENTS.md rule 4
  these numbers are measured, never asserted in tests.
* **B2 — unit steps + 0-cost + deque.** The real change (1–3 above).
  Expect fewer expansions; levels identical.
* **B3 — no-inverse pruning** (2 in proposal + 5 above). Measure
  incremental gain separately.

### Verification (B)

* `just test-all` incl. exact move-level assertions; `just check`.
* Expansion-count deltas per corpus puzzle (B1 baseline vs B2 vs B3).
* Wall-clock before/after via `bench/bench_solve.py --ab`, same `-O3`
  binaries, interleaved pinned runs.

### Risks (B)

* The 0-1 BFS rework of `disassemble_rec` is the delicate part; the
  grouping/`checkSubproblems` recursion above it is unchanged.
* Peak live-node memory may shift either way (fewer successors per node,
  but longer chains) — watch RSS on SolidSixPieceBurrs.

## Non-goals

* Touching the assembler search, voxel grids, symmetries, GUI widgets,
  or `src/lua`/`subprojects`.
* Changing reported disassembly levels or solution ordering.
* `thread_c` → `std::jthread` (separate concern, see std-replacements doc).

## Overall branch impact (vs pre-branch `e4a3644`, same-window A/B)

Measured with `bench/bench_solve.py --ab` (5 interleaved pinned runs) and
peak RSS per fresh process. Result counts identical everywhere.

### CPU

| Puzzle | Pre-branch med | Branch med | Delta |
|---|---|---|---|
| Excelsior (100% disassembly) | 2.47s | 3.27s | **+32% (regression, see below)** |
| Simplicity | 4.34s | 4.46s | +2.8% (noise) |
| kangaroo | 2.44s | 2.49s | +2.0% (noise) |
| SolidSixPieceBurrs | 10.31s | 10.12s | −1.8% (noise; 96% assembler anyway) |

Part C in isolation (pre-C vs post-C): −7% disassembly where measurable
(Excelsior −7.7%, Simplicity disassembly component −7.4%). Everything else
about the branch is perf-neutral within the box's ±7% noise floor (the
assembler, untouched, dominates most totals).

The Excelsior regression is real (non-overlapping ranges, reproduced across
three bench sessions): unit chains materialize every intermediate slide
position as an allocated, hashed, queued and expanded node where the old
search jumped point to point. Part C's cheaper per-node cost offsets a
quarter of it, not all.

### Memory (peak RSS, fresh process per run)

| Puzzle | Pre-branch | Branch |
|---|---|---|
| Simplicity | 11.5 MB | 11.5 MB (unchanged) |
| SolidSixPieceBurrs | 11.7 MB | 11.8 MB (unchanged) |
| Excelsior | 11.7 MB | **19.4 MB (+66%, +7.7 MB)** |

The Excelsior delta is the 0-1 BFS closed set: it retains every visited
node (~300 bytes/node all-in with `unordered_set` overhead), while the old
3-front scheme dropped old fronts (and paid re-expansions instead). Small
searches don't notice; at millions of visited states (hours/days puzzles)
this is ~300 MB per million — the knob to turn if such puzzles start
swapping (see options). Part C's parent cache is bounded separately
(≤4 MB by construction, KBs typical) and does not move these numbers.
(An earlier 452 MB reading for Simplicity was a `RUSAGE_CHILDREN`
accumulation artifact — max over all children in the process — retracted;
careful re-measurement gives ~11.5 MB on all three binaries.)

### Code simplicity (honest ledger)

Footprint vs branch point (`src/` only): roughly +450/−110 lines, plus
~90 test lines and the `bench/` harness. Removed: 3-front rotation,
multi-step escalation, hand-rolled hash tables (previous PR), `next`
intrusions. Added: 0-1 BFS machinery (deque + stale-skip + relaxation +
pop-order separation handling), state-99/duplicate advance rules,
(dir,set)-coalescing walk, incremental prepare + parent cache, move
statistics.

The core is genuinely more subtle than before: two cost models coexist
(waylength for search, states for reporting — equal by construction, but
only via the (dir,set) rule in *both* `newNode` and the tree walk), and
0-cost edges make BFS ordering non-obvious (Dijkstra pop-order return is
load-bearing). This doc carries the reasoning; the three traps in Part C
(fill-vs-closed base, fresh-fill-isn't-fixpoint, ABA) are written down so
they don't get reintroduced. Price of admission for the expansion-count
wins (CD_Pack 900k→143 nodes at search level) — but the wall-clock ledger
above is what it is.

### Visible behavior change (accepted)

Pelikan root level `98.2.4.2` → `101.2.4.2` (same tree, finer ruler —
three slides change drag set mid-slide). Goldens updated with justification
in `test_solver.cpp` + `test/python/test_burrtools.py`. Everything else
bit-identical.

## Further options (ordered)

1. **Excelsior regression** — the one red number. Hypotheses: unit-position
   visit volume (each slide position allocated/hashed/expanded) + fat
   same-distance frontiers from 0-cost dives. Ideas: bound the visited set
   with a 0-1-BFS-compatible window (research-y); reduce per-node cost
   further (see 2–3); accept (correct + usually faster-or-equal).
2. **Merge-dedup before allocation** — duplicate generation runs ~20:1 on
   merge-heavy searches (state-99 enumeration). Check the table for a merge
   candidate before `newNodeMerge` allocates+fills+hashes+deletes it.
   Est. ~10% of disassembly on such searches. Untouched.
3. **Closure first sweep** — Part C still runs one full sweep per hit
   (needed: the base is a fresh fill, not a fixpoint). A correct
   skip-first-sweep needs closed-state bases plus upward repair — designed
   and rejected as too complex for the gain; recorded here so it stays
   rejected for a reason, not by accident.
4. **`checkmovement` micro-opts** — 28% of disassembly across 13.5M tiny
   calls on Excelsior. B3 + memo already take the structural wins; what
   remains is instruction-level (profile-guided, matrix layout). Low
   priority.
5. **Assembler + Part A pool** — the assembler is 65–100% of most corpus
   totals and neither branch touches its search. The pool parallelizes
   across assemblies (throughput, not latency); assembler-search
   improvements are a separate project.
6. **PGO** (`-Db_pgo=generate/use`) — cheap experiment, branchy search
   code sometimes likes it. Not tried.
7. **Memory at scale** — if million-state searches swap: front-bounded
   closed sets compatible with 0-1 BFS, or NG-style SoA node storage
   (flat vectors + indices, no per-node `new`, interned moves — the one
   NG idea with proven structural merit). Both are big reworks; only if
   measured swapping demands it.

## Verification (overall)

Every phase: `just build`, `just test-all` (what CI runs), `just check`,
plus `just build-tsan` for part A. Benchmarks recorded in this doc's
corpus table before calling any phase done.

## B results (implemented)

Committed on `disassembler-optimizations`: B1 counters (`9a3a35b`), B2/B3
(`f56f492`), Pelikan golden + user-facing notes (`3422b5a`), test
ownership fix (`7e93717`).

### What changed

* `movementAnalysator_c::find()` state 2 generates unit steps only (the
  `nextstep++` escalation is gone; a duplicate success advances to the next
  piece — without that the state machine loops forever, found the hard way).
* `newNode()` takes waylength step 0 for continuations with the same stored
  direction AND same moved piece set, 1 otherwise (removals always 1).
* `disassemble_rec` is a 0-1 BFS (deque, distance labels in waylength,
  Dijkstra pop-order return for separations, stale-entry skip, replaceNode
  relaxation with requeue). The old 3-front scheme is gone. The table
  interaction is a single probe via `nodeHash::insertOrUpdate()`
  (NEW/KNOWN/UPDATED), not a `find()` plus `insert()`.
* State building in `checkSubproblems` coalesces (dir,set)-runs so a slide
  reports as one move; `movesText` counting is unchanged code.
* Inverse moves are skipped only for exact single-piece retraces (sound by
  the vacated-cells argument); everything else is left to the closed set.

### Move accounting change (Pelikan 98 → 101)

Same solution tree (identical `2.4.2` tail, valid by construction), finer
ruler: three root slides change their drag set mid-slide (a wedged piece
drops out after the first unit), so each counts as 2 moves instead of 1.
Goldens updated in `test/test_solver.cpp` and `test/python/test_burrtools.py`
with this justification. All other 263 test cases unchanged; full BTFiles
sweep: 85/85 solvable preserved, same 13 timeouts, same 2 crashes.

### Expansion counts (first-assembly search probe, checks/success/nodes)

| Puzzle | Before (B1) | After (B2/B3) |
|---|---|---|
| SolidSixPieceBurrs | 108 / 0 / 0 | 108 / 0 / 0 |
| PelikanBurr | 133 / 7 / 10 | 133 / 14 / 10 |
| Third_Times_the_Charm | 363 / 3 / 3 | 363 / 6 / 3 |
| elephant burr | 238 / 4 / 5 | 238 / 8 / 5 |
| Burrly Sane (Prof.) | 330 / 6 / 6 | 327 / 6 / 3 |
| Simplicity | 327 / 3 / 3 | 326 / 4 / 2 |
| Hog Wild 2 | 236 / 2 / 2 | 236 / 4 / 2 |
| kangaroo | 235 / 1 / 1 | 235 / 2 / 1 |
| detonator | 183 / 3 / 3 | 182 / 4 / 2 |
| hippo burr | 234 / 0 / 0 | 234 / 0 / 0 |
| Excelsior | 329 / 5 / 5 | 326 / 4 / 2 |
| CD_Pack | 1312180 / 1312053 / 900053 | 218 / 132 / 143 |

Notes: `success` roughly doubles on small searches from one redundant
re-check per new node (state 99 returns to state 2 with the same
piece/dir; the duplicate is dropped, one wasted `checkmovement`). CD_Pack
shows the mechanism working at scale: its slide-heavy first assembly went
from 900k drained nodes to 143. The probe drains one search level from the
root, so B3's deeper-level skips are invisible here — wall clock below is
the real measure.

### Wall clock (pre-B `2887eb5` vs B head, both `-O3`, 6 interleaved pinned runs)

| Puzzle | Before med | After med | Delta |
|---|---|---|---|
| SolidSixPieceBurrs | 10.30 | 10.33 | +0.2% |
| Third_Times_the_Charm | 6.65 | 6.35 | −4.5% |
| kangaroo | 2.50 | 2.62 | +5.0% |
| Simplicity | 4.40 | 4.46 | +1.2% |
| Excelsior | 2.67 | 3.46 | **+29%** |
| CD_Pack | 1.20 | 1.21 | +0.8% |

Excelsior is a real regression (ranges 2.33–3.15 vs 3.20–3.84, no overlap).
It is the most disassembly-dominated puzzle in the corpus (7 assemblies,
112 assembler iterations — nearly all time is search). Likely cause: unit
chains materialize every intermediate slide position as an allocated,
hashed, queued and expanded node where the old search jumped point to
point; on slide-heavy deep searches the extra node visits outweigh the
saved `checkmovement` calls. Open follow-up, not investigated further here.

## Where time really goes (profiled, BT_PROFILE instrumentation, since removed)

Assembler/disassembler split (`burrTxt` with vs without `-d`, problem 0):

| Puzzle | Assemble only | + Disassemble | Disassembly share |
|---|---|---|---|
| SolidSixPieceBurrs | 8.14s | 8.48s | ~4% |
| kangaroo | 1.40s | 2.16s | ~35% |
| Simplicity | 2.27s | 3.55s | ~36% |
| Excelsior | 0.00s | 2.65s | ~100% |
| CD_Pack | 0.96s | 0.93s | ~0% (noise) |

Takeaway: on most of the corpus the **assembler** dominates (65–100%).
Disassembly optimization cannot move those totals — part A (thread pool)
is the throughput lever there, plus future assembler work itself.

Disassembly internals (Excelsior, ~2.6s, per `movementAnalysator` instance):

* `prepare()` (pair matrices + Bill Cutler closure, rerun per expanded
  node): **1.70s, 65%**. Biggest lever; see proposal below.
* `checkmovement()`: **0.74s over 13.5M calls (~55ns each), 28%**.
* `newNode()` (alloc + fill): **0.14s over 927k calls** — allocation is
  NOT the bottleneck; no pool allocator needed.
* Duplicate generation ≈ 20:1 (507k generated vs 25k stored on the largest
  subproblem) — the state-99 merge enumeration mints mostly doomed
  candidates. Distance relaxations are rare (1151 updates, 0.2%).

Tried and measured on top of B (same-window interleaved A/B):

* Early return on 0-cost generated separations (Dijkstra certificate):
  **dropped** — measured neutral-to-negative (+3–17%, picks a different
  same-distance subtree with costlier subproblems). The pop-order return
  stays (required for 0-1 BFS optimality).
* `(piece,dir)` memo skipping the provably redundant post-merge re-check:
  **kept** (`035006d`) — newNode calls −36% on Excelsior, wall-clock
  neutral (±2%, inside noise), zero behavior change. (Cautionary note: an
  earlier comparison looked negative only because the instrumented build
  carried always-on `steady_clock` timers into the benchmark — ~40ns per
  `checkmovement` call × 13.5M calls. Instrumentation removed afterwards.)
* B3 inverse-retrace skip is invisible in single-level probes (needs
  non-root nodes) — its effect is inside the wall-clock numbers only.

## Concrete proposal from here

1. **Incremental `prepare()`** — the 65% lever. Consecutive expanded nodes
   (especially 0-cost chains) differ by one unit move of few pieces, yet
   `prepare()` refills all P² pair entries and reruns the full closure.
   Recompute only pairs involving moved pieces and propagate the closure
   incrementally. Medium-high complexity, touches hot math — needs its own
   design + validation against the corpus.
2. **Merge-dedup before allocation** — check the table for a merge
   candidate before `newNodeMerge` allocates+fills+hashes+deletes it.
   Smaller win (~10% of disassembly on merge-heavy searches).
3. **Part A pool** — for assembler-dominated puzzles disassembly work is
   a side show; parallelize across assemblies instead.
4. Watch item: Excelsior +29% under B (unit-position visit overhead).
   Revisit after (1): if incremental prepare lands, the per-node cost it
   multiplies shrinks, which directly attacks the regression.

## Part C — Incremental `prepare()` (design)

### Cost model (measured, Excelsior)

Per expanded node, `prepare()` (`movementanalysator.cpp:39-164`) does:

1. **Fill** — P² `getMoValue()` lookups (relative offset + orientations of
   each ordered piece pair). Mostly cache hits after warmup, but P² hashes
   per node regardless.
2. **Closure** — Bill Cutler's second pass per direction to fixpoint
   (`m[x][y] = min(m[x][y], m[x][i]+m[i][y])`): O(D·P³) per pass, several
   passes (`do/while(again)`).

Together ~65% of disassembly (~34µs/pop). Both phases redo work that is
identical to the previously expanded node whenever only k≪P pieces moved —
which is every 0-cost continuation and most sibling expansions.

### Design

Keep **one** previous matrix, not per-node matrices (per-node storage is
the memory blowup NG's streaming containers exist to avoid):

* Members: `prevMatrix` (same size as `matrix`), `prevSearch` (the node
  the previous `prepare()` ran for, **refcounted** — see soundness),
  `prevPieces` (the pieces-vector identity), `prevN`, plus reusable
  `dirtyRows`/`dirtyCols` bitsets (D·n each).
* `prepare()` fast path, taken when `searchnode->getComefrom() ==
  prevSearch && pieces == prevPieces && n == prevN`:
  1. `memcpy` prev → current (bulk, ~4KB for P=18 — ~1µs vs ~34µs).
  2. Diff moved set M (`nd` vs parent positions, O(P)); recompute via
     `getMoValue` only pairs touching M (2kP−k² lookups, not P²).
  3. Closure as a **dirty worklist**, not full sweeps: evaluate pair
     (x,y) iff its row or column saw a change; changed results re-dirty
     their row+column; flags only grow within a call; loop while anything
     changed. Over-evaluation is always safe; termination follows from the
     same bounded-decrease argument as the current loop. Converges to the
     identical least fixpoint (chaotic iteration).
* Else: today's full `prepare()`, unchanged code.
* After either path: snapshot matrix → prevMatrix, rotate the refcounted
  `prevSearch` (incRef new, decRef old — the standard dance, no ABA since
  the owner stays alive while referenced).

Deque locality makes the hit rate high for free: 0-chain children are
pushed front and popped immediately after their parent, so every
continuation hits; siblings miss and pay exactly today's price.

### Soundness (the three traps)

1. **ABA on the owner pointer.** Comparing raw pointers across sessions is
   unsound (freed node memory can be reused). Solved by refcounting
   `prevSearch`: the compared object is guaranteed alive.
2. **Stale matrix cells.** Diagonal cells are never written by any session
   (the `i != j` skip) and loops never read outside the current subset, so
   a same-subset `memcpy` base is exact; subset/identity mismatch falls
   back to full compute.
3. **Closure equivalence.** The worklist evaluates a superset of the
   triples the full sweep would use to propagate each change (any changed
   input permanently dirties its row+column for the rest of the call), so
   no propagation is missed; values only decrease, so it terminates at the
   same fixpoint.

### Validation

Development-only cross-check (removed before merge): run the incremental
path **and** the full path per `prepare()`, `bt_assert` matrix equality,
solve the whole BTFiles shortlist + `just test-all`. Ship only after a
clean run, then benchmark with `bench/bench_solve.py --ab` (expect the
Excelsior regression to shrink: per-node cost is exactly what it
multiplies). Threading note: all new state is per-`movementAnalysator_c`
members, so Part A workers (one analysator each) stay race-free.

Cross-check debugging found two real bugs before they could ship, both
documented here so nobody re-learns them:

* **Snapshot fill, never closed state.** The first version snapshotted the
  post-closure matrix. Closure only decreases, so a base value relaxed via
  a triple that no longer holds could never be repaired upward (first
  mismatch at the first grandchild expansion). The snapshot is the
  post-fill, pre-closure matrix on both paths.
* **Fresh fill is not a fixpoint — the first sweep must be full.** The
  first version skipped all triples whose row/column saw no fill change,
  but the memcpy'd base is parent *fill*, not a fixpoint, so unchanged
  triples still had relaxations pending. Now sweep 1 evaluates everything
  (identical to the full version's first sweep) and only later sweeps use
  the dirty worklist.

### Validation results

* Cross-check clean: `just test-all` with the check active, plus 7
  BTFiles/example puzzles solved with it active (Excelsior, Third_Times,
  elephant, kangaroo, Simplicity, CD_Pack, SolidSix, Pelikan) — zero
  mismatches over millions of `prepare()` calls.
* `just test-all` 3/3 + `just check` clean on the final code; all
  regression levels identical (Pelikan still `101.2.4.2`).
* Wall clock, pre-C vs post-C same-window interleaved A/B (median of 5,
  pinned). Part C cannot affect the assembler, so assembler deltas are
  pure noise floor (±7% on this box: SolidSix asm-only 9.30 → 9.98):

| Puzzle | Disassembly component (total − asm-only) | Delta |
|---|---|---|
| Excelsior (100% disassembly) | 3.14 → 2.90 | **−7.7%** |
| Simplicity | 1.98 → 1.83 | **−7.4%** |
| CD_Pack / kangaroo / Third_Times / SolidSix | ≤0.6s components or assembler-dominated | inside noise |

* Consistent −7% where disassembly dominates. Modest vs the 65% prepare
  share because the single-previous hit rate only covers 0-chain children,
  not siblings (same parent, popped later).
* Follow-up (not done): small LRU of parent matrices keyed by parent
  pointer instead of one previous — siblings share a parent and pop
  consecutively in FIFO order, so they'd hit too. Needs refcount care per
  entry; same soundness arguments as here.
* Follow-up DONE, reverted: parent-matrix LRU, sizes 8→1024 (own design,
  nothing from bt2 — NG has no equivalent). Hit rates climb nicely
  (Simplicity 18%→43%→65%→86%→99%, Excelsior 5%→8%→12%→21%→27%→71%),
  touch-on-hit HURTS (58% vs 65%, replacement dynamics), ring buffer
  instead of move-to-front list — but wall clock gets WORSE at every size
  (+15–37% same-window: Excelsior +30%, Simplicity +15%). Diagnosis: per
  hit, the saved getMoValue lookups (~25ns each, L1-resident) are smaller
  than the added machinery — 1024-entry scans stream ~48KB of headers per
  prepare and evict the matrix working set from L1D, plus two full-matrix
  copies per hit. The first closure sweep (the dominant cost) is not saved
  by hits anyway. Lesson: hit rate without cost-per-hit accounting is
  meaningless here. Stayed with single-previous (−8% real); the LRU code
  was fully reverted (cross-check validated at every step, then removed).
