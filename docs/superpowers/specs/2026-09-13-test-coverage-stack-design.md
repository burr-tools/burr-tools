# Unit Test Coverage Stack — Design

**Date:** 2026-09-13
**Status:** Approved for planning

## Problem

BurrTools has 22 test cases and 5040 assertions, all of them integration-level:
`test_solver.cpp` drives complete puzzle solves, and `test_cubemesh.cpp` /
`test_minkmesh.cpp` / `test_manifold_smoke.cpp` exercise mesh generation
end to end. That suite is valuable, but it leaves the core domain units
untested in isolation. A bug in voxel transformation, XML escaping, or
hash-table growth surfaces only if some full puzzle solve happens to
depend on it, and the resulting failure points at the solver rather than
at the defect.

Three further problems compound this:

1. The repository has no coverage measurement at all — no `gcovr`, `lcov`,
   or `llvm-cov` in the build, the justfile, or CI. There is no baseline
   number, so no change can demonstrate that it improved anything.
2. `src/lib/bitfield_test.cpp`, `src/lib/voxel_0_test.cpp`, and
   `src/lib/main_test.cpp` are real tests written against Boost.Test that
   were never ported to Catch2. They are absent from `meson.build`, so
   they do not compile and do not run. They are dead code that looks like
   test coverage.
3. Integration tests are slow to attribute. A unit-level failure in
   `disassemblerHashes_c` currently manifests as a wrong move count in a
   Pelikan burr regression.

## Goals

- Establish coverage measurement so every subsequent change reports a real
  before/after number.
- Add unit-level tests for four subsystems: voxel/symmetry, serialization,
  disassembly internals, and the puzzle/problem model.
- Remove the three orphaned Boost.Test files, porting the two that carry
  real assertions.
- Change no production code. This is a test and tooling effort; any
  production bug the new tests uncover is reported, not silently fixed
  inside a coverage PR.

## Non-Goals

- Rewriting or restructuring the existing integration tests.
- Testing `src/gui/` (FLTK widgets and OpenGL viewports need a display and
  a different harness).
- Touching `src/lua/` or `subprojects/`, per AGENTS.md rule 5.
- Chasing a specific coverage percentage. The target is meaningful tests
  for meaningful units, with the number as evidence rather than as the goal.

## Delivery shape

Five PRs in a true stack, each targeting the previous branch:

| # | Branch | Base | Scope |
|---|--------|------|-------|
| 0 | `coverage/tooling` | `master` | Coverage measurement + this spec |
| 1 | `coverage/voxel-symmetry` | `coverage/tooling` | voxel, symmetries, bitfield, grouping |
| 2 | `coverage/serialization` | `coverage/voxel-symmetry` | xml, gzstream, save/load roundtrip |
| 3 | `coverage/disassembly` | `coverage/serialization` | hashes, nodes, movement cache, moves |
| 4 | `coverage/puzzle-model` | `coverage/disassembly` | puzzle, problem, assembly, solution |

The order is a dependency order, not a preference. PR 1 introduces
`test/test_helpers.h`, which PRs 2–4 build their fixtures on. PR 2's
puzzle load/save helpers give PRs 3 and 4 a cheap way to construct
fixture puzzles from disk.

Each PR's diff shows only its own tests, keeping review small. The cost is
that amending an early PR cascades a rebase through the rest.

## Testing style

Hybrid, matching the existing house style rather than importing a new one:

- **Property/invariant tests** for the algebraic core — transformation
  composition, symmetry group membership, roundtrip identities,
  serialization fixpoints. AGENTS.md rule 4 already mandates this style
  for solver tests, and `test_minkmesh.cpp` already pairs fixed cases with
  a random-shape stress case.
- **Example tests** for boundaries and error paths — empty voxel spaces,
  1x1x1 shapes, out-of-range `get2`, hotspot defaults, malformed XML,
  `bt_assert` rejection paths.

Conventions taken from the existing files: helpers in an anonymous
namespace at the top of the file, `TEST_CASE("description", "[tag]")`,
includes prefixed `lib/` / `tools/` / `halfedge/`.

Per AGENTS.md rule 4, no test asserts an exact iteration count.

## PR 0 — Coverage tooling

Adds a `coverage` recipe to the justfile:

- Configure a separate `build-cov` directory with `-Db_coverage=true`
  (a Meson builtin; no `meson.build` change needed).
- Build, run the suite, then run `gcovr` filtered to `src/lib`,
  `src/tools`, and `src/halfedge`, excluding `src/lua` and `subprojects`
  per AGENTS.md rule 5.
- On macOS, pass `--gcov-executable "xcrun llvm-cov gcov"`. Apple Clang
  emits gcov-format data that plain `gcov` cannot read. CI runs gcc on
  ubuntu and needs no override.
- Emit both a terminal summary and an HTML report.

Also adds a non-blocking CI job that installs `gcovr` and prints the
summary, documents the recipe in AGENTS.md, and records the measured
baseline in the PR body.

Local prerequisite: `brew install gcovr`. CI: `apt-get install gcovr`.

No production code changes.

## PR 1 — Voxel and symmetry

**Cleanup.** Port `bitfield_test.cpp` and `voxel_0_test.cpp` to Catch2 as
`test/test_bitfield.cpp` and fold the voxel case into `test/test_voxel.cpp`.
Delete all three orphaned files including `main_test.cpp`, whose only
content is the Boost test-runner entry point.

**New helper.** `test/test_helpers.h`: build a voxel from ASCII layer art,
instantiate each of the five grid types, and deep-compare two puzzles.

**Files added:** `test/test_helpers.h`, `test/test_voxel.cpp`,
`test/test_symmetries.cpp`, `test/test_bitfield.cpp`.
**Files deleted:** `src/lib/bitfield_test.cpp`, `src/lib/voxel_0_test.cpp`,
`src/lib/main_test.cpp`.

**Tests**, parametrized across `GT_BRICKS`, `GT_TRIANGULAR_PRISM`,
`GT_SPHERES`, `GT_RHOMBIC`, and `GT_TETRA_OCTA`:

- Transformation composition against the multiplication table;
  `normalizeTransformation`; identity transform is a no-op.
- `identicalWithRots` invariance under rotation; `getMirrorTransform`.
- Symmetry group membership and consistency.
- `getIndex` / `indexToXYZ` roundtrip over an entire voxel space.
- `get2` out-of-range behaviour.
- Bounding box correctness after `translate`, `resize`, `minimizePiece`.
- `connected()` for face versus edge connectivity, on both connected and
  deliberately disconnected shapes.
- `fillHoles`, `countState`, `count`.
- Hotspot defaults and `setHotspot`.
- `scale` / `scaleDown` roundtrip.
- `operator==` and `identicalInBB`, including the colour-sensitive paths.
- `grouping_c` group assignment, in both its succeeding and failing cases.

## PR 2 — Serialization

- `xmlWriter_c`: tag nesting, attribute escaping and content escaping for
  `<`, `>`, `&`, and quote characters, and `xmlWriterException_c` raised on
  a mismatched `endTag`.
- `xmlParser_c`: entity replacement text, `require()`, `skipSubTree()`,
  `prevTag()`, and exceptions on malformed input. Extends the existing
  `test/malformed/` corpus rather than replacing it.
- Roundtrip: construct a `puzzle_c` in memory, save it through
  `xmlWriter_c`, reload it, and assert deep equality across shapes,
  colours, problems, groups, and solutions.
- `gzstream`: a missing file, and plain-versus-gzip detection through
  `openGzFile`.

Temporary files are written to a Catch2-managed temporary directory, never
into the source tree.

**Files added:** `test/test_xml.cpp`, `test/test_roundtrip.cpp`, plus new
fixtures under `test/malformed/`.

## PR 3 — Disassembly internals

- `disassemblerHashes_c`: insert, lookup, collision handling, and growth
  past the initial capacity.
- `disassemblerNode_c`: comparison, hashing, and move accumulation.
- `movementCache_0` and `movementCache_1`: cached movement values equal a
  freshly recomputed value.
- `disasmToMoves_c`: step interpolation is monotonic, `moving()` flags are
  correct, and positions at the first and last step match the separation
  tree.
- `separation_c`: `movesText` output and sequence counts on a known small
  disassembly.

**Files added:** `test/test_disassembly.cpp`.

## PR 4 — Puzzle and problem model

- `puzzle_c`: shape add, remove, and exchange with index stability;
  colour management; comment handling.
- `problem_c`: piece counts including min/max ranges, shape mapping,
  result shape, solution add and remove, and solution sorting.
- `assembly_c`: transform, compare, and `smallerRotationExists`.
- `solution_c`: ownership of its assembly and disassembly trees.

**Files added:** `test/test_puzzle_model.cpp`.

## Verification

Every PR in the stack must pass, before it is opened:

- `just build`
- `just test`
- `just check` (cppcheck, per AGENTS.md rule 6)
- `just coverage`, with the before and after numbers recorded in the PR body.

## Risks

**`bt_assert` depends on build type.** Tests that exercise misuse paths
rely on `bt_assert` throwing `assert_exception`, which happens only in
debug builds; a release build aborts the process instead. Those tests must
be guarded so they do not run under a release configuration. The existing
`test_solver.cpp` assert tests already depend on this behaviour, so the
constraint is pre-existing rather than new.

**Apple Clang coverage format.** `-Db_coverage=true` with Apple Clang
produces data plain `gcov` cannot parse, hence the `xcrun llvm-cov gcov`
shim. If the shim proves unreliable, the fallback is source-based coverage
via `-fprofile-instr-generate -fcoverage-mapping` read with `llvm-cov
report`, at the cost of divergence from the CI path.

**Rebase cascade.** A true stack means review feedback on PR 1 forces a
rebase of PRs 2 through 4. Accepted deliberately in exchange for small,
reviewable diffs.

**Tests may find real bugs.** If a new test fails against current
behaviour, the default is to report it and mark the test appropriately
rather than to fix production code inside a coverage PR. Production fixes
belong in their own PR where they can be reviewed on their merits.
