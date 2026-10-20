# Rotation Moves — Validation and Redesign

**Date:** 2026-10-19
**Status:** Proposal; phase 1 (geometry core + validation tests) done
**Scope:** `src/lib/rotation*.{h,cpp}`, `src/lib/movementanalysator.*`,
`src/lib/disassemblernode.h`, `src/lib/disassembly.*`,
`src/lib/disasmtomoves.*`, `src/gui/voxelframe.cpp`, `test/`
**Branch:** `rotations` (based on `master`). PR #50's rotation code
(`colin-features`, 60 commits behind master) is not on master; it is
ported piece by piece on top of the geometry core instead of rebased.
The audit of its rules lives on branch `rotations-pr50`.

## Problem

PR #50 adds 90° rotation moves to the disassembler. Grssam reported
(PR #50 comments, Clutch 4) that

1. the GUI shows rotating pieces cutting through other pieces mid-turn,
2. moves are reported that a physical copy cannot make, because the solver
   treats cubes as if they had heavily rounded edges, and
3. the "default" and "Crowell" rotation solvers disagree (0 vs. 1 solution).

## Measurement

`rotationRules_c::allowRotation` was compared against a dense-sampled
(2000 steps) exact 2D overlap test on ~9.7k random candidates (face-connected
moving group of 2–5 cubes, random static cubes, every pivot/axis/sense, end
pose free):

| Outcome | Cases |
|---|---|
| Rules allow | 7695 |
| Allowed, but full-size sharp cubes overlap during the pure rotation (up to 0.48 deep) | 812 |
| …of which only a mid-turn "wiggle" translation avoids the overlap — wiggle not recorded, not animated | 297 |
| …of which only the 0.0405/side cube shrink avoids it | 336 + 170 |
| …of which the 23-step sampling tunnels through a corner (fails even shrunk and dense) | 9 |
| Rules reject a move that full-size cubes can do by pure rotation | 0 |

So the 1. symptom (GUI interpenetration) is mostly a *recording* problem:
`arcClear` accepts a move if a pure rotation **or** one of 24 fixed
"wiggle" paths (rotation + `dshift·sin 2θ` in-plane translation) is clear,
but `setRotationInfo` only keeps the pivot, and `disasmToMoves_c` animates a
pure `glRotatef` about it. The 2. symptom is the implicit, hard-coded cube
shrink (`BEVEL_REMOVE = 0.0405`, i.e. 0.081 total play between neighbours)
plus the coarse sampling.

## Assessment of the PR #50 approach

Worth keeping:

* The integration point: a third phase in `movementAnalysator_c::find`,
  rotation edges in the disassembler graph via `dir >= ROTATION_DIR_BASE`,
  orientation (`trans`) as part of the node.
* The GUI plumbing for an animated tumble and the "Debug Rotations" overlay.
* `AAA_most_rotations.xmpuzzle` as regression puzzle.

Not worth keeping:

1. **Feasibility is a stack of heuristics, not a geometric model.**
   Sandwich, perpendicular-plane capture and axis-cross are hand rules
   ported from Fortran `SimpleRot`; the actual sweep is a sampled 2D test
   with a magic cube shrink and three magic wiggle amplitudes
   (`0.1498…`, `0.2321…`, `0.378680`). Nothing states what physical object
   is being modelled, so nothing can be validated against physics, and every
   disagreement is a debate about constants.
2. **The motion that was checked is not the motion that is stored.**
   The wiggle is discarded, so the animation cannot be faithful by
   construction.
3. **The motion family is arbitrary.** Pure rotation about a half-grid pivot
   plus 24 fixed wiggles. Real moves combine rotation with arbitrary small
   translations ("shimmy"); anything outside the 25 paths is silently
   missed, anything inside them is accepted only up to sampling.
4. **Two near-copy generators** (`rotationmoves_0.cpp`,
   `rotationmoves_crowell.cpp`, ~900 lines) differ in pivot sets,
   a per-direction cap (`DCT_MAX`, makes the search incomplete), reverse
   skipping, and in whether the largest piece may move. Their disagreement
   on Clutch 4 is an artefact of these filters, not of physics.
5. **State duplication.** Node equality is translation- but not
   rotation-invariant. Rotating group *S* and rotating its complement the
   other way yield the same physical configuration but different nodes;
   the default generator enumerates both (Crowell avoids it by never moving
   the largest piece).
6. **Hot-path cost.** Per candidate: `getenv` (up to 6×), `std::set`
   rebuilds, `collectWorldCells` for every piece, subsets × O(bbox²) pivots
   × 6. No cache of anything.
7. **Untested conventions.** The stored end state is
   `sym->transAdd(trans, ROT_*)` plus a rotated hotspot; the checked end
   state is `rotateCell` of the start cells. Nothing tests that they agree.

## Proposed design

### Exact reduction to 2D

A rotation about an axis-aligned line, combined with translation
perpendicular to it, keeps every cube in its axial layer. Cubes in
different layers can only touch on a face plane, never overlap. A
**planar rigid motion** of group *M* among static cubes *S* is therefore
collision-free iff, for every axial layer, the moving squares of that layer
avoid the static squares of that layer. This is exact, not an
approximation. Axial translation during the turn is excluded (it is
composable from separate translation moves in all but contrived cases).

### Physical model (explicit, two parameters)

Every cube is a real, slightly undersized object:

* **play** `g` — the cube has side `1 − g` and sits centred in its grid
  cell, so neighbouring cubes have a gap `g` (clearance of real pieces);
* **edge rounding** `r` — the edges parallel to the rotation axis are
  rounded with radius `r` (a rounded square = core square of side
  `1 − g − 2r` ⊕ disk `r`).

There is no overlap tolerance: a configuration is valid iff for every
same-layer pair (moving, static) `signedDistance(core_m, core_s) ≥ 2r`.
`signedDistance` of two convex polygons is exact and cheap: SAT minimum
overlap when they intersect (this *is* the penetration depth for convex
polygons), minimum vertex–edge distance otherwise.

`g > 0` is required and enforced (`bt_te`): with `g = 0` the start pose
already has cubes in face contact. A cube turning about a corner it shares
with a diagonal neighbour touches it during the whole turn, so no finite
check can prove it clear. The checker would subdivide down to its minimum
step, which took 111 s for one table in a measurement.

PR #50's implicit model is `g = 0.081, r = 0`. The defaults are to be
calibrated against physically verified moves (see Validation); they are
solver settings and are reported with every result, never hidden constants.

### Certified path checker (the single source of truth)

A path is a piecewise-linear curve in configuration space
`q = (θ, du, dv)`: moving point `p ↦ R(θ)(p − c) + c + d`, `c` a fixed
reference point (the group's in-plane bounding-box centre). Along a linear
segment of parameter length 1, no point moves more than
`L = |Δd| + ρ·|Δθ|`, `ρ` = max distance of a moving vertex from `c`.
Signed distance is 1-Lipschitz in point displacement, so

* if `sd(s) − 2r ≥ L·δ` at a sample `s`, the whole interval
  `[s−δ, s+δ]` is valid;
* if `sd(s) < 2r` at a sample, the path is invalid;
* otherwise bisect, down to a minimum step, below which the segment is
  rejected (conservative).

This gives a **sound** verdict for any path — no tunnelling, no step-count
constant. It is used by the generator, by the tests, by `burrTxt
--verify-rotations`, and by the GUI's contact highlighting.

### Lookup tables for fixed paths (`pathTable_c`, `rotationTables_c`)

Along a *fixed* motion every (moving cube, static cube) pair collides or
not independently of all other cubes: clearance is the minimum over
pairs. So for a fixed path the feasibility test needs no geometry at run
time. A table keyed by the moving cube's in-plane offset from the pivot
lists the static offsets that block it, and the path is blocked iff a
moving cube finds one of its blockers occupied in its own layer. Filled
by the certified checker, the table is *exactly* as precise as the
checker. It is not an approximation.

* Keys: doubled in-plane offsets relative to the pivot, both even for a
  pivot at a cube centre, both odd for one at a cube corner. Edge-midpoint
  pivots cannot map the lattice onto itself by a quarter turn. One table
  serves all three axes, because the in-plane frames are chosen so that
  positive angles are right-handed for every axis.
* Which static cubes to try: a cube can only be hit if its centre is
  within `√2·(1 − g) + shift` of the moving centre's pure arc. A cube at
  offset `(r, r)` swings out to `r·√2` along an axis, so the search must
  reach beyond the table's square of moving offsets. The first version
  stopped at `r + 2` cells and missed blockers for `r ≥ 5`; a test now
  compares the outermost entries of a radius-8 table with an unpruned
  brute force.

**Rotate-and-shift paths.** PR #50's wiggle done right: a fixed family of
paths, one table each, exact for exactly the motion that is recorded and
animated.

* `PATH_PURE`: the quarter turn about the pivot.
* `PATH_WIGGLE`: the turn with an offset `a·sin(πt)` along a compass
  direction, a polyline of 16 segments (the polyline *is* the motion).
* `PATH_SLIDE`: shift by `a`, turn about the shifted pivot, shift back.

`rotationTables_c` orders them by disturbance: the pure turn, then per
amount `a` (ascending) the 8 wiggles and the 8 slides.
`firstClear(...)` returns the index of the first clear path, which is
what a move records (`pathSpec_t` names the path; `makePath` rebuilds the
waypoints for checking and animation).

Only the +90° tables for directions (1,0) and (1,1) are computed with the
checker; the other 6 directions are `turned()` copies (turning the whole
scene about the pivot commutes with the motion), and all −90° tables are
`mirrored()` copies. Tests compare derived tables with directly computed
ones.

Measured with `./build/test_burrtools "[rotations-table-stats]"` (dev
build, amounts {0.1, 0.2, 0.3, 0.4}, 65 paths per sense):

| radius | build | entries, pure turns | entries, all paths |
|---|---|---|---|
| 6 | 0.5–0.6 s | ~11k | 0.75–0.82M |
| 8 | 1.3–1.4 s | ~25k | 1.7–1.9M |

On 19,086 random candidate turns with a free end pose (radius 6),
13.3–13.7k are possible as pure turns and shifted paths add 1,250–1,680
(7–9% more moves, for play 0.081, 0.02 with rounding 0.05, and 0.01).
Every kind and amount is the first rescuing path for some of them, so the
default family keeps all four amounts. The random scenes are sparse; a
corpus measurement on real puzzles (dense) should confirm the choice.

Run-time cost per candidate move: Σ blockers over the moving cubes per
path, ≤ 30–46 lookups per cube and path, paths tried in order until one
is clear. This maps directly onto bitboards: OR the moving cubes' shadow
masks, AND with the occupancy of the other pieces. A per-(shape,
orientation, pivot offset, axis, sense, path) shadow cache, like
`movementCache_c`, removes even the OR. Memory is ~8 bytes per entry
(15 MB at radius 8), also a reason to move to bitmasks.

What tables cannot do: moves whose path is *searched for*. The family is
a deliberate choice of plausible hand motions; the planner below remains
the fallback for anything else.

### Solver integration (`rotationtables.{h,cpp}`)

Rotation moves are optional and cost nothing when off:

* `solveThread_c::PAR_ROTATION_MOVES` (with `PAR_DISASSM`) and
  `burrTxt -R` switch them on. Without the flag no tables are requested,
  every disassembler gets an empty pointer, and the (future) rotation
  phase of `movementAnalysator_c::find` is skipped by one null check per
  search node.
* Tables are obtained in the solver's constructor, before any worker
  thread exists, and then only read; workers share one
  `shared_ptr<const rotationTables_c>` handed down
  `disassemblerPool_c` → `disassembler_0_c` → `disassembler_a_c` →
  `movementAnalysator_c`. No locks, no `call_once` on the hot path.
* Not a singleton: `rotationTablesFor(problem, settings)` serves tables
  from a cache of `weak_ptr`s keyed by the settings (model and shift
  amounts). Concurrent and repeated solves share one build; the tables are
  freed when the last solver releases them; a larger radius also serves
  smaller requests. Different puzzles may use different models, and tests
  can build small tables without fighting global state.
* `rotationSettings_t` (model + amounts) is a solver argument, default
  `defaultRotationSettings()`. The GUI will later take play and rounding
  from the puzzle's physical description.
* The radius comes from the problem: largest result-shape dimension + 1.
  Only cube grids with a valid result get tables.
* Cost when on: building the default family took ~1.8 s for PelikanBurr
  in the dev build, paid once per settings while any solver holds them.
  The GUI must not build on the UI thread once it offers the option.

### Move generation: endpoints first, then paths

For a moving group *M*, axis *a*, quarter-turn sense *s*, the end
orientation is fixed; the end translation `d` must put every cube on the
lattice (`d ∈ d₀ + ℤ²`) and the end pose must be free. Each such lattice
`d` corresponds to exactly one pure-rotation pivot, `d = (I − R)(P − c)`,
so enumerating endpoints subsumes the pivot enumeration of both current
generators.

For each free endpoint, in order of cost:

1. **Path family** about its pivot — `rotationTables_c::firstClear`
   lookups. The pure turn covers the large majority of moves; the shifted
   paths add the shimmy moves.
2. **Planner** (optional, setting): search configuration space
   `(θ, du, dv)` for any path. Lazy BFS/A* over a grid of cells of size
   `h × h × Δθ`. A cell is free if its centre configuration has clearance
   `≥ h/√2 + ρ·Δθ/2`, which makes the whole cell free, so
   neighbouring free cells form a valid path (resolution-complete, sound).
   The translation domain is bounded to the group's footprint so a loose
   piece does not flood the search. One BFS per `(M, a, s)` yields every
   reachable endpoint at once. The found cell path is shortcut and then
   re-certified with the checker.

Extensions that fall out for free: half turns (continue to θ = 180° and
collect lattice endpoints there), and per-move minimum clearance as a
quality measure ("this move needs 0.06 play").

### Canonical states

Never move the reference piece of a node (lowest index, as `hash()`
already treats piece 0 as the frame). Rotating a group containing it is
the same physical move as rotating the complement the other way, so only
subsets without it are enumerated — this halves the subset loop and
removes the duplicate states. Symmetric pieces: normalise `trans` to the
shape's canonical orientation id so equal shapes compare equal (a
separate, smaller change, measured on its own).

One generator replaces both current ones. "Crowell mode", if it is still
wanted, becomes a parameter preset (play, rounding, planner on/off), not a
second code path.

### Recording and visualisation

* The node / `state_c` stores a `rotationMove` record: moving group,
  axis, sense, and the path (`pivot` for pure rotations, else the list of
  waypoints `(θ, du, dv)`, typically 2–6 after shortcutting).
* One library function `rotationPose(move, t) → (R, d)` evaluates the path;
  the checker samples exactly this function and the GUI applies exactly
  this transform (replacing the `glRotatef`-about-pivot code in
  `voxelframe.cpp`). What is shown is what was checked, by construction.
* Rotation moves are not in any release, so the file format can change
  freely (no version bump; see AGENTS.md "Compatibility").
* GUI: colour contacts where clearance along the path drops below `2r`
  ("this move needs play/rounding"), which is what grssam asked for.

## Validation

1. **Checker unit tests**: hand-built cases (cube spinning next to a
   face-neighbour collides unless `r ≥ (√2−1)/2`; L-tromino about its
   corner; known clear/blocked cases), plus random paths cross-checked
   against an *independent* dense-sampling oracle written separately in the
   test (different algorithm: vertex-in-polygon and edge crossing, not
   signed distance).
2. **End-state consistency**: for all 24 orientations × 3 axes × 2 senses,
   the cells of `transform(transAdd(t, ROT_*))` placed at the rotated
   hotspot equal the rotated cells, and `rotationPose(move, 1)` equals the
   stored end state.
3. **Differential test of the generator**: random configurations; every
   generated move must certify; every endpoint the planner finds at fine
   resolution must also be found by the generator (measures
   incompleteness, reported, not asserted to zero).
4. **Corpus**: `AAA_most_rotations` and other rotation puzzles; every
   rotation step of every reported solution is re-certified by the checker
   (`burrTxt --verify-rotations`, also usable by users on their files).
5. **Physical ground truth**: a small set of moves grssam verified on
   physical copies (Clutch 4 and others), labelled possible/impossible,
   becomes a regression table and calibrates the default `r`/`g`.
6. **Independent geometry cross-check (one-off, not CI)**: a Python script
   using shapely re-checks the corpus moves with polygon booleans.

A physics engine (Bullet, PhysX) is not useful here: contacts are
compliant and tolerance-driven, results are not yes/no, and they do not
answer "does *some* path exist". Exact convex geometry is both simpler and
stronger.

## Plan

* **Phase 1 (done)** — `src/lib/rotationgeometry.{h,cpp}`: layered cross
  sections, signed distance, pose, certified path checker;
  `test/test_rotation_geometry.cpp` with validation items 1 and the
  orientation half of 2, and the path tables (`pathTable_c`, `rotationTables_c`) with their tests. Results:
  * the `ROT_*` symmetry ids of PR #50 *are* consistent with the vector
    quarter turns for all 24 orientations (one suspect ruled out);
  * the checker agrees with an independent dense oracle on random
    scenes and paths (never clear when the oracle sees an overlap);
  * the hidden audit `./build/test_burrtools "[rotations-audit]"` (branch
    `rotations-pr50`, it needs PR #50's code) checks
    `rotationRules_c` against its *own* model (`g = 0.081, r = 0`):
    of 7512 allowed moves, 619 (8.2%) need the unrecorded wiggle and
    7 are impossible even with it (sampling tunnelling).
* **Phase 2** — port from PR #50 what is worth keeping (the
  `movementAnalysator_c` phase, rotation edges and orientation in
  `disassemblerNode_c`, `state_c` rotation data, the GUI tumble), with a
  new generator (endpoints + pure rotation via checker),
  canonical states, single code path; record the move; `rotationPose` drives
  the GUI. Remove `rotationRules_c` heuristics that the differential test
  shows are not implied by geometry; keep sound ones only as prefilters
  (test: prefilter rejects ⇒ checker rejects).
* **Phase 3** — configuration-space planner for shimmy moves; path
  recording with waypoints; contact highlighting; `--verify-rotations`.
* **Phase 4** — calibration with grssam's physical cases; benchmark
  (`just bench` corpus with rotations on) and the `BURRTOOLS_NO_ROT_PLANNER`
  A/B toggle.

Open questions: default `r`/`g` (needs physical data); whether half turns
are wanted as single moves; whether rotation-move solutions should report
the minimum play they need.
