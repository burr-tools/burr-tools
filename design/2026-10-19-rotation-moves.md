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

### Physical model (explicit parameters)

Every cube is a real, slightly undersized object:

* **play** `g` — the cube has side `1 − g` and sits centred in its grid
  cell, so neighbouring cubes have a gap `g` (clearance of real pieces);
* **edge rounding** `r` — the edges parallel to the rotation axis are
  rounded with radius `r` (a rounded square = core square of side
  `1 − g − 2r` ⊕ disk `r`);
* **bevel** `k` — those edges are cut at 45°, `k` off each adjacent face
  (the core square becomes an octagon; added for 3D-printed pieces, whose
  edges are beveled, not rounded). puzzlecad's `$burr_bevel` is the width
  of the bevel face, so `k = $burr_bevel / (√2 · $burr_scale)`, and its
  `$burr_inset` gives `g = 2 · $burr_inset / $burr_scale`. Every cube is
  beveled, also where puzzlecad leaves an edge flat (between cubes of one
  piece, and on concave edges), which makes the model slightly optimistic
  there.

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
* `PATH_WIGGLE`: the turn with an offset `a·b(θ)` along a compass
  direction, `b(θ) = (cos θ + sin θ − 1)/(√2 − 1)`, a polyline of 16
  segments (the polyline *is* the motion).
* `PATH_SLIDE`: shift by `a`, turn about the shifted pivot, shift back.

*Why this wiggle profile.* A square of half side `h` turning about its
centre reaches `h·(cos θ + sin θ)` along an axis, so its leading face
advances by `h·(√2 − 1)·b(θ)`. The same shape, scaled by `0.5 + h`, holds
for a cube touching a corner pivot. A tight turn keeps the turning piece in
contact with a static one, with the play only separating them at the start
and end. Shifting by the bulge does that: a cube's corner glides along the
plane its face started on (closed-form test: spinning beside a face
neighbour clears iff `a ≥ h(√2 − 1) − g`). PR #50's `sin 2θ` approximates
`b` (within 0.043) but lags it everywhere except at 0°, 45° and 90°, which
is where the gap is tightest. On random 2D candidates (pure turn blocked,
8 directions × amounts 0.1–0.4, 10,000 scenes per model) the bulge
rescued every turn the sine did, and more:

| profile | play 0.081, sharp | puzzlecad (0.0126, bevel 0.0317) |
|---|---|---|
| `sin(πt)` (before) | 1373 | 970 |
| bulge `b(θ)` | 1416 | 1173 |
| contact, `max(0, (a+g)·b − g)` | 1315 | 1171 |
| `b^0.85` / `b^1.2` (other seeds) | 1386 / 1433 vs 1478 | 1235 / 904 vs 1216 |

Holding contact with the neighbour itself (the clipped profile) is too
tight for a fixed family: it has no slack when the amount does not match,
and gives no shift early when a corner passes another corner. Powers of
`b` do not beat it consistently. A bulge computed from the beveled
octagon rescued exactly the same turns as the square's, so the profile
does not depend on the model. Amounts from the geometry (`h(√2−1)` and
`(0.5+h)(√2−1)`, 0.190 and 0.397 at play 0.081) tie the 0.1–0.4 grid with
puzzlecad cubes but lose with play 0.081 (1312 vs 1431), so the grid stays.
Adding 0.5 and 0.6 rescues a lot more (1633 / 1317); larger shifts are the
main thing the family misses, together with shifts that change direction
mid-turn (see the planner, phase 3).

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
the same physical move as rotating the complement the other way, so such a
move is stored as the complement turning, with the path marked inverted
(`<rotation inverse="1">`: the other pieces follow the path relative to
the turning ones). Groups with piece 0 must still be searched as groups of
their own: for a wiggle or slide the complement's motion has its shift
turning along with it, which is not in the path family, and pivots and
reach are judged on the turning group's footprint. (Searching only groups
without piece 0, as first implemented, made the result depend on the piece
order: PelikanBurr came apart in 5, 6 or 7 turns, FantasTIC not at all
with its smallest piece first.) Symmetric pieces: normalise `trans` to the
shape's canonical orientation id so equal shapes compare equal.

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

1. **Checker unit tests**: hand-built cases (cube spinning in place, as a
   pure turn, next to a face-neighbour collides unless its edges are cut
   back far enough, while a bulge wiggle away from the neighbour clears
   with sharp edges once `a ≥ h(√2 − 1) − g`; for the pure turn, with
   cube side `a = 1 − g` and half side `h = a/2`, a rounding
   `r ≥ (a/√2 − (1 − h))/(√2 − 1)` (0.264 at `g = 0.081`, 0.5 at `g = 0`)
   or a 45° bevel `k ≥ h − √((1 − h)² − h²)` (0.175 at `g = 0.081`, a bevel
   face of 0.247 cells); L-tromino about its
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
* **Phase 2, solver (done)** — written fresh on master instead of
  porting PR #50's generators; none of `rotationRules_c` was needed:
  * `movementAnalysator_c::find` state 3 (only with tables): every group
    without piece 0 (canonical states, no duplicate "turn the rest"
    moves; groups enumerated up to `MAX_ROTATION_PIECES` = 16 pieces), centre
    and corner pivots in the group's footprint, both senses about all
    axes, end pose free and not a no-op, first clear path of the family.
    The path family is closed under reversal (slides reverse into slides
    of the other sense, wiggles into wiggles), so every rotation edge is
    undirected, which the three-front breadth-first search relies on.
  * `disassemblerNode_c`: rotation in `dir ≥ ROTATION_DIR_BASE` (axis,
    sense), `amount` = path index, in-plane pivot in former padding — the
    node stays 48 bytes.
  * `state_c`: piece orientations (`<dt>`) in every state of a disassembly
    made with rotation moves, and the rotation leading into a state
    (`<rotation axis pu pv path sense pieces [du dv amount]/>`); nothing new is
    written without rotation moves. The state loader now skips unknown
    tags instead of misparsing them.
  * `burrTxt -R`: `level: 2.4, 2 rotations`; `-p` names the turning
    pieces, axis, pivot and path and draws pieces in their orientations.
  * `AAA_most_rotations.xmpuzzle` (from PR #50, with its saved solution in
    PR #50's format removed): no disassembly without rotations; with them
    level 2.4 with 2 rotations, both needing a wiggle. Its test replays
    every step independently (no overlaps, turned pieces land on their
    start cells turned about the pivot, path re-certified by the checker
    directly) and after a save/load round trip.
* **Phase 2, GUI (done)** — `poseAt(makePath(spec), t)` drives the
  animation, so the GUI shows exactly the certified polyline:
  * `piecePositions_c` gained `getTrans` (orientation per step) and
    `getTurn` (angle, axis, world pivot, shift while a piece turns);
    `disasmToMoves_c` fills them from the states' `dt`/`rotation`, with the
    pivot moved by the same display offset as its subtree.
  * `voxelFrame_c` swaps a piece's mesh when its orientation changes and
    draws a turning piece with `T(P + d) · R(θ) · T(−P)` on top of its
    placement.
  * Solver panel: "Rotation Moves" (cube grids only) sets
    `PAR_ROTATION_MOVES` and is used by the add-disassembly buttons; a wait
    cursor covers the table build, which still runs on the UI thread.
  * Test: for every animated rotation the drawn cube centres at the start
    and end of the turn coincide with the recorded states (no jump);
    checked visually on the X display with AAA_most_rotations.
* **Phase 3** — configuration-space planner for shimmy moves; path
  recording with waypoints; contact highlighting; `--verify-rotations`.
* **Phase 4** — calibration with grssam's physical cases; benchmark
  (`just bench` corpus with rotations on) and the `BURRTOOLS_NO_ROT_PLANNER`
  A/B toggle.

## Review of the implementation (2026-10-20)

Things shaped by the code they were fitted into, and how they would
ideally be done:

* **Fixed:** `firstClear` took vectors and built a `std::set` per call —
  an API designed before the solver used it; the solver now builds one
  `occupancy_c` bitmap per group, and a precomputed blocked-on-every-path
  set rejects most candidates without walking the family.
* **Fixed:** groups were enumerated exhaustively (2^(n−1) per pivot), a
  habit carried over from PR #50. Groups are now unions of closures of the
  pairwise "blocks on every path" relation, Cutler-style; a test compares
  against the exhaustive search.
* **Fixed (pre-existing bug found on the way):** `print_rec` drew
  sub-separations with shapes indexed by the wrong piece number.
* **Fixed, performance:** a per-pair cache like `movementCache_c`. Whether
  piece j blocks piece i on every path at a pivot depends only on their
  parts, orientations, relative position and the pivot relative to i; each
  analysator caches, per pair, a bitmap of those pivots (only pivots within
  the tables' reach of all of i's cubes), filled from the inverse table
  `pivotsBlockedAt`. Together with trig-free table building, a parallel
  table build and the closure search: DraculasDentalDesaster with `-R`
  went from not finishing in 10 minutes to 7.5 s wall / 45 s CPU on 8
  threads (0.05 s without rotations), peak RSS 153 MB (11 MB without).
  Remaining cost: walking the inverse lists on cache misses. Since then
  (`rotationContext_c`): one pair cache shared by all workers and a table
  radius from the filled extents of result and pieces, 7.5 -> 5.5 s wall,
  45 -> 28 s CPU.
* **Fixed:** `<dt>` orientations are dropped from a disassembly found with
  rotation moves that uses none, so it saves exactly like one without.
* **Fixed:** frame conventions (in-plane coordinates, pivots, quarter turns
  of points) exist once, in `rotationGeometry`; the analysator caches shapes
  per part and in-plane cubes per part, orientation and axis; the piece cap
  is 32.
* **Observation:** with rotation moves, the breadth-first search finds
  shorter disassemblies that use turns once pieces are partly pulled out
  (PelikanBurr: 3 rotations with pure turns only). Whether that is wanted
  in solution rankings, or rotations should only be used when translations
  fail, is a product decision.
* **Node encoding:** rotations reuse `dir`/`amount` and padding of
  `disassemblerNode_c` to avoid growing every node. Implicit; an explicit
  move tag would be clearer if the node layout is revisited.
* **Duplicated shape transform:** `movementAnalysator_c::cellsAroundHotspot`
  transforms shapes itself because `movementCache_c::getTransformedShape`
  is private.
* **Fixed:** symmetric pieces. A turn that maps a symmetric piece onto a
  translated copy of itself used to yield a node with another `trans` than
  the equivalent translation, so the search could visit the same physical
  state twice. `rotationContext_c::canonical` maps every orientation to the
  smallest one with the same cells (colours included) and the hotspot shift
  that keeps them in place; the start node and every turned piece are
  stored that way. Because a turn no longer necessarily changes a piece's
  `trans`, `<rotation>` lists its turning pieces (`pieces="0 2"`) and the
  animation, `-p` and the replay test use that list. When a disassembly
  uses no turn, the canonical shift is undone and orientations are dropped,
  so it still saves exactly like one without rotation moves. Neutral on the
  corpus (same results, same time within noise): the assembler already
  places most pieces in their smallest orientation, and turns of symmetric
  pieces onto themselves are rare there.
* **Fixed (crash):** the per-part caches of in-plane cubes grew lazily, so
  pairing two copies of a part in different orientations could move the
  first one's cubes while they were read (AlPackino and CubeInCage with
  `-R` crashed). They are sized for every orientation up front.
* **Fixed (completeness):** rotation moves no longer depend on the piece
  order, see Canonical states. Cost: about 30% more time on
  DraculasDentalDesaster with `-R`, twice as much on PelikanBurr; a pure
  turn of a group with piece 0 could be skipped where the complement is
  searched anyway.
* **Fixed:** limits are no longer silent: groups wider than the tables and
  (sub)problems of more than 32 pieces are recorded in `rotationContext_c`;
  burrTxt prints a note and the GUI says "finished, turns may be missing".
* **Fixed:** the tables are built in the background by the solver, the
  first worker that needs them waits; the GUI no longer blocks.
* **GUI:** `entryBounds()` ignores a turning piece (fit-to-view during an
  animation only).

Open questions: default `r`/`g` (needs physical data); whether half turns
are wanted as single moves; whether rotation-move solutions should report
the minimum play they need.

## TIC corpus results (2026-10-21)

Scripts, raw data and the explainer page are in
[`rotation-study/`](rotation-study/README.md). The puzzles themselves are
not in the repository (`puzzles/` is gitignored):

* `puzzles/TICs/decoded`: 21 TICs and RIBs with puzzlecad sources, so each
  has a printed model (play 0.009–0.025, bevel 0.054–0.071, from
  `scripts/puzzlecad2xmpuzzle.py`). These were printed and come apart
  physically.
* `puzzles/BTFiles/Andrew Crowell`: Crowell's collection, 319 files (146
  4x4x4 TICs, 98 5x5x5 TICs, 45 ARCparent cubes, 20 RIBs, 10 RIPs; some
  twice as `X` / `X_SolutionFile`). No printed models.

PR #50's `0.0405`-per-side shrink is play 0.081 with sharp edges, our
default model, so the TICs were designed and checked under the default
model. That makes it the like-for-like comparison.

### Decoded TICs (21)

| | `sin` wiggle | bulge wiggle | bulge, amounts to 0.6 |
|---|---|---|---|
| Come apart, default model | 17 | 17 | 17 |
| Come apart, printed model | 3 | 5 | 5 |
| Mean smallest bevel at printed play | 0.122 | 0.109 | 0.107 |
| Mean smallest play, sharp | 0.0569 | 0.0563 | 0.0560 |

The bulge newly takes PedanTIC and StarTIC_2 apart under their printed
models, and lowers the bevel needed on 13 TICs (typically 0.117 → 0.097).
It needs slightly more on CagedCube_1 (0.181 → 0.197), HypnoTIC and
scRIBble: a fixed path family is not monotone in the profile.

PR #50 on the same files (300 s limit): Classic engine 14, Crowell engine
7, either 15. Ours takes apart all 15 plus CagedCube_1 and FantasTIC.
ElfTIC, HypnoTIC, ThreeTIC and TriumviraTIC come apart in neither; ours
needs play 0.083–0.101 for them.

**Printed models:** 16 of 21 printed TICs still do not come apart under
their own model. They typically need a bevel of about 0.097 at their play
of about 0.011, against the 0.054 printed. Possible reasons, not yet
separated: real pieces flex and wear; the bevel model has only the printed
play at face contacts (a pure spin beside a face neighbour would need a
0.39 bevel at play 0.0108); and missing motions (see ThreeTIC below).

### Crowell collection (319)

Default model; `minPlay` is the smallest play (sharp) at which a puzzle
comes apart, by a 6-step bisection.

| Group | `sin` | bulge | bulge, amounts to 0.6 |
|---|---|---|---|
| 4x4x4 TICs | 125 / 146 | 125 / 146 | 125 / 146 |
| 5x5x5 TICs | 82 / 98 | 82 / 98 | 86 / 98 |
| ARCparent cubes | 43 / 45 | 43 / 45 | 43 / 45 |
| RIBs | 20 / 20 | 20 / 20 | 20 / 20 |
| RIPs | 6 / 10 | 6 / 10 | 6 / 10 |
| **Total** | **276** | **276** | **280** |
| Default runs, total time | 111 s | 107 s | 209 s |

* Bulge vs `sin`: the same puzzles come apart. `minPlay` is lower on 49
  puzzles and higher on 26 (313 comparable).
* Amounts 0.5 and 0.6 add TritanIC_2 and HolopTIC (5x5x5) and lower
  `minPlay` on 23 puzzles, never raising it. They cost a median 1.65× the
  run time (90th percentile 2.5×, worst 3.85×; 12.6 s → 48.6 s).
* PR #50's Crowell engine (60 s limit): 176 come apart, 8 time out. None of
  them fails in ours; ours takes apart 92 that it does not, plus 8 where it
  timed out.
* PR #50's Classic engine (60 s limit), partial at 182 of 319 files: 129
  come apart, 6 time out. The only puzzle it takes apart and ours does not
  is Crowell's 4x4x4 ThreeTIC (twice, with its solution file); ours takes
  apart 24 that it does not, plus 2 where it timed out.
  *(Run in progress; to be updated when it completes.)*

### ThreeTIC: what the family misses

PR #50's Classic engine accepted 1000 distinct rotations while searching
Crowell's ThreeTIC (`BT_ROT_JSON` dump, branch `rotations-pr50`). Each was
re-checked with the certified checker over all its layers, at play 0.081
with sharp edges:

| | Rotations |
|---|---|
| A path of our 65-path family clears it | 848 |
| Only amounts 0.5–0.8 clear it | 0 |
| Some other motion is clear (grid search, certified) | 152 |
| …of which a 0.05-step grid of shift vectors catches (wiggle or ramp-hold-ramp) | 86 |
| …of which only the search finds a motion | 66 |
| No motion found (PR #50 wrong) | 0 |

So PR #50 does not accept impossible moves here: our family samples shift
directions and amounts too coarsely. A typical missing path shifts by about
(0.25, −0.20), 39° off the axis and 0.32 long, during the first 25° of the
turn, holds it until about 81°, then returns. The nearest family paths
shift along exact diagonals ((0.21, −0.21) at 0.3, (0.28, −0.28) at 0.4).
Finer amounts alone already help: with amounts every 0.05 (0.05–0.45, still
8 compass directions, 145 paths) ThreeTIC comes apart (level 7.9, 5
rotations), and HolopTIC with amounts every 0.0125 (449 paths).

### What more paths cost

Wall time, release build, with other jobs running (ratios only):

| Puzzle | 65 paths | 145 paths | 449 paths |
|---|---|---|---|
| AAA_most_rotations (mostly table build) | 0.16 s | 0.32 s | 0.92 s |
| PelikanBurr | 4.2 s | 7.5 s | 26.3 s |
| DraculasDentalDesaster | 14.2 s | 13.7 s | 27.7 s |
| ThreeTIC (Crowell) | 0.16 s, apart: no | 0.33 s, yes | 0.95 s, yes |
| HolopTIC (Crowell) | 0.26 s, no | 0.53 s, no | 2.4 s, yes |

Time grows roughly with the number of paths: table building does, the
blocked-on-every-path pre-check rejects fewer candidates when the family
is larger, and each remaining candidate walks more tables. Per-path shadow
bitmasks cached per piece pair (see "Lookup tables") would make a check
nearly independent of the family size, leaving only the one-time table
build to grow.

### Open questions from the corpus

* Default amounts: every 0.05 (2× time, recovers ThreeTIC) vs. the current
  0.1–0.4, vs. a denser grid of shift vectors once checks are bitmasks.
* Why printed TICs fail under their own model: needs physically verified
  moves, ideally on one of the 16.
* The configuration-space planner (phase 3) covers what no fixed family
  reaches (66 of ThreeTIC's 152, the direction-changing paths).
