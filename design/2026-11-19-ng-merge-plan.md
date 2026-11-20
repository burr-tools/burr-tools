# Taking in BurrTools NG's mathematics: plan and record

**Date:** 2026-11-19, last updated 2026-11-21 (C3 done, B7 planned, layer model; the stack's cut revised: `ordered` dropped before the session, C4 step 1 held back)
**Branch:** the review stack `solver-merge/1-docs-bench` … `solver-merge/31-fltk-prism-tools` on top of `rotation-moves/7-pieces-model`, local, not pushed; the top branch is where work goes on (section 0). The history it was rewritten from is tag `solver-merge/history-2026-11-21`.
**Scope:** the solver core (assembler, disassembler, movement analysis, symmetry, grids), the interface between it and the front ends, and the file format where grids reach it.
**Sources:** this repository; BurrTools NG (<https://gitlab.com/roever/burrtools>, Andreas Röver's rewrite, "NG" below), studied at commit `fdae7951`. NG is GPL v3 and its author agreed to its code moving here: a file with NG code says so in its header and is GPL v3 from then on (paragraph in `COPYRIGHT`). The comparison of the two code bases that started this work, with measurements: <https://claude.ai/artifact/EuPQSUKaxmUVyF4Gv1v2Xj>.
**This note replaces** four earlier ones, which were merged into it and removed: the solver merge plan, the grid plan, the solver interface note and the grid contract (`design/2026-11-15-solver-merge-plan.md`, `2026-11-15-ng-grids-plan.md`, `2026-11-15-solver-interface.md`, `2026-11-18-grid-api-contract.md`). Their last versions are in git at `f07f15fc`. Step names from them (1.1 to 1.11, 3.1, 3.3, G0 to G4) are kept for the work that is done, because code comments and commits use them.

## 0. Start here

**State.** NG's assembler ideas are in: one assembler (`assembler_1`) with two engines, the bit-parallel `SimdHuangCover` where the matrix fits and Dancing Cells (`CellsCover`) for everything else; NG's symmetry breaker for piece ranges; `assembler_0`, `SimdExactCover` and dancing links as a search engine are gone (section 7.1). NG's grids are in as data (G0 to G4, section 7.2), with movement on all six (C6), filled-cell caching in `movementCache_lattice_c`, and STL export; rhombic and tetra-octa files open on NG's grids everywhere but FLTK (C1), triangular prisms everywhere with the native prism grid removed (C3), and FLTK views, solves and edits NG-grid files (C2's front-end part; Qt's new-file list is data-driven, and Qt's 2D voxel editor does not exist yet — P4). The core interface (sections 4 and 5, and `design/2026-11-20-core-api.md`) is accepted and built through B5: `solverSpec_t` (B3), `solverSession_c` with its queue and `problemCollector_c` (B4), `ordered` dropped (reorder buffer and merger thread removed, TLA+ specs updated), and all front ends (`burrTxt`, `burrTxt2`, Python, FLTK) plus all tests migrated onto `solverSession_c` with `solveThread_c` deleted (B5). Since then: B6 (cells as `(n, k)`, `shape_c`, the assembler's placements on it; its last clause deferred to the end, section 10); progress within a task in both engines (7.1); C4 step 1 (sphere turns computed per shape, not used by the solver yet; held back from the stack, 3.A); A1.2 measured (no engine rule with threads either); the layer model of 5.6 in `uicore` (`btui::LayerModel`) and the FLTK editor for NG's grids on it (C2, PR 30); the 0.8 prism features FLTK had lost with C3 back, for every described grid (PR 31: tool tab; new triangular, hexagonal and Cairo prism puzzles); B7 (Python on a par with the GUIs) planned, not built. Everything is local and unpushed, as a stack of 31 PR branches (below, and the table in 3.A).

**Next**, in this order unless the user says otherwise:

1. **B7**, the Python library on a par with the GUIs (section 4.8): first the solution STL export out of `src/gui/stlexportsolution.cpp` into the library, then `Puzzle` on any grid, stored results, STL export in Python. Planned only; the user asked for it to be planned, not built, 2026-11-21.
2. **C4 rest** (sphere turn tables per problem, wider transformation numbers, quaternions in files) goes with **E**, whose timing is the user's (section 10).
3. **A3**: push the stack when the user says (procedure below).

**Qt:** no new Qt features for now (user, 2026-11-21): C2's Qt part (the Qt 2D editor on the layer model, the hexagonal-prism check in the Qt GUI) waits.

Open questions for the user: section 10. Questions for NG's author: section 9.

**The branches (A1).** The work is a stack of local branches, `solver-merge/1-docs-bench` … `solver-merge/31-fltk-prism-tools`, one per PR, on top of `rotation-moves/7-pieces-model` (the rotation stack's top, PR #210); the structure follows the work as done (user, 2026-11-21); 3.A lists the PRs. There is no separate working branch: the top branch is the working branch, and every commit is in exactly one PR. The history the stack was rewritten from (with its fixups and docs commits) is tag `solver-merge/history-2026-11-21`, kept as a record only. Review guides, one per PR: `out/a1-pr-guides.md` (untracked, beside the repository; the PR descriptions are made from it at A3). Every commit was built and passed the fast suite and `just test-py` with `-Dqt_gui=disabled`; the Qt suites run in CI.

*Working on it.* New work is a new commit on the top branch, as a new PR branch when it is a new purpose (`git checkout -b solver-merge/32-… solver-merge/31-fltk-prism-tools`), else on the top PR's branch. A fix to an earlier PR goes into that PR's commit, not on top: `git commit --fixup=<commit>`, then `git rebase -i --autosquash --update-refs <first PR's parent>` from the top branch, which moves every PR branch above it as well. Changes to this note, `design/2026-11-20-core-api.md` and `bench/merge-checks/` fold into PR 1, so its notes are always this note's latest form. After a rebase, build and test the commits that changed (a Sonnet subagent can do the per-commit runs) and update `out/a1-pr-guides.md`. *Pushing* (A3, only when the user says): push every branch, open the PRs from the guides, link them with `gh stack link --remote origin <bottom … top>` on top of #210; later fixes as above, then `git push --force-with-lease` for the branches that moved.

**Baselines.** `out/merge-checks/*.txt` hold the corpus counts from before B6 (engines, described, described-solutions, engine-choice, engine-choice-mt). The corpus modes compare variants of one binary, so a change common to all variants (placement enumeration, conversion) only shows against these files: compare the counts per problem after such a change.

**Rules for every step.**

- Read `CLAUDE.md` and the `llm_docs/` file it names for the area touched. Use `just` targets (`just build`, `just test`, `just build-release`, `just bench`, `just build-tsan`).
- A new engine, rule or default ships behind a `solverConfig_t` field with a `BURRTOOLS_` variable, which needs five places: `src/lib/solver_config.h` (both structs and the comment table), `solver_config.cpp` (`resolve()`), `src/tools/envvars.h` (the `--help` table; `test/test_envvars.cpp` fails without it), `test/test_solver_config.cpp`, `llm_docs/benchmarking.md`. It is compared on the corpus, and only then becomes the default.
- Results never change. After a solver change, from the repo root with a release build:
  - `python3 bench/merge-checks/corpus_check.py engines` (default, Dancing Cells forced, the fallback path; about 15 minutes);
  - `symbreak` near the symmetry breaker, `described` and `described-solutions` near the grids;
  - `./build/test_burrtools "[cells]"`, then `just test`;
  - `just build-tsan` and `./build-tsan/test_burrtools "[cells]"` for anything that touches threads.

  A `MISMATCH` line is a bug; `TIMEOUT` lines are problems over the 15 s limit (18 or 19 of 520 on this machine).
- Timings: `bench/merge-checks/best_of.sh "VAR=1" -t 1 <puzzle>` for a quick figure (best of three; single runs vary by 10% and more), `just bench` for anything that goes into a decision. Release binary only. Do not assert iteration counts in tests (`llm_docs/solver_rules.md`).
- Commit locally in small commits; do not push or open PRs without the user. Never `git add -A`: the working tree holds files of the user's that are not part of this work (modified `design/2026-09-1*.md`, untracked notes and output).

**Outside the repository.**

- NG is not vendored. To cross-check: clone NG at `fdae7951` and Microsoft GSL, then follow `bench/ng-crosscheck/README.md` (needs gcc 15 or clang 21). The cross-check stays until the port is done and verified, then goes. Its Floyd-Warshall patch for NG is kept as a record only.
- The puzzle corpus under `puzzles/BTFiles/` is not in the repository (`puzzles/` is ignored); the corpus checks and `just bench` need it. Without it they run over `examples/` only, 30 problems, which is not enough.

**Settings.** `BURRTOOLS_RANGE_SYMBREAK` (`rangeSymmetryBreaker`, default on): with piece ranges, a shape used exactly once still breaks the result's symmetry. `BURRTOOLS_MERGE_COLUMNS` (`mergeColumns`, default on): merges plain voxel columns covered by the exact same `(row, weight)` set when feeding the engines, shrinking the SIMD tier and row width without changing the matrix, `reduce()`, or saved searches (`RhombicDodecahedraBox.xmpuzzle` NG rhombic `4.83 s → 0.87 s` all threads, `7.90 s → 1.54 s` one thread). `BURRTOOLS_BITPARALLEL=0` selects Dancing Cells for every problem. `BURRTOOLS_CELLS`, `BURRTOOLS_CELLS_FALLBACK`, `BURRTOOLS_ONE_ASSEMBLER`, `BURRTOOLS_SPLIT_ORDER` and `BURRTOOLS_SPLIT_BREADTH_FIRST` were removed; setting one is an error that names the replacement.

## 1. The agreement, and where it leads

On 2026-11-19 the user and Andreas Röver agreed:

1. NG's mathematical foundations go into BurrTools, and NG is discontinued.
2. The solver does no I/O of its own.
3. Dancing Cells is part of the solver.
4. The new grid system comes in where it makes sense.
5. The boundary between the core and the UI/CLI is redesigned.
6. A program that wants crash recovery pauses the solver, saves its state and resumes. The solver itself writes nothing.

The target is one solver library with no GUI and no file format in it, and several front ends beside it. Each stage takes the better part of the two code bases:

| Stage | Target | From | State |
| --- | --- | --- | --- |
| Problem model | One formulation: pieces with min..max counts, required and optional result cells | NG | Done: `assembler_1` handles every problem |
| Matrix build | Anchor sweep, reduction, column merging; a symmetry breaker that also works with piece ranges | Ours, plus NG's breaker rule | Done (1.1) |
| Exact cover | Bit-parallel engine where it fits, Dancing Cells for the rest, no dancing links | Ours, and NG's algorithm (Knuth's), written here | Done (1.2 to 1.10) |
| Parallel search | Task pool with split on demand, in every engine | Ours | Done |
| Save and resume | Opaque engine state string + pending assemblies; same solver, version and machine only (error if incompatible) | Ours, and NG's decision lists | Done for Dancing Cells; the interface makes it the whole contract (B) |
| Symmetry | Groups computed from the grid, not generated tables | NG | Done for described grids (G1 to G3). Not for spheres, whose turns are no group of the grid (5.8) |
| Grids | Grids as data; a fast path for cubes | NG's model, our cube code | G0 to G4 done; C |
| Movement rules | Derived from cell shapes, so more grids disassemble | NG | Done (G4) |
| Duplicates | Mirror duplicates always removed, mirror partners swapped | NG's only behaviour, our default | B1 removes the option to keep them |
| Disassembler | Vectorised closure, bitboard move search, per-worker cache, parallel pool | Ours | Unchanged; D adds motions with check cells and exits |
| Rotation moves | Cube-grid module | Ours | Unchanged |
| Interface | Problem in; results out via a bounded queue; start, pause, state, resume and abort; storage on the host side | New | Designed (sections 4, 5), B |

**Not taken from NG:** its SQLite document, its ImGui front end, Lua and grid descriptions in files (grids are defined in C++; user, 2026-11-19), and its disassembler and parallel split, where ours are faster (NG's split hands over the deepest subtree and does not engage; its disassembler was 5 to 20 times slower on the puzzles measured). Whether large result sets ever get a database is our own decision for later; pause, save and resume need none.

## 2. Principles

1. **Every step is an option first**, behind a setting, and becomes the default after the corpus comparison (section 0).
2. **Results never change.** Each step is checked by assembly and solution counts against the previous code over the corpus, and against NG through `bench/ng-crosscheck/` while that exists.
3. **NG's code may be ported**, with its origin in the file header. Where NG is the reference for code we port, the port is checked against it.
4. **Small, reviewable PRs**, one purpose each, in a stack, each with a short guide: what changed, how it was checked, the numbers.
5. **Compatibility for released formats only** (`llm_docs/compatibility.md`): a format never in a release tag may change without a legacy reader. Saved search state (`<assembler version="2.2">`) is post-0.8.0, so unreleased; a paused search resumes only with the same solver, version and machine that saved it, and incompatible resume data is rejected with an error.
6. **The Python library can do what the GUIs do** (user, 2026-11-21). Whatever a front end does with a puzzle that is not presentation (opening and saving files, creating a puzzle on any grid, storing results, converting, editing operations, STL export) lives in the library or `uicore`, not in a GUI's widget code, so Python can offer it. A design that only a GUI could use is the wrong design, and a review asks of every new feature where Python reaches it. Section 4.8 lists what Python lacks today.

## 3. The plan

Five phases. B and C are independent and can alternate. D and E come after B.

### A. Land the work so far (review only, no new code)

1. The stack of PRs, one purpose each, as built (user, 2026-11-21: the cut follows the work as done; revised the same day: `ordered` goes in its own PR before the session, and C4 step 1 is held back). Fixes are folded into the commit they fix; there are no docs commits: this note is in PR 1 in its current form. Each PR has a review guide (`out/a1-pr-guides.md`): what changed, how it was checked, the numbers, how to review it. Every commit is built and tested with `-Dqt_gui=disabled`; the Qt suites run in CI. The stack goes on top of the rotation stack's top PR (#210, `rotation-moves/7-pieces-model`), not on `master`: the two share 26 files (`assembler_1.*`, the solver-config tests, `envvars.h`, `mainwindow.cpp`, …). It merges as a whole, with no release in between (user, 2026-11-21), so a PR may leave a gap a later PR closes (FLTK cannot edit prisms between 28 and 30).

   **Part 1: the assembler.**

   | PR | Branch | Content | Size | Review notes |
   | --- | --- | --- | --- | --- |
   | 1 | `solver-merge/1-docs-bench` | This note, `design/2026-11-20-core-api.md`; `bench/merge-checks/`; `bench/ng-crosscheck/` | +2,300, no production code | The tools every later guide cites |
   | 2 | `2-range-symbreak` | 1.1: symmetry breaker with piece ranges, `BURRTOOLS_RANGE_SYMBREAK` | +42 | Changes default behaviour, so it stands alone; corpus `symbreak` |
   | 3 | `3-cells-engine` | 1.2, 1.3 and the tuning: `CellsCover` as an option, its tests, the credit to Andreas | +1,900 | The algorithm against NG's `dancingcell.cpp` |
   | 4 | `4-cells-fallback` | 1.4, 1.5: Dancing Cells in place of dancing links where the bit-parallel engine does not run | +70 | Default change; corpus `engines` |
   | 5 | `5-assembler-perf` | Assembly building, placement table, hole count by popcount | +90 | Prerequisite for 6; timings |
   | 6 | `6-one-assembler` | 1.8, 1.10: one assembler, two engines; `assembler_0`, `SimdExactCover`, the dancing-links search and the step button go | +1,000 / −11,000 | Mostly deletion; saved state 2.2 |
   | 7 | `7-keep-mirror` | B1: mirror duplicates always removed; Python's `keep_mirror=` and `burrTxt2 -m` deprecated | +200 / −225 | API deprecation |

   **Part 2: grids.**

   | PR | Branch | Content | Size | Review notes |
   | --- | --- | --- | --- | --- |
   | 8 | `8-movement-cache-wide` | 3.1: cache entries with more than 8 directions | +95 | `just bench` unchanged |
   | 9 | `9-ng-maths` | G1: NG's transformations, symmetry groups, MPR, ported | +4,400 | Diff against NG at `fdae7951`; review the adaptations |
   | 10 | `10-ng-grids` | G2: NG's grid registration and the six descriptions; fixes M2 and M4 + m5 as commits of their own | +3,300 | As 9, the fix commits in full |
   | 11 | `11-described-assembly` | G3: described grids as grid types, with assembly | +1,900 | The adapter classes; corpus `described` |
   | 12 | `12-described-disassembly` | G4 | +280 | Corpus `described-solutions`; NG cross-check |
   | 13 | `13-prism-movement` | C6: movement for hexagonal and Cairo prisms | +610 | Chosen vectors and why |
   | 14 | `14-rhombic-tetraocta` | C1: rhombic and tetra-octa on NG's grids, per front end; the bench puzzle | +820 | File-format behaviour per front end |
   | 15 | `15-sphere-check` | The `[spheres]` guard tests and `sphere_bruteforce.py` | +510, tests only | Today's 120 turns handle straight pieces (5.8) |
   | 16 | `16-described-stl` | C2's STL part: Minkowski export and bevelled 3D view for described grids | +270 | Volume and bounding box parity |
   | 17 | `17-merge-columns` | `BURRTOOLS_MERGE_COLUMNS` | +300 | Uses PR 14's rhombic bench puzzle for its numbers |

   **Part 3: the core interface (B3 to B5).**

   | PR | Branch | Content | Size | Review notes |
   | --- | --- | --- | --- | --- |
   | 18 | `18-solver-spec` | B3: `solverSpec_t`, then the engines on it | +900 / −430 | Commit 2 is type changes |
   | 19 | `19-drop-ordered` | Results in no particular order: the pool's merger thread and reorder buffer go; `solveThread_c` and `burrTxt` serialize their callbacks; TLA+ `DisasmPool` and `Pipeline` unordered only | +340 / −760 | Before the session, so the session is built unordered-only; `just spec-check` |
   | 20 | `20-solver-session` | B4: `solverSession_c`, `resumeData_t`, `problemCollector_c`, `solveThread_c` as a thin wrapper; the TLA+ model `SolverSession` | +4,100 / −1,170 | The session against its TLA+ model |
   | 21 | `21-burrtxt-session` | B5: `burrTxt` on the session | +160 / −120 | `just test-regression` |
   | 22 | `22-python-session` | B5: Python's `SolutionIterator` on the session | +160 / −270 | `just test-py` |
   | 23 | `23-fltk-session` | B5: `burrTxt2` and the FLTK GUI on session and collector; the remaining tests migrated, `solveThread_c` deleted | +1,050 / −1,080 | FLTK by hand |

   **Part 4: on the interface: front ends for NG's grids, B6, C3.**

   | PR | Branch | Content | Size | Review notes |
   | --- | --- | --- | --- | --- |
   | 24 | `24-fltk-ng-grids` | C2: FLTK views and solves NG-grid files; Qt's new-file list from data | +230 | |
   | 25 | `25-lattice-cache` | Filled cells cached in `movementCache_lattice_c` | +76 | Described prisms' disassembly to native speed |
   | 26 | `26-shape` | B6: math value types, `cellAddr_t`, `shape_c`; the assembler's placements on it | +1,850 / −310 | Corpus counts equal to `out/merge-checks` |
   | 27 | `27-task-progress` | Progress within a task for both engines | +450 | `subtree_progress.h` first |
   | 28 | `28-prisms` | C3: triangular prisms on NG's grid everywhere; the native prism grid removed | +570 / −3,300 | Commit 2 is deletion |
   | 29 | `29-layer-model` | C2, 5.6: `btui::LayerModel` in `uicore` | +470 | The contract in `layermodel.h` |
   | 30 | `30-fltk-layer-editor` | C2: the FLTK editor for NG's grids on the layer model; true mirrors | +1,160 | Brings back prism editing |
   | 31 | `31-fltk-prism-tools` | The tool tab of NG's grids; new triangular, hexagonal and Cairo prism puzzles in FLTK | +1,010 | 0.8 parity for prisms |

   **Held back:** C4 step 1 (`sphereShapeTurns()`, sphere turns as integer quaternions), branch `solver-merge/held-c4-sphere-turns` on top of PR 31. Nothing uses it yet and the rest of C4 goes with E, which may reshape it, so it is reviewed with E (user, 2026-11-21).

   Whether to open the parts at once or one after the other depends on review capacity; the user decides when pushing (A3).
2. Done 2026-11-21: `corpus_check.py engine-choice-mt` (default thread count, 4 cores; `out/merge-checks/engine-choice-mt.txt`): 509 problems agree; bit-parallel 78.0 s in total, Dancing Cells 146.7 s. Of the 22 problems over 0.3 s, Dancing Cells is faster on one (Second_Time_Lucky, 0.81x) and 1.14x to 3.7x slower on the rest (Alpaca 3.08x). So step 1.9 stands with threads too: no engine rule beyond "bit-parallel where it fits".
3. Push and open the stack only when the user says so.

### B. The core interface

| Step | What | Check |
| --- | --- | --- |
| B1 | Remove keep-mirror (section 5.4): `PAR_KEEP_MIRROR`, `createMatrix`'s `keepMirror` argument, the FLTK switch. On its own, first. Not stored in released file formats, so no migration. Python's `keep_mirror=` and `burrTxt2 -m` shipped in v1.8: they stay accepted, and asking for mirrors prints a deprecation warning and is ignored; they go fully in a later release (user, 2026-11-20) | `just test`, `just test-py`; `corpus_check.py` with keep-mirror off unchanged |
| B2 | The user reviews sections 4 and 5 of this note before any code | Done 2026-11-20: both accepted, with the answers in 4.7 (revised 2026-11-20: bounded pull queue, in-place resume, explicit resume-error policy) |
| B3 | `solverSpec_t` and `makeSolverSpec` (4.2). Unit tests: a spec from every example file answers every accessor as its `problem_c` does. Then the engines take `const solverSpec_t &` in place of `const problem_c &`: `findAssembler`, `assembler_1`, `assembler.cpp`, the `assembly_c` methods, movement caches, disassemblers, pool, rotation tables. Call sites change type, not logic | Done 2026-11-20: no engine includes `problem.h` or `puzzle.h` (the assembler's `save(xmlWriter_c &)` went with B4; `assembly_c`, `separation_c` and `gridType_c` still read and write themselves as XML, which moves to the hosts with E), `[solverspec]`, `just test` (10/10), `just test-py` (12/12), `just test-regression` (20/20), TSan 0 races, `corpus_check.py engines` 0 mismatches |
| B4 | Session and result queue (4.4 to 4.7): `solverSession_c` exposes `start`, `pause`, `state`, `resume`, `abort` and a bounded result queue (`pop`/`drain` + optional wake hook); `solveThread_c`'s storing, sorting and limits move to a host-side `problemCollector_c` consuming from the queue; `assembler_c::save(xmlWriter_c &)` becomes the state string in `resumeData_t`; `state()` (while paused) snapshots `resumeData_t` and `problem_c` stores it. `solveThread_c` becomes a thin wrapper over session and collector until B5 | Done 2026-11-20: header audit clean (`problem.h`, `puzzle.h`, `xml*.h`), `[session]` (3,329 assertions covering bounded queue, ordered/unordered, countOnly, in-place & cross-session pause/resume at every stop point, resume validation, `problemCollector_c`, `problem_c` `resumeData_t` XML roundtrip), `just test` (10/10), `just test-py` (12/12), `just test-regression` (20/20), `just build-release` (`--werror`, `NDEBUG`), TSan (81 cases / 7,284 assertions, 0 races) |
| B5 | Front ends on the interface: FLTK with `burrTxt2` (one PR), then `burrTxt`, then Python, which pulls from the session's queue (user, 2026-11-20). `solveThread_c` goes. The Qt GUI uses the interface from the start when it gets a solver | Done 2026-11-20: `burrTxt`, Python (`SolutionIterator`), `burrTxt2`, FLTK GUI (`mainwindow.cpp`) and all remaining tests migrated onto `solverSession_c` (+ `problemCollector_c`); `src/lib/solvethread.{h,cpp}` removed. Verified with `just test`, `just test-py`, `just test-regression`, `just build-release`, TSan, and live FLTK GUI test on `PelikanBurr.xmpuzzle` |
| B6 | Cell addresses and placements in the API (5.2, 5.3): a shape type over (n, k) with the operations of 5.3, beside `voxel_c`, and the assembler's placement enumeration on it; `voxel_c` becomes the dense storage behind it | Done 2026-11-20 but for the last clause, which is deferred until after all other work (user, 2026-11-21; section 10): `shape_c`, `cellAddr_t`, the math value types and the grids' (n, k) maps (`8b1708bc`, `[shape]`); `assembler_1_c::prepare()` and `placementFinder_c` on `shape_c` (`e23756ab`). `just test`, `just test-py`, `just test-regression`, `just build-release`; corpus `engines`, `described`, `described-solutions` no mismatch and every count equal to the earlier runs. Described triangular-prism matrix prep 1.31x to 1.02x native |
| B7 | The Python library on a par with the GUIs (principle 6, section 4.8): `Puzzle` on any grid; solving can store its results in the problem (through `problemCollector_c`, as `burrTxt2` and the FLTK GUI do), so a saved file holds them; STL export of shapes and of solutions. First the export gap: the solution export (all pieces assembled into one file, or one file per piece type, with the file naming that keeps clashing names apart) moves from the FLTK dialog (`src/gui/stlexportsolution.cpp`) into the library, and the FLTK dialog and Python call it; the Qt GUI gets it from there when it exports solutions. Independent of E: files go through the document model either way | `just test-py` with new cases (a puzzle per grid saved and loaded back; solve, store, save, load; STL of a shape and of a solution, assembled and per piece, compared with what the FLTK dialog writes) |

### C. Grids

| Step | What | Check |
| --- | --- | --- |
| C1 | Rhombic and tetra-octa on NG's grids for everything but FLTK (G5 in commits; 6.4). Grid types 3 and 4 are converted when a file is read, saved assemblies with them; files are written as NG's grid types. The conversion on read is the library's default; the FLTK GUI reads with it off and keeps the native grids, with its editors, until FLTK retires (user, 2026-11-20). So FLTK reads and writes types 3 and 4 as today and still refuses NG-grid files, while `burrTxt`, Python and the Qt GUI read types 3 and 4 as NG grids and write NG types; Qt offers one rhombic and one tetra-octa grid, the NG ones, and its brick-to-rhombic and brick-to-tetra-octa conversions produce them. `voxel_3_c`, `voxel_4_c` and their tables stay while FLTK needs them. Measure the doubled rhombic cells on a large rhombic puzzle, which the corpus lacks (write one for `bench/`) | Every rhombic and tetra-octa file of the corpus loads, its saved assemblies land on the same cells, its problems give at least the native assembly counts (native = FLTK's path, still present) (more only where a piece sits half a diagonal away, each case looked at); solutions agree with NG (`crosscheck.sh --described`); a saved file loads back unchanged |
| C2 | Front ends for described grids. Qt: the new-file list as data (today five hard-coded entries), names, convert list; editing per 5.6; on the rhombic grid, whether the editor paints our old voxel (two NG cells) or NG's cell; bevel faces in the 3D view. STL: described grids through the Minkowski mesher (`minkmesh.cpp:974`), with the cell polyhedra as building blocks (**done 2026-11-20**, including `getSTLMesh()` bevelled 3D view and volume/bbox parity with native rhombic and tetra-octa). FLTK: view and solve; editing on every described grid with `layerGridEditor_c` over `btui::LayerModel` (**done 2026-11-21**, PR 30); the tool tab of every described grid (`ToolTab_lattice`: size, flips, turns, nudges, minimize, centre, origin, x2/x3 where exact, the inside/outside tools, fill holes, grid scale) and new puzzles on triangular, hexagonal and Cairo prisms (PR 31; user 2026-11-21: what is supported can be created). `burrTxt` and Python: nothing beyond capability flags | A hexagonal-prism puzzle can be created, edited, solved, viewed and exported in the Qt GUI, checked in the running app; snapshot tests per `src/qtgui/AGENTS.md`. The editing contract of 5.6 is a layer model in `uicore` (cells of a layer, their 2D outlines, hit-testing), shared by the Qt editor and an FLTK editor for NG's grids, which gives the FLTK GUI back prism editing (lost with C3) and adds hexagonal and Cairo prisms |
| C3 | Triangular prisms: the native class measured against the described grid; if level within a few percent, the described grid replaces `voxel_1`, `movementcache_1` and the table generator | Done 2026-11-21 (user: "convert to the new prism", the removal is worth it). Measured: within 5% in every phase after B6 (prep 1.02x, search 1.04x, disassembly 1.05x). `cada2e85`: every reader converts type 1 (the FLTK GUI's too, which then views and solves prisms; prism editing and tools are restored on the layer model in PRs 30–31), with saved assemblies and disassemblies (moves become NG's translations, checked per cell; `[prismimport]` checks every state of Prisgon's disassemblies cell by cell); corpus `described-solutions` 503 problems, no mismatch, the 7 prism files (6 with disassemblies) load. `7ece508a`: `voxel_1`, `symmetries_1`, `movementcache_1`, `tabs_1` and FLTK's prism editor removed, type 1 only read (+129/−3,262) |
| C4 | Spheres in the contract, for any shape (5.8): valid turns computed per shape, a turn table and symmetries object per problem, wider transformation numbers. Needs B6. Today's code has no straight-piece gap (5.8, checked 2026-11-20). Step 1 done 2026-11-21 (branch `solver-merge/held-c4-sphere-turns`, outside the stack until E): `sphereShapeTurns()` and `sphereTurn_t` (integer quaternion + mirror), equal to the 120's valid turns on 200+ touching clusters. The rest (turn table and symmetries per problem, transformation numbers wider than 8 bits in 68 places across lib, GUIs and Python, turns stored as quaternions) changes the file format, so it goes with E | Problems with touching pieces unchanged (BallRoom: 2, 1, 3; the corpus's sphere problems; the `[spheres]` guard tests); puzzles with spread pieces against `sphere_bruteforce.py` extended to them |
| C5 | Cylinders and helical puzzles (5.9): cyclic translations, turns about the axis, exits. Needs B6 and D1 | To be planned |
| C6 | Movement vectors for the hexagonal and Cairo grids, in their C++ descriptions (NG has none: "no movement analysis"). Done 2026-11-20: hexagonal (1,1,1) axis and (1,-1,0), (1,0,-1), (0,1,-1) to the edge neighbours; Cairo (0,0,1) and the two shortest in-plane lattice vectors (2,2,0), (-2,2,0); check cells match an independent sweep | Done: hand-counted puzzles and domino tilings on both grids |

Cubes stay native: their fast path is the reason the solver is fast. Spheres keep their native cell storage; C4 replaces their fixed turn tables by turns computed per shape.

### D. Disassembler

1. Movements as one-parameter motions with check cells, plus exits (5.1). Cylinders need this; everything else works without it.
2. Disassembly within one hard assembly is serial here and in NG and is the limit for puzzles like Excelsior. Not planned; listed so it is not forgotten.

### E. The file format, once

Files store the grid id, cells as (n, k) in compact runs and placements as (g, t) (a sphere turn as an integer quaternion, 5.8), and every older released format becomes an importer (5.7). Python and `burrTxt` follow. It is the expensive, public step and ships with a release, so it waits until B6 and C1 have settled the representation.

### Order

```
A ──────────────► (push when the user says)
B1 ► B2 ► B3 ► B4 ► B5 ► B7 (any time; before E is not needed)
               └──► B6 ──► E
C1 ► C2 ► C3        └──► C5 (with D1)
C4 after B6 (5.8); C6 any time
```

## 4. The core interface

Accepted by the user on 2026-11-20 (B2). Built: 4.3 (B3), 4.4 to 4.7 (B4), all front ends migrated and `solveThread_c` removed (B5).

### 4.1 What the solver reads and writes today

Surveyed on this branch.

**What the engines read from `problem_c`.** A small, read-only part of it:

| Accessor | Users |
| --- | --- |
| `getNumberOfParts`, `getPartShape`, `getPartMinimum`, `getPartMaximum`, `getShapeIdOfPart`, `getNumberOfPieces`, `getPartIdToPieceId` | assembler (about 110 calls), `assembly_c`, movement cache, disassemblers |
| `getResultShape` (a free function in `problem.h`), `maxHolesDefined`/`getMaxHoles`, `resultValid` | assembler, rotation tables |
| `getPartGroupId`, `getPartGroupCount`, `getNumberOfPartGroups` | disassemblers (`disassembler_a.cpp`) |
| `placementAllowed` (colour constraints) | `assembler.cpp` matrix build |
| `getPuzzle().getGridType()`, `colorNumber()`, `getRotationModel()` | everything; the rotation tables also *write* `setRotationModel` |

They arrive as `const problem_c &` through `findAssembler` (`gridtype.cpp`), the assembler constructor, `movementCache_c(const problem_c &)`, `disassembler_0_c`, `disassembler_a_c`, `disassemblerPool_c`, `movementAnalysator_c`, and the `assembly_c` methods `validSolution`, `placedPieces`, `transform`, `smallerRotationExists`, `sort`, `createSpace`, `createPieceSpaces` (`assembly.h`).

**What `solveThread_c` writes into `problem_c`** (`src/lib/solvethread.cpp`): `setAssembler` (and with it the restore of saved state, `problem.cpp:950-1000`), `addSolution` in three variants, `removeSolution`, `sortSolutions`, `incNumAssemblies`, `incNumSolutions`, `addTime`, `finishedSolving`, `removeAllSolutions`, `stashAssemblies`/`takeStashedAssemblies`. The solution limit, the "keep every n-th" drop, the live sort and the "keep the most complex" choice (`SRT_COMPLETE_MOVES`, `SRT_LEVEL`) are decided inside `solveThread_c::onDisassemblyResult` under `problem_c::lockSolutions()`.

**Saved search state.** `problem_c::save` (`problem.cpp:100-210`) writes `assm->save(xml)` (an opaque text with a version attribute), then the solutions, then the stash of assemblies found but not yet disassembled. On load the text waits in `assemblerState`/`assemblerVersion` until the next start hands it to `setPosition`. So the state is already opaque bytes plus a version; what is not separated is *who* writes it (the engine, into the host's XML writer).

**Front ends.** Four; the Qt GUI does not solve yet.

| Front end | How it solves | Pause/continue |
| --- | --- | --- |
| FLTK GUI (`src/gui/mainwindow.cpp`) | `solveThread_c`, polls `currentAction`, `getProgress`, the problem's counters and solutions | yes, also across saving the file |
| `burrTxt2` | `solveThread_c` | no |
| `burrTxt` (`burrTxt.cpp:531`) | `findAssembler` directly, disassembles inline | no |
| Python (`py_solution_iterator.cpp:160`) | `findAssembler` directly, unordered streaming, inline disassembly (deliberately, `py_solution_iterator.h:19-34`) | no |

### 4.2 Aims

1. An engine does no I/O of its own (no file I/O, no stdout/stderr) and never includes `problem.h`, `puzzle.h` or `tools/xml.h`. It gets an immutable description of the problem and produces results and resume state for whoever asked.
2. Another document model (a test harness, a batch driver, a future results store) can use the solver without building a `problem_c`.
3. The lifecycle is **start, pause, resume and abort**. `pause()` drains and parks the workers into a `Paused` state; `state()` can be requested only while paused (never while running) and returns a `resumeData_t` snapshot without tearing down the session, so `resume()` continues with the built matrix, reductions and caches intact. Cross-process resume constructs a new session from `resumeData_t`. There are no checkpoints during a run: crash recovery is the host pausing, draining the queue, saving `state()` with the results, and calling `resume()`.
4. Output leaves through a **bounded thread-safe queue** of assemblies and disassemblies, transferring ownership to the host on the host's own thread. The solver never mutates a shared `puzzle_c` or `problem_c` in place.
5. Resume carries **no cross-solver, cross-version or cross-machine compatibility guarantees**: a saved state is only guaranteed to resume with the same solver that produced it; elsewhere it may work, and a state that cannot be used is rejected with an error.
6. Nothing about results changes: same assemblies and solutions, same pause/continue guarantees (LIFECYCLE-1 to 6 in `solvethread.h`).

Not aims: changing the engines' algorithms, the threading inside them, or the file format (that is E).

### 4.3 `solverSpec_t`: the problem as a value

All types live in `src/lib/solver/` (new) and depend only on `voxel.h`, `gridtype.h`, `assembly.h`, `disassembly.h`, `solver_config.h` and `rotationgeometry.h` (the rotation model). Names are proposals.

```c++
struct solverPart_t {
  unsigned int shapeId;   // index into solverSpec_t::shapes, kept for results and messages
  unsigned int min, max;  // count range, min <= max, max >= 1
  // group membership for the disassembler: (group id, count) pairs, as problem_c keeps them
  std::vector<std::pair<unsigned short, unsigned short>> groups;
};

struct solverSpec_t {
  std::shared_ptr<const gridType_c> grid;      // grid, symmetries, movement cache factory
  std::vector<std::shared_ptr<const voxel_c>> shapes;  // every shape a part or the result uses
  std::vector<solverPart_t> parts;             // problem_c's parts, same order
  unsigned int result;                         // index into shapes
  unsigned int colours;                        // colorNumber()
  std::set<uint32_t> colourConstraints;        // problem_c's placement constraints, same encoding
  std::optional<unsigned int> maxHoles;        // problem_c::maxHolesDefined/getMaxHoles
  std::optional<rotationModel_t> rotationModel;

  // the accessors the engines use today, with the same meaning, so B3
  // is a type change at the call sites and not a rewrite
  unsigned int getNumberOfParts() const;
  const voxel_c * getPartShape(unsigned int part) const;
  unsigned int getPartMinimum(unsigned int part) const;
  unsigned int getPartMaximum(unsigned int part) const;
  unsigned int getShapeIdOfPart(unsigned int part) const;
  unsigned int getNumberOfPieces() const;
  unsigned int getPartIdToPieceId(unsigned int part) const;
  const voxel_c * getResultShape() const;
  bool resultValid() const;
  bool placementAllowed(unsigned int part, unsigned int resultColour) const;
  // group accessors as in problem_c
};

solverSpec_t makeSolverSpec(const problem_c &);   // lives with problem_c, not in src/lib/solver
```

- **A snapshot, immutable, shared.** Built once when a solve starts and held as `std::shared_ptr<const solverSpec_t>` by the assembler, the disassembler pool and the movement caches. Shapes are copies, so an edit in the document during a solve cannot reach a running engine. Today the GUI prevents such edits; afterwards the type does.
- **Parts, not pieces.** The engines think in parts with count ranges and expand to pieces themselves; keeping that avoids touching the matrix build.
- **No solving options.** Flags, limits and sorting are about what to do with results; they go to the session (4.4) or the host consumer (4.5). The spec describes a puzzle and nothing else.
- **The rotation model is read-only here.** Today `rotationtables.cpp` calls `puzzle.setRotationModel` to record the model it built tables for, so the next solve can detect a change and reset. With a spec, the session reports the model it used in `solverRunInfo_t` (4.5), and the host stores it.
- With B6 the shapes become the contract's shape type (5.3), and `voxel_c` its storage.

### 4.4 `solverSession_c`: lifecycle and controls

Replaces `solveThread_c` as the thing a front end creates; `solveThread_c` stays as a thin wrapper until B5.

```c++
struct solverOptions_t {
  bool reduce, disassemble, keepRotations, completeRotations, rotationMoves;
  bool countOnly = false;        // count assemblies/solutions without enqueuing results; the engines still build
                                 // each assembly internally (as solveThread_c did)
  size_t queueCapacity = 256;    // soft bound for backpressure while running (4.7)
  solverSettings_t settings;     // threads, engines, rotation mode (unchanged)
};

enum class solverState_e { Idle, Running, Paused, Finished, Stopped, Error };

class solverSession_c {
public:
  // Optional wake hook: called with no data when results or a terminal state
  // become available (e.g. QMetaObject::invokeMethod / Fl::awake), so GUIs
  // need not poll. May be called from solver threads, possibly while
  // solver-internal locks are held; must not block or call back into the session.
  solverSession_c(std::shared_ptr<const solverSpec_t>, solverOptions_t,
                  std::function<void()> onWake = {});
  ~solverSession_c();            // aborts and joins if still active

  // Optional, only in Idle (before start()): re-instantiate a paused search.
  // A resumeData_t that cannot be used makes start() return false before any
  // worker starts, with the session in Error (ERR_CAN_NOT_RESTORE_*); see 4.6.
  void setResume(resumeData_t);

  // Lifecycle:
  //   Idle           -> start()  -> Running
  //   Running        -> preparation done with start(true) -> Paused (onWake fires; start(true) returns at once,
  //                     so join() or wait for Paused before state(); with resume data start() prepares
  //                     synchronously and the session may already be Paused when it returns)
  //   Running        -> pause()  -> Paused  (drains/parks workers & pool; see 4.7)
  //   Paused         -> resume() -> Running (continues with built matrix and caches intact)
  //   Idle|Running|Paused -> abort() -> Stopped (abandons the run; total, idempotent, LIFECYCLE-3)
  bool start(bool stopAfterPreparation = false);
  void pause();
  bool resume();
  void abort();
  void join();

  // State snapshot: ONLY legal while Paused (throws / precondition failure while Running).
  // Copies pending assemblies and serialises remaining tasks so resume() stays valid.
  resumeData_t state() const;

  // Pulling results on the host's own thread (4.5):
  //   pop() returns a queued result at once if there is one, in every state; otherwise it blocks while the
  //   session is Running and returns std::nullopt once the queue is empty and the session is not Running
  //   (Paused/Finished/Stopped/Error), so a host drains everything produced before a pause.
  //   drain() pops up to maxItems without blocking and returns how many were taken.
  std::optional<solverResult_t> pop();
  size_t drain(std::vector<solverResult_t> & out, size_t maxItems = SIZE_MAX);

  // Polled status and progress (lock-free, safe from any thread)
  solverState_e sessionState() const;
  unsigned int currentAction() const;
  unsigned int currentActionParameter() const;
  float getProgress() const;
  unsigned long getIterations() const;
  uint64_t assembliesFound() const;
  uint64_t solutionsFound() const;
  std::optional<solverRunInfo_t> runInfo() const;   // populated once Paused/Finished/Stopped/Error
};
```

- `PAR_JUST_COUNT` becomes `solverOptions_t::countOnly`: count-only runs enqueue nothing while keeping `assembliesFound()` / `solutionsFound()` exact; the engines still build each assembly internally, as `solveThread_c` did.
- `PAR_DROP_DISASSEMBLIES` (store assembly without move list) and solution limits/sorting stay on the host side when consuming `solverResult_t`, removing the three `addSolution` paths from the solver. `PAR_KEEP_MIRROR` is gone with B1.

### 4.5 `solverResult_t` and the result queue

```c++
struct solverResult_t {
  uint64_t seq;   // discovery index: unique and gap-free, continues across pause/resume and setResume();
                  // result order is unspecified, so the queue order may differ from seq order
  std::unique_ptr<assembly_c> assembly;
  // Null when disassemble is false, or when the assembly does not come apart.
  std::unique_ptr<separation_c> separation;
};

struct solverRunInfo_t {
  double seconds;
  unsigned long iterations;
  uint64_t assembliesFound;
  uint64_t solutionsFound;
  bool complete;
  std::optional<rotationModel_t> rotationModel;
  unsigned int rotationsSkipped;
  assembler_c::errState error = assembler_c::ERR_NONE;
  int errorParam = 0;
};
```

A bounded queue as the primary output puts all thread-safety and backpressure logic once inside `solverSession_c` instead of duplicating it across front ends:

- **Host-side `problemCollector_c`:** holds today's storing, limit, drop, live sort and "keep the most complex" logic from `solveThread_c::assembly` and `onDisassemblyResult`, called on the host's own thread as it drains `solverResult_t` items into `problem_c`. `problem_c` is no longer mutated from solver worker threads. Moving that code is the step where the regression corpus must show identical files (B4).
- **FLTK GUI:** drains batches on its UI tick (or via `Fl::awake` from `onWake`) into `problemCollector_c` on the main thread.
- **Qt GUI:** passes a `QMetaObject::invokeMethod` wake hook as `onWake` and drains batches on the GUI thread directly into its QML models.
- **`burrTxt` / `burrTxt2`:** pulls with a simple `while (auto r = session.pop())` loop on the main thread.
- **Python:** calls `session.pop()` in `__next__()` (releasing the GIL while waiting); closing or dropping the iterator calls `session.abort()`.

### 4.6 `resumeData_t` and compatibility

```c++
struct resumeData_t {
  std::string engine;     // solver/engine id, state format version, and compatibility tag
  std::string state;      // opaque engine text (e.g. Dancing Cells decision list or bit-parallel seeds)
  std::vector<std::unique_ptr<assembly_c>> pending;   // found, not yet disassembled (today's stash)
  uint64_t nextSeq = 0;   // seq the resumed session hands out next (stored next to the state)
};
```

- **Explicit `pending` assemblies:** `assembly_c` is engine-independent (every host storing solutions already knows how to persist it), so keeping `pending` explicitly in `resumeData_t` respects the boundary between the assembler's opaque `state` string and the session's assembly-to-disassembler handoff, and keeps `.xmpuzzle` storage unchanged in B. `state()` clones `pending` so a subsequent `resume()` on the same session still disassembles them.
- **No compatibility guarantees:** a saved state is only guaranteed to resume with the same solver/engine, BurrTools version, machine/build and problem/options that produced it. That is a limit on what is promised, not a rule to reject: elsewhere it may well work (today's state does not depend on the machine). When a state cannot be used (unknown engine or format version, a problem or options that do not match, damaged text, or an engine that cannot run here, e.g. an x86-only engine's state on ARM), the session makes no attempt to migrate or guess: `start()` returns `false` before any worker starts and the session is in `Error` with `ERR_CAN_NOT_RESTORE_*`. Dancing Cells' decision-list state avoids depending on dancing-links internals, which is an internal simplification, not a cross-engine or cross-version resume promise.

### 4.7 Threading, ordering and backpressure

| Operation | Thread | Concurrency & blocking |
| --- | --- | --- |
| Producers pushing into queue | assembler worker threads (when `!disassemble`) or disassembly workers (when `disassemble`), as they finish: queue order may differ from `seq` order | Internal queue mutex; blocks when `Running` and `queue.size() >= queueCapacity` (backpressure); calls `onWake()` on empty → non-empty or terminal transition, from a solver thread and possibly under solver-internal locks (it must not block or call back into the session) |
| `pop()` / `drain()` | host's own thread | Single-consumer safe without external locking; `pop()` returns a queued item at once in any state, blocks only while the queue is empty and the session is `Running`, and returns `std::nullopt` once the queue is empty and the session is not `Running`; `drain()` never blocks |
| `pause()` | host's thread | Stops new task dispatch, lifts the soft `queueCapacity` cap for the bounded `O(num_threads)` in-flight items so workers never deadlock against a full queue, parks workers and pool, and transitions to `Paused` |
| `state()` | host's thread (only while `Paused`) | Lock-free against workers (all parked); snapshots engine state and clones `pending` |
| `resume()` | host's thread (only while `Paused`) | Restores soft `queueCapacity` cap and unparks workers with matrix, reductions and caches intact |
| `start(true)` | host's thread (only while `Idle`) | Returns at once; the session reaches `Paused` when preparation is done (`onWake` fires). With resume data the preparation runs inside `start()`, which may return with the session already `Paused` |
| `abort()` | host's thread (any state) | Signals immediate stop, unblocks any waiting producers and consumers (`pop()` returns `std::nullopt`), total and idempotent |

- **Order.** Order is unspecified; only the set and count are promised. Results enter the queue as worker threads finish them. `seq` is a unique, gap-free discovery index (also across pause/resume), so the queue order may differ from `seq` order.
- **Pause/save invariant.** When `pause()` returns, every result produced up to the pause point is either already sitting in the queue or in `state().pending`. The host drains the queue, stores those results together with `state()`, and then either calls `resume()` or discards the session. Nothing is lost or repeated across pause at any point (B4's check).

**Risks.** `problem_c` carries the result shape's symmetry and the mirror information the assembler needs, which must travel in the spec; the rotation model's reset on change crosses the same boundary; the GUI polls progress from the solver object, which has to stay cheap and lock-free; in-place `resume()` must cleanly park and wake worker threads (or re-enter the task pool on the retained matrix and caches) without leaking threads or races under TSan.

**Decided by the user, 2026-11-20:**

1. Directory and names: `src/lib/solver/`, `solverSpec_t`, `solverSession_c`, `solverResult_t`, `resumeData_t`.
2. The spec copies every shape at start.
3. Bounded pull queue (`pop()` / `drain()` + optional `onWake` hook) as the primary output mechanism instead of multi-threaded push callbacks; `problem_c` is updated on the host thread, never from solver threads.
4. In-place `pause()` → `state()` → `resume()` on a live session (plus `abort()`), keeping matrix/reductions/caches intact for checkpoints and GUI pause/continue; `state()` is only valid while paused.
5. `pending` assemblies stay explicit in `resumeData_t` (engine-independent value type, cloned by `state()`).
6. Resume carries no compatibility guarantees across solvers, versions, machines or mismatched options: it may work elsewhere, and resume data that cannot be used makes `start()` return `false` with the session in `Error` (user, 2026-11-20).
7. Python pulls from the session's queue (streaming, in the order results arrive).

### 4.8 What the Python library lacks (2026-11-21)

Python (`src/python/`) loads and saves puzzle files through the document model (`puzzle_c`), and solves through `solverSession_c` (B5). Against principle 6 it lacks:

- **A puzzle on any grid.** `Puzzle()` creates bricks only; the grid must be a constructor argument (by name and id, the ids of `gridType_c`).
- **Stored results.** `problem.solve()` hands assemblies and solutions to the caller only; nothing goes into the problem, so `save()` writes no results. The host-side collector the other front ends use (`problemCollector_c`) is the way in.
- **STL export.** The exporters are library code without GUI (`gridType_c::getStlExporter()`, `stlExporter_c::write()` for a shape or several, parameters by name, the rotation-model check), and `assembly_c::createPieceSpaces()` gives the pieces of a solution as placed. But **the solution export is GUI code**: exporting a solution assembled into one file, or one file per piece type with names made safe and kept apart when they clash, exists only in the FLTK dialog (`src/gui/stlexportsolution.cpp`, about 60 lines that are not presentation); the Qt GUI exports shapes only. It moves into the library before anything else calls it (B7).

Design rule from here on (principle 6): every new front-end feature names where Python reaches it, and keeps its logic out of widget code.

## 5. The grid contract

What the library promises about grids, shapes and placements. Accepted by the user on 2026-11-20 (B2). Built: cell addresses, shapes and the assembler on them (5.2, 5.3; B6), the layer model of 5.6 (`uicore`), sphere turns per shape (5.8, C4 step 1); not yet: placements and files in the new form (E), cylinders (5.9).

**Why.** Today the library exposes how each grid is stored. A shape (`voxel_c`) is a dense box addressed by (x, y, z) in the grid's own coordinates, and the emulated grids leave most positions unused (rhombic: 24 of every 125); FLTK's editors, the file format, `burrTxt` and Python read and write those coordinates. An assembly stores each piece as a transformation number plus a position in those coordinates, which is why the native rhombic format cannot hold a piece moved by half a cube diagonal (that position is 2.5 units away, 6.4). FLTK has one editor and one tool tab per grid. The 3D views, the meshes and the solver are already generic, and the Qt GUI has no editor yet, so a grid-neutral contract costs little now and removes per-grid code from every front end later. The user's rule: the representation is an implementation detail; clients do not depend on it.

### 5.1 Grid

A value object with a stable identity (an id, a name, a version). It provides:

- **Cell kinds**, finitely many. Each has two shapes: the **space cell**, the region the cell owns (cells fill space without gaps; neighbours and editing layers come from it), and the **solid**, what is drawn and what collides. For every grid but spheres the solid is the space cell.
- **Translations**: a finitely generated abelian group T acting on cells, written as integer coordinates n ∈ ℤᵃ × ℤ/N₁ × …. For the periodic grids this is the lattice, ℤ³; a cylinder's angular direction is ℤ/N (5.9).
- **Turns**: a finite list of rotations, and separately mirrors, acting on cells together with T. A **placement** is a turn g and a translation t. The list may be larger than the grid's symmetry group: a turn is **valid for a shape** when it puts every cell of the shape onto a cell (spheres need this, 5.8). Spheres have no list, because over all shapes their valid turns are infinitely many: the grid computes each shape's valid turns, and a problem collects them in a table of its own (5.8). For the other grids the list is the symmetry group and every turn is valid for every shape.
- **Group operations** (for spheres, within a problem's table): compose, invert, compare, "is a mirror", the canonical form of a placement.
- **Neighbours**: cells sharing a face, an edge or a corner, derived from the space cells.
- **Movements** for disassembly: a finite list of one-parameter motions (a straight slide along a translation for the periodic grids; a turn about an axis for cylinders), each with its **check cells**: the cells that must be free for a cell to make one step. A grid may also list **exits**: pushes along a fixed direction after which a piece has left the puzzle, with the cells each cell sweeps through as check cells. The grid computes them from its solids (NG's collision test for polyhedra, distances for spheres, exact rules for cylinders). This is what `movementCache_c` already consumes.

### 5.2 Cell address

A cell is `(n, k)`. `n` (a `vec3i_t`) says which copy of the grid's repeating unit cell the cell is in; the unit cell is the parallelepiped spanned by the lattice vectors (the columns of `L`), which NG's code calls the *epiped*. `n` ranges over `ℤ³` for periodic grids and over `(θ mod N, z)` for cylinders. `k ∈ [0, kindCount)` says which of the cells inside that unit cell is meant, so the cell sits at `p = base[k] + L·n` (NG's `getBaseForVoxelPosition(p) -> (k, n)`). Raw embedded `(x, y, z)` voxel coordinates let rotations be written as 3×3 matrices, but complex grids leave most triples unused (e.g. 24 of 216 in rhombic). Every `(n, k)` with `0 ≤ k < kindCount` names a real cell; translating by `dn` is simply `(n + dn, k)`. Applying a turn, physical position (`vec3d_t`/`pose3d_t`), outline, solid and neighbours are questions to the grid. Small linear-algebra value types (`vec3i_t`, `vec3f_t`/`vec3d_t`, `mat3d_t`, `pose3d_t`) are part of the core API so geometry, placements, and disassembly animations never pass raw pointer triples or `double m[9]` arrays across the boundary.

### 5.3 Shape

A finite set of cells, each with attributes (filled or variable, colour). Operations: apply a placement, test equality up to translation, normalise, compute the shape's symmetry (the placements that map it onto itself). No box size, hotspot or origin in the contract.

### 5.4 Assembly

A list of (piece, placement). Two assemblies are the same when a placement that maps the result onto itself maps one onto the other, mirrors included, a mirrored piece being replaced by its mirror partner. This is BurrTools' default and NG's only behaviour (NG's `assembler.h:703-790`, `:990-1030`).

**Decided by the user, 2026-11-18:** the "keep mirrored assemblies" option goes (B1). It dates from when finding mirrored assemblies was hard, and it is not stored in released file formats.

### 5.5 Geometry for viewing and export

Each kind provides its solid as a mesh in physical space and the placement of a cell's solid. A generic mesher hides faces two filled cells share, applies bevels, writes STL. Picking returns cell addresses. No per-grid drawing code.

### 5.6 Editing

The grid offers one or more layer families (planes for periodic grids; for a cylinder, horizontal layers and the unrolled wall of one ring). For a layer it lists the cells it meets with their 2D outlines, and hit-tests a point. Editing tools work on cell sets and placements (mirror and span tools are placements). A GUI needs no per-grid editor; the cross-section code is ported (`calculatePolygonIntersection` in `src/lib/latticegrid/grid.cpp`).

### 5.7 Solving and files

A problem is a result shape and pieces with counts or ranges. The assembler enumerates placements from the turns, the translations and the shapes, and builds whatever dense indices it likes (bitsets, the SIMD matrix, the cube fast paths); they never cross the API. The disassembler steps pieces along the grid's movements using the check cells; a cyclic coordinate wraps; a piece is removed when a movement takes it clear of the others, or by an exit.

A file stores the grid id, cells as (n, k) in compact runs, and placements as (g, t), g an index into the grid's turn list (a sphere turn as an integer quaternion, 5.8). Older formats are importers; nothing writes them. The new format is written by the release that introduces it; files of earlier releases stay readable.

**Private:** dense arrays, transformation tables, the 5-periodic rhombic emulation (kept only while FLTK needs it, C1), the cube fast paths, the sphere grid's 120 matrices. A grid may keep hand-tuned code (cubes will) as long as it implements the contract.

### 5.8 The grids in the contract

| Grid | Kinds | Translations | Turns | Movements | Implementation |
| :--- | :--- | :--- | :--- | :--- | :--- |
| Cubes | 1 | ℤ³ | 24 + 24 mirrors | 6 slides | native, fast paths |
| Triangular prisms | NG's | ℤ³ | NG's | NG's | NG's description (C3) |
| Rhombic | NG's rhombic tetrahedra (two per old voxel) | NG's lattice, with the half diagonal | NG's | NG's 13 | NG's description (C1) |
| Tetra-octa | NG's octahedral tetrahedra | NG's lattice = ours | NG's | NG's | NG's description (C1) |
| Hexagonal, Cairo prisms | NG's | ℤ³ | NG's | 4 / 3 slides (C6) | NG's description |
| Spheres | 1 | FCC | computed per shape, a table per problem (5.8) | none yet | native (C4) |
| Cylinders, helices | rings × layer kinds | ℤ/N × ℤ, or a screw | turn about the axis, flip | lift, turn about the axis, exits | new (C5) |

**Spheres.** BurrTools puts a sphere on every (x, y, z) with x + y + z even, the face-centred cubic packing; translations keep that parity (`voxel_2_c::onGrid`). The turn list has 120 rotations (`tabs_2/rotmatrix.inc`, exact matrices with entries in thirds), of which only 24, the cube group, are symmetries of the packing: `voxel_2_c::transform` turns a shape by any of them and fails unless every ball lands on a packing position. The extra turns are real: the packing contains hexagonal layers (the sloping faces of a square ball pyramid), and a piece lying in one can turn by 60° within it. Checked 2026-11-18 with an independent brute-force counter (own placements, exact cover and duplicate removal; not kept): with the 120 turns it gives `burrTxt`'s counts on all three problems of `examples/BallRoom.xmpuzzle` (2, 1, 3) and on about 40 random sphere puzzles; with the 24 cubic turns only, BallRoom's 14-ball pyramid has 1 assembly, not 2.

*Why spheres do not fit NG's model* (the user, 2026-11-20, from the discussion with Andreas; NG's `help_en.md` only says "spheres in BurrTools 0.6.3 are broken"). Over all shapes, the turns that are valid for some shape are infinitely many: a piece of two balls far apart can be turned onto any packing vector of the same length, at angles no fixed list holds. Example: balls at 0 and (5,5,0). The packing has 84 vectors of that length, the 120 reach 12 of them, and the rotation about z with cos 4/5 that puts the second ball on (7,1,0) is not among them. Only a concrete puzzle has finitely many: a shape that is not a straight line has finitely many valid turns, because a turn is fixed by where two non-parallel difference vectors go, and there are finitely many packing vectors of each length. NG's model needs all turns up front, as symmetries of the whole grid forming a closed set whose subsets are the symmetry groups (NG's `grid.h:26-31`), so it cannot hold spheres.

*What the 120 are* (checked 2026-11-20 in exact arithmetic; script not kept): exactly the rotations that take two non-parallel pairs of touching balls to touching pairs. Consequences:

- For a piece whose balls form one touching cluster and do not lie on a line, the 120 are complete: every valid turn of such a piece is among them. A straight piece needs only the cube group, which takes every touching direction to every other.
- A piece with balls that do not touch is searched with the 120 only, which misses turns like the (7,1,0) one above. BurrTools does not refuse such pieces today.
- The 120 are not a group: 6,912 of the 14,400 products of two are outside them (`transAdd` gives `TND`). For touching pieces not on a line this does no harm: a result symmetry applied after a valid placement gives another valid turn of the piece, so the product is in the list. The cube group times the 120 stays in the 120.
- For a straight piece, a result symmetry outside the cube group composed with an arbitrary turn of the piece can fall outside the list (1,152 such pairs for a piece along (1,1,0)), but this never happens in the solver: placements are stored normalised, a straight piece's normalised turn is always a cube rotation, and a cube rotation composed with any of the 240 stays among them. Checked 2026-11-20: brute force and `burrTxt` agree on 82 sphere puzzles on flat (111) patches with non-cubic symmetries; guard tests `[spheres]` in `test/test_symmetries.cpp`, counter `bench/merge-checks/sphere_bruteforce.py`.

**In the contract:** one kind whose space cell is the rhombic dodecahedron around the ball (they fill space; two balls touch exactly when their dodecahedra share a face) and whose solid is the ball. Addresses map one-to-one onto today's even-parity coordinates.

**Decided by the user, 2026-11-20: any sphere shape**, not only pieces of touching balls. What follows:

- **Turns per shape.** Spheres have no turn list. The grid computes a shape's valid turns:
  1. take two non-parallel difference vectors a, b of the shape (short ones keep the search small);
  2. list the packing vectors a′, b′ with |a′| = |a|, |b′| = |b| and a′·b′ = a·b;
  3. for each pair, take the rotation with a → a′, b → b′, a×b → a′×b′, and keep it if every ball lands on the packing.

  The mirrors are these turns composed with the point reflection. Every such turn is a rational matrix. A straight shape has infinitely many valid turns (any rotation about its line) but finitely many placements, so it gets one canonical turn per image direction d′: the half-turn about d + d′, which is rational. For d′ = −d it is a half-turn about an integer axis perpendicular to d.
- **Turns per problem.** A problem's turn table holds the result's symmetry group (finite unless the result is straight) and each piece's valid turns up to the piece's own symmetry. Every product the duplicate check forms is in this table: a result symmetry applied after a valid placement gives another valid placement. So composition is never partial, and the straight-piece gap of today's list goes away.
- **Code.** Transformation numbers are `unsigned char` (`assembly.h`, with `TND` = 255). A symmetry group is an index into a per-grid list of at most 255 (`symmetries_t` is `uint8_t`; spheres have 241 groups today). A spread piece can have more turns than that, and its symmetry groups cannot be listed in advance. So spheres need a symmetries object built per problem, with wider transformation numbers. With B6's shape type that is the natural place for it: the assembler already builds its dense indices per problem (5.7).
- **Files.** A sphere turn is stored as a primitive integer quaternion q (the rotation v ↦ q v q̄ / |q|², with a fixed sign rule) plus a mirror flag, not as an index. Every rational rotation has such a quaternion, and today's 120 need components of at most 3 (norms 1 to 12; checked 2026-11-20). The turn numbers in older files map through the table of the 120. This is part of E.
- **Results and cost.** Pieces of touching balls get exactly today's turns. A spread piece gets more: the two-ball piece at distance (5,5,0) can point 84 ways, not 12. So placements grow with how spread the pieces are, and puzzles with spread pieces can gain assemblies. Each such case gets looked at.

### 5.9 Cylindrical and helical puzzles

The user's example (2026-11-18): a cup with a central post, filled with ring segments in layers. A piece can be lifted along the axis, turned about the axis (which loops after a full circle), and flipped upside down; nothing else.

- **Cells.** Polar: ring r (finitely many), angular sector θ, layer z. Each ring is one cell kind (its sectors are congruent); sectors of different rings are not.
- **Translations.** T = ℤ/N × ℤ: θ ↦ θ + a (mod N), a turn about the axis, which the contract treats as a translation because it moves every cell to a cell of the same kind and commutes with the other translations; z ↦ z + b. The ring never changes: a segment of one radius does not fit another.
- **Turns.** The identity and the flip, a half turn about a horizontal axis: (θ, z) ↦ (−θ, −z). The mirror (θ, z) ↦ (−θ, z) follows 5.4.
- **Movements.** Lift: z + 1, check cell the one above. Turn: θ + 1 within the same ring and layer, check cell the next sector, exact because a turn about the axis keeps every point at its radius. A piece is removed when lifted clear of the cup. The disassembler's positions gain a cyclic coordinate.
- **Exits.** A piece of the outer ring can be pushed out sideways through an opening in the cup's wall. That does not end on cells, so it is an exit: a straight push along a fixed direction after which the piece has left. Its check cells per cell are the cells its solid sweeps through on the way out, here the outer cells between its own sector and the opening's, in its layer. The cup is a fixed piece made of wall cells (a further ring), and the opening is where it has none. Exits are not special to cylinders; any grid can list them (a slot in a box at an angle no step takes).
- **Helical puzzles** fit with a screw as the translation: one step turns by one sector and rises by 1/N of a layer, so the cells along the thread are ℤ (or ℤ × ℤ/k for k threads), and the thread's turn is its movement.
- Confirmed by the user: the wall pieces (zigzag and L) stay in one ring across several layers and sectors. Radial play matters for printing only, not for solvability, so there is no radial motion besides exits.

What it takes: three generalisations the periodic grids do not need (cyclic translation coordinates, movements that turn about an axis with grid-supplied check cells, exits). Geometry is curved, so the solid is a mesh approximation for drawing and STL, and the check cells come from exact rules, not a polyhedron collision test. NG cannot express these grids (it requires three independent lattice translations).

**Open:** the editor's layers for cylinders (the horizontal disc and the unrolled wall of each ring).

### 5.10 How far the code is from it

- NG's grid descriptions match 5.1 and 5.5 for the polyhedral grids. They address cells by coordinates plus base positions rather than (n, k); a fixed map.
- `voxel_c` leaks storage: box sizes, raw coordinates with unused positions, the hotspot.
- Transformation numbers are turn indices; positions are raw coordinates.
- FLTK's native per-grid editors, `burrTxt` and Python use raw coordinates. For described grids FLTK edits on `uicore`'s layer model (`layerGridEditor_c` + `ToolTab_lattice`, PRs 30–31) and keeps its native editors for bricks, spheres, rhombic and tetra-octa.
- The Qt GUI is generic except for the editor it does not have yet.

## 6. Grids as data: NG's model in this code base

### 6.1 The approach

NG defines a grid by data: a lattice, the cell positions inside one lattice cell, a convex polyhedron per position, and movement directions. Neighbours, valid transformations, symmetries and collision rules are derived from that. That derivation is ported (`src/lib/latticegrid/`, NG's namespaces and class names kept) and sits behind `voxel_c`, `symmetries_c` and `movementCache_c` as one more grid family (`voxel_lattice_c`, `symmetries_lattice_c`, `movementCache_lattice_c`), next to the native ones. Assemblers, disassembler, task pools and both GUIs' 3D views work through those interfaces and needed no change. Rejected: a hand-written class per new grid (about 500 lines of voxel class, a symmetry table set and a movement cache each, with generators that are in no build file), and replacing our grid model by NG's (it would discard the dense arrays and everything written against them, for no gain on cubes, where almost all puzzles are).

The property that makes it work: for a periodic grid and one of its rotations R, the translations t for which "rotate by R, then shift by t" maps the grid onto itself are one lattice class. So each rotation gets one fixed shift, shapes are compared up to lattice translation, and that is what `transform()`, `identicalInBB()` and `onGrid()` already mean on the emulated grids. G0 verified it on all six grids.

### 6.2 NG's model

In NG's `grid.h`, `grid.cpp`, `transformation.*`, `symmetry.*`, `algebra.h`, `mpr.h`, all now in `src/lib/latticegrid/`:

- **Description:** a storage id, a name, the lattice as a 3x3 integer matrix in cell coordinates and a float matrix in physical coordinates, the cell positions inside one lattice cell ("base positions"), one convex polyhedron per base position, optional movement vectors, an optional physical offset, and editor snap points.
- **Coordinates:** every cell has one integer triple; not every triple is a cell. A cell is `base position + lattice matrix * n`.
- **Registration** checks the polyhedra (convex, planar faces, closed), normalises base positions into the cell, builds an O(1) lookup from a coordinate to its base, and finds face, edge and corner neighbours by matching faces.
- **Grid symmetries** (`findGridSymmetries`): tries 30 rotations and 30 mirrored ones about candidate centres on a half-integer raster and keeps those that map every base position and every polyhedron vertex onto the grid.
- **Transformations:** 60 fixed ones, the cube group (24) and the hexagonal group (12) and their point reflections. They do not form a group; a product outside the 60 is "invalid". Matrices are integers over denominator 3, so 60 and 120 degree turns are exact.
- **Symmetry groups:** 136 closed subsets, enumerated at start-up, grid-independent.
- **Movement checks** (`calculateMovementChecks`): per base position and direction, the neighbour offsets that must be empty for the cell to advance one step, computed by sweeping the cell's polyhedron (shrunk to 0.99) one step against every neighbour polyhedron within two lattice cells, with a convex collision test (MPR). The free distance of shape A against B along d steps each cell of A while its check cells are empty in B.

The six grids:

| Grid (NG storage id) | Lattice cell (rows) | Cells per lattice cell | Movement vectors | Cell |
| --- | --- | --- | --- | --- |
| Cubes (0) | identity | 1 | 3 | unit cube |
| Triangle prisms (4) | (2,1,1), (-1,1,1), (-1,-2,1) | 2 | 4 | up and down triangular prism |
| Hexagonal prisms (3) | (1,1,1), (-1,0,1), (0,-1,1) | 1 | none in NG (4 added in C6) | hexagonal prism |
| Cairo prisms (5) | (2,-2,0), (2,2,0), (0,0,1) | 4 | none in NG (3 added in C6) | pentagonal prism |
| Rhombic tetrahedra (1) | (6,0,3), (0,6,3), (0,0,3) | 24 | 13 | tetrahedron, half of our rhombic voxel |
| Octahedral tetrahedra (2) | (3,3,3), (-3,3,0), (0,0,3) | 10 | 9 | tetrahedron |

NG's importer (its `import.cpp:1503-1603`) maps our grids onto these: bricks unchanged; triangular prisms by `xn = x+1; yn = (3y+1)/2 - (x + y%2)/2; zn = -xn-yn;` then z added to all three; rhombic by spreading each period of 5 to 6 and writing each of our voxels as two of NG's; tetra-octa unchanged. These are `convertToDescribed()` in `converter.*`.

### 6.3 How it sits here

- **Grid types**, one enum value each (user, 2026-11-15): `GT_HEXAGONAL_PRISM`, `GT_CAIRO_PRISM`, `GT_RHOMBIC_TETRAHEDRA`, `GT_OCTAHEDRAL_TETRAHEDRA`, `GT_DESCRIBED_TRIANGULAR_PRISM` (now the triangular prism grid everywhere, C3), and for cross-checks `GT_DESCRIBED_BRICKS` (5 to 10), written as `<gridType type="N"/>` like the native ones; each maps to its NG storage id in code. `gridType_c::isDescribed()`, `offeredForNewPuzzles()`. Descriptions are compiled in (`griddescriptions.cpp`, `registerNgGrids()`).
- **`latticeGridInfo_c`** (`src/lib/latticegridinfo.*`), built once per grid: our dense transformation numbers (NG's valid rotations in order, then the same mirrored), integer maps with the one shift class per transformation, the product table, NG's groups as our bitsets, face, edge and corner neighbours, drawing geometry.
- **`voxel_lattice_c`:** NG's coordinates in the dense array, moved only by lattice vectors; the hotspot is always a cell position; `identicalInBB` adds the lattice test; exact scaling via `scale()`/`canScale()` where the grid supports it (cubes, triangular prisms, rhombic, tetra-octa; PR 31). `transform(0)` is the identity and leaves the space alone, because `assembly_c::transform` takes the result's position under transformation 0 as the original's.
- **`symmetries_lattice_c`:** a symmetry group is NG's group number; the unique, minimiser and unified tables are computed from the product table; `calculateSymmetry` transforms the shape (at most 48 transformations).
- **`movementCache_lattice_c`:** steps each cell along NG's movement vector and tests NG's check cells for its kind (with filled cells and base indices cached per shape, PR 25). A grid disassembles when it has movement vectors and all are lattice vectors: all six described grids now do (C6 added movement vectors for hexagonal and Cairo prisms).
- **Shared code:** `voxel_c::getMeshInternal` takes faces with any number of corners; `voxel_c::drawingBounds()`, because a described space is a parallelepiped that can reach below 0.
- **Front ends today:** FLTK views, solves and edits every described grid (`layerGridEditor_c` over `btui::LayerModel` and `ToolTab_lattice`, PRs 24, 30, 31) and offers triangular, hexagonal and Cairo prisms for new puzzles (while keeping native rhombic and tetra-octa); Qt's new-file list is data-driven from `offeredForNewPuzzles()` (offering NG's rhombic, tetra-octa and all three prism grids); `burrTxt -D` solves a puzzle converted to its described grid.
- **Where a grid reaches outside these classes** (sites for C1 and C2): `gridtype.{h,cpp}`; `converter.cpp`; STL in `stl_0.cpp` and `minkmesh.cpp`; Qt's `documentcontroller.cpp`, `qml/NewFileDialog.qml`, `toolscontroller.cpp`; FLTK's `guigridtype.cpp`, `grideditor_N.*`, `grideditor_layers.*`, `tooltabs.*`; tests' `test_helpers.h` (`ALL_GRIDS`, `NATIVE_GRIDS`), `test_grid_invariants.cpp`, `test_symmetries.cpp`, `test_voxel.cpp`, `test_disassembly.cpp`.

**Risks still open.** Movement checks come from a float collision test (NG's 0.99 shrink is the margin; cubes and prisms agree exactly with the native rules, G2). NG's edge-loop code has a "can go wrong" TODO (`src/lib/latticegrid/grid.cpp:655`); only edge and corner neighbours depend on it, the solver uses face neighbours. Dense arrays with few valid cells (rhombic: 24 of 216) cost memory and placement loops; measured in C1 (`RhombicDodecahedraBox.xmpuzzle`) and mitigated by `BURRTOOLS_MERGE_COLUMNS` (PR 17) and `(n, k)` placement enumeration (B6, PR 26).

### 6.4 Rhombic and tetra-octa: the facts behind C1

Checked 2026-11-18 by a script against `griddescriptions.cpp` and `burrTxt`:

- **Tetra-octa is NG's octahedral tetrahedra:** the same cells (the importer's map is the identity) and the same translations (`voxel_4_c::onGrid`: multiples of 3 with even parity = NG's lattice (3,-3,0), (3,3,0), (3,0,3)).
- **Our rhombic voxel is two of NG's rhombic tetrahedra**, and the pairing is a partition: 192 voxels in 8 periods are 384 NG cells, each used once, every pair convex.
- **Our rhombic grid has fewer piece positions than NG's.** `voxel_3_c::onGrid` allows multiples of 5 only, which is NG's (6,0,0), (0,6,0), (0,0,6). NG's grid also allows the body-centred (3,3,3), half a cube diagonal, under which no pair of ours maps onto a pair. Rhombic dodecahedra sit around cube centres and around cube corners; only NG's grid can move a piece from one to the other. A one-piece puzzle whose result is the piece's dodecahedron moved by half a diagonal: native "Piece 1 can be placed nowhere", NG's grid 1 assembly. Neither grid has the face-centred (3,3,0); positions like that cut cells.

**Decided by the user, 2026-11-18:** solve what is solvable physically, so NG's lattice. The first version of G5 offered the conversion to the user (built, then dropped before review): how a grid is implemented is not the user's choice. Files: types 3 and 4 stay readable, converted on load (released formats), with saved assemblies (positions through the importer's map, transformations through a table from `tabs_3`/`tabs_4` numbers to the described grid's; neither grid disassembles natively, so there are no disassemblies to convert); search state is dropped (unreleased). Written: NG's grid types. Rhombic has to (a half-diagonal position cannot be written in type 3); tetra-octa follows for one write path, though it could keep type 4 if older releases should open new tetra-octa files.

## 7. Record: what is done

Numbers are from a release build on an 8-thread laptop VM, single runs unless said otherwise: good for factors of two, not for percentages. Intermediate measurements that later work superseded are in the removed notes at `f07f15fc`.

### 7.1 The assembler (steps 1.1 to 1.11)

**What the first measurements corrected.** The comparison document credited Dancing Cells with NG's lead on piece-range problems (Lomino 11x11: NG 6.2 s, we 10.7 s, one thread). Counting nodes showed the gap was symmetry breaking: NG visited 1.09 M nodes, we 4.3 M, because `assembler_1_c::prepare()` switched the breaker off as soon as any shape had a range. NG restricts a piece every assembly uses exactly once, whatever the other ranges. With that rule here (1.1) both search the same tree, and on the same matrix our bit-parallel engine is the faster one:

| Problem, 1 thread | Bit-parallel (ours) | Dancing Cells (ours) | Dancing Cells (NG) | Dancing links (ours, gone) |
| --- | --- | --- | --- | --- |
| Lomino 11x11 | 3.3 s | 6.5 s | 6.2 s | 12.7 s |
| Lomino 12x12 | 11.5 s | 22.3 s | 22.9 s | 47.8 s |
| SolidSix | 0.45 s | 2.0 s | 3.5 s | 5.7 s |

So the bit-parallel engine stays first, and Dancing Cells replaced dancing links for everything it does not take (over 32,768 columns, over its memory budget, variable cells with piece ranges): equal to 2.3x faster on the larger problems, much faster on some small ones (Alpaca 13.9 s to 3.9 s, Simplicity 1.67 s to 0.06 s, one thread), with no size limit and a saved state that does not depend on engine internals.

| Step | What | Result |
| --- | --- | --- |
| 1.1 | Symmetry breaker with piece ranges: a shape with min = max = 1 breaks the result's rotational symmetry even when other shapes have ranges (`BURRTOOLS_RANGE_SYMBREAK`) | Default on. 19 corpus problems with ranges, 14 finish in 40 s with identical counts; Lomino 13x13 checked separately. Lomino 11x11 10.7 s to 3.4 s, 12x12 41.8 s to 11.7 s (one thread) |
| 1.2, 1.3 | Dancing Cells (`CellsCover`) behind `ISimdHuangCover`, in the task pool, split on demand, stop/resume as decision lists; tests `[cells]` | Done. Sparse-set lists per column, backtracking by restoring list lengths, binary select/discard branching; a node's solutions are exactly the row sets containing the selected and none of the discarded rows, so one decision list describes a donated piece, a stopped task's remainder and the saved state alike (remainder type 3). Written from Knuth before NG's code could be used; compared line by line with NG's `dancingcell.cpp` on 2026-11-19: same algorithm (Knuth's F with binary branching). Andreas is credited in `cells_cover.h` |
| 1.4, 1.5 | Dancing Cells as `assembler_1`'s fallback; it also takes variable cells with piece ranges (at a node where every minimum is met it keeps branching on the rows left and reports at the leaves) | 520 corpus problems, none differs |
| 1.6 | Forced-item pass in the bit-parallel engine | Not done, optional: the search already picks the column with the fewest rows |
| 1.7 | Symmetry breaker by placement count | Not planned: the breaker divides the search by the same factor whichever piece carries it |
| 1.8 | One assembler for every problem | Done 2026-11-16: `assembler_0` and `SimdExactCover` removed (about 4,600 lines). 437 problems verified first, none differs. The remaining gap on problems with millions of assemblies was a per-candidate count of empty hole columns, now a popcount; a node-to-placement table replaced a binary search per placement. Afterwards `assembler_1` is faster (best of three: LogisTIC 2.53 s to 2.14 s, Hog Burr 36.8 s to 33.6 s, Burr-Glar 21.6 s to 18.9 s, one thread; corpus of 437 problems 40.3 s to 37.9 s) |
| 1.9 | Engine choice by more than size | No rule (one thread): of 68 problems that finish, 7 take over 0.3 s; Dancing Cells is faster on one (Third Times the Charm, 0.92x), level on CondorsPeeper, 1.1x to 2.85x slower on the rest. Duplicate pieces do not predict it. A1.2 repeats it with threads |
| 1.10 | Retire dancing links | Done 2026-11-16 with 1.8: `assembler_1.cpp` 4,330 to about 1,500 lines. Saved state (`<assembler version="2.2">`, unreleased) is a state (0 not started, 1 stopped, 2 complete) plus, when stopped, the engine type (2 bit-parallel, 3 Dancing Cells) and its seeds. Every run with work left goes through the task pool, resumes included. `debug_step()`, `getAssembly()` and FLTK's step button are gone. Tests of removed internals deleted; where dancing links was the independent reference its counts are pinned |
| 1.11 | Review preparation | Superseded by A1 |

**Dancing Cells after tuning** (Lomino 11x11, one thread: 7.9 s at first, 6.2 to 6.5 s now, NG 6.2 s): open columns kept in a list of their own, so nodes and snapshots do not walk every column; a per-column "rows still wanted" counter in the inner loop; the search divided into at least 16 tasks before the workers start. Tried and dropped (slower or no difference): leaving a full column's list untouched, a running lower bound for the range column, the order of rows within the chosen column, tie-breaking between columns. NG's forcing step (2026-11-19): measured level within 2% on SolidSix, Lomino 11x11 and 12x12, Alpaca and Third Times the Charm, identical corpus counts, so not kept; our engine rejects a node without a forced row by one size check already. 95% of the time is taking rows out of lists in `select()`, the algorithm's own work. Progress within a task: done 2026-11-21 for both engines (`subtree_progress.h`): each worker publishes how far it is into its task through a slot (a node with n ways gives each 1/n of its weight; Dancing Cells gives the search with a row 1/r of its frame, r the column's rows), `getFinished()` adds the slots to the completed units with a snapshot read, and a donated piece gets the units of its share. 3 s of Burr-Glar: 149 distinct values of 149 on Dancing Cells (5 to 8 before), 148 on the bit-parallel engine. Still open: a hole count kept up to date, for problems with many variable cells.

### 7.2 Grids (G0 to G4)

- **G0, the assumptions** (`bench/ng-crosscheck/ngGridDump.cpp`). Check A: on all six grids every transformation has no valid shift or exactly one lattice class of them. A brute-force search over positions alone agrees except on the Cairo grid, where positions admit a second class for 16 transformations that the pentagons' orientation rules out; so shifts come from NG's polyhedron test. Check B: our tetra-octa cells are NG's octahedral tetrahedra on all 5,832 coordinates of an 18³ box; each of our 648 rhombic cells in a 15³ box becomes exactly two NG cells, together all 1,296 of the corresponding 18³ box.

  | Grid (NG id) | Bases | Rotations / mirrored valid | Largest group | Directions | Movement checks (max per base and direction) |
  | --- | --- | --- | --- | --- | --- |
  | Cubes (0) | 1 | 24 / 24 | 47 | 3 | 3 (1) |
  | Triangle prisms (4) | 2 | 12 / 12 | 132 | 4 | 14 (2) |
  | Hexagonal prisms (3) | 1 | 12 / 12 | 132 | 0 | 0 |
  | Cairo prisms (5) | 4 | 8 / 8 | 125 | 0 | 0 |
  | Rhombic tetrahedra (1) | 24 | 24 / 24 | 47 | 13 | 3,888 (20) |
  | Octahedral tetrahedra (2) | 10 | 24 / 24 | 47 | 9 | 528 (10) |

- **G1, the grid-independent mathematics:** `algebra.h`, `mpr.h`, `transformation.*`, `symmetry.*`, `issymmetricalong.h` ported whole, each with a header naming its origin; `pssst::strong` became plain structs, `iterate3d` returns a vector, `std::print` became `fprintf`. Tests `[latticegrid]`: NG's rotation-constructor and MPR tests, product and inverse tables, closure of all 136 groups, our `tabs_0` (48) and `tabs_1` (24) matrices each found exactly once among NG's 60. `COPYRIGHT` has the licence paragraph.
- **G2, registration and the six grids:** `grid.*` ported whole; the six descriptions as C++ data in `griddescriptions.*`. Tests: every grid registers with the counts above and one shift class per transformation; face neighbours symmetric; the triangular-prism checks, mapped through the importer, are exactly `movementcache_1.cpp`'s cells.
- **G3, assembly on described grids** (6.3). Tests `[lattice]`: transformations keep cells cells and compose as `transAdd` says; symmetry tables agree with transforming random shapes; converted example puzzles have the native counts; `ALL_GRIDS` includes the six grids. Hexagonal and Cairo counts, which NG's importer cannot load, are checked independently: a crooked ten-cell region tiled by every two-cell piece has as many assemblies as its adjacency graph has perfect matchings. `corpus_check.py described`: 517 problems on bricks, prisms, rhombic and tetra-octa, no mismatch.
- **G4, disassembly on described grids** (with step 3.1: a movement cache entry holds more than 8 directions, the values past the eighth beside the table so native entries stay one cache line). Converted cube and prism examples have the native solutions (`described-solutions`). Rhombic and tetra-octa examples, which do not disassemble natively, match NG (`crosscheck.sh --described`): HexSticks 33 assemblies and 2 solutions, AugmentedSecondStellation 2 and 1, FourPieceTetrahedron 1 and 1; of the 14 such corpus problems, 9 agree, BrokenSticks problem 1 has no result on either side, and PiecesOfEight differs (section 9).

### 7.3 Disassembler

- 3.1: done with G4.
- 3.3: a Floyd-Warshall patch for NG's movement closure (`bench/ng-crosscheck/closure-floyd-warshall.patch`) measured 17% on NG's disassembler (Excelsior 7.55 s to 6.28 s, Supernova 4.58 s to 3.73 s; ours take 0.56 s and 0.42 s). Offering it upstream is dropped with NG; kept as a record.

## 8. Decisions

| Date | Decision |
| --- | --- |
| 2026-11-15 | NG's code may be ported (GPL v3, author's permission); origin in file headers, `COPYRIGHT` paragraph |
| 2026-11-15 | One `gridType` value per described grid; descriptions compiled in; FLTK view and solve only; `src/lib/latticegrid/` with NG's namespaces and class names, our adapter classes in our naming |
| 2026-11-16 | One assembler, two engines; dancing links goes without waiting for a release |
| 2026-11-18 | Keep-mirror goes (5.4) |
| 2026-11-18 | Rhombic and tetra-octa on NG's lattice, old types read and converted, NG types written (6.4) |
| 2026-11-19 | The agreement with Andreas (section 1): NG discontinued, no solver I/O, pause/save/resume as the only crash recovery |
| 2026-11-19 | No Lua and no grid description files: grids are defined in C++ |
| 2026-11-19 | The project moves to GPL v3 or later, at a later point; nothing to do now |
| 2026-11-20 | Why spheres do not fit NG's model: over all shapes the valid turns are infinitely many, finite only for a concrete puzzle (5.8) |
| 2026-11-20 | Spheres for any shape, not only touching pieces: turns per shape and per problem, stored as quaternions (5.8) |
| 2026-11-20 | Sections 4 and 5 accepted (B2), then section 4 refined: bounded pull queue (`pop`/`drain` + `onWake`) instead of push callbacks, in-place `pause`/`state`/`resume` + `abort`, explicit `pending` assemblies in `resumeData_t`, no resume compatibility guarantees across solvers/versions/machines, Python on the queue with `ordered = false` |
| 2026-11-20 | FLTK keeps the native rhombic and tetra-octa grids until it retires; everything else uses NG's (C1) |
| 2026-11-20 | Python's `keep_mirror=` and `burrTxt2 -m` deprecated, not removed (B1) |
| 2026-11-20 | `ordered` dropped: result order unspecified (it already was with several threads; NG promises none either); the pool's merger and reorder buffer go |
| 2026-11-21 | A1's stack follows the work as done (3.A); `ordered` goes in its own PR before the session; C4 step 1 is held back until E; the stack merges as a whole, with no release in between |
| 2026-11-21 | The Python library must be able to do what the GUIs do (principle 6): any grid, saving with results, STL export; planned as B7, not built yet |
| 2026-11-21 | Triangular prisms on NG's grid everywhere, the FLTK GUI included; the native prism code goes (C3). FLTK then gains described-grid editing on `uicore`'s layer model (true mirrors in the nearest grid mirror plane), `ToolTab_lattice`, and new triangular/hexagonal/Cairo prism puzzles (C2, PRs 29–31) |
| Open | Release timing of the file format (E); cylinder editor layers (5.9); the rhombic editor's unit (C2); when to push (A3) |

## 9. Questions for Andreas, while he is reachable

The answers go away with NG.

1. **PiecesOfEight:** NG finds twice our assembly counts (86, 24, 40, 24, 11, 7 against 43, 12, 20, 12, 6, 4). Mirror handling does not explain it: NG's code removes mirror images and computes its grids' mirror symmetries, and our native tetra-octa grid gives our counts too. Until it is explained, count comparisons with NG on these grids need care.
2. **CubeInCage:** NG finds no solution where we find one, natively and on described cubes alike; between the two disassemblers, not the grids.
3. ~~NG's users, `.dbpuzzle` files~~: no importer (user, 2026-11-21); BurrTools need not read them.
4. **Anything else** in NG's mathematics that section 1's table misses, for example the shape generator (NG's node type 30) or anything in the analysis.

## 10. Questions for the user

Marked during work the user could not be asked about; work went on with what needed no answer. Answered ones stay, struck through, with the answer.

1. ~~C3, saved disassemblies~~: answered 2026-11-21: convert to NG's prisms everywhere, FLTK included, and remove the native code; the saved disassemblies are converted (my recommendation, not contradicted).
2. ~~B6, "`voxel_c` becomes the dense storage behind it"~~: answered 2026-11-21: a refactor that blocks nothing waits until after all the other work. `shape_c` with its `fromVoxel()`/`toVoxel()` bridges stays as it is until then.
3. ~~A1, deviations from the PR table~~: accepted 2026-11-21 ("rearrange commits and PRs to fit the actual work").
4. ~~FLTK editor for NG's grids~~: "please continue" taken as the go-ahead 2026-11-21; built (PR 30). **Mirror tools**, decided 2026-11-21: mirror X/Y are true mirrors in the grid's own mirror plane nearest the middle of the space (`LayerModel::mirrorPlane`: the grid's flip followed by a lattice translation that is a pure reflection, measured from the cells' geometry; one plane for every layer, so a piece stays symmetric in 3D). A mirror image counts only when it is a cell, its outline included. Where an image is outside the space the status line says so; where no cell of the shape has one the buttons are greyed out. No diagonal mirrors (no new features). Every described grid has x and y mirrors, Cairo included; an earlier version mirrored about the exact middle of the box, which on Cairo is never a mirror plane, so the tools never acted there (user: nearest plane, 2026-11-21, after asking whether it would surprise; the marking shows the partner before a click). Compared: our removed native prism editor reflected indices (x -> sx-1-x), which every cell survived but which was a true mirror only for odd sx (X) and even sy (Y); NG's editor has no mirror or stack tools at all, and its layers are physical planes over an unbounded grid. **Full layers** need the space stored differently: on the triangular and hexagonal grids voxel z is not physical up (they stack along (1,1,1)), so a box's layers are slanted slices and some cells' images fall outside the box whatever the plane; an upright region of cells, or those grids described with voxel z up, changes storage and files, so it goes with B6's deferred clause or E.
5. **E, the file format**: may it start? C4's rest (turns stored as quaternions) depends on it, and it ships with a release (3.E).
6. **A3**: when to push the stack.
7. **An FLTK crash, not reproduced** (2026-11-21, PR 31 testing): once, the FLTK GUI died (core file, no message) during: open Prisgon, rotate S1 about z with the new tab, File > New (don't save), triangular prism, new shape, type 9 into the X size and Return, click the layer slider and the editor. Replayed six times on the normal build, once under ASan and UBSan, five times under gdb (installed 2026-11-21), plus every button of the new tab with a backtrace handler: no crash. The core of the one crash was deleted before a debugger was available; cores land in the working directory (`core_pattern` is `core`), so a recurrence can be read with `gdb build/burrtools core.<pid>`.
