# uicore: rules for authors

For `src/uicore/`.

- **No toolkit.** No `<Q…>` or `<FL/…>` include, no Qt types, no FLTK
  calls. Standard C++20 and the library (`src/lib`, `src/halfedge`,
  `src/tools`). If something needs a window to be decided, it does not
  belong here; if a Qt controller holds a decision that needs no window, it
  belongs here.
- **Add a file in three places:** the source in `libburr_uicore`, its test
  `test/test_ui_<name>.cpp` in `uicore_test_src` (root `meson.build`), and a
  line in `llm_docs/uicore_map.md`.
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
  [`gui-migration.md`](../design-spec/gui-migration.md#the-legacy-gui-while-both-exist).
- **Changing the puzzle:** `DocumentSession::puzzle()` is read-only. A
  change goes through `edit(kind, shape, f)` — one undo step when `f`
  returns that it changed something, recorded even if `f` throws — or
  `editWithoutUndo(f)` for what is not an undo step (solver results), so
  no change can miss its step. A change to shapes' voxels or size goes
  through `editVoxels(kind, shape, shapes, f)` (or a stroke), naming the
  shapes `f` may change: the problems using one whose voxels changed lose
  their solutions, kept in the step for its undo.
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
