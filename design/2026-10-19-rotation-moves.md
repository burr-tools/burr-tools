# Rotation Moves

**Started:** 2026-10-19 · **Up to date with:** branch `rotations`, 2026-10-30
**Status:** Implemented on `rotations` (not yet on `master`, not in a
release). The one design document for rotation moves; the study's
scripts, raw data and explainer pages are indexed in
[`rotation-study/README.md`](rotation-study/README.md).
**Scope:** `src/lib/rotationgeometry.*`, `src/lib/rotationtables.*`,
`src/lib/movementanalysator.*`, `src/lib/disassembler_a.cpp`,
`src/lib/disassemblernode.h`, `src/lib/disassembly.*`,
`src/lib/disasmtomoves.*`, `src/lib/problem.*`, `src/lib/solution.*`,
`src/lib/stl*.{h,cpp}`, `src/lib/voxel.*`, `src/burrTxt.cpp`,
`src/gui/` (solver tab, `voxelframe`, STL dialogs, `optimizeturns`),
`src/qtgui/stlexportcontroller.*`, `src/uicore/scenemesh.*`,
`src/python/`, `test/test_rotation_*`.

## What this is

Many cube puzzles, TICs above all, only come apart when a piece or a group
of pieces turns by 90 degrees. This branch lets the disassembler find such
turns, records the exact motion of each, animates it, and answers the
question a designer has about them: how much clearance do the pieces need?

In short:

* **Geometry.** A turn is checked for real pieces: cubes of side
  `1 − play` with beveled edges, against every static cube of its layer,
  by a certified checker (no tunnelling, no magic constants). What is
  animated is exactly what was checked.
* **Search.** For each candidate group and pivot, a fixed family of 881
  motions per sense (pure turn, wiggles, slides, ramps) is looked up in
  precomputed tables; optionally a planner searches all motions for turns
  near the threshold. Groups come from closures of a pairwise blocking
  relation; states are canonical, so results do not depend on piece order.
* **The play a puzzle needs.** Per turn and per disassembly the play is
  measured (proven upper bound, lower bound for the motions searched); per
  puzzle the smallest play or bevel at which it comes apart is bisected.
* **The model is the puzzle's.** Play and bevel are stored with the
  problem, used by the solver, the STL export and the 3D view, set in the
  GUI, burrTxt and Python, and can be tightened to what a solution was
  measured to need.

## Background: PR #50

PR #50 (`colin-features`, ported, not rebased; its audit is on branch
`rotations-pr50`) added 90° turns to the disassembler. grssam reported
(PR #50, Clutch 4) that the GUI showed turning pieces cutting through
others, that moves were reported a physical copy cannot make, and that its
"default" and "Crowell" solvers disagreed (0 vs. 1 solution).

Measured against a dense exact 2D overlap test on ~9.7k random candidates
(moving group of 2–5 cubes, random static cubes, every pivot, axis and
sense), PR #50's `rotationRules_c` allowed 7695 moves; 812 of them overlap
as pure rotations of full-size cubes (up to 0.48 deep): 297 only work with
a mid-turn "wiggle" that is checked but not recorded or animated, 506 only
with its hidden 0.0405-per-side cube shrink, and 9 not at all (its
23-step sampling tunnels through a corner). It never rejected a possible
pure rotation.

What we kept: the integration point (a third phase in
`movementAnalysator_c::find`, rotation edges in the disassembler graph,
orientation as part of the node), the GUI plumbing for an animated turn,
and `AAA_most_rotations.xmpuzzle`. What we did not, and why:

1. **Feasibility was a stack of heuristics** (sandwich, plane capture,
   axis-cross rules from Fortran `SimpleRot`, a magic shrink, three magic
   wiggle amplitudes). Nothing said what physical object was modelled, so
   nothing could be validated, and every disagreement was a debate about
   constants.
2. **The motion checked was not the motion stored**, so the animation
   could not be faithful.
3. **The motion family was arbitrary** (pure turn plus 24 wiggles), and
   "clear" was only up to sampling.
4. **Two near-copy generators** (~900 lines) with different filters,
   including a per-direction cap that made the search incomplete; their
   Clutch 4 disagreement came from those filters, not from physics.
5. **State duplication**: turning a group and turning its complement the
   other way gave different nodes for one configuration.
6. **Hot-path cost**: `getenv` per candidate, set rebuilds, no caching.
7. **Untested conventions** between stored and checked end states.

Everything below was written fresh on top of master; none of
`rotationRules_c` was needed.

## The physical model

Every cube is a real, slightly undersized object, described by two values
in cube sizes (`rotationGeometry::model_t`):

* **play** `g` — the cube has side `1 − g`, centred in its cell, so
  neighbouring cubes are `g` apart. `g > 0` is enforced: with `g = 0`
  cubes start in face contact, and a cube turning about a corner it shares
  with a diagonal neighbour touches it throughout, which no finite check
  can prove clear (one table took 111 s before this was refused).
* **bevel** `k` (`chamfer`) — the edges parallel to the turn axis are cut
  at 45°, `k` off each adjacent face; the square cross-section becomes an
  octagon. Printed pieces are beveled, and a bevel approximates sanded
  wooden edges as well.

From puzzlecad: `g = 2 · $burr_inset / $burr_scale`,
`k = $burr_bevel / (√2 · $burr_scale)` (its defaults: 0.0126 and 0.032;
the converter `scripts/puzzlecad2xmpuzzle.py` on branch
`puzzlecad-script` prints the model). The model bevels every
cube edge, also where puzzlecad leaves one flat (between cubes of one
piece, concave edges), and the STL export fills concave edges instead:
slightly optimistic there.

*Why no rounding.* The first model also had an edge rounding radius,
exact in the checker and tested. Nothing used it (puzzle files, puzzlecad
and the corpus study all had 0), and as a third value it would have cost a
dimension in the file, the GUI and the bisections. Removed 2026-10-30;
`burrTxt -m` takes `play,bevel`.

*Why the model belongs to the puzzle.* The model describes the physical
pieces, not how the solver searches. A problem stores it
(`problem_c::setRotationModel`, `rotationPlay` and `rotationBevel` on
`<problem>`), invalid values are refused in the file and by the setter,
and another model is an edit like any other (the solutions become
unknown). `rotationContextFor` and `optimizeTurns` apply it themselves
(`rotationSettingsFor`), so no front end can forget it; the solver's
`rotationSettings_t::model` is only the default. A fresh solve with
rotation moves stores the model it used, so a saved file says which pieces
its disassemblies are for.

*The default.* A problem without a model uses play 0.002 and bevel 0.03:
the STL export's offset 0.01 mm and bevel 0.3 mm on 10 mm cubes, which it
had before rotation moves. So a puzzle that never turns exports and draws
exactly as before. (PR #50's implicit model, play 0.081 with sharp
edges, was the default until 2026-10-30, and the corpus results below are
at that model; briefly 0.081 with bevel 0.032. Once the STL export and the
view used the model, a plain burr exported with 0.405 mm offset, so the
default went back to the export's.) Turns usually need more clearance than
that default: the corpus measured a play around 0.05 with sharp edges, or
a bevel around 0.1 at a print's play of about 0.01. The GUI's Rotation
Moves, Play and Bevel tooltips say so, and `burrTxt -R` prints a note for
a problem without a model.

## Geometry

### Exact reduction to 2D

A rotation about an axis-aligned line, combined with translation
perpendicular to it, keeps every cube in its axial layer; cubes of
different layers can only touch on a face plane. So a planar rigid motion
of group *M* among static cubes *S* is collision-free iff, in every layer,
the moving squares avoid the static ones. Exact, not an approximation.
Axial shifts during a turn are excluded (composable from separate
translations in all but contrived cases).

### Certified path checker

A path is a polyline in configuration space `q = (θ, du, dv)`: a moving
point goes to `R(θ)(p − c) + c + d`. Along a segment of parameter length
1 no point moves more than `L = |Δd| + ρ·|Δθ|` (`ρ` the largest distance
of a moving vertex from `c`), and signed distance is 1-Lipschitz, so a
sample with clearance `sd` proves `±sd/L` around it clear; a sample below
zero proves the path blocked; otherwise the interval is bisected, down to
10⁻⁷, below which it counts as blocked. The verdict is **sound**: "clear"
is a proof, and the only error is rejecting a move within 10⁻⁷ of
tangency. Clearance is the signed distance of convex polygons: the
separating-axis penetration depth when they overlap, the smallest
vertex–edge distance otherwise. Since 2026-10-21 pairs are first
classified by in- and circumcircles, and an undecided sample's clearance
still clears what it covers (half the table build time, same verdicts).

*Why sample and not intersect exactly.* Exact contact events have closed
forms only where a segment is a pure turn or a pure shift; on a wiggle
segment "corner reaches edge" is `a·cos θ + b·sin θ + c·θ + e = 0`, whose
roots need the same Lipschitz bound to bracket. The contacts that matter
are near-tangent (a corner gliding past a face at the play), exactly where
exact floating-point intersections are fragile; signed distance has no
special cases and gets the bevel for free. And one checker then serves the
tables, the planner's arbitrary paths, the replay tests and the play
measurement. A second geometry path would only pay if the table build
became the bottleneck again (it takes 0.2–0.5 s for the compass family).

## The fixed path family and its tables

### Tables

Along a fixed motion each (moving cube, static cube) pair collides or not
independently of the other cubes. So for a fixed path, a table keyed by a
moving cube's in-plane offset from the pivot lists the static offsets
that block it, and a turn is blocked iff some moving cube finds a blocker
occupied in its own layer. Filled by the certified checker, the tables are
exactly as precise as the checker, with no geometry at search time.

* Keys are doubled in-plane offsets from the pivot (both even for a pivot
  at a cube centre, both odd at a cube corner; edge-midpoint pivots cannot
  map the lattice onto itself). One table serves all three axes, the
  in-plane frames chosen so positive angles are right-handed for each.
* Which static cubes to try: those whose centre comes within
  `√2·(1 − g) + shift` of the moving centre's arc, which reaches beyond
  the square of moving offsets (`r·√2` for offset `(r, r)`; a first
  version stopped at `r + 2` and missed blockers for `r ≥ 5`; a test
  compares a radius-8 table with brute force).
* Only a few directions per profile and amount are computed; the rest are
  `turned()` copies (turning the scene commutes with the motion), and
  every −90° table is a `mirrored()` copy. Tests compare derived tables
  with computed ones.
* `firstClear` returns the first clear path in family order (least
  disturbance first), which is what a move records. Each blocker is stored
  once with a bitset of the paths it blocks; the check ORs the bitsets of
  the occupied ones and stops when every path is blocked, so its cost is
  nearly independent of the number of paths (2026-10-21; PelikanBurr at
  449 paths went from 5.2× to 3.2× the 65-path time). A precomputed
  blocked-on-every-path set rejects most candidates without a walk.
* The radius is the largest filled extent of result and pieces, plus one;
  only cube grids with a valid result get tables.
* **Kept between solves** (2026-10-28): tables are cached by settings in
  `weak_ptr`s (concurrent solves share one build, a larger radius serves a
  smaller request), and the two most recently used sets stay alive after
  their last solver (two: the planner uses two), so designs solved one
  after another in one process build them once (21 decoded TICs in
  Python: 0.75 s kept against 3.66 s rebuilt). `releaseRotationTables()`
  frees them. Fill-on-demand was rejected (a solve reads 37–82% of the
  entries, the inverse index needs them all, and it would put
  synchronization on the hot path), and so was a disk cache (tables from
  older code could be silently stale).

### The motions

* `PATH_PURE`: the quarter turn about the pivot.
* `PATH_WIGGLE`: the turn with an offset `a·b(θ)` along a direction,
  `b(θ) = (cos θ + sin θ − 1)/(√2 − 1)`, a 16-segment polyline.
* `PATH_SLIDE`: shift by `a`, turn about the shifted pivot, shift back.
* `PATH_RAMP`: shift out while turning from 0 to `holdFrom`, hold, return
  from `holdTo` to 90° (15–45, its reverse 45–75, and the 45–45 tent).
* `PATH_PLANNED`: a polyline the planner found (below).

*Why the bulge profile.* A square of half side `h` turning about its
centre reaches `h·(cos θ + sin θ)` along an axis, so its leading face
advances by `h·(√2 − 1)·b(θ)`; shifting by that bulge lets a corner glide
along the plane its face started on (closed form: a cube spinning beside a
face neighbour clears iff `a ≥ h(√2 − 1) − g`). PR #50's `sin 2θ` lags it
everywhere but 0°, 45° and 90°, exactly where the gap is tightest. On
10,000 random scenes per model the bulge rescued every turn the sine did,
and more (1416 vs 1373 at play 0.081, 1173 vs 970 with puzzlecad cubes);
clipped contact profiles and powers of `b` did not beat it consistently,
and a bulge computed from the beveled octagon rescued exactly the same
turns, so the profile does not depend on the model.

*The family, and why so many paths.*

| Date | Family | Paths per sense | Why |
|---|---|---|---|
| 2026-10-20 | pure, 8 compass wiggles and slides, amounts 0.1–0.4 | 65 | first version |
| 2026-10-21 | amounts every 0.05 from 0.05 to 0.45 | 145 | ThreeTIC and ElfTIC come apart; possible once the bitsets and the cheaper certification made paths cheap |
| 2026-10-28 | **wide (default):** the 145 first, then wiggles and slides in the 8 other of 16 directions, amounts to 0.55, and ramps and tents in all 16 | 881 | the compass directions were the main limit (below); strictly better on both corpora |

The wide family is a superset of the compass one, so nothing the compass
family clears is lost, and it is closed under reversal (slides reverse
into slides of the other sense, ramps 15–45 into 45–75, wiggles and tents
into themselves), so every rotation edge is undirected, which the
three-front breadth-first search needs. `BURRTOOLS_ROT_FAMILY=compass`
selects the 145-path family for speed. Pruning the wide family to what
the corpora use was measured and rejected: a set cover fitted to one
model's turns missed 5–12% of the other's, so a puzzle outside the corpus
would lose moves.

## The search

### Move generation

`movementAnalysator_c::find` gains a third phase (only with tables): for
every candidate group, both senses about every axis, pivots at cube
centres and corners within the group's footprint, the end pose must be
free and not a no-op, and the first clear path of the family is recorded.
Each lattice end pose corresponds to exactly one pure-rotation pivot, so
enumerating pivots covers every endpoint.

**Groups** are unions of closures of the pairwise "blocks on every path"
relation (Cutler-style), not all 2^(n−1) subsets; a test compares with the
exhaustive search. Whether piece *j* blocks piece *i* at a pivot depends
only on their parts, orientations, relative position and the pivot, so a
**pair cache** of blocked pivots, shared by all workers of a solve
(`rotationContext_c`), answers most of it; with trig-free, parallel table
builds DraculasDentalDesaster with `-R` went from not finishing in 10
minutes to 5.5 s wall.

**Canonical states.** Turning a group that contains piece 0 (the frame of
the node hash) is the same configuration as turning its complement the
other way; such a move is stored as the complement turning, with the path
marked inverted (`<rotation inverse="1">`). Groups with piece 0 are still
searched as groups of their own: for a wiggle the complement's shift turns
along, which is not in the family, and pivots are judged on the turning
group's footprint. (Searching only groups without piece 0, as first done,
made results depend on piece order: PelikanBurr in 5, 6 or 7 turns,
FantasTIC not at all with its smallest piece first.) **Symmetric pieces**:
`rotationContext_c::canonical` maps an orientation to the smallest one
with the same cells and colours, so a turn mapping a piece onto a
translated copy of itself gives the same node as the translation; hence
`<rotation>` lists its turning pieces.

**Limits are reported, not silent**: groups wider than the tables, the
planner giving up, (sub)problems of more than 32 pieces
(`MAX_ROTATION_PIECES`), and unions of closures not tried when there are
very many loosely blocked groups set flags in `rotationContext_c`; burrTxt
prints a note for each, the GUI says "finished, turns may be missing".

**Cost when off**: none. Without the option no tables are requested,
every disassembler gets an empty context, and the rotation phase is one
null check per node. When on, the solver builds the tables in the
background; the first worker that needs them waits, the GUI does not
block.

### Recording, file format, animation

* `disassemblerNode_c` stores a rotation in `dir ≥ ROTATION_DIR_BASE`
  (axis, sense), the path index in `amount`, the pivot in former padding:
  the node stays 48 bytes (implicit; an explicit move tag would be clearer
  if the layout is revisited).
* `state_c` gets piece orientations (`<dt>`) and the rotation leading into
  it: `<rotation axis pu pv path sense pieces [inverse] [dir amount]
  [from to] [waypoints]/>`. A disassembly that uses no turn saves exactly
  as one without rotation moves. Planned waypoints are saved with 17
  digits, so the loaded polyline is bit for bit the certified one; the
  loader rejects truncated paths and impossible orientations. Rotation
  moves are in no release, so the format changes freely (AGENTS.md
  "Compatibility").
* `poseAt(makePath(spec), t)` drives the FLTK animation
  (`piecePositions_c::getTrans/getTurn`, `voxelFrame_c` draws
  `T(P + d) · R(θ) · T(−P)`): what is shown is what was checked. A test
  checks the drawn cube centres at the start and end of every turn against
  the recorded states.
* `burrTxt -R`: `level: 2.3, 2 rotations`; `-p` names the turning pieces,
  axis, pivot and path.

## The planner (opt-in)

### Why

Andrew Crowell's collection ships solution files (one assembly snapshot
per step), so his disassemblies can be compared step by step with ours
(`rotation-study/scripts/xcheck.cpp`, 150 files). Where we take a puzzle
apart, most snapshots are states of our disassembly. 18 puzzles did not
come apart from his assembly; running from every snapshot finds the one
step we could not make, always a single quarter turn with no limit of the
generator involved. The compass family blocked all 17 at play 0.081; a
configuration-space search found motions for 10 of them at that play, the
rest needed 0.084–0.092. Their paths were asymmetric (most of the shift
early), in non-compass directions, some holding a shift for most of the
turn, some changing direction mid-turn. And they sat right at the play:
"does it come apart at this play" is the wrong question for them; "this
puzzle needs play 0.085" is one a designer can act on.

### How (`rotationGeometry::planTurn`)

Over a grid of shifts `|du|, |dv| ≤ 0.5` (0.01 cells) and angles (0.5°):

* **Impossibility proofs first**: every path passes every angle, so if at
  some angle no shift reaches the floor, nothing does (pure-turn Lipschitz
  bound, then a quadtree over the shift box per angle, worst first).
* **Widest path**: max-heap Dijkstra over cells valued by their centre
  clearance: the path with the largest smallest clearance.
* **Fast mode** (what the search uses): goal-directed through cells half a
  cell above the floor, then the widest path down to the floor itself,
  within a budget of 60,000 cells (a turn beyond it is given up and
  reported).
* **Certified value**: the shortcut polyline's minimum clearance by
  Lipschitz branch and bound (within 10⁻⁴); the polyline is what is
  recorded and animated.
* **Optimistic bound**: the widest path over `centre + cell margin`
  values: no motion in the box has more clearance.

In the search (`listRotations`, `burrTxt -Q`, `BURRTOOLS_ROT_PLANNER=1`),
the planner runs only on turns the family clears at `play + slack` (0.02)
but not at the play, with groups from the relaxed tables' closure
relation. A sound filter ("blocks on every motion in the box") blocks
almost nothing and made tens of thousands of candidates per node, so this
is a heuristic: a turn the family cannot make even with `slack` more play
is never planned. A turn and its reverse share a cache entry keyed by the
scene (sense, moving cubes and the static cubes within reach, relative to
the pivot), planned from the side with the smaller key, so an edge exists
in both directions or neither.

**Cost, and why it stays opt-in.** With the planner (and the 145-path
family) 302 of the 319 Crowell files come apart against 280, none lost, at
5 to 200 times the time of the family-only search (MagellanTIC 0.27 → 29 s
at slack 0.02). Half the time is cell clearances, a quarter the grid
search, the rest budgeted searches of turns impossible but not at a single
angle. The wide family alone reaches 300 at 3–5 times the family cost, so
it became the default and the planner stays a fallback.

## The play a puzzle needs

**Clearance and play.** For sharp cubes, more play `Δ` takes `Δ/2` off
every face, and for convex shapes shrunk by `δ` each the signed distance
grows by at least `2δ`: along a fixed path, clearance at `p + Δ` is at
least clearance at `p` plus `Δ`. So a path with smallest clearance `c < 0`
is clear at `p − c`. Only in that direction: taking play away can cost
more than `Δ`, because a grown square's corners stick out (the first
version used it both ways and wrongly reported TriumviraTIC at 0.004). More
play and more bevel both only take material away, so both only add turns.

**Per turn** (`rotationGeometry::requiredPlay`): the widest path at the
model's play gives a bracket, then fast-mode feasibility halves it to
0.0005. `playBound_t::upper` is proven to work along the returned path;
`lower` comes from the optimistic bound: below it no motion with shifts up
to `maxShift` gets through (a wilder one might). The lower bound is loose
by about the cell margin.

**Per disassembly** (`optimizeTurns`): every turn's `requiredPlay`,
`turnPlay_t::upper` and `lower` the largest of them; `upper` is a proven
upper bound on the play the puzzle needs, `lower` bounds only this
disassembly's hardest turn. Each turn's path is replaced by the widest
path at the model's play, the motion with the most room, the one to
animate. `optimizeControl_t` adds progress, a stop between turns, and a
re-plan-only mode. (`upper` / `lower` were `needs` / `atLeast` until
2026-10-30: "needs" read as the minimum, which it is not.)

**Per puzzle** (`burrTxt -M play:lo,hi`, `-M bevel:lo,hi`): bisection over
whole quiet solves, the other value fixed, then a solve at the value
found. `bevel:` is the question for printed puzzles, whose play the
printer fixes. With `-P` it tightens: HypnoTIC with `-M play:0.02,0.15`
finds 0.0896, and the disassembly found there needs only 0.0823.

**Kept with the solution.** `solution_c::setPlay` stores `upper`, `lower`
and the bevel they hold at (`playUpper`, `playLower`, `playBevel` on
`<solution>`); a new disassembly drops them. burrTxt never wrote files, so
`burrTxt -w file` writes the solved puzzle: the problem runs the assembler
as in the GUI's solve thread (it ends solved, with its counts), solutions
come in as found, and the model they are for is stored; with `-x` the
re-disassembled solutions are replaced.

**Optimize Turns (FLTK GUI).** A Solver tab button runs `optimizeTurns`
on the selected solution on a worker thread, with progress and Cancel, on
a copy of the disassembly (the main window keeps animating the original;
the solution takes the copy only after the dialog closes). It shows both
bounds and offers:

* **Use play *upper***: the tightest play proven to work becomes the
  problem's play, because that is the point of optimizing (tighter pieces
  hold together better). Then every turn is re-planned at the new play
  (`measure = false`): a path chosen at a larger play is not proven at a
  smaller one. That re-plan cannot be cancelled, so the paths are never
  left unproven. The other solutions become unknown, as after any edit.
* **Keep play**: the turns keep the widest paths at the problem's play.

Not modelled, and worth saying with any number: flexing and wear,
friction, imperfect alignment, axial wobble during a turn. The numbers
are the play of ideal rigid cubes.

## Front ends

| | burrTxt | FLTK GUI | Python | Qt GUI |
|---|---|---|---|---|
| Rotation moves on | `-R` | Rotation Moves (cube grids) | `solve(rotation_moves=True)` | not yet (Solver workspace) |
| Play and bevel | `-m play,bevel` (for the run), else the file's | Play, Bevel fields (undoable edit) | `Problem.rotation_model` | — |
| Planner | `-Q`, `BURRTOOLS_ROT_PLANNER` | `BURRTOOLS_ROT_PLANNER` | env | — |
| Path family | `BURRTOOLS_ROT_FAMILY` | same | same | — |
| Measure a solution's play | `-P` | Optimize Turns | — | — |
| Smallest play or bevel | `-M play:` / `-M bevel:` | — | — | — |
| Write results | `-w file` | save | `Puzzle.save` | — |

burrTxt says when `-m`, `-P`, `-Q` or `-M` have no effect. Python keeps
tables between solves too (`release_rotation_tables()`).

**STL export.** The cube exporter can follow a model
(`stlExporter_c::followModel`: offset = play / 2 · unit size, bevel =
bevel · unit size) until bevel or offset is set by hand, so the printed
pieces are the ones the turns were checked for. Export Solution to STL
starts from its problem's model, Export STL (FLTK and Qt) from the model
of the problems using the shape when they agree, and a line under the
parameters says whether the pieces are the checked ones (`stlModelNote`).
With the default model the export starts from exactly its old defaults.
The dialogs now pass on only the fields actually edited (they used to
write every field back from two-decimal text, which would have overwritten
the values just derived from a new unit size) and show four significant
digits.

**STL view style.** The 3D views draw `voxel_c::getSTLMeshFor(model)`
where a problem stores a model (for a single shape, when every problem
using it stores the same one), otherwise their own look.

## Validation

* **Checker** (`test_rotation_geometry.cpp`): closed forms (a cube
  spinning beside a face neighbour is blocked unless beveled by
  `k ≥ h − √((1 − h)² − h²)`, 0.175 at play 0.081, and clears with a bulge
  wiggle of `a ≥ h(√2 − 1) − g`), the L-tromino about its corner, random
  paths against an independent dense oracle (vertex-in-polygon and edge
  crossing, not signed distance), derived against computed tables,
  outermost table entries against brute force.
* **Conventions**: for all 24 orientations × 3 axes × 2 senses, PR #50's
  `ROT_*` symmetry ids agree with the vector quarter turns, and the pose at
  the end of a path equals the stored end state.
* **Planner** (`test_rotation_planner.cpp`, two Crowell turns embedded as
  cells): blocked for the family, found by widest and fast mode, certified
  value below every sampled clearance, reverse path certifies with the
  same value, a boxed-in cube is proven impossible, `requiredPlay` within
  range with its lower bound below, more play adds at least as much
  clearance, planned paths survive save and load bit for bit.
* **Disassemblies** (`test_rotation_disassembly.cpp`): AAA_most_rotations
  does not come apart without turns and comes apart as 2.3 with them (at
  play 0.081 with the planner, slack 0.04); every step replays as a valid
  motion and after a save/load round trip; the animation matches the
  states; turns offered do not depend on piece order; the closure search
  matches the exhaustive one; `optimizeTurns`' progress, stop and re-plan.
* **Model and front ends**: the model in the file (round trip, refused
  values, edits making solutions unknown), solution play, the exporter
  following a model, the shape model helpers, the STL look at a model,
  burrTxt `-m`, `-M`, `-w` (CLI smoke test), the Qt STL dialog, Python's
  `rotation_model`.

## Corpus results

The puzzles are not in the repository (`puzzles/` is gitignored):
`puzzles/TICs/decoded` (21 TICs and RIBs with puzzlecad sources, so each
has a printed model, play 0.009–0.025, bevel 0.054–0.071; all printed and
taken apart physically) and `puzzles/BTFiles/Andrew Crowell` (319 files).
Unless said otherwise "default model" below is play 0.081, sharp: PR #50's
shrink, which the TICs were designed and checked under, so the
like-for-like comparison.

| | 65 paths, `sin` | 65, bulge | 145 (0.05 steps) | 881 (wide, default) |
|---|---|---|---|---|
| Decoded TICs apart, default model | 17 / 21 | 17 | 19 | 20 |
| Decoded TICs apart, printed model | 3 / 21 | 5 | 10 | 10 |
| Decoded TICs, mean smallest play (sharp) | 0.0569 | 0.0563 | 0.0476 | 0.0408 |
| Decoded TICs, mean smallest bevel at printed play | 0.122 | 0.109 | 0.088 | 0.0737 |
| Crowell apart, default model | 276 / 319 | 276 | 280 | 300 |

* **Against PR #50** (same files): on the decoded TICs PR #50's engines
  take apart 15 (300 s limit), ours all of them and more. On Crowell's
  (60 s limit) its Crowell engine 176, Classic 199, either 230. With the
  145-path family every one of them came apart in ours except GemTIC,
  whose two disputed turns cannot be certified there at 0.081 (essentially
  zero clearance; PR #50's sampling lets them through); the wide family
  takes GemTIC apart too, 300 in all.
* **ThreeTIC**: of 1000 turns PR #50 accepted, 848 clear the 65-path
  family, 152 need another motion (86 a finer grid of shift vectors
  catches, 66 only a search), none was impossible. So PR #50 was not wrong
  there; the family was too coarse.
* **What the family should learn** (2026-10-28, 17 blocked Crowell turns,
  candidate families against the planner's widest path at 0.081): finer
  amounts or larger shifts clear none; 48 directions clear 4;
  ramp-hold-ramp profiles 3; 16 directions with bulge, tent and holds the
  same 6 as 48 directions with ramps, at 640 paths instead of 17,280. **The compass
  directions were the main limit**, early asymmetric shifts the second.
  Four turns change shift direction mid-turn and stay out of reach of
  every fixed family: that is what the planner is for.
* **Printed models**: 10 of 21 printed TICs come apart under their own
  model. At their printed play (about 0.011) they need a bevel of 0.074 on
  average (wide family; 0.088 with 145 paths), against 0.054–0.071
  printed. Not separated yet: flexing and wear, the model's play at face
  contacts, motions no search covers.
* **Cost of more paths**: mostly the table build (perf: ~70% of CPU in
  building tables at 449 paths). One burrTxt run with the wide family:
  ThreeTIC and MysTIC 0.13 → 0.7 s, PelikanBurr 1.9 → 4.75 s; with kept
  tables a further TIC design takes about 0.1 s. At the tiny default play
  (0.002) AAA_most_rotations takes 2.0 s against 1.2 s at 0.081 (more
  near-contact subdivision).

## Decisions and history

| Date | Decision | Why |
|---|---|---|
| 2026-10-19 | Exact 2D reduction, explicit model, certified checker, tables | PR #50 had no stated model and checked what it did not record |
| 2026-10-20 | Solver and GUI end to end; closures and pair cache; canonical and symmetric states; limits reported; tables built in the background | correctness and cost (DraculasDentalDesaster from >10 min to 5.5 s) |
| 2026-10-21 | Bulge wiggle; bitsets in `firstClear`; cheaper certification; amounts every 0.05 | corpus evidence; the check stopped scaling with the family |
| 2026-10-25 | Planner, `PATH_PLANNED`, `-Q`, `-P`, `-M` | Crowell's blocked turns sit at the threshold; designers need the play |
| 2026-10-28 | Kept tables; `rotationConfig_t` and env toggles; wide family default; pruning rejected | the table build is the cost; strictly better results |
| 2026-10-30 | Rounding removed; model stored per problem; `upper`/`lower`; `-M bevel:`; `-w`; play per solution | the model is the pieces', results must persist |
| 2026-10-30 | STL export and view use the model; default back to the export's; hints | print and view the checked pieces without changing plain puzzles |
| 2026-10-30 | Optimize Turns with "Use play" | the measured tightest play is the result designers want |

## Open

* **Physical ground truth**: grssam's verified moves (Clutch 4 and
  others) as a regression table; why printed TICs need more bevel than
  printed.
* **Planner cost**, to make it a default: a sound candidate filter
  (would also remove the slack heuristic), cheaper impossibility proofs for
  turns blocked by connectivity, incremental clearance along the angle
  axis; tighter lower bounds by refining only straddling cells.
* **Use play** fits one disassembly; the other solutions become unknown
  and are not re-checked. Re-solving at the new play, or choosing a play
  that keeps a chosen set of solutions, would close that.
* **GUI**: the planner as an option next to Rotation Moves (the
  add-disassembly buttons run on the UI thread); the Qt GUI's Solver
  workspace (rotation moves, model fields, Optimize Turns); its drawing of
  turned pieces waits for PR #181 (local branch `rotations-qtgui-draw`).
* `--verify-rotations` (re-certify every turn of a file's solutions) was
  planned, not built; `-x -R` re-disassembles instead.
* Half turns as single moves; rotations in solution rankings (with turns,
  the search finds shorter disassemblies that turn partly removed pieces;
  whether rotations should only be used when slides fail is a product
  decision).
* `-M` re-solves from scratch per step; the planner cache is per solve.
* Piece-order and closure tests run with the family only.

## Reproducing

Build the study helpers against the release library (`just build-release`
first):

```bash
c++ -I build-rel -I src -I subprojects/manifold-3.5.2/include -DNDEBUG -std=c++20 -O2 -pthread \
    design/rotation-study/scripts/xcheck.cpp -o xcheck \
    -Wl,--start-group build-rel/libburr_core.a build-rel/subprojects/manifold-3.5.2/libmanifold.a -lz -Wl,--end-group
# same for diag.cpp
./xcheck ".../HepTIC_SolutionFile.xmpuzzle"                     # snapshot vs state match
FROM=1 PARTIAL=1 ./xcheck ".../MysTIC_SolutionFile.xmpuzzle"    # the blocked step
./diag ".../MysTIC_SolutionFile.xmpuzzle" 11                    # family vs grid search over plays
REQ=1 ./diag ...; PLAN=1 [FEAS=1] ./diag ...; FAM=1 ./diag ...; DUMP=1 ./diag ...
build-rel/burrTxt -d -R -Q -P puzzle.xmpuzzle
build-rel/burrTxt -d -R -P -M bevel:0,0.2 -m 0.0126,0 -w solved.xmpuzzle puzzle.xmpuzzle
./build/test_burrtools "[rotations-table-stats]"                # table sizes and build times
```

The corpus scripts (`tics.py`, `crowell.py`) and their results are
described in [`rotation-study/README.md`](rotation-study/README.md).
