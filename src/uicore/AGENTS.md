# src/uicore — guide for AI agents and contributors

The user interface's logic without a toolkit: everything that decides *what*
the GUI does, kept free of Qt and FLTK so it is tested in plain C++ and could
serve another front end. `src/qtgui` (burrtools-qt) turns it into Qt objects
and pixels; the legacy FLTK GUI shares two parts of it, the undo history
and the piece colours.
Namespace `btui`, built as `libburr_uicore` (root `meson.build`).

`src/qtgui` uses it together with the library's classes, not instead of
them (see the architecture in [`../qtgui/README.md`](../qtgui/README.md)).
The API here is in-tree only and changes with the later phases (the Puzzle
and Solver workspaces, the voxel editor); do not treat it as stable.

Read with it: [`../qtgui/AGENTS.md`](../qtgui/AGENTS.md) (the layer above),
[`../qtgui/README.md`](../qtgui/README.md) (architecture and status), the
root [`AGENTS.md`](../../AGENTS.md), and the spec
([`../../design-spec/BurrTools-SPEC-FULL-FINAL-v6.md`](../../design-spec/BurrTools-SPEC-FULL-FINAL-v6.md)),
whose component and acceptance ids (C06, AC-C13-03, …) the comments cite.

## Map

| File | What it decides |
| :--- | :--- |
| `commands` | **the command table**: every menu item and shortcut once — key, label (`&` access key), menu, divider, shortcuts per platform, the legacy callback it replaces, the macOS/Windows label and menu differences |
| `layoutmodel` | workspaces, card collapse per workspace, Focus 2D / 3D, the column widths for a window width and density |
| `documentsession` | open, save (beside the file, then renamed over it), import, new, the modified state, undo / redo |
| `drawcolours` | the puzzle's colours as the Drawing colour footer edits them: names, add (up to the library's 63), edit, remove (and whether it loses solutions) |
| `puzzlehistory` | the undo history (snapshots, stroke batching, depth limit) — **shared with the legacy GUI** |
| `settingsstore` | the settings file: `key = value` lines, typed reads with defaults, an atomic save |
| `camera`, `viewcube`, `vecmath` | the 3D view's camera (orbit, pan, zoom, eased animations, projections) and the view cube's faces, edges, corners and hit testing |
| `scenemesh` | the vertices the 3D view draws for a shape, per voxel style (Flat, Classic, STL), each vertex carrying its voxel; picking meshes and ray picks |
| `assemblyscene`, `depthsort` | whole puzzles and assemblies as scenes (image export); back-to-front order for translucent layers |
| `palette` | the shape colours, and black or white text on them (WCAG contrast) — the colours **shared with the legacy GUI** |
| `imagepages`, `vectorexport` | the image export's page layout; vector export (PS, EPS, TeX, PDF, SVG, PGF) |
| `puzzletools` | the status table's checks, convert, import assemblies |
| `shapelist` | the Shapes list's operations: new, duplicate, delete (and whether it loses solutions), rename, weight, reorder; positional ids |
| `voxeledit` | the voxel editor's rules: tools 1–4 and the modifiers' quick tools, Mirror and Span, the 2D planes and layers, what a tool does to a voxel, the 2D cell size, grid size (steps, apply to all, the blocked shrink) |
| `gridcells` | the 2D grid's cells per voxel type (squares, prism triangles, sphere circles, rhombic and tetra-octa super-cells) and hit testing |
| `vocabulary` | the settings' and modes' fixed vocabularies (theme, density, voxel style, projection, colour view, image mode, …): one table of values and words each |
| `windowplacement` | where the window opens: the saved place if still on a screen, else centred |

Tests: `../../test/test_ui_<name>.cpp`, tagged `[ui]`, compiled into
`test_burrtools` (`uicore_test_src` in the root `meson.build`); benchmarks in
`../../test/bench_ui.cpp` (`[bench]`, hidden); `test/alloccount.h` counts
allocations for "no allocation per …" checks.

```sh
./build/test_burrtools "[ui]"          # all of uicore's tests
./build/test_burrtools "[ui][scenemesh]"
just bench-ui                          # the benchmarks, release build
```

## Rules for authors

- **No toolkit.** No `<Q…>` or `<FL/…>` include, no Qt types, no FLTK
  calls. Standard C++20 and the library (`src/lib`, `src/halfedge`,
  `src/tools`). If something needs a window to be decided, it does not
  belong here; if a Qt controller holds a decision that needs no window, it
  belongs here.
- **Add a file in three places:** the source in `libburr_uicore`, its test
  `test/test_ui_<name>.cpp` in `uicore_test_src` (root `meson.build`), and a
  line in the map above.
- **Every behaviour has a Catch2 test**, tagged `[ui][<area>]`, written as a
  sentence of what holds ("a save replaces the file whole, and a failed one
  leaves the old file as it was"). Test the edge cases the GUI can reach:
  an empty puzzle, row -1 from a view tearing down, every grid type
  (`bttest::ALL_GRIDS`), both platforms' command tables.
- **Ownership:** library factories return owning raw pointers — wrap them
  at once (`std::unique_ptr`). `bt_assert` is for invariants (it compiles
  away in release); input the caller can get wrong is rejected or clamped,
  not asserted. A function the GUI calls must not throw for input it can
  produce.
- **Determinism:** no clocks, threads or I/O hidden in a computation; time
  and paths come in as arguments (see `updatecheck`'s `gate(…, now)` in
  `src/tools` for the pattern), so tests pin them.
- **The command table is the only source of menus and shortcuts.** Its
  tests check it against legacy's menu tables (every legacy command present,
  or in `dropped` with a reason), unique keys, unique shortcuts per
  platform, unique access keys per menu. A new command: a `Command` value, a
  row (label with an access key no sibling uses), its handling in
  `qtgui/CommandController::trigger`, and its legacy callback name if it
  replaces one.
- **Shared with legacy:** a change to `puzzlehistory` or to `palette`'s
  colours changes the legacy GUI too — run `test_puzzle_history.cpp` and
  the `[ui]` tests, and keep legacy's behaviour unless the change is meant
  for both. `palette`'s colours are legacy's arithmetic to the bit (double
  where it used double); `test_ui_palette` pins them.
  The rules while both GUIs exist (fixes go where both get them, legacy
  moves onto uicore piece by piece) are in
  [`gui-migration.md`](../../design-spec/gui-migration.md#the-legacy-gui-while-both-exist).
- **Changing the puzzle:** `DocumentSession::puzzle()` is read-only. A
  change goes through `edit(kind, shape, f)` — one undo step when `f`
  returns that it changed something, recorded even if `f` throws — or
  `editWithoutUndo(f)` for what is not an undo step (solver results), so
  no change can miss its step.
- **Files:** write beside the target and rename over it; report failure,
  never a partial write as success (`DocumentSession::writePuzzle`,
  `SettingsStore::save`).
- **Voxel styles and other enums:** handle every value, without `default:`
  in a switch, so a new one is caught by the compiler; a new voxel style
  also needs its render rule in `qtgui/SceneController::applyStyle` and its
  setting value. Every mesh vertex carries its voxel (`cell`): Dim other
  layers and picking depend on it.
- **Colour for text:** text on a shape colour is chosen by
  `prefersWhiteText` (the higher WCAG contrast, at least 4.5:1). Do not add a
  second rule anywhere.
- **Performance:** meshes are rebuilt once per change, not per frame; the
  hot paths (`scenemesh`, `depthsort`, `vectorexport`) have benchmarks and
  allocation checks — run `just bench-ui` before and after a change to
  them, and keep "no allocation per triangle / per frame" tests passing.
- Style: the repository's (prose comments, `f(void)`, two-space indent);
  the spec's ids in comments where a rule comes from it.

## Reviewing a change (for an agent reviewing a hand-written PR)

For each gap, name the rule, point to the file that does it right, and
suggest the smallest fix.

- A Qt or FLTK include or type in `src/uicore` → move that part to the
  toolkit layer, keep the decision here.
- Logic added to a Qt controller or QML that needs no window → suggest it
  belongs here, with a test.
- New behaviour without a `[ui]` test, or a test that only exercises the
  happy path where the GUI can pass edge values (empty, -1, every grid).
- A menu item or shortcut defined outside `commands.cpp`; a legacy command
  that disappears without an entry in `dropped`; a reused access key.
- A change to `puzzlehistory` or to `palette`'s colours without considering
  the legacy GUI.
- A raw owning pointer from the library, an assert guarding input, a
  function reachable from the GUI that can throw on reachable input.
- A change to the puzzle outside `DocumentSession::edit` /
  `editWithoutUndo` (a `const_cast`, a reference kept from inside `f`).
- A file written in place, or a write result ignored.
- A `switch` with `default:` over a project enum; a new voxel style or enum
  value not handled everywhere (`applyStyle`, settings, tests).
- A second text-colour rule, or colour logic duplicated from `palette`.
- Per-frame or per-triangle allocations added to `scenemesh` / `depthsort`;
  a hot path changed without benchmark numbers.
- A new file missing from `libburr_uicore`, its test missing from
  `uicore_test_src`, or this map not updated.
