# The Solver workspace in burrtools-qt (P11) — plan, resolutions, status

Phase P11 of the redesign (`design-spec/BurrTools-SPEC-FULL-FINAL-v6.md`,
`migration/00-phase-plan.md`): the Solver workspace, components C19 (left
card), C20 (right card) and C21 (3D scene and explorer drawer), primitives
PR-27…PR-32, contract `overview/06 §11`, input `overview/04` "Solver tab
input" and §8, journeys J14–J16. This file gathers in one place everything
P11 owes — the spec's own items, the work earlier phases handed to it, the
open questions it must close — and what the legacy source answers to those
questions. `src/qtgui/README.md` keeps the status and the decisions once a
part lands; this file is the checklist while it is built.

## 1. What P11 owes

### 1.1 The spec's components

| Part | Spec | Content |
| :--- | :--- | :--- |
| P11a Model and controller | §11, C19 table | options per problem, the run state machine and its enabling, progress and counters, the solutions with their disassembly state, `solutions_shown` (filter + display sort with reverse), Manage (remove, delete and analyse disassemblies; confirmation; not undoable, only modified), the keyboard map `solver_key` |
| P11b Left card (C19) | AC-C19-01…05 | Problem selector + popover, What to check radio cards + "Don't keep the disassemblies", Search options (collapsed, "n changed"), Run row + progress + status grid, Step-by-step explorer buttons |
| P11c Right card (C20) | AC-C20-01…08 | header with count and List / Slider, the list (PR-28 rows, sortable headers, DA icons, footer), the slider view (title, prev / slider / next, six tiles, Sort-by segment), the empty states, the disassembly player (PR-29, fixed height, speeds 0.5×–8×), Manage menu with hover preview + confirm (PR-32), Export to STL, Piece display (PR-30 chips, fixed height, Show all) |
| P11d Scene and drawer (C21) | AC-C21-01…06 | the selected solution at the move position (one camera), piece states solid / wireframe / hidden, wireframe = all hull edges (back fainter) + ≈ 6 % tint, Display ▸ Show through solid pieces (default on), assembler-state mode with its caption, read-only view (no edit affordances; toolbar Orbit, Pan, Display, Focus; Display menu: Wireframe pieces, Projection, Voxel colour), tag "P1 label · Solution 3 of 5", status sentence, empty messages; the docked drawer (PR-31) |
| Density | density.md §1 | `solverListRow` 28/26, `player` 112/84, `pieceDisplay` 104/76 (listed as pending in `scripts/check_density_tokens.py` until read) |
| Keyboard (P12b part) | overview/04 §8, AC-C20-07 | one key map on list, solution slider and move seekbar; chips' keys; sticky focus |
| Focus order | overview/04 §9 | problem selector → radio cards → options → Run → explorer → List/Slider → sort headers (one stop) → list or slider → transport → speed → seekbar → Manage / Export → chips → Show all |

### 1.2 Handed to P11 by earlier phases (README, *Deferred to the next phase*)

| Item | From | What it asks |
| :--- | :--- | :--- |
| Edits while the solver runs | legacy `threadStopped()` | refuse shape, colour and problem edits, strokes, undo and redo, and replacing the document (New, Open, Import), and saving, while a solve runs; the gate cannot be only `DocumentController::edit` |
| Solution counts under the lock | same row | the lists that count solutions read them under `problem_c::lockSolutions()` while a solve runs |
| The solver's progress announced | Accessibility, WCAG 4.1.3 | the progress is a real `ProgressBar` with a name, and the run's state changes are announced (`App.status.flash`) |
| Solver results go through `editWithoutUndo` | Next phases, the #140 review | deletions, analyses and finished runs mark the document modified without an undo step (`DocumentSession::editWithoutUndo` / `markModified`) |
| The weight's motion frame | ShapesCard's weight tooltip, AC-C21-06 | heavier shapes stay, lighter ones move |
| The viewport's "Solver scene arrives…" placeholder and the two `PlaceholderCard`s | P1 | replaced by the real cards and scene |

### 1.3 Not in P11 (stays where it is)

- The rest of P8 (accessibility rows, hardening, cut-over) — unchanged.
- The legacy dialogs other than *Export Solution to STL* are already ported.

## 2. The open questions, answered from the legacy source

| OQ | Answer (where) | What P11 does |
| :--- | :--- | :--- |
| OQ-25 Manage semantics | `mainwindow.cpp` `cb_DeleteSolutions`, `cb_DeleteDisasm`, `cb_DeleteAllDisasm`, `cb_AddDisasm`, `cb_AddAllDisasm`: **All** removes every saved solution; **Before / At / After** the ones before, at, after the slider position; **w/o DA** every solution with neither a disassembly nor its info (level only); **D DA** removes the selected one's disassembly, **D A DA** every one's (assemblies kept); **A DA** runs `disassembler_0_c` on the selected assembly and stores the result when there is one; **A A DA** on every one, **A M DA** on the ones without a stored disassembly. All only `markModified()` | as legacy; Before / After over the list's displayed order (the product owner's rule); Analyse in the background with "Analysing n assemblies…" |
| OQ-26 Drop | `solveThread_c::setSolutionLimits(limit, drop)`: **keep only every n-th solution** ("save only every x-th solution, the others are dropped"), bounds 1…10⁸, default 1. Not a level | the row is **"Keep every"** n-th (min 1, default 1), help "Keep only every n-th solution found; 1 keeps them all". The spec's "Drop below level" does not exist in the solver |
| OQ-27 Sort found solutions by | the solver knows *Unsorted*, *Moves for Complete Disassembly* (default) and *Level* (`SRT_*`); at the limit, Moves and Level drop the lowest so the most complex are kept, Unsorted thins evenly; `setLiveSort(3)` keeps the list in piece order | Search order → `SRT_UNSORT`; Number of pieces → `SRT_UNSORT` + live sort by pieces; Level → `SRT_LEVEL`; Moves → `SRT_COMPLETE_MOVES` (default). Level / Moves need disassembly (fall back to Search order) |
| OQ-28 Placement / Movement browsers | `placementbrowser.cpp`: two sliders, piece and placement, the 3D view shows that piece at that placement inside the result; `movementbrowser.cpp`: a tree of disassembler nodes with *analyse node*, *analyse next levels*, *add movement*, *prune*, *step back* | the drawer: Placements as piece list + placement stepper; Movements as the breadth-first levels (§3, P11d; the tree itself waits) |
| OQ-29 Prepare / Start / Continue / Stop / Step | `cb_BtnStart` removes all solutions and resets the problem, then runs; `cb_BtnPrepare` = Start that returns after preparation; `cb_BtnCont` continues the problem as it is (a prepared or a stopped search); `cb_BtnStop` → `stop()`, the thread ends at `ACT_PAUSING`; Step = `assembler->debug_step(1)` on the GUI thread, enabled with an assembler, no thread and the problem not solved | as legacy. **Deviation:** Continue is enabled in *ready* too (legacy: it runs the prepared search; the spec's table disables it, which would leave Prepare useless but for the explorer) |
| OQ-30 "Move 19 (14.2)" | `SolutionAnim` range 0…`sumMoves()`, value the position; `MovesInfo` "sumMoves (movesText)" | "Move 14.2 / 19" as the spec says |
| OQ-31 Groups | the disassembler moves a group's pieces together; nothing in the player | no extra UI (default) |
| OQ-32 Just count | nothing is saved; the counters count | counters only; the list's empty state says the run only counted |
| OQ-33 Export Solution to STL | `stlexportsolution.cpp`: the exporter's parameters, file name, binary; exports the selected solution's assembly | ported as its own dialog (P11c); until then the button says so |

Other facts read there:

- **Errors** at the end of a run (`mainWindow_c::update`): too many / too few
  units, a piece placed nowhere (legacy then selects that shape in Entities),
  restore refused, cannot start, unhandable puzzle; an internal error
  (`ACT_ASSERT`). Each a message; the Qt GUI says the same words.
- **Activity words:** prepare piece n, optimize piece n, assemble,
  disassemble, pause, finished, please wait, error; without a thread:
  nothing, finished, pause, partial (`SS_UNKNOWN`).
- **Time:** `timeToString` (seconds, minutes, hours, days, months, years,
  millennia, ages); time left = used / progress − used, "unknown" at 0.
- **Weight (AC-C21-06)** is the disassembler's: `movementAnalysator_c::newNode`
  swaps moving and still pieces when the moving side is heavier, so a stored
  disassembly already keeps the heavy pieces in place. The player shows it as
  stored.
- **Piece names** (`PieceVisibility`): "S3", or "S5.1", "S5.2" for a part
  with more than one copy; visibility cycles 0 → 1 → 2 → 0 (solid,
  transparent, hidden).
- **Following new solutions** while a solve runs (`followingTail`): the view
  rides the newest best unless the user picked another. P11 keeps the
  selection by assembly number and starts on the first one found.

## 3. Decisions taken for the build (each logged in README's P11 decisions)

1. **Logic in uicore, `btui::solver`:** options → solver parameters, the run
   state from the problem and the thread, the buttons' enabling, the shown
   list (filter, sort, reverse), Manage's row sets (preview = deletion), the
   piece ids and their visibility cycle, the player's arithmetic (snap,
   step, speed, end), the time words, the key map. Catch2 `[ui][solver]`.
2. **`SolverController`** (`App.solver`) owns the one `solveThread_c` and the
   background analysis, polls them on a timer (as legacy's `update()`), and
   tells `DocumentController` that the document is busy; it reads solutions
   only under the lock.
3. **The busy gate is in `DocumentController`:** every edit path (`edit`,
   `strokeEdit`, `beginStroke`, undo, redo, the flows that replace the
   document, save, convert) asks `busy()` first and refuses with one status
   message, "Stop the solver first" — legacy's `threadStopped()` message.
4. **Options are per problem for the session**, not in the file (the file
   format has no place for them; legacy keeps them per window). The spec's
   "with file" is a deviation.
5. **The Solver's problem** is its own (legacy `solutionProblem`); opening the
   Solver adopts the Puzzle workspace's selection unless a solve is running.
6. **The scene:** each piece's mesh is built once per solution and voxel
   style, at its assembled place; a move position only offsets them
   (`btui::SolutionScene`). Wireframe edges are the meshes' feature edges
   (between faces of different normals), drawn as overlay lines; X-ray draws
   them as the ghosts' lines are drawn (full in front, faint behind), off
   draws them depth-tested.
7. **Analyse runs in the background** on copies of the assemblies; the
   results are applied on the GUI thread under the lock. Manage and every
   edit are refused meanwhile.

## 4. Order of work and status

| Step | Content | Status |
| :--- | :--- | :--- |
| 1 | this plan | done |
| 2 | `btui::solver` + tests | done |
| 3 | the busy gate in `DocumentController` + tests | done |
| 4 | `SolverController` (run, poll, rows, selection, player, visibility, Manage, Analyse, Step) + Qt tests | done (`test_solver.cpp`) |
| 5 | `btui::SolutionScene` (pieces at a position, feature edges) + tests; ViewportController's Solver frames | done |
| 6 | QML: SolverLeftCard (C19) | done |
| 7 | QML: SolverRightCard (C20): list, slider, player, manage, chips | done (`tst_solver.qml`) |
| 8 | Viewport: tag, hint, toolbar, Display menu's Wireframe section, empty messages, assembler-state caption | done |
| 9 | Explorer drawer (C21, PR-31) | done; legacy's movement tree deferred |
| 10 | Export Solution to STL dialog | done (the STL dialog's solution mode) |
| 11 | README: status, decisions, deferred; density tokens off the pending list; gallery rows for the new primitives | done; macOS references wait for a Mac |
| 12 | review of 2026-11-13 (design, performance, accessibility, keyboard, undo, grids, 1000+ results, mixed results): every item fixed, with tests | done; README's P11 decisions list the changes |

Found on the way: **D DA keeps the level** (`solution_c::removeDisassembly`
keeps a `separationInfo_c`), so legacy's "delete the disassembly" makes a
solution *level only*.
