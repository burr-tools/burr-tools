# Qt GUI: reviewing a change (for an agent reviewing a hand-written PR)

Read the diff against `llm_docs/qt_gui_rules.md`, `llm_docs/qt_gui_qml.md`,
`llm_docs/qt_gui_accessibility.md` and `llm_docs/qt_gui_tests.md`; for each
gap, name the rule, point to the file that already does it right, and
suggest the smallest fix. Ask for a test when behaviour changed without one.

**Structure**
- Logic that could be tested without a window but landed in a controller or
  in QML → suggest moving it to `src/uicore` with a Catch2 test.
- A shortcut or menu item outside `uicore/commands.cpp`; a setting not in
  `SettingsController`; an environment variable not in `envvars.h`.
- A new `Q_OBJECT` header missing from `qtgui_moc_headers`, a new QML file
  missing from `qml/meson.build`, a new controller missing from README.

**C++**
- `Q_PROPERTY` without `FINAL`/`NOTIFY`, `qreal`, a setter that emits without
  comparing, `default:` in a switch over a project enum.
- A library call reachable from QML outside `guarded()`; a raw owning
  pointer; a file written in place or a write result ignored.
- GUI objects read from the render thread outside `synchronize()`, the
  window's `QRhi` used on the GUI thread, a mesh mutated after hand-off, a
  QObject touched from a worker thread.

**QML**
- A panel or dialog without a wireframe, or one whose numbers no longer
  match the code (the test catches numbering, not a stale drawing — read it
  against the UI).
- Missing `objectName`s; strings without `qsTr()`; literal colours or sizes
  instead of `Theme`; symbol characters outside the app font in strings.
- Untyped functions, `instanceof`, `property var` where a type exists, a
  delegate using outer ids without `pragma ComponentBehavior: Bound`; an AOT
  baseline raised without a reason.
- A dialog not loaded on demand, not counted in `modalOpen`, without
  `title` / `defaultButton`; work added before the first frame.

**Accessibility** (most often missed in hand-written UI)
- A clickable item without role, name, state or press action; a field or
  group without a name; decorative text not ignored.
- A tooltip only on hover, or help text that vanishes in Minimal.
- A control that cannot be reached or operated by keyboard; a focus that is
  lost when something hides; a ring missing on keyboard focus or shown on a
  mouse click; a read-only item in the Tab order.
- Text below 4.5:1 or a state below 3:1 in either theme; a state shown only
  by hue; text on a shape colour not using `prefersWhiteText`.
- A message to the user that is not announced (not through
  `App.status.flash` or a dialog).

**Tests and docs**
- Behaviour without a test; a QML test using `forceActiveFocus` for
  keyboard checks or leaving the window unactivated; a test that could reach
  the network.
- Reference images changed without a visual change that explains them.
- README's status / deferred / deviations not updated for what the PR did.
