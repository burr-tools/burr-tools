# Rotation Moves

**Started:** 2026-10-19 · **Up to date with:** 2026-11-13
**Status:** Implemented, not yet in a release. The one design document
for rotation moves; the study's scripts and result summaries are in
[`rotation-study/`](rotation-study/README.md), its raw data and explainer
page under the tag `rotation-study-2026-10-30`.
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
  precomputed tables; a planner decides the turns the family misses, in
  one of three rotation modes (fast: the family only; balanced, the
  default: the turns the family makes with more play; slow: every turn with
  a free end pose). Every mode accepts only turns certified at the model.
  Groups come from closures of a pairwise blocking relation; states are
  canonical, so results do not depend on piece order.
* **The play a puzzle needs.** Per turn and per disassembly the play is
  measured, and the bevel at the puzzle's play (proven upper bounds, lower
  bounds for the motions searched).
* **The model is the puzzle's.** Play and bevel are stored with the
  puzzle, used by the solver, the STL export and the 3D view, and set in
  the GUI, burrTxt and Python. Changing them resets the problems.

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

*Why no rounding.* An earlier draft also had an edge rounding radius,
exact in the checker and tested. Nothing used it (puzzle files, puzzlecad
and the corpus study all had 0), and as a third value it would have cost a
dimension in the file, the GUI and the bisections, so it was dropped
before review.

*Why the model belongs to the puzzle.* The model describes the physical
pieces, not how the solver searches, and the shapes are shared between
problems, so the puzzle stores it (`puzzle_c::setRotationModel`,
`rotationPlay` and `rotationBevel` on `<puzzle>`). Invalid values are
refused in the file and by the setter. Other pieces are an edit of every
problem (`problem_c::piecesChanged`): its solutions become unknown, so
the user solves again, and less play or a smaller bevel, which adds
material, also drops the disassemblies that turn (their assemblies stay):
their turns were only proven for the old pieces. Storing the model that
applies already changes nothing. A tighter model does not ask the turns
again to keep what still works: a model change always resets.
What a problem needs is not stored apart: it follows from its solutions'
measured play (below). `rotationContextFor` and `optimizeTurns` apply the
model themselves (`rotationSettingsFor`), so no front end can forget it;
the solver's `rotationSettings_t::model` is only the default. A fresh solve with rotation moves stores the model it
used, so a saved file says which pieces its disassemblies are for.

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
three-front breadth-first search needs. It is the only family the front
ends use; tests can build the compass part alone (the 145 paths,
`rotationSettings_t::wideAmounts` empty) for its smaller tables. Pruning the wide family to what
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
other way; in the search such a move is the complement turning, with the
path inverted. The file lists the pieces that follow the path, so for such
a move they include piece 0, which tells a reader the turn is inverted;
there is no flag for it. Groups with piece 0 are still
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
The GUI also says "finished, with other rotation moves" when the solve
state was found (in whole or in part) with another mode than the one now
chosen (`problem_c::solvedWithRotationMode`, in memory only).

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
  it: `<rotation axis pu pv angle pieces waypoints/>`, with `axis` x, y
  or z, `angle` 90 or -90 degrees (right-handed about the axis), `pieces`
  the pieces that follow the path and `waypoints` the path's polyline,
  `θ du dv` per pose. Every turn is written as its polyline
  (`makePath`), whether the family or the planner found it: the file does
  not name family paths, so the family can change without the format. A
  loaded turn is a `PATH_PLANNED` path; animation, `optimizeTurns` (the
  recorded path is certified again) and `burrTxt -p` ("path of N poses")
  only need the polyline. A disassembly that uses no turn saves exactly
  as one without rotation moves. A puzzle saved with rotation data (a
  rotation disassembly, the puzzle's `rotationPlay`/`rotationBevel` or a
  solution's `playUpper`/`playLower`/`playBevel`) writes `<puzzle
  version="3">` so older BurrTools versions reject it cleanly instead of
  skipping `<dt>` and `<rotation>`; plain puzzles stay at `version="2"`.
  Waypoints are saved with 17
  digits, so the loaded polyline is bit for bit the certified one; the
  loader rejects truncated paths, paths that do not run from the identity
  to the quarter turn, shifts beyond two cells or turns beyond half a turn
  (no search makes them), and impossible orientations. The polylines
  cost room: a wiggle is 17 poses, some 650 bytes. Crowell's TIC_Box_I
  solved at `-m 0.081,0` (24 solutions, 72 turns, 57 of them wiggles)
  saves 110 kB uncompressed and 6.5 kB gzipped (the `.xmpuzzle`), against
  64 kB and 3.5 kB with family names. Rotation
  moves are in no release, so the format changes freely (AGENTS.md
  "Compatibility").
* `poseAt(makePath(spec), t)` drives the FLTK animation
  (`piecePositions_c::getTrans/getTurn`, `voxelFrame_c` draws
  `T(P + d) · R(θ) · T(−P)`): what is shown is what was checked. A test
  checks the drawn cube centres at the start and end of every turn against
  the recorded states.
* `burrTxt -R`: `level: 2.3, 2 rotations`; `-p` names the turning pieces,
  axis, pivot and path.

## Rotation modes

Decided 2026-11-13 (`rotationMode_e`, `rotationSettings_t::mode`). Every mode accepts
only turns certified at the puzzle's model, a family path the tables clear
or a path `answerTurn` certifies, so results found in one mode hold in any
other; the modes differ in which candidate turns they look at.

| Mode | Groups (closure relation) | A turn no family path clears at the play | Tables |
|---|---|---|---|
| off | – | – | none (no context) |
| fast | "blocks on every family path" at the play | dropped | the model's |
| balanced (default when on) | "blocks on every family path" at the gate's play | planned (`plannedPath`, `answerTurn`) if a family path clears it at the gate's play, else dropped | the model's and the gate's |
| slow | "a cube turns onto a cube of the other piece" (end pose) | planned, no gate | the model's |

* **The gate** is play + max(play, 0.04) (`rotationGatePlay`), at most
  what leaves the cubes larger than their bevel: twice the play, but never
  less than 0.04 more, because twice a tiny measured play fails the round
  trip ("The play a puzzle needs"). The gate's tables are derived from the model's (below).
* **Slow** needs no second set of tables. Its groups are closed under the
  end-pose relation only (`rotationGeometry::endPosePivot`: the one pivot
  at which a moving cube lands on a static one, which no motion can
  avoid), so they are exactly the groups and pivots with a free end pose,
  and every such turn the family misses goes to the planner. The suite
  checks it against the exhaustive search like the other modes.
* Fast's turns are a subset of balanced's, and balanced's of slow's
  (test: "fast, balanced and slow find nested sets of turns"). A puzzle
  that comes apart in one mode comes apart in the next.
* **Surface.** burrTxt `-R [fast|balanced|slow]` (alone: balanced, or what
  `BURRTOOLS_ROT_MODE` says); the FLTK solver tab's "Rotation moves: Off /
  Fast / Balanced / Slow" (off by default, as the old check button); Python
  `solve(rotations="off"|"fast"|"balanced"|"slow")`, off by default. The choice is per solve and never saved. Changing
  it after a solve keeps the results; the solver tab then says "finished,
  with other rotation moves", so the user can solve again.
* **Turns at every state**: the search looks for turns in every state, not
  only when slides fail, so the levels stay minimal over slides and turns
  together, and a puzzle that needs no turn still pays for looking.

**What each mode costs** (2026-11-13, release build, this VM: 8 logical
CPUs, burrTxt's default threads, one run per file, nothing else running,
300 s limit per file; `burrTxt -d -R <mode> -q`, times summed over the
files, planner time summed over threads; scripts in `out/rot10-modes/`,
untracked). The slow rows are **indicative only**: the slow runs were
stopped before the end (Crowell after 234 of its 319 files, in file-list
order), as it was clear enough that slow is slow.

| Corpus, model | Mode | Apart | Time | Median | Worst | Timeouts (300 s) | To planner | Planner runs | Possible | Impossible | Planner CPU-s |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 21 decoded TICs, printed | fast | 10 | 55 s | 2.0 s | 4 s | 0 | – | – | – | – | – |
| | balanced | 16 | 103 s | 3.7 s | 8 s | 0 | 1466 | 1183 | 1162 | 21 | 20 |
| | slow | 16 | 335 s | 8.3 s | 59 s (MagellanTIC) | 0 | 1648562 | 1630177 | 2720 | 1627456 | 254 |
| Crowell, 319 files, 0.081 sharp | fast | 300 | 491 s | 1.4 s | 9 s | 0 (2 crash, #178) | – | – | – | – | – |
| | balanced | 311 | 1714 s | 3.2 s | 148 s (DiagonalCube) | 0 (2 crash) | 250967 | 132645 | 69070 | 63498 | 2242 |
| Crowell, the first 234 files | fast | 221 | 276 s | | | 0 | – | – | – | – | – |
| | balanced | 232 | 787 s | | | 0 | 44583 | 38440 | 23281 | 15150 | 406 |
| | slow | 232 | 2895 s | 5.2 s | 126 s (Rotothorpe3) | 0 | 22008768 | 20919609 | 38305 | 20881234 | 2929 |
| DraculasDentalDesaster and PelikanBurr (examples, need no turn), each at the default model and at 0.081 sharp: 4 runs | fast | 4 | 91 s | | 48 s | 0 | – | – | – | – | – |
| | balanced | 3 | 427 s | | 300 s | 1 (PelikanBurr, default model) | 1727 | 1653 | 1106 | 547 | 32 |
| | slow | 0 | 1202 s | | | 4 (all) | | | | | |

Undecided planner runs (runs − possible − impossible): none on the
decoded TICs in balanced, 1 in slow; 77 on Crowell in balanced; 9 and 70
on Crowell's first 234 files in balanced and slow. An undecided turn is
not taken.

* Balanced costs two to three times fast (the gate's second set of
  tables, the larger search and the planner) and takes 6 more decoded
  TICs and 11 more Crowell files apart. At Crowell's play 0.081 its gate
  is twice the play (0.162): the numbers agree with the "2 × play" row of
  the gate measurement below.
* Slow asks the planner about every turn with a free end pose, nearly all
  of them impossible (99.8 %), and found nothing balanced did not: the
  same 16 TICs, the same 232 of the 234 Crowell files and no extra
  solution, at 3 to 4 times balanced's time. On the big puzzles that need
  no turn it did not finish within 300 s at all.
* Balanced on PelikanBurr at the default model (play 0.002, gate 0.042)
  did not finish within 300 s either (fast: 12.5 s; at play 0.081
  balanced takes 15 s). A puzzle that needs no turn still pays for
  looking for turns at every state; this case was not looked into.
* Runs killed by the limit print no counters, so the slow rows of the big
  puzzles have none.

## The planner

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

### How (`rotationGeometry::decideTurn`)

A motion of the turning group in its plane is a curve of poses `(θ, d)`:
the angle turned, from 0 to 90°, and a shift with `|du|, |dv| ≤ maxShift`
(0.75). The planner decides whether such a curve exists in the space of
poses itself (`rotationposespace.cpp`), without sampling clearances:

* **Exact obstacles.** A moving cube at `m` (from the pivot) overlaps the
  static cube at `s` exactly when the shift lies in the convex polygon
  `s − R(θ)m + K(θ)`, `K(θ) = O ⊕ R(θ)O`, `O` the cube's outline (a
  square, or an octagon with bevels). Its edge normals are those of `O`
  and of `O` turned, so "inside", "outside" and "outside by q" are a few
  dot products per pair of cubes.
* **Boxes.** A box of poses is judged at its centre, with a bound on how
  far an obstacle moves inside the box (the angle's half width times the
  lever arm, plus the reach of the shift along the normal): FREE (every
  pose keeps the margin, 10⁻⁴), BLOCKED (every pose collides) or MIXED
  (not known). A MIXED box can be split, the angle in two or the shift in
  four, whichever is longer as a distance; boxes are split only where the
  search asks.
* **Three answers.** Each round searches the cheapest chain of
  face-adjacent boxes from the start pose to the end pose. A chain of FREE
  boxes is a motion: POSSIBLE. No chain through FREE and MIXED boxes: no
  motion with these shifts exists, IMPOSSIBLE, a proof. Otherwise the
  chain's MIXED boxes are split and the search runs again. A MIXED box at
  the resolution (1/8192) is not split; a cheapest chain that needs one
  is UNDECIDED, as is a search that runs out of boxes (500,000) or is
  stopped (`gaveUp`).
* **Which chain.** A FREE box costs next to nothing (its size, so short
  chains win), a MIXED box its deficit: how far its centre is from making
  the whole box FREE. The search then splits where a passage is likely
  (counting MIXED boxes instead took thousands of rounds on some turns).
* **Coarse first.** Boxes are split only down to a level, 1/32 at first,
  halved whenever the cheapest chain is stuck at it. A wide passage is
  found among few large boxes before any crevice is followed to the
  resolution: 16 slow solver queries went from 41 s to 1.3 s together
  (the worst from 11.6 s to 0.2 s), with paths that have 0.0003–0.009 of
  room instead of 10⁻⁵.
* **The path** is the chain's box centres, shortcut to a polyline
  (`shortcutPath`) and certified by the checker like any other path; the
  polyline is what is recorded and animated. No POSSIBLE answer has failed
  to certify (5,585 solver queries on the printed TICs).

**One question** (`answerTurn`, 2026-11-11): "can this group make this
turn at this model?" is asked of one function by everyone who asks it.
A known path (a recorded one, or one proven before) that certifies is a
yes; otherwise `decideTurn` decides, and a POSSIBLE answer comes back
shortcut (keeping half the margin) and certified, or else as UNDECIDED.
A known path may be inverse, the motion of the other pieces, as the
search records a turn in which they followed it; it is certified with
the two roles swapped. The search's planned turns and `requiredPlay`
use it; `rotationContext_c::turnPath` is
what the search asks per candidate (family, gate, planner).

The shift bound matters. With shifts up to 0.5 (the first planner's box)
eight of 98 recorded turns of the printed TICs are impossible at their
printed play; with 0.75 four of them are possible, and 1.0 adds none.

**The widest motion** (`widestTurn`) is the largest margin for which a
chain exists, bisected between the planner's margin and the smaller
clearance of the two end poses. Each step is the same search with poses
that have less room than the margin counted as BLOCKED, so a margin that
cannot be kept is ruled out like an impossible turn instead of being
refined to the resolution everywhere (6 to 26 s per such step became
milliseconds; the 98 recorded turns of the printed TICs take 0.08 s each
in the median, 6 s at worst). `turnPlan_t::clearance` is the polyline's
certified minimum clearance (Lipschitz branch and bound, within 10⁻⁴).

A first planner (2026-10-25 to 2026-11-11, never merged) searched a grid
of shifts (0.01) and angles (0.5°) with sampled clearances. It could not
tell "impossible" from "not found at this grid" except at a single angle,
gave up on a third of the turns it was asked about at the printed models,
and lost paths narrower than its cells; the pose-space search replaced it
(see "Cost" below).

In the search (`listRotations`, "Rotation modes" above), balanced runs
the planner only on turns the family clears at the gate's play but not at
the play, with groups from the gate tables' closure relation; slow on
every turn with a free end pose the family misses. A sound filter ("blocks
on every motion in the box") blocks almost nothing and made tens of
thousands of candidates per node, so balanced's gate is a heuristic: a
turn the family cannot make even at the gate's play is never planned
there, which is what slow is for. A turn and its reverse share a cache entry keyed by the
scene (sense, moving cubes and the static cubes within reach, relative to
the pivot), planned from the side with the smaller key, so an edge exists
in both directions or neither.

**Cost, and what limits it** (2026-11-11: the 21 decoded TICs at their
printed models, one `burrTxt -d -R` run each, release build, sums
over the puzzles; the grid planner is the same tree with the first
planner in place, its "none" a search that found nothing, not a proof):

| gate `slack` | planner | turns planned | possible | impossible / none | undecided / gave up | planner time | run time | apart |
|---|---|---|---|---|---|---|---|---|
| (family only) | – | – | – | – | – | – | 44 s | 10 |
| 0.02 | grid | 377 | 241 | 24 | 112 | 72 s | 164 s | 14 |
| 0.02 | pose space | 381 | 377 | 4 | 0 | 2.7 s | 93 s | 14 |
| 0.04 | grid | 1090 | 607 | 131 | 352 | 218 s | 303 s | 16 |
| 0.04 | pose space | 1183 | 1162 | 21 | 0 | 20 s | 111 s | 16 |
| 0.0864 | grid | 3868 | 714 | 2764 | 390 | 251 s | 332 s | 16 |
| 0.0864 | pose space | 4021 | 1975 | 2046 | 0 | 91 s | 188 s | 16 |

* Every turn is decided, and no query takes longer than 0.42 s (the grid
  planner gave up on a third of them at the two smaller gates).
* The planner is no longer what a run with it costs: at the present gate it
  is 3 % of the run. The rest of the difference to the family alone is
  the second set of tables the gate needs (at `play + slack`) and the
  larger search.
* The gate decides what is found. At 0.02 nearly every turn it lets
  through is possible (377 of 381), so turns are being missed behind it;
  at 0.04 ElfTIC and ThreeTIC come apart as well, and 0.0864 adds
  nothing but impossible turns. The same puzzles come apart with either
  planner: the grid's give-ups were not what kept the other five
  together (BioTIC, CagedCube_1, FantasTIC, HypnoTIC, MagellanTIC).

On Crowell's 319 files (play 0.081, sharp, 120 s limit, one run each)
the family takes 300 apart in 452 s; with the planner (gate 0.02) 311 come apart in 1125 s
(median 1.4 and 3.0 s a file; DiagonalCube with its 7592 assemblies 2.3
and 55 s). New are CagedCubeJr_4, CrypTIC, HypnoTIC, SyntheTIC_3,
TacTICian and TritanIC_6, and three puzzles gain a solution. StarTIC_1
and the four RIPs stay together; two files hold no problem (issue #178).

**The gate's width, and where the time goes** (2026-11-11, with the
gate's width settable on a development branch; counters with
`BURRTOOLS_ROT_STATS=1`; the gate's tables derived from the model's,
below). This measurement chose balanced's gate, max(play, 0.04) more. Release build, 8 threads, one run each, nothing else
running, 120 s limit; times are sums over the files, planner time is
summed over threads:

| Corpus | Gate | Apart | Time | Median | Gate passed | Planner runs | Possible | Impossible | Planner CPU-s (impossible) |
|---|---|---|---|---|---|---|---|---|---|
| 21 decoded TICs, printed | family | 10 | 45 s | 1.6 s | – | – | – | – | – |
| | 0.02 | 14 | 69 s | 2.5 s | 461 | 381 | 377 | 4 | 3 (0) |
| | 0.04 | 16 | 85 s | 3.5 s | 1466 | 1183 | 1162 | 21 | 18 (0) |
| | 2 × play | 16 | 75 s | 2.8 s | 1088 | 881 | 798 | 83 | 8 (0) |
| 6 test TICs | family | 0 | 9 s | 1.4 s | – | – | – | – | – |
| | 0.02 | 3 | 26 s | 3.4 s | 1286 | 915 | 912 | 3 | 13 (0) |
| | 0.04 | 3 | 39 s | 5.9 s | 2376 | 1736 | 1680 | 56 | 26 (0) |
| | 2 × play | 3 | 42 s | 7.5 s | 2691 | 1984 | 1714 | 270 | 28 (1) |
| Crowell, 319 files, 0.081 sharp | family | 300 | 389 s | 1.2 s | – | – | – | – | – |
| | 0.02 | 311 | 836 s | 2.0 s | 53824 | 33566 | 25205 | 8361 | 503 (258) |
| | 0.04 | 311 | 1144 s | 2.3 s | 151348 | 75272 | 43579 | 31693 | 1088 (594) |
| | 2 × play | 310 | 1705 s | 3.1 s | 672432 | 266720 | 83562 | 182979 | 1599 (577) |

No planner run was undecided except 179 at 2 × play on Crowell (gate at
play 0.243). The same puzzles come apart at 0.04 and 2 × play on the
TICs (ElfTIC and ThreeTIC need the gate a little above 0.02: 2 × their
play is 0.0216); on Crowell neither adds a puzzle to 0.02, and 2 × play
loses DiagonalCube to the time limit. The L puzzles stay together at
every width (their last pair, see "Open").

Where a run with the planner spends its time (`perf`, release build):

* **The second set of tables.** On the decoded TICs a run is about 94 %
  table building, family only or not; the gate needs a second set of the
  same cost. Since the 2026-11-11 change below it costs about half
  that: PedanTIC 2.4 → 5.2 s became 2.3 → 4.2 s, SkepTIC 2.4 → 5.9 s
  became 2.4 → 4.7 s; 24 mid-sized Crowell files 36 → 31 s. On Crowell's
  249 files under 5 s this flat cost is half of what the planner adds.
* **The planner** where many assemblies each ask many turns:
  DiagonalCube (7592 assemblies) spends 186 of its 226 planner CPU-s on
  4693 impossibility proofs, about 40 ms each (17 ms single-threaded).
  Splitting more boxes per round halves the rounds but costs more time:
  the cost is in classifying boxes, not in the repeated chain search.
* **The larger search** (more groups from the relaxed relation, more
  candidates) is small on the TICs; on RIBeye it nearly doubles the
  candidates (45110 → 82582).

**The gate's tables from the model's.** With more play a cube pair only
gets clearer, so the tables at the gate's play are those of the model
with only the model tables' blockers certified again
(`rotationTables_c(model, tighter)`); they are equal to tables built
from scratch (test), at about half the build time (0.77 → 0.38 s at
0.0126 / 0.032, radius 4). Sampling a blocker's waypoints before
certifying it made it slower. The gate's tables take the radius of the
model's.

History: with the first planner and the 145-path
family, 302 of the 319 Crowell files came apart against 280, at 5 to 200
times the time of the family-only search; the wide family alone reaches
300, so it became the default and the planner a fallback.

### Leaving the others (`rotationGeometry::decideRemoval`)

The same search with a goal that is a set of poses (2026-11-11, step 5 of
`design/2026-11-11-rotation-consistent-solver.md`): may the group, by a
motion in the plane of one axis, reach a pose from which a straight slide
takes it away? Not used by the search; a study tool for puzzles that do
not come apart.

* **The motion** is decideTurn's, judged against the same layer only:
  turning about the pivot by any angle between `-maxAngle` and `maxAngle`
  (90° by default: one query covers both senses and turns broken off),
  shifts up to `maxShift` in each in-plane direction.
* **The goal** (`removalGoal_t`). Along the axis (sign +1 or −1): the pose
  must be clear of the static cubes of every layer the group passes on
  the way out, not only its own: each moving cube's footprint against
  every static cube of its own layer and of the layers ahead of it (the
  same convex obstacles `s − R(θ)m + K(θ)`, one per such pair). Sliding
  along the axis keeps the footprints, so a pose clear of all of them
  slides out; this holds for fused prints too (the solid between two
  layers of a piece has the footprint of its cubes). In the plane along a
  unit vector `w`: clear of the obstacles of its layer swept backwards
  along `w`, `c + K(θ) ⊕ {−t w}`, a convex polygon whose support is that
  of `K` along normals with `n · w ≥ 0` and infinite along the others, so
  signed tests along the normals of `K` and the two normal to `w` decide
  it exactly.
* **Boxes** carry a second state, GOAL_ALL (every pose a goal pose, with
  the margin), GOAL_NONE (one goal obstacle covers the box, or it is
  BLOCKED) or not known, with its own list of undecided obstacles and a
  deficit. The cheapest chain ends in any box that may hold a goal pose,
  paying that deficit (STUCK if the box is at the level); a chain of FREE
  boxes ending in a GOAL_ALL box is POSSIBLE, the path ending at that
  box's centre. When every box reachable from the start is GOAL_NONE the
  answer is IMPOSSIBLE: no motion within the bounds reaches a goal pose.
* **Certificate** (`certifyRemoval`): the path by `planarScene_c::
  certifyPath`; for the axis, the end pose's clearance in a scene where
  each static cube ahead is copied into every moving layer behind it;
  in the plane, `certifyPath` of the slide up to where every moving cube
  is two cells beyond every static one along `w`.

Which slides: the two along the axis (what a turn broken off and pulled
out along its axis needs) and the 8 compass directions in the plane. Any
in-plane escape becomes, with enough shift, one of four lattice
directions (the static pieces are bounded), so the diagonals only make
answers at a small shift bound earlier. With the axis that is every
lattice direction; what is not covered is a motion that turns about more
than one axis.

Tested on boxed bars (a lid with a slot that needs a quarter turn; two
lids with crossed slots, which the group must clear both of; a gap in
the ring for the plane) and on random scenes (every POSSIBLE certified,
no IMPOSSIBLE where the start pose already slides out).

**L 36 and L 42** (`rotation-study/scripts/removal.cpp`, the last two
pieces of the files' last snapshot, logs in `out/review-194/removal/`,
release build, nice 19 on a loaded VM): at 0.03 / 0.04 every query is
IMPOSSIBLE, in all three planes, along both axis directions and the 8
plane directions, either piece moving, angles up to ±90° with shifts up
to 2.5 and up to ±180° with shifts up to 4 and 10 cells: 30 proofs per
pair and setting, at most 0.9 s each (mostly 0.01–0.6 s, 250–7,000
boxes). Where the class starts to part them (shift 2.5, ±90°): L 36 between
play 0.07 and 0.09 at bevel 0.1 and between 0.10 and 0.12 at bevel 0.04 (by a turn
about Z of about −23° and a slide along Z, after a shift of 2.5 cells),
not at play 0.03 up to bevel 0.2 (with shifts up to 10: possible there,
as the earlier escape test found); L 42 between 0.15 and 0.2 at both
bevels. The check model 0.1 / 0.1 parts L 36's pair (POSSIBLE,
certified): about the pivot of its known turn (doubled 3, 3, 0) with the
default shift bound by an in-plane slide −v after a turn broken off at
−73° (3.1 s, 18,219 boxes); about the piece's centre only from a shift
bound of 2.5 (1.8–15 s per query, 60,000–420,000 boxes). So L 36 and L 42
do not part at 0.03 / 0.04 by any motion of one rigid piece in one plane
followed by a straight slide: a removal move in the search would not take
them apart.

## The play a puzzle needs

**Clearance and play.** For sharp cubes, more play `Δ` takes `Δ/2` off
every face, and for convex shapes shrunk by `δ` each the signed distance
grows by at least `2δ`: along a fixed path, clearance at `p + Δ` is at
least clearance at `p` plus `Δ`. So a path with smallest clearance `c < 0`
is clear at `p − c`. Only in that direction: taking play away can cost
more than `Δ`, because a grown square's corners stick out (the first
version used it both ways and wrongly reported TriumviraTIC at 0.004). More
play and more bevel both only take material away, so both only add turns.

**Per turn** (`rotationGeometry::requiredPlay`): more play only adds
motions, so the smallest play with a POSSIBLE answer is bisected to
0.0005, starting from the model's play (or from larger plays, up to 0.4,
when the turn is not possible there; plays under 0.001 are not looked
at). `playBound_t::upper` is proven to work along the returned,
certified path. `lower` is the largest play with an IMPOSSIBLE answer,
bisected the same way: at it no in-plane motion with shifts up to
`maxShift` gets through (a wilder one might). Between the two the
planner is undecided or within the tolerance; on the printed TICs'
turns the gap is 0.0014 at most, and a turn takes 0.1 s in the median,
12 s at worst (those that need next to no play: their passages are
hairlines near the smallest play looked at).

**Per disassembly** (`optimizeTurns`): every turn's `requiredPlay`,
`turnPlay_t::upper` and `lower` the largest of them; `upper` is a proven
upper bound on the play the puzzle needs, `lower` bounds only this
disassembly's hardest turn. Each turn's path is replaced by the widest
path at the model's play, the motion with the most room, the one to
animate. The planner wants its margin and its shift bound, so it can miss
a path the solver's tables certified (one clear by a hair, or with a
larger shift): a turn whose recorded path is certified at the
model's play is bounded by that play: `requiredPlay` asks every play
with the recorded path as a known path, so the bound can also fall below
the model's play along it. When the widest motion finds no certified
path, the recorded one stays; every path a turn gets is certified at the
model's play. `optimizeControl_t` adds progress and a stop between turns.
(`upper` / `lower` were `needs` / `atLeast` until
2026-10-30: "needs" read as the minimum, which it is not.)

**The bevel at a given play** (`rotationGeometry::requiredBevel`,
2026-11-11): the printer fixes the play, and what is left to choose is
the bevel. More bevel only takes material away, so it only adds motions,
and the same bisection as for the play (one function, `bisectModel`)
runs over the bevel at the model's play: `upper` the smallest bevel
proven possible (0 when the turn works sharp), `lower` the largest
proven impossible, not found when even the largest bevel the play
allows (`validModel`) does not do. `optimizeTurns` measures both per
turn; `turnPlay_t::bevelUpper`, `bevelLower`, `bevelFailed` are the
disassembly's. These are two one-dimensional answers (play at the
puzzle's bevel, bevel at the puzzle's play), not the trade-off between
the two.

**Per puzzle**: `burrTxt -M play:lo,hi` / `bevel:lo,hi` bisected the
smallest play or bevel at which a puzzle comes apart over whole solves
(2026-10-25 to 2026-11-13). It was a debugging aid and was removed with
the modes; searching upwards for the play at which a puzzle comes apart
(grssam's "reverse optimize", #194) is not planned now.

**Kept with the solution.** `solution_c::setPlay` stores `upper`, `lower`
and the bevel they hold at (`playUpper`, `playLower`, `playBevel` on
`<solution>`), and the bevel bounds with the play they hold at
(`bevelUpper`, `bevelLower`, `bevelPlay`, all three or none); a new
disassembly drops them. burrTxt `-P` prints both, the GUI's moves
tooltip and the Optimize Turns dialog show both. burrTxt never wrote files, so
`burrTxt -w file` writes the solved puzzle: the problem runs the assembler
as in the GUI's solve thread (it ends solved, with its counts), solutions
come in as found, and the model they are for is stored; with `-x` the
re-disassembled solutions are replaced.

**Optimize Turns (FLTK GUI).** A Solver tab button runs `optimizeTurns`
on the selected solution on a worker thread, with progress and Cancel, on
a copy of the disassembly (the main window keeps animating the original).
It only reports: the play the disassembly needs at the puzzle's bevel and
the bevel it needs at the puzzle's play, each with its upper and lower
bound, next to the puzzle's play and bevel and the margin (the puzzle's
value minus the upper bound; below 0 a turn may not work as printed).
**Keep** stores the measurement with the solution and gives its turns the
widest paths at the puzzle's play; the puzzle's model is never changed:
the printer fixes the play.

**The round trip** (2026-11-11): solve at a model, measure what the
disassembly needs, solve again there; the puzzle must come apart, since
the search and the measurement ask the same question (`answerTurn`).
AAA_most_rotations does in the suite; `rotation-study/scripts/roundtrip.py`
runs a list of puzzles (`-R balanced`, or another mode; the columns below
were measured with the gate's width then settable). The
decoded TICs and the test TICs at their printed models; "needs play" at
the model's bevel and "needs bevel" at its play are the smallest upper
values over the solutions (0.0013 is the smallest play looked at, 0.001,
to the tolerance); a cell says whether it came apart again at the
measured play / at the measured bevel:

| Puzzle | Model | Needs play | Needs bevel | gate 0.02 | gate 0.04 | gate 2 × play |
|---|---|---|---|---|---|---|
| BioTIC | 0.0092,0.0653 | – | – | together | together | together |
| CagedCube_1 | 0.0108,0.0653 | – | – | together | together | together |
| ElfTIC | 0.0108,0.0544 | 0.0013063 | 0.03485 | together | yes / yes | **no** / **no** |
| FantasTIC | 0.0108,0.0544 | – | – | together | together | together |
| GalacTIC | 0.0108,0.0653 | 0.0062063 | 0.057137 | yes / yes | yes / yes | **no** / yes |
| HypnoTIC | 0.0108,0.0598 | – | – | together | together | together |
| LocomoTIC | 0.0108,0.0544 | 0.0016125 | 0.03485 | yes / yes | yes / yes | **no** / yes |
| LunaTIC | 0.0108,0.0544 | 0.0013063 | 0.03485 | yes / yes | yes / yes | **no** / yes |
| MagellanTIC | 0.0108,0.0544 | – | – | together | together | together |
| NotchedTIC | 0.0108,0.0544 | 0.0016125 | 0 | yes / yes | yes / yes | yes / yes |
| ocTIC | 0.0108,0.0544 | 0.0013063 | 0 | yes / yes | yes / yes | yes / yes |
| PackTIC_7 | 0.0108,0.0544 | 0.0013063 | 0.03485 | yes / yes | yes / yes | **no** / yes |
| PedanTIC | 0.0246,0.0653 | 0.016856 | 0.051781 | yes / yes | yes / yes | yes / yes |
| scRIBble | 0.0117,0.0589 | 0.0013344 | 0 | yes / yes | yes / yes | **no** / yes |
| SkepTIC | 0.0108,0.0544 | 0.002225 | 0.03485 | yes / yes | yes / yes | yes / yes |
| StarTIC_2 | 0.0108,0.0544 | 0.0016125 | 0 | yes / yes | yes / yes | yes / yes |
| ThreeTIC | 0.0108,0.0544 | 0.0016125 | 0.03485 | together | yes / yes | **no** / **no** |
| tRIBe | 0.0117,0.0589 | 0.0013344 | 0.033131 | yes / yes | yes / yes | yes / yes |
| tRIBes | 0.0140,0.0707 | 0.0014062 | 0 | yes / **no** | yes / yes | **no** / yes |
| TriumviraTIC | 0.0140,0.0707 | 0.0014062 | 0.028446 | yes / **no** | yes / yes | **no** / yes |
| TwisTIC | 0.0108,0.0544 | 0.0013063 | 0.03485 | yes / yes | yes / yes | **no** / yes |
| L 36 | 0.03,0.04 | – | – | together | together | together |
| L 41 | 0.03,0.04 | – | – | together | together | together |
| L 42 | 0.03,0.04 | – | – | together | together | together |
| TIC 28 | 0.0126,0.032 | 0.012238 | 0.03125 | yes / yes | yes / yes | yes / yes |
| TIC 2.99 | 0.0126,0.032 | 0.012238 | 0.03125 | yes / yes | yes / yes | yes / yes |
| TIC 2.9 | 0.0126,0.032 | 0.012238 | 0.03125 | yes / yes | yes / yes | yes / yes |

Every failure is the gate, not the measurement: each measured turn is
possible (`answerTurn` certified a path), but no family path clears it at
the gate's play, so the search never asks the planner. tRIBes at 0.014
sharp and TriumviraTIC at 0.014 / 0.0284 come apart with the gate at
0.04, 0.1 and 0.3 and not at 0.02; with the gate at 2 × play, a measured
play of 0.0013 leaves a gate of 0.0026, and most puzzles stay together.
The gate at 0.04 passes every case, and so does balanced's gate, which is
never narrower.

Not modelled, and worth saying with any number: flexing and wear,
friction, imperfect alignment, axial wobble during a turn. The numbers
are the play of ideal rigid cubes.

## Front ends

| | burrTxt | FLTK GUI | Python | Qt GUI |
|---|---|---|---|---|
| Rotation moves and mode | `-R [fast\|balanced\|slow]` (alone: balanced, or `BURRTOOLS_ROT_MODE`) | Rotation moves: Off / Fast / Balanced / Slow (cube grids) | `solve(rotations="balanced")`, default off | not yet (Solver workspace) |
| Play and bevel | `-m play,bevel` (the puzzle's), else the file's | Play, Bevel at the top of the Puzzle tab (undoable edit) | `Puzzle.rotation_model` | — |
| Measure a solution's play and bevel | `-P` | Optimize Turns (reports only) | — | — |
| Write results | `-w file` | save | `Puzzle.save` | — |

**For a new front end** (the Qt GUI's Solver workspace is still to be
built): everything above goes through the library, and the FLTK GUI
(`src/gui/mainwindow.cpp`, `optimizeturns.*`) is the reference. What it
has to get right:

* **Settings:** `solverConfig_t::rotation` (`rotationConfig_t::mode`)
  holds the mode the user chose, `solverConfig_t::resolve()` gives
  `solverSettings_t` with `rotation`; never read `BURRTOOLS_*` yourself.
  Offer the four modes (`rotationModeName`), off by default, balanced as
  the usual choice; the choice is per solve, not the puzzle's. Record the
  mode a solve ran with (`problem_c::noteSolveRotationMode`) and say when
  the chosen one differs (`solvedWithRotationMode`).
* **Solving:** pass `rotationContextFor(problem, settings.rotation,
  /*background*/ true)` to the solver (`solveThread_c` does it); `nullptr`
  means rotation moves are off or impossible (only cube grids). After a
  solve, `solveThread_c::rotationsSkipped()` (or
  `rotationContext_c::getSkipped()`) non-zero means "finished, turns may
  be missing" and must be shown. Building tables takes a moment the first
  time; outside a solve thread (the FLTK add-disassembly buttons) it runs
  on the calling thread.
* **The model:** it is the puzzle's, one for all problems and shapes; put
  its fields with the puzzle's settings, not the solver's. Edit through
  `puzzle_c::setRotationModel` / `clearRotationModel`, validate with
  `puzzle_c::validRotationModel`, and record it as an undoable edit. A
  change makes every problem unknown, and a tighter model deletes the
  disassemblies that turn: **drop anything that points into a solution's
  disassembly (an animation, `disasmToMoves_c`) before calling it**, and
  rebuild it after (`mainWindow_c::cb_RotationModel`).
* **Animation:** `disasmToMoves_c` / `piecePositions_c::getTrans` and
  `getTurn` give each piece's orientation and, during a turn, its axis,
  pivot and pose; draw `T(P + d) · R(θ) · T(−P)` like `voxelFrame_c`, or
  turning pieces cut through others. Show the moves line with
  `separation_c::sumRotations` and the measured play (`solution_c::getPlay`).
* **Optimize Turns:** run `optimizeTurns` on a copy of the disassembly on a
  worker thread with `optimizeControl_t` (`stop`, `done` for progress);
  report the play and bevel bounds with the puzzle's values and the
  margin, never change the model; on Keep, `solution_c::setDisassembly`
  and `setPlay` on the main thread, with the animation dropped first, as
  above.
* **STL:** `stlExporter_c::followModel` with `rotationModelOf(puzzle)`, and
  show `stlModelNote`; the view's STL style draws `voxel_c::getSTLMeshFor`
  of the puzzle's stored model (both already in the Qt GUI).
* **Memory:** tables are kept between solves (two sets);
  `releaseRotationTables()` frees them.

burrTxt says when `-m` or `-P` have no effect. Python keeps
tables between solves too (`release_rotation_tables()`).

**STL export.** The cube exporter can follow a model
(`stlExporter_c::followModel`: offset = play / 2 · unit size, bevel =
bevel · unit size) until bevel or offset is set by hand, so the printed
pieces are the ones the turns were checked for. Both exports start from
the puzzle's model, and a line under the parameters says whether the
pieces are the checked ones (`stlModelNote`).
With the default model the export starts from exactly its old defaults.
The dialogs now pass on only the fields actually edited (they used to
write every field back from two-decimal text, which would have overwritten
the values just derived from a new unit size) and show four significant
digits.

**STL view style.** The 3D views draw `voxel_c::getSTLMeshFor(model)`
where the puzzle stores a model, otherwise their own look.

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
  cells, and a GalacTIC turn): blocked for the family, decided possible
  with a certified path and impossible with less play, a turn and its
  reverse decided alike, a turn through a wall proven impossible at every
  play and bevel, undecided when stopped, out of boxes or too close to
  call, never contradicting a certified path (random scenes), a wide
  passage found before the crevices; the widest motion's value below
  every sampled clearance and above the first motion's, its reverse
  certified with the same value; `requiredPlay` possible at its upper
  and impossible at its lower bound; more play adds at least as much
  clearance; planned paths survive save and load bit for bit.
* **Disassemblies** (`test_rotation_disassembly.cpp`): AAA_most_rotations
  does not come apart without turns and comes apart as 2.3 with them (at
  play 0.081, balanced); every step replays as a valid motion and after a
  save/load round trip; the animation matches the states; turns offered do
  not depend on piece order (each mode); the closure search matches the
  exhaustive one (fast, balanced, slow); the three modes' turns are
  nested; `endPosePivot` lands a cube on the other; a model change resets
  and a tighter one drops turning disassemblies; `optimizeTurns`'
  progress and stop.
* **Model and front ends**: the model in the file (round trip, refused
  values, edits making solutions unknown), solution play, the exporter
  following a model, the shape model helpers, the STL look at a model,
  the modes and the removed variables (`test_solver_config.cpp`), the
  in-memory solve mode, burrTxt `-m`, `-R` with and without a mode,
  `BURRTOOLS_ROT_MODE`, `-w` (CLI smoke test), the Qt STL dialog, Python's
  `rotation_model` and `rotations` modes.

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
| 2026-10-25 | Planner, `PATH_PLANNED`, `-P` | Crowell's blocked turns sit at the threshold; designers need the play |
| 2026-10-28 | Kept tables; `rotationConfig_t` and env toggles; wide family default; pruning rejected | the table build is the cost; strictly better results |
| 2026-10-30 | Rounding removed; model stored per problem; `upper`/`lower`; `-M bevel:`; `-w`; play per solution | the model is the pieces', results must persist |
| 2026-10-31 | The model moves to the puzzle; what-if questions take it as an argument | shapes are shared between problems, so the pieces are the puzzle's; a problem's need follows from its solutions |
| 2026-10-30 | STL export and view use the model; default back to the export's; hints | print and view the checked pieces without changing plain puzzles |
| 2026-10-30 | Optimize Turns | the measured tightest play is the result designers want |
| 2026-11-11 | The planner decides turns in the space of poses (`decideTurn`); the grid planner is gone | three sound answers instead of "found" and "gave up", narrow passages no longer lost between grid cells, and 3 to 27 times less planner time |
| 2026-11-13 | Rotation modes off / fast / balanced (default) / slow; gate play + max(play, 0.04); a model change always resets; Optimize Turns only reports | one question for every front end, the planner by default, a slow mode without the gate's heuristic; the printer fixes the play |

## Open

* **Play 0.03 / bevel 0.04** (2026-11-03,
  [`rotation-study/results/2026-11-03-test-tics.md`](rotation-study/results/2026-11-03-test-tics.md);
  planner 2026-11-11): with the family alone the test TICs come apart,
  the L puzzles do not. With the planner TIC 28, Clutch 4 and Clutch 3 come
  apart at their printed model as well (0.0126 / 0.032: their blocked
  turn needs play 0.0122 there, which the first planner's grid missed),
  and the nine turns of the L route that the family misses are all
  possible. L 36, L 41 and L 42 still stay together, at every gate up to
  0.3, and for L 36 and L 42 it is their last two pieces: of the 144 and
  116 quarter turns of one about a lattice pivot with a free end pose,
  two and none are possible at 0.03 / 0.04, and neither of the two
  separates the pair. Nor does any motion in one axis plane take the
  piece out (`decideTurn` towards a pivot three cells off, shifts up to
  10 cells, both senses, three axes: impossible). L 36's pair gets out
  about Z from play 0.09 at bevel 0.1, play 0.2 at bevel 0.04 or bevel
  0.2 at play 0.03; L 42's from 0.2 / 0.1. A turn broken off and a slide
  along its axis or in the plane does not part them either (proven with
  `decideRemoval`, shifts up to 10, turns up to ±180°; "Leaving the
  others"). So what they need is a motion about two axes, something
  outside the model (flexing), or the files' last snapshot is not where
  the pieces part in the hand. L 41 has no snapshots.
* **Physical ground truth**: grssam's verified moves (Clutch 4 and
  others) as a regression table; why printed TICs need more bevel than
  printed.
* **The gate** (settled 2026-11-13, "Rotation modes"): balanced gates at
  play + max(play, 0.04), slow asks the planner about every turn with a
  free end pose. Left open: a sound filter (which would remove the
  heuristic from balanced) and cheaper impossibility proofs
  (DiagonalCube).
* **What `decideTurn` does not look at**: one group turning in one
  plane, shifts up to 0.75, the end pose on the lattice. Motions to any
  pose from which the group slides out are decided by `decideRemoval`
  (above), outside the search; whether the search should get such a
  removal move is open (L 36 and L 42 do not need one, see there).
* **Planner time** where it is still seconds: `requiredPlay` on turns
  that need next to no play (up to 12 s), and the coarse-first ladder on
  PedanTIC, which it made slower (planner 0.5 s instead of
  0.2 s at the present gate).
* **GUI**: the Qt GUI's Solver workspace (rotation modes, model fields,
  Optimize Turns); its drawing of turned pieces waits for PR #181 (local
  branch `rotations-qtgui-draw`). The FLTK add-disassembly buttons run on
  the UI thread, in slow mode too.
* `--verify-rotations` (re-certify every turn of a file's solutions) was
  planned, not built; `-x -R` re-disassembles instead.
* Half turns as single moves; rotations in solution rankings (with turns,
  the search finds shorter disassemblies that turn partly removed pieces;
  whether rotations should only be used when slides fail is a product
  decision).
* **Fused prints**: the checker treats every cube as separate, a print
  fuses a piece's cubes (the play-wide gaps between them solid, concave
  edges filled, stacked cubes fused across the slab between layers). A
  turn can be certified that a print misses by up to about play/2 to
  play/√2: nothing at the default play, over 1 mm at 0.081. Preferred
  fix: fused shapes in `planarScene_c` as an exact check of each turn
  the tables accept (they stay a sound pre-filter); tables of the fused
  shapes would be faster but rework the tables.

## Reproducing

Build the study helpers against the release library (`just build-release`
first):

```bash
c++ -I build-rel -I src -I subprojects/manifold-3.5.2/include -DNDEBUG -std=c++20 -O2 -pthread \
    design/rotation-study/scripts/xcheck.cpp -o xcheck \
    -Wl,--start-group build-rel/libburr_core.a build-rel/subprojects/manifold-3.5.2/libmanifold.a -lz -Wl,--end-group
# same for need.cpp and removal.cpp; diag.cpp is of its date (it wants the three-field model and the first planner)
./xcheck ".../HepTIC_SolutionFile.xmpuzzle"                     # snapshot vs state match
FROM=1 PARTIAL=1 ./xcheck ".../MysTIC_SolutionFile.xmpuzzle"    # the blocked step
./diag ".../MysTIC_SolutionFile.xmpuzzle" 11                    # family vs grid search over plays
REQ=1 ./diag ...; PLAN=1 [FEAS=1] ./diag ...; FAM=1 ./diag ...; DUMP=1 ./diag ...
./need scene.txt 0.04                                           # what one dumped turn needs (requiredPlay, widestTurn)
MAXSHIFT=2.5 ./removal ".../L 36...xmpuzzle" 37 0.03,0.04 0.1,0.1  # the last pair leaving by a motion and a slide
build-rel/burrTxt -d -R -P puzzle.xmpuzzle                      # balanced; -R fast, -R slow
build-rel/burrTxt -d -R slow -P -m 0.0126,0.032 -w solved.xmpuzzle puzzle.xmpuzzle
./build/test_burrtools "[rotations-table-stats]"                # table sizes and build times
```

The corpus scripts (`tics.py`, `crowell.py`) and their results are
described in [`rotation-study/README.md`](rotation-study/README.md).
