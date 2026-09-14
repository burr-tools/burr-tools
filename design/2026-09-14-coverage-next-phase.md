# Test Coverage, Next Phase — Design

**Date:** 2026-09-14
**Status:** Planned, not started
**Predecessor:** [`2026-09-13-test-coverage-stack.md`](2026-09-13-test-coverage-stack.md),
delivered as #55, #56, #58, #59, #61 and #63.

## Where things stand

The first phase added unit coverage for voxels and symmetries, XML
serialization, the disassembly engine and the puzzle/problem model, and fixed
nine defects those tests uncovered. Line coverage of the engine went from 51.4%
to 56.4%.

Measured on master at `5f07969b`, on macOS with Apple Clang:

| | |
|---|---|
| Lines | 56.4% (9031 of 16000) |
| Functions | 65.9% (813 of 1233) |
| Branches | 35.3% (6444 of 18251) |
| Suite | 18328 assertions, 190 cases |

**Every figure in this document is from that toolchain.** CI reported 54.0%
(6937 of 12858 lines) for the same commit on Linux with gcc. The denominators
differ because line and branch counting is compiler-dependent, so the two
numbers are not comparable and the Linux one is canonical for tracking progress
across pull requests. The *relative* sizes below — which areas hold the
uncovered mass — hold on either toolchain; the absolute line counts do not.
Re-measure before quoting any of them as a current figure.

Note the filter, too. `just coverage` covers `src/lib`, `src/tools` and
`src/halfedge`. The GUI is not counted at all, so this describes the engine,
not the application.

Open at the time of writing: #65 (the fast/slow suite split, and Windows tests
under Wine). #64 merged while this was being written.

## Reproducing the measurement

```
just coverage          # summary
just coverage-html     # per-line HTML report
```

Three things to know before trusting a number:

- **Delete `build-cov` first if in doubt.** Stale `.gcda` files accumulate
  across runs and *inflate* the result. A measurement taken over a dirty
  coverage directory once read 53.5% against a true 52.8%.
- **macOS needs the `xcrun llvm-cov gcov` shim**, which the justfile already
  passes. Apple Clang emits data plain `gcov` cannot parse.
- **To get a before/after pair without two builds**, build once and run the
  instrumented binary twice — once with the new tests excluded by tag
  (`./build-cov/test_burrtools "~[model]"`) and once in full, running `gcovr`
  after each and deleting `.gcda` in between. The "before" figure is then
  exact rather than approximated from a different build.

## Where the uncovered code actually is

6,969 lines are uncovered. They are not spread evenly:

| Lines | Area |
|---|---|
| 2,062 | halfedge mesh library, `voxel_2_mesh.cpp`, `stl*.cpp`, `triangulate.h` |
| 1,393 | generated symmetry tables (`src/lib/tabs_*/*.inc`, 10 files) |
| 1,069 | everything else, chiefly error and write paths |
| 965 | grid-specific voxel implementations, `voxel.cpp`, `symmetries_*.cpp` |
| 868 | assembler / solver core |
| 612 | `solvethread`, `print`, `converter`, `ps3dloader`, `voxeltable` |

## What is not worth chasing

**The generated tables.** `src/lib/tabs_N/symcalc.inc` is emitted by
`generator_N.cpp` and included as a function body: a long chain of
`if (v.transform(137) && pp->identicalInBB(&v))` blocks, one per candidate
transformation. Covering them all means constructing shapes that exhibit every
symmetry of every grid — combinatorially large, and close to worthless for
finding defects, because the blocks are machine-emitted and uniform. Grid
parametrization (P1) will incidentally raise them; nothing should target them
directly.

**`solvethread.cpp` (200 lines) and `print.cpp` (164).** Background-solve
threading and print output. Both are awkward to test deterministically and both
are GUI-facing.

**`converter.cpp` (61) and `ps3dloader.cpp` (59).** Legacy import paths.

That is roughly 1,900 lines not worth pursuing. Covering 60% of what remains
would put the engine near **75%** — or about 83% if generated tables were
excluded from the denominator, which is a defensible reporting change but is a
change of denominator, not new testing, and should be described as such
wherever it is reported.

`voxeltable.cpp` (104 lines, 0%) is listed above with the GUI-facing code
because only `mainwindow.cpp` and `statuswindow.cpp` consume it, but it is a
self-contained lookup structure much like the disassembler's node hashes, which
tested well. It is a cheap target, not a skip.

---

## P1 — Grid parametrization

**Goal.** The suite is overwhelmingly single-grid: roughly 65 hardcoded
`GT_BRICKS` sites against about 15 uses of `ALL_GRIDS`. Whole grid
implementations barely execute as a result.

| Lines | File | Covered |
|---|---|---|
| 267 | `voxel.cpp` | 59% |
| 196 | `voxel_3.cpp` | 34% |
| 145 | `voxel_1.cpp` | 52% |
| 136 | `voxel_4.cpp` | 34% |
| 103 | `voxel_2.cpp` | 56% |
| 68 | `voxel_0.cpp` | 70% |
| 28 | `symmetries_2.cpp` | 52% |

Plus incidental movement in the generated tables, which are grid-specific for
the same reason — `tabs_2/symcalc.inc` sits at 5% largely because few tests use
that grid.

**Approach.** Convert `GT_BRICKS`-only cases in `test_voxel.cpp` (21 sites),
`test_symmetries.cpp` (7), `test_voxel_connect.cpp` (6) and
`test_puzzle_model.cpp` (8) to run over `ALL_GRIDS`, using the existing
`gridName()` helper so a parametrized failure names the grid that broke.

**Hazards.** This is not a mechanical find-and-replace.

- A shape that is legal on the cube grid may be illegal or differently shaped on
  the rhombic or tetra-octa grids. Fixtures built with `fromLayers()` ASCII art
  need per-grid review, and some will need per-grid expected values rather than
  one shared constant.
- Some cases are *legitimately* grid-specific — anything asserting a particular
  transformation number, neighbour count or bounding box. Parametrizing those
  produces either a false failure or, worse, an assertion so loose it holds on
  every grid and therefore tests nothing.
- Prefer asserting a property that holds on all grids (a transform composed with
  its inverse is the identity) over a value that happens to match on all of them.

**Expected yield.** 700–1,200 lines. **Cost:** moderate. **Defect yield:**
proven — the sphere `transAdd` defect fixed in #59 came from exactly this kind
of grid-specific code.

---

## P2 — Mesh and STL export

**Goal.** The largest genuine gap, and the newest code in the repository.

| Lines | File | Covered |
|---|---|---|
| 606 | `halfedge/modifiers.cpp` | 1% |
| 475 | `voxel_2_mesh.cpp` | 0% |
| 231 | `halfedge/polyhedron.cpp` | 44% |
| 211 | `halfedge/vector3.cpp` | 23% |
| 109 | `halfedge/vertex.cpp` | 20% |
| 97 | `halfedge/face.cpp` | 14% |
| 72 | `stl_2.cpp` | 0% |
| 70 | `stl.cpp` | 0% |
| 55 | `triangulate.h` | 74% |
| 40 | `stl_0.cpp` | 46% |
| 28 | `halfedge/volume.cpp` | 0% |

2,062 lines in total.

**Approach.** This is the one package that deserves its own design document
rather than being started directly from this one. The outline:

- `vector3` is pure arithmetic and should be tested directly and exhaustively —
  it is 211 uncovered lines of small, total functions, the cheapest real
  coverage in the repository.
- `polyhedron`, `face`, `vertex` and `halfedge` form a mesh data structure whose
  invariants are testable without golden files: every half-edge has a twin, twins
  are mutual, face loops close, Euler characteristic holds for a closed mesh.
- `modifiers.cpp` is the largest single uncovered file at 1%. Its operations
  should be checked against invariants preserved or established by each
  modifier, not against recorded output.
- The STL writers should be checked by parsing back what they emit, in both
  ASCII and binary form, rather than by byte comparison against a fixture.

**Prefer invariants to golden files.** Mesh output is verbose and sensitive to
floating-point detail; a recorded-output test will either be brittle or will be
"fixed" by re-recording, which silently ratifies whatever regression prompted
it.

`test_cubemesh.cpp`, `test_minkmesh.cpp` and `test_manifold_smoke.cpp` already
exist and establish some vocabulary to build on.

**Expected yield.** 800–1,400 lines. **Cost:** high — genuine design work.
**Defect yield:** likely high; it is the most recently written code and the
least exercised.

---

## P3 — Assembler depth

**Goal.** `assembler_1.cpp` (403 uncovered, 57%) and `assembler_0.cpp` (395,
48%) are the heart of the application, where a defect is most costly.

**Approach.** These need actual solves, which is why this is listed last. Once
#65 lands the runtime budget exists: a `[stress]`-tagged solver package runs in
the slow suite, so it costs CI and `just test-all` but not the everyday `just
test` loop.

Use the bundled example puzzles as oracles — several ship saved solutions whose
counts are recorded in the file, so `getNumAssemblies()` and
`getNumSolutions()` have a known-correct expected value that does not have to be
invented. Cover the assembler's configuration surface rather than just running
more puzzles: piece ranges, colour constraints, `maxHoles`, and the early-exit
paths.

**Expected yield.** 300–500 lines. **Cost:** moderate, mostly in wall-clock.
**Defect yield:** high value per defect found, though assertions beyond
"produced the expected solution count" are hard to write.

---

## P4 — Error and write paths

**Goal.** Branch coverage is 35.3% against 56.4% of lines, and the gap is
concentrated in error handling and the serialization *write* side. This is the
best defect density in the codebase: eight of #59's nine fixes came from
`xml.cpp` error paths alone.

| Lines | File | Covered |
|---|---|---|
| 225 | `tools/xml.cpp` | 68% |
| 208 | `problem.cpp` | 64% |
| 156 | `disassembly.cpp` | 63% |
| 104 | `voxeltable.cpp` | 0% |
| 78 | `assembly.cpp` | 82% |
| 66 | `puzzle.cpp` | 70% |
| 16 | `tools/gzstream.cpp` | 84% |

**Approach.**

- `xml.cpp`'s remaining mass is the writer and the parser's rejection branches.
  The parser's error paths are reachable by feeding malformed documents; the
  writer's by round-tripping documents that use each feature.
- `disassembly.cpp`'s uncovered remainder is `state_c::save()`,
  `separation_c::save()` and parser error branches — writing a disassembly back
  out to XML. #61 deliberately left this to a serialization-focused package.
- `problem.cpp`'s untested surface was catalogued while writing #63: the
  colour-constraint API (`allowPlacement` / `disallowPlacement` /
  `placementAllowed`), part groups (`setPartGroup`, `getPartGroupId`,
  `getNumberOfPartGroups`), `getPartMinimum` / `getPartMaximum`,
  `exchangeParts`, `addSolution`, `setMaxHoles` and the assembler accessors.
- `puzzle.cpp`: `removeProblem` and `exchangeProblems` are untested.
- `assembly.cpp`: `validSolution`, `containsMirroredPieces`, `addPieces` and
  `getPieceInfo` are untested. `transform` is currently exercised at one
  transformation on a shape where the per-piece re-normalisation branch never
  fires, and always with a null `mirrorInfo_c *` — mirror handling and
  re-normalisation are both uncovered. `smallerRotationExists` is only called
  with `pivot == 0`, no mirror info and `complete == false`.
- `voxeltable.cpp` is a self-contained lookup structure and can be tested
  directly.

**Expected yield.** 590–700 lines, and a disproportionate share of the branch
coverage. **Cost:** low. **Defect yield:** the highest of the four.

---

## Recommended order

**P4, then P1, then P2, with P3 last.**

P4 first: it is the cheapest package and has by far the best defect density.
P1 second: mechanical enough to go quickly, with a track record of finding real
bugs in grid-specific code. P2 third, and it should get its own design document
first — it is the biggest genuine gap but also the most design work. P3 last,
because it buys the fewest lines per hour even though what it protects matters
most.

Each package should be a single pull request against master, as the first phase
did. The first phase stacked five pull requests on each other and every rebase
cascaded through the whole chain; there is no reason to repeat that now that the
predecessor work has merged.

## Conventions these tests follow

Learned in the first phase, mostly the hard way.

**Every assertion must be able to fail.** The recurring defect in this work was
a test that passes for the wrong reason: a fixture with one shape, where
removing it dropped the count to zero and the assertion held whether or not the
code under test did anything; a symmetry fixture invariant under every
permutation; a sortedness check whose input was already sorted. Before
committing a case, ask what would have to break for it to fail — and where it is
cheap, find out by breaking the production code, watching the test fail, and
reverting.

**State the property the unit owns**, not the property the whole system happens
to exhibit. A monotonicity assertion that holds for the system but not for the
unit will be wrong the first time a legitimate input violates it.

**Do not fix production code inside a coverage change.** Pin the current
behaviour, comment what looks wrong, and raise the fix separately — #59 and #64
are the precedent. A judgement call that a maintainer might reasonably decline
belongs in its own commit, last, so it can be dropped alone.

**No commas or asterisks inside a `TEST_CASE` name.** The test framework reads a
comma as a separator between specs and an asterisk as a wildcard, so such a case
cannot be selected by name. A name may span two adjacent string literals — check
the concatenated name, not the first fragment. Ten such names shipped in one
change because a pattern match only inspected the first literal; the reliable
check is to run each name as an exact filter and confirm it selects.

**Guard assertion tests with `#ifndef NDEBUG`.** `bt_assert` throws
`assert_exception` in debug builds and compiles to `((void)0)` under `NDEBUG`,
so an unguarded `REQUIRE_THROWS_AS` fails in a release build.

**Watch declaration order for reference-counted fixtures.** `disassemblerNode_c`
holds a manual refcount and other fixtures hold raw pointers into it. Automatic
variables are destroyed in reverse declaration order, so anything referencing a
fixture at scope exit must be declared after it. Getting this wrong aborts the
test binary rather than failing an assertion.

**Read saved solutions rather than solving.** Example puzzles ship assemblies
and disassembly trees that parse straight from XML via
`getSavedSolution(n)->getDisassembly()`. They are repo data, so they cannot
shift underneath a test the way solver output could, and they cost nothing.

**One fact, one assertion.** Asserting a single property once per element of a
large fixture inflates the suite's assertion count without covering anything
further. Scan for the first violation and assert on that; the failure still
names the offending index.

**Tag expensive cases `[stress]`** so they land in the slow suite. `just test`
is the fast loop; `just test-all` is what CI runs and what should pass before
calling work done.

## Risks

**Parametrizing across grids can weaken assertions.** The failure mode is an
assertion loosened until it holds on every grid, which is worse than the
single-grid version it replaced. Where grids genuinely differ, prefer a per-grid
expected value over a weaker shared one.

**Mesh tests can ossify.** Golden-file comparisons of mesh output get
re-recorded rather than investigated when they fail. Invariants do not have this
failure mode.

**The percentage is not the goal.** The first phase moved lines 51.4% → 56.4%
but its value was nine confirmed defects, including an unbounded loop, four XML
parser bugs and a stream failure that crashed the command-line tools on a
mistyped filename. P1 would move the number fastest; P4 would find the most
bugs. They are different jobs and worth choosing between deliberately.
