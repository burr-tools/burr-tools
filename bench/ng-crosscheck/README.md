# NG cross-check

An optional tool for cross-checking this repository's solver against
BurrTools NG (https://gitlab.com/roever/burrtools), an independent
implementation. It runs NG's solver library on our `.xmpuzzle` files so both
solvers can be compared: same assembly and solution counts, and wall times.
It is not part of the normal build, `just` targets or CI.

## Prerequisites

- A checkout of NG (tested at commit `fdae7951`).
- gcc >= 15 or clang >= 21 (NG builds as C++26).
- Microsoft GSL headers, e.g.
  `git clone --depth 1 --branch v4.0.0 https://github.com/microsoft/GSL`
- zlib, CMake >= 3.20, Ninja (or another generator), `patch` (only for ngsplit).
- A built `burrTxt` of this repository (default `build-rel/burrTxt`).

## Build

```
cmake -S bench/ng-crosscheck -B <builddir> -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DNG=<ng checkout> -DGSLDIR=<GSL>/include [-DNG_SHALLOW_SPLIT_PROTOTYPE=ON]
ninja -C <builddir>
```

This builds `ngbench` and `nggriddump` against NG's unmodified sources.
With `-DNG_CLOSURE_PATCH=ON` it also builds `ngbench_fw`: `ngbench` with
`closure-floyd-warshall.patch` applied to a copy of NG's headers (the
movement closure of NG's disassembler as one Floyd-Warshall pass in place of
repeating a triple loop until nothing changes; step 3.3 of
`design/2026-11-19-ng-merge-plan.md`). With
`-DNG_SHALLOW_SPLIT_PROTOTYPE=ON` it also builds `ngsplit` (see below).

## Run

```
bench/ng-crosscheck/crosscheck.sh --ng-bin <builddir>/ngbench [--burrtxt <burrTxt>] \
    [--threads N] [--disasm] [--timeout S] <file.xmpuzzle>[:problemIndex] ...
```

Defaults: `--burrtxt build-rel/burrTxt`, `--threads 1`, `--timeout 240`.
For each problem it runs `burrTxt` and `ngbench`, then prints one line with
both wall times, both `assemblies/solutions` counts and `OK` or `MISMATCH`
(or `TIMEOUT/NONE` if one side printed no result). Without `--disasm` only
assemblies are compared. With `--described` our side solves the puzzle
converted to its described grid (`burrTxt -D`; NG's grids as data, step G3
of `design/2026-11-19-ng-merge-plan.md`), which is what NG's importer does
too, so rhombic and tetra-octa puzzles can be disassembled on both sides. The exit status is non-zero if any problem
mismatched or timed out. Examples:

```
bench/ng-crosscheck/crosscheck.sh --ng-bin build-ng/ngbench --disasm --threads 4 \
    examples/SolidSixPieceBurrs.xmpuzzle "puzzles/BTFiles/James Fortune/kangaroo.xmpuzzle"
```

## The drivers

- `ngbench.cpp`: `ngbench file threads disasm(0|1) [problemIndex]`. Imports the
  `.xmpuzzle` into an in-memory NG database and solves with NG's
  `MultiThreadedSolver`. Storing of results is off unless the environment
  variable `NGB_STORE=<path>` names a `.dbpuzzle` to store into. The printed
  time excludes the import.
- `ngsplit.cpp`: `ngsplit file threads [problemIndex] [slice]`. Assembly only,
  with a prototype "split on demand" scheduler. `NG_DEEP=1` uses NG's
  original split instead.

- `ngGridDump.cpp`: `nggriddump [-v]`. Step G0 of
  `design/2026-11-19-ng-merge-plan.md`: registers all six of NG's grids (the
  Lua descriptions as C++ initialisers) and checks (A) that every
  transformation has at most one lattice class of valid shifts, as NG computes
  them and by a brute-force search over voxel positions, and (B) that our
  tetra-octa cells are NG's octahedral-tetrahedra voxels and our rhombic cells
  map two to one onto NG's rhombic tetrahedra under NG's importer. Prints the
  movement checks of cubes and triangular prisms; `-v` prints every shift and
  every grid's checks. Exit status 0 when both checks hold.

`nggrids.h` holds NG's six grid descriptions for both drivers.

Limits of `ngbench`: NG rejects
puzzles with multiple result shapes and pieces with variable voxels.

## The shallow-split patch

`dancingcell-shallow-split.patch` is a prototype change to NG's
`src/lib/dancingcell.cpp` used by `ngsplit`. It is not NG's code as shipped:
there is no save/resume, and it copies the whole solver for each split.
The build copies NG's file into the build directory and applies the patch
there; NG's checkout is never modified. The patch applies with `patch -p1`
or `git apply` inside an NG checkout.

Licence note: the patch is derived from NG's source, which its author has
agreed may be used here under the GPL version 3 (see `COPYRIGHT`). It is
kept as a patch only, as a record of the cross-check.

## Results of the first run (2026-11-14, 8-thread laptop VM)

A single run each, so treat the times as rough. Times in seconds
(ours / NG); counts agreed everywhere. "asm" is assemblies only,
"disasm" adds disassembly (solutions).

| Problem | asm 1 thread | asm 8 threads | disasm 8 threads | assemblies / solutions |
|---|---|---|---|---|
| SolidSixPieceBurrs | 0.44 / 3.551 | 0.15 / 3.576 | 0.16 / 3.651 | 588 / 179 |
| PelikanBurr | 0.02 / 0.026 | 0.03 / 0.027 | 0.08 / 0.415 | 12 / 1 |
| DraculasDentalDesaster | 0.01 / 0.012 | 0.01 / 0.011 | 0.05 / 0.485 | 84 / 1 |
| LominoSquareProblems9-15 #1 | 0.65 / 1.358 | 0.20 / 1.322 | 0.19 / 1.307 | 5 / 5 |
| LominoSquareProblems9-15 #0 | 0.15 / 0.309 | 0.05 / 0.270 | 0.05 / 0.282 | 9 / 9 |
| kangaroo | 0.49 / 0.968 | 0.15 / 0.610 | 0.29 / 1.675 | 9831 / 2 |
| unlucky block | 0.02 / 0.019 | 0.02 / 0.020 | 0.02 / 0.030 | 1 / 1 |
| Excelsior | 0.03 / 0.008 | 0.01 / 0.007 | 0.55 / 6.297 | 7 / 1 |
| Simplicity | 0.05 / 0.059 | 0.02 / 0.059 | 0.14 / 2.753 | 188 / 1 |
| Tippy | 0.04 / 0.032 | 0.03 / 0.033 | 0.09 / 1.300 | 460 / 111 |
| Supernova | 0.02 / 0.007 | 0.01 / 0.007 | 0.42 / 3.745 | 10 / 1 |
| Third_Times_the_Charm | 1.85 / 1.895 | 0.61 / 1.913 | 0.63 / 2.672 | 71 / 1 |

Our times are wall clock of the whole `burrTxt` process (including loading);
NG's are the `time=` it reports (excluding import). The asm columns found 0
solutions by construction.
