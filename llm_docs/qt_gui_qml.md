# Qt GUI: QML panels, dialogs and primitives

For `src/qtgui/qml/`. Accessibility rules are in `llm_docs/qt_gui_accessibility.md`.

- **Every panel and every `*Dialog.qml` opens with a wireframe**: a
  `// Wireframe (each number marks its code below):` comment block drawing
  the UI, its parts numbered ① ② …, each number marking exactly once, as a
  trailing `// ①`, the line that opens that part. Give a new panel or dialog
  one from the start, and keep drawing and numbers in step when parts are
  added, moved or removed. A new panel also goes into the list in
  `everyPanelAndDialogHasAWireframeNumberedToItsCode`
  (`test/qtgui/test_qtgui.cpp`); dialogs are found by name. Convention and
  example: [`src/qtgui/README.md`](../src/qtgui/README.md), "Reading the QML".
- **`objectName` on every item a user sees or uses** (the spec's element
  ids, dotted: `entities.shapes.row.2.label`). Tests find items by it, and
  `BURRTOOLS_QML_OUTLINES=1` labels the one under the mouse with it.
- **Every user-visible string in `qsTr()`**, with `%1` placeholders, never
  concatenation of sentence parts.
- **Colours, sizes, radii, durations from `Theme`**, never literals. Text on
  a shape's colour uses the model's `chipTextColor` / `App.viewport.shapeTextColor`
  (`btui::prefersWhiteText`, a WCAG rule), never a local formula.
- **Only characters the app font has** in string literals: `›` not `▸`, no
  emoji or symbol glyphs (draw them, or use `Icon`). Windows otherwise reads
  every installed font for a fallback (hundreds of ms), and
  `uiTextNeedsNoFallbackFont` fails. Comments may use anything.
- **Typed for ahead-of-time compilation:** typed properties, function
  parameters and returns; `as` casts instead of `instanceof`; indexed loops
  over item lists; `pragma ComponentBehavior: Bound` in files whose delegates
  use outer ids. `qtgui_qml_aot` fails when the count of bindings left to
  JavaScript grows: type what you added, or, when it truly cannot be
  helped (a method call through a Qt base type), update the baseline in the
  same change and say why.
- **Dialogs:** their own file, a `Loader { active: false; source: … }` in
  `Main.qml`, opened with `showDialog()`, counted in `modalOpen` (message
  dialogs too). Build them on `BtDialog`: set `title` (its accessible name),
  `defaultButton` (Enter presses it) and, where a field should take the
  keyboard first, `initialFocus`.
- **Start-up:** nothing before the first frame that the first frame does
  not need (`startContent()`, dialogs on demand). Measure with
  `BURRTOOLS_STARTUP_TRACE`; the performance guide
  (`design/2026-10-08-qtgui-performance-guide.md`) has the method.
- **Primitives:** reuse `BtButton`, `IconButton`, `Segmented`, `Dropdown`,
  `BtSwitch`, `BtCheckBox`, `CheckRow`, `BtToolTip`, `BtDialog`, `Card`, …
  before writing a control. A new primitive gets a row in `Gallery.qml` and
  reference images (`just update-snapshots <row>`).
- Both themes and both densities: check light and dark, Standard and
  Minimal. In Minimal, help text hidden from view must stay reachable
  (tooltip, `Accessible.description`) — AC-DEN-03.
- **Sizes come from density.md §1, never from a component section's own
  numbers.** §1 overrides C00–C21 where they disagree (C07's h58 tool
  strip is §1's h48). Anything §1 lists is read from `Theme` (add the
  token's property if it is missing), not written as a literal, and §2's
  Minimal changes (icon only, hidden captions and help text) come with the
  component, not later. `qtgui_density_tokens`
  (`scripts/check_density_tokens.py`) fails on a token no component reads;
  a component built later is listed there with its phase, and leaves the
  list when it reads its tokens. Every card gets a test that checks its §1 sizes
  and §2 changes in both densities (`tst_editor`'s
  `test_theCardFollowsTheDensity` is the pattern).
