# Rotation Moves — Optimized Paths and the Play a Puzzle Needs

**Date:** 2026-10-25
**Status:** Implemented on branch `rotations` (planner, planned paths,
opt-in fallback in the search `burrTxt -Q`, `burrTxt -P`, `burrTxt -M`).
The planner is off by default because of its cost (see "Cost").
**Scope:** `src/lib/rotationgeometry.*`, `src/lib/rotationtables.*`,
`src/lib/movementanalysator.cpp`, `src/lib/disassembler_a.cpp`,
`src/lib/disassembly.*`, `src/burrTxt.cpp`, `test/test_rotation_planner.cpp`,
`test/test_rotation_disassembly.cpp`, `design/rotation-study/`
**Builds on:** [`2026-10-19-rotation-moves.md`](2026-10-19-rotation-moves.md)
(its phase 3, "configuration-space planner"). Read that first for the
physical model, the certified checker, the path family and the tables.

## Problem

Andrew Crowell's collection (`puzzles/BTFiles/Andrew Crowell/`, not in the
repository) ships solution files: for most puzzles `X.xmpuzzle` has a
companion `X_SolutionFile.xmpuzzle` whose problem holds one "solution"
per step, each an assembly snapshot of every piece's position and
orientation (`x` for a removed piece). Consecutive snapshots give every
step exactly, turns included. Two quirks: a snapshot sometimes shows a
piece only part of the way out, and a removal can share a snapshot with
the next move of another piece (coordinates are renormalized after a
removal).

`design/rotation-study/scripts/xcheck.cpp` compares such a file with our
disassembly of its first snapshot, state by state, modulo rigid motions of
the whole puzzle (every configuration is moved so that its lowest piece is
back in its assembly pose). Results on all 150 solution files (summary
lines in `results/crowell_xcheck_summary.txt`):

* Where we take a puzzle apart, most snapshots are exactly states of our
  disassembly; 17 files match every snapshot (BT_TIC, EclipTIC, EcstaTIC,
  EmphaTIC, KeyTIC, tRIBes, RIPple, Cluster, BrainiAC2, ...). Low match
  rates (3KingsTIC 7/39, MagellanTIC 22/53) are different but valid paths.
* 18 puzzles do not come apart from Crowell's assembly. Running our
  disassembler from every snapshot (`FROM=1 PARTIAL=1`) finds the one step
  we cannot make: the first snapshot we can solve from comes right after
  it. QuarTIC's solution file stops after 5 snapshots, all pieces still in;
  the other 17 steps are listed in `results/crowell_blocked_steps.txt`
  (file and snapshot index k, the step is k-1 -> k).

`design/rotation-study/scripts/diag.cpp` then dissects each step. Every
one is a single quarter turn; the pivot lies in the group's footprint,
there is no axial shift, the group is within the tables' radius: no limit
of the generator is involved. Our path family blocks all of them at the
default play 0.081. A configuration-space grid search (shifts up to 0.5
cells) finds motions for some of them at that play; the rest need a
little more:

| Step (snapshot k, turning pieces, axis) | Family clears from play | Play needed (planner, `requiredPlay`) |
|---|---|---|
| GemTIC 15, f +X | 0.100 | 0.0063 |
| TriumviraTIC 10, b +X | 0.100 | 0.0079 |
| HolopTIC 11, d −Z | 0.100 | 0.0266 |
| TIC_Box_III 23, b +Z | 0.100 | 0.0422 |
| TemplarCross 13, b −Y | 0.100 | 0.0551 |
| PackTIC_3 21, d −X | 0.100 | 0.0569 (not below 0.0409) |
| PackTIC_5 4, bdeg −X | 0.100 | 0.0572 |
| CagedCubeJr_4 6, e +Y | 0.100 | 0.0641 |
| TritanIC_2 8, c +Y | 0.100 | 0.0701 |
| MysTIC 11, cd −Z | 0.100 | 0.0801 (0.0766 on a 0.004 grid; not below 0.0489, 0.0645) |
| SyntheTIC_3 8, bcd −Y | 0.100 | 0.0836 |
| TacTICian 12, b −Z | 0.100 | 0.0843 |
| HypnoTIC 3, c −Y | 0.120 | 0.0856 (not below 0.0568) |
| TritanIC_6 19, c −X | 0.100 | 0.0859 |
| CagedCubeJr_2 16, e −Z | 0.100 | 0.0866 |
| CagedCubeJr_1 12, d −X | 0.100 | 0.0913 |
| StarTIC_1 15, cf −Y | 0.120 | 0.0915 |

("Play needed" on the default planner grid, 0.01 cells and 0.5°; lower
bounds where computed. CagedCubeJr_1 and TemplarCross come apart in a
normal run anyway, on another assembly than Crowell's.)

Two findings:

1. **The family is too coarse near the threshold.** The paths that get
   through are asymmetric (most of the shift early, a slow return), point
   in non-compass directions, and some hold a shift for most of the turn
   (MysTIC: ramp, hold, ramp). The bulge wiggle is symmetric about 45° and
   only goes in 8 directions. This matches the ThreeTIC study of
   2026-10-21 in the parent doc.
2. **These turns sit right at the play.** A yes/no answer at one play is
   the wrong question for them. Whether a physical copy comes apart
   depends on its actual play, which the solver does not know. "This
   puzzle needs at least play 0.085" is an answer a designer can act on.

## Goal

Approximate the play a puzzle needs to come apart in reality: per turn,
the smallest play at which *some* motion of the group gets through
(optimize the path for clearance instead of trying a fixed family); per
disassembly, the largest of its turns' plays; per puzzle, the smallest
play at which there is a disassembly at all. And on the way, find the
turns the family misses at the default play.

## Clearance and play

For sharp cubes, adding play `Δ` takes `Δ/2` off every face. A square of
half side `h − δ` is the square of half side `h` shrunk by `δ` (an inward
offset), and for two convex sets shrunk by `δ` each,
`sd(A ⊖ δ, B ⊖ δ) ≥ sd(A, B) + 2δ`. So along any fixed path

    clearance at play p + Δ  ≥  (clearance at play p) + Δ.

A path with smallest clearance `c < 0` at play `p` is therefore clear at
play `p − c`. **The direction matters:** taking play away can cost more
than `Δ`, because a grown square's corners stick out beyond the offset
shape. (The first version used the conversion both ways and reported
TriumviraTIC as needing play 0.004; the path was blocked there.) So a
positive clearance only says "possible at this play", and the play a
turn needs is found by bisection. The test "more play adds at least as
much clearance along a path" checks the inequality on random scenes.

## Design (as built)

### 1. Clearance-optimal turn planner (`rotationGeometry::planTurn`)

Input: a `planarScene_c` (moving and static cubes, axis, pivot, model), a
sense and `planSettings_t`: shift box `|du|, |dv| ≤ maxShift` (0.5
cells), grid `shift × shift × 90°/steps` (0.01 cells, 180 steps), `floor`
(cells with less clearance are never entered), `upper`, `widest`,
`budget`.

* **Impossibility proofs first.** Every continuous path passes every
  angle; if at some angle no shift in the box reaches `floor`, nothing
  does. First the pure turn with the Lipschitz bound
  (`clearance(θ, 0) + √2·maxShift < floor`), then a quadtree over the shift
  box at each angle, worst angles first (`sliceBlocked`). An open angle
  costs a sample or two; a blocked one is found within the first few.
  (A coarse optimistic grid search before the fine one made no
  measurable difference once the slice proof existed and was removed.)
* **Widest path.** Max-heap Dijkstra over grid cells, a cell's value being
  the clearance at its centre: the path from start to end pose with the
  largest smallest value. It expands only cells at least as good as the
  answer.
* **Fast mode** (`widest = false`, what the search uses): a goal-directed
  search (furthest angle first) through cells at least half a cell margin
  above `floor`; if that finds nothing, the widest path down to `floor`
  itself. Both are capped at `budget` (60,000 evaluated cells); a turn
  exceeding it is given up (`turnPlan_t::gaveUp`, reported as
  `rotationContext_c::SKIP_PLANNER`, a burrTxt note). MysTIC's turn keeps
  only a few thousandths of a cell at 0.081 and needs the second search.
  Budgets of 20,000 / 8,000 lose MysTIC / also TritanIC_2 for 20–40% less
  time.
* **Certified value.** The cell path is shortcut, then its minimum
  clearance is computed on the continuous polyline by Lipschitz branch
  and bound (`planarScene_c::minClearance`, within 10⁻⁴;
  `clearanceAtLeast` for a yes/no). The polyline is what is recorded and
  animated, so the value certifies the actual motion.
* **Optimistic bound** (`upper = true`): the widest path over
  `centre + margin` values (margin = largest displacement within a cell,
  `h/√2 + ρ·Δθ/2`) with diagonal neighbours: no motion within the box has
  more clearance.
* **Early exit:** below `floor` a cell's exact value does not matter
  (`planarScene_c::clearanceAbove`).
* **Buffers:** dense per-thread arrays over the grid, only touched entries
  reset (`gridBuffers_t`).
* **Reverse:** `reversePath` gives the same motion seen from the end.

**Required play of a turn** (`rotationGeometry::requiredPlay`): the best
path at the model's play gives a bracket (if blocked by `c < 0`, that path
is clear at `play − c`); then fast-mode feasibility halves the bracket to
0.0005. The lower end comes from the conversion: with optimistic bound
`u` at the play found, nothing gets through below `needs − u`. The lower
bound is loose (the optimistic search pays the full cell margin, about
0.03 on the default grid); a finer grid tightens both.

### 2. Path kind `PATH_PLANNED`

`pathSpec_t` gained `PATH_PLANNED` and `waypoints`. `makePath` returns
them (and checks they end at the quarter turn), so `poseAt`, the
animation, `-p` and the replay tests work unchanged. Saved with 17
digits, so the loaded polyline is bit for bit the certified one:

```xml
<rotation axis="0" pu="4" pv="2" path="planned" sense="1" pieces="1"
          waypoints="0 0 0 -0.0873 0.12 0.03 ... -1.5707963267948966 0 0"/>
```

The loader checks the waypoints run from the identity to the quarter
turn. No version bump: rotation moves are not in a release (AGENTS.md
"Compatibility").

### 3. Planner fallback in the search (opt-in)

`movementAnalysator_c::listRotations` keeps the family as the fast path.

* **Which turns get planned.** A sound filter would need "blocks on every
  motion within the box" as the pairwise relation that builds groups. With
  shifts up to 0.7 cells that blocks almost nothing beyond the end cell,
  so nearly every union of pieces becomes a candidate: tens of thousands
  per search node in a 6-piece puzzle. Instead the planner runs on turns
  the family clears at `play + slack` but not at the play itself, and
  groups come from the relation of those relaxed tables
  (`rotationContext_c::closureTables`, built in `rotationContextFor`). The
  relaxed relation blocks less than the real one, so no family turn is
  lost. **A heuristic:** a turn the family cannot do even with `slack`
  more play is never planned. The 10 Crowell misses possible at 0.081 are
  all cleared by the family at 0.100, so the default slack is 0.02. (0.04
  also reaches the turn that saves AAA_most_rotations a move, at about
  twice the cost; 0.01 still takes HolopTIC and MysTIC apart.)
* **Cache, symmetric.** `rotationContext_c::plannedPath` keys a turn by
  its scene: sense, moving cubes and the static cubes within reach, as
  offsets from the pivot (layers relative to the lowest moving one), so
  the key is independent of axis and position. A turn and its reverse
  both use the smaller of their two keys and plan from that side, storing
  the path and its reverse. So an edge exists in both directions or in
  neither, which the three-front breadth-first search needs. Shared by
  all workers (mutex); results depend only on the key, so two workers
  planning the same key get the same.
* **Recording.** Node path indices at or beyond the family's size name a
  planned path in the context's store; `rotationContext_c::pathSpec`
  resolves both kinds (used by `disassembler_a_c` when building states).
  A planned turn of a group with piece 0 is stored as the complement
  following the path inverted, like family wiggles and slides.
* **Settings.** `rotationSettings_t::{planner = false, slack = 0.02,
  plan}`. On with `burrTxt -Q`, and with `-M`.
  `BURRTOOLS_NO_ROT_PLANNER=1` turns it off where it is on (AGENTS.md
  env table). The GUI does not use it yet.

### 4. Required play of a disassembly (`burrTxt -P`, `optimizeTurns`)

`optimizeTurns` (rotationtables.cpp) runs `requiredPlay` on every turn
of a found disassembly, the scene rebuilt from the state before the turn
(the turning pieces' own motion, `turnSense`). It replaces each turn's
recorded path by the widest path at the model's play (inverse cleared):
the motion with the most room, the most realistic one to animate. The
level line gets the largest turn's play:

    level: 10.3.1.2, 3 rotations, needs play 0.0801 (not below 0.0489)

Every turn of that disassembly is proven possible at the printed play and
translations need none, so it is a **proven upper bound on the play the
puzzle needs**. "not below" bounds that disassembly's hardest turn only,
not the puzzle. Slow (seconds per turn); only reported disassemblies are
optimized. `separation_c::editState/editLeft/editRemoved` were added for
this.

### 5. Minimum play of a puzzle (`burrTxt -M lo,hi`)

Bisects the model's play (tolerance 0.001) over whole quiet solves
(`comesApart` in burrTxt.cpp), then solves and prints at the play found.
More play only adds turns (up to the planner's resolution, budget and
slack), so the bisection is meaningful, but it gives an upper bound.
With `-P` it tightens: HypnoTIC, `-M 0.02,0.15` finds 0.0896; the
disassembly found there needs only 0.0823 (the fast planner misses some
of its turns below 0.089, `-P` proves them all at 0.0823). So HypnoTIC
needs at most 0.0823. Each bisection step is a full solve with its own
tables (a few minutes total on the larger puzzles).

Not modelled, and worth saying with any number: flexing and wear,
friction, imperfect alignment, axial wobble during a turn. The numbers
are the play of ideal rigid cubes.

## Results

### The 17 puzzles, whole runs

`burrTxt -d -R` against `-d -R -Q` (release build, 8 threads, idle
machine):

| Puzzle | without planner | `-Q` |
|---|---|---|
| TriumviraTIC | none, 0.13 s | apart, 0.55 s |
| MysTIC | none, 0.13 s | apart, 0.65 s |
| PackTIC_3 | none, 0.13 s | apart, 0.83 s |
| PackTIC_5 | none, 0.13 s | apart, 0.80 s |
| TritanIC_2 | none, 0.20 s | apart, 2.4 s |
| CagedCubeJr_4 | none, 0.21 s | apart, 2.4 s |
| HolopTIC | none, 0.20 s | apart, 3.5 s |
| TemplarCross | 1 solution, 0.21 s | 2 solutions, 0.85 s |
| GemTIC | none, 0.21 s | apart, 0.78 s |
| TIC_Box_III | none, 0.24 s | apart, 4.0 s |
| CagedCubeJr_2 | none, 0.21 s | apart (another assembly), 1.3 s |
| CagedCubeJr_1 | 2 solutions, 0.21 s | 2 solutions, 2.5 s |
| HypnoTIC, TacTICian, TritanIC_6, SyntheTIC_3, StarTIC_1 | none | none (they need 0.084–0.092) |

### Cost, and why the planner is opt-in

Whole Crowell collection (319 files, slack 0.04, single thread, 300 s
limit): with the planner 302 files come apart against 280, none lost
(CornerCube only timed out: 1952 assemblies, 78 s with 8 threads). The
wall times of that run are useless, the laptop slept during it. Sample on
an idle machine, single thread, slack 0.04: HepTIC 0.12 → 0.6 s,
FantasTIC 0.13 → 1.1 s, NervousTIC 0.13 → 5.9 s, HolisTIC 0.23 → 7.8 s,
RIB 0.8 → 31 s, 3KingsTIC 0.25 → 54 s, MagellanTIC 0.27 → 53 s. With
slack 0.02: MagellanTIC 29 s, 3KingsTIC 11 s, NervousTIC 4.6 s. That is
5 to 200 times the family-only search, for the few percent of puzzles
near the threshold.

perf (AAA_most_rotations): half the time in cell clearances, a quarter in
the grid search itself; the rest of the cost is budgeted searches of
turns that are impossible but not at a single angle (no slice proof).
AAA_most_rotations with the planner (slack 0.04) comes apart in 2.3
instead of 2.4 moves.

## Validation

`test/test_rotation_planner.cpp` (fast suite, about 1 s in the dev
build), on two Crowell turns embedded as cells (dumped with
`diag.cpp DUMP=1`):

* PackTIC_3's turn: the family is blocked; widest path and fast mode find
  certified paths; the certified value is below every sampled clearance;
  the reversed path certifies on the reversed scene with the same value.
* MysTIC's turn: the fast mode (default grid) finds the path with less
  than 0.01 to spare.
* A cube boxed in by a ring of cubes: no path, and a proof (`upper < 0`).
* `requiredPlay` on PackTIC_3: within range, lower bound below it, path
  certifies at the play found.
* More play adds at least as much clearance (random scenes and paths).
* A planned path survives save and load bit for bit.

`test/test_rotation_disassembly.cpp`: the AAA_most_rotations disassembly
is built with the planner (slack 0.04, helper `withPlanner`), expects
level 2.3 and at least one planned turn, and its replay and animation
tests now cover a planned path.

All gates passed on 2026-10-25: `just build`, `just test-all`,
`just check` (which needed default member initializers in `model_t`,
`pathSpec_t`, `stateRotation_t`, `rotationSettings_t`, `cacheEntry_t`;
those `uninitMemberVarNoCtor` warnings already failed on the branch
before), `just build-release`, `just test-release`, `just test-regression`
(20/20).

## Reproducing

Puzzles are not in the repository; `puzzles/BTFiles/Andrew Crowell/`
next to the repository root. Build the helpers against the release
library (`just build-release` first):

```bash
c++ -I build-rel -I src -I subprojects/manifold-3.5.2/include -DNDEBUG -std=c++20 -O2 -pthread \
    design/rotation-study/scripts/xcheck.cpp -o xcheck \
    -Wl,--start-group build-rel/libburr_core.a build-rel/subprojects/manifold-3.5.2/libmanifold.a -lz -Wl,--end-group
# same for diag.cpp
./xcheck ".../HepTIC_SolutionFile.xmpuzzle"            # snapshot vs state match
FROM=1 PARTIAL=1 ./xcheck ".../MysTIC_SolutionFile.xmpuzzle"   # blocked step
./diag ".../MysTIC_SolutionFile.xmpuzzle" 11           # family vs grid search over plays
REQ=1 ./diag ...; PLAN=1 [FEAS=1] ./diag ...; DUMP=1 ./diag ...
build-rel/burrTxt -d -R -Q -P puzzle.xmpuzzle
build-rel/burrTxt -d -R -P -M 0.02,0.15 puzzle.xmpuzzle
```

`diag.cpp` also has `FINE=h NTH=n W=w [PLAY=p]`, the standalone grid
search used for the first diagnosis. Its `PLAN`/`REQ` modes call the
library.

## Open

* **Cost**, to make the planner a default: fewer candidates (a sound
  filter would also remove the slack heuristic), a cheaper impossibility
  proof for turns blocked by connectivity rather than at one angle, an
  incremental clearance along the grid's angle axis.
* **Lower bounds** are loose; refining only cells whose interval
  straddles the answer would tighten them cheaply.
* **GUI:** a "plan turns" option next to "Rotation Moves" (its "add
  disassembly" runs on the UI thread, so it needs the background solver
  first), and showing `-P`'s play per solution.
* **The decoded TICs** (`puzzles/TICs/decoded`) against their printed
  models with `-M -P`: do the printed plays agree with the numbers?
* `-M` re-solves from scratch per step; the planner cache is per solve.
* `test_rotation_disassembly.cpp` piece-order and closure tests run with
  the family only (the planner is off by default); planner order
  independence is argued (scene-keyed cache) and tested only via the
  reverse-path symmetry.
