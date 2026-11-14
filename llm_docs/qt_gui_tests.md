# Qt GUI: tests and docs

Where the suites live: `llm_docs/qt_gui_map.md`. What to run before pushing:
`llm_docs/verification.md`.

### Tests

- A behaviour change comes with a test: Qt Test (`test_qtgui.cpp`, a
  `Fixture` with a scratch settings file) for controllers; Qt Quick Test
  (`qml/tst_*.qml`) for QML behaviour; Catch2 for uicore.
- **A new way to change the puzzle gets a save case** in `TestSaving`
  (`test_saving.cpp`): make the edit through the controller, then
  `saveAndReopen()` and compare `describe()` -- the whole puzzle, line by
  line -- with the reopened file. Per voxel type where the edit differs
  by type (`addTypes()`).
- A spec acceptance criterion a test checks is named in the test's
  comment (`// AC-C06-04`), so a search finds what covers it.
- QML tests fail on any warning (`failOnWarning`). Activate the window
  before keyboard checks, and move focus with real `keyClick(Qt.Key_Tab)`:
  `forceActiveFocus` does not reliably give a control focus in the test
  window. Opening a puzzle can raise modal messages (its comment, an
  unfinished search): dismiss them before driving the window.
- No test reaches the network or the user's files; render tests use WARP
  or lavapipe and skip without a backend unless
  `BURRTOOLS_REQUIRE_GPU_TESTS` is set.
- Gallery reference images change only for an intended visual change, and
  that change commits the new images.

### Docs

Update `src/qtgui/README.md` when a phase item lands, is deferred (the
deferred table: item, spec reference, why, when) or deviates from the spec.
Keep the wireframes, the controller table and `--help` true.
