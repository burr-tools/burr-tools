# src/qtgui — guide for AI agents and contributors

`burrtools-qt`, the redesigned GUI: Qt 6 Quick (QML) on C++ controllers, the
3D view drawn through Qt's GPU layer (QRhi). It runs beside the FLTK program
(`src/gui`) until it replaces it. Read, in this order, as the task needs:

- [`README.md`](README.md) — architecture, status per phase, **what is deferred
  to the next phase and why**, deviations from the spec, the test suites.
- [`../uicore/AGENTS.md`](../uicore/AGENTS.md) — the toolkit-free logic below
  this folder; most "what happens" decisions belong there, not here.
- [`../../design-spec/BurrTools-SPEC-FULL-FINAL-v6.md`](../../design-spec/BurrTools-SPEC-FULL-FINAL-v6.md)
  — the spec (components C00–C22, primitives PR-xx, acceptance criteria
  AC-xx, phases). Code comments cite its ids; search for them there.
- [`../../design/2026-10-08-qtgui-performance-guide.md`](../../design/2026-10-08-qtgui-performance-guide.md)
  — start-up and rendering rules, what was measured, what was tried and
  dropped.
- The root [`AGENTS.md`](../../AGENTS.md) — build system, gates, house rules
  for the whole repository.

## Map

| Where | What |
| :--- | :--- |
| `main.cpp` | the program: options (`--help`), start-up order and trace, the early Direct3D device, the launch update check |
| `app.{h,cpp}` | `App`, the QML singleton: owns every controller, the status sentence, internal-error rescue |
| `*controller.{h,cpp}`, `shapesmodel`, `keyboardcues`, `popupwindowstyle` | one QObject per concern (table in README); QML reaches them as `App.document`, `App.layout`, … |
| `scenecontroller`, `viewportcontroller`, `stl/imageexportcontroller` | camera, gestures and the `SceneFrame` each view draws; `SceneController::applyStyle` is the one rule for how a voxel style is drawn |
| `editorcontroller`, `voxelgriditem`, `editcursor` | the voxel editor: tool, Mirror and Span, plane and layer, strokes, Area mode and grid size (`App.editor`); the 2D grid (QPainter); the composed edit cursors and the Edit button's glyph |
| `inspectorcontroller` | the inspector (C03–C05): sections' open state, each voxel type's Transform buttons, whether an operation may run and why not, running it through `btui::shapeops`, its 3D preview |
| `voxelviewport`, `viewcubeitem`, `scenerenderer`, `offscreenrenderer`, `pipelinecache`, `earlydevice`, `shaders/` | the 3D view on QRhi: render thread, offscreen renders, pipeline cache, shaders compiled by `qsb` |
| `theme.{h,cpp}`, `resources/design-tokens.json` | every colour, size and duration of the spec, light and dark, Standard and Minimal |
| `qml/` | the QML module `BurrTools.Ui`: `Main.qml` (the window), panels, `*Dialog.qml`, primitives (`Bt*`, `IconButton`, `Segmented`, …), `Gallery.qml` |
| `qmloutlines`, `selfcheck`, `guarded.h`, `localpath.h` | developer overlay, `--self-check`, exception fence for QML entry points, file-dialog URLs to paths |
| `meson.build`, `qml/meson.build` | sources, the moc list (`qtgui_moc_headers`: every `Q_OBJECT` header), QML files, shaders, the AOT import tree |

Tests live in `../../test/qtgui/` (`test_qtgui.cpp`; `test_viewport.cpp`,
`test_editor.cpp`, `test_inspector.cpp`, `test_voxeltypes.cpp` -- every
voxel type edited end to end --, `test_saving.cpp` -- every kind of edit
saved and read back --; `qml/tst_*.qml`; snapshots) and
`../../test/test_ui_*.cpp` for uicore.

## Commands

```sh
just build-qt          # or: ninja -C build   (the program is build/burrtools-qt)
just run-qt [file]     # run it;  just run-gallery: every primitive in every state
just test-qt           # the Qt suites;  just test-all: everything CI runs
just qml-aot           # bindings left to the JavaScript engine vs the baseline
just update-snapshots <row>   # rewrite a gallery row's reference images, after a deliberate visual change
just build-release && just test-release && just check     # before calling a change done
burrtools-qt --help    # options and every environment variable;  --self-check: invariants
BURRTOOLS_QML_OUTLINES=1 burrtools-qt   # outlines named items; hover shows objectName and File.qml:line
BURRTOOLS_STARTUP_TRACE=1 burrtools-qt  # start-up timings (quit: also quit at app ready)
```

On Windows these run in an MSYS2 UCRT64 shell. Close a running
`burrtools-qt.exe` before building: Windows locks it and the link fails. A
stale `libburr_qtgui.a` ("thin archive" errors) is fixed by deleting it.

## Rules for authors

### Where code goes

- **Decisions in uicore, presentation here.** What a command does, which
  faces a voxel shows, how a camera turns, how wide a card is, how a file is
  saved: `src/uicore`, unit-tested without a window. This folder adapts it to
  Qt and draws it. If logic here needs a window only to be tested, move it.
- **The library directly, for data.** Controllers may include `src/lib` and
  `src/halfedge` headers to read puzzles, shapes, problems and meshes and to
  call the library's operations. A rule about those objects still goes
  below: into uicore, or into `src/lib` when it is the object's own (an
  exporter's parameter limits), so the legacy GUI gets it too.
- **The puzzle changes through the document.** A controller changes the
  puzzle with `DocumentController::editStructure(shape, f)`, which makes
  it one undo step and tells the views; `session().puzzle()` is
  read-only (`src/uicore/AGENTS.md`).
- **State in controllers, not QML.** QML binds to controller properties and
  calls `Q_INVOKABLE`s; it does not keep the truth. New concern, new
  controller owned by `App` (constructor in `app.cpp`, a `CONSTANT`
  property in `app.h`, README's controller table).
- **Every menu entry and shortcut comes from the command table**
  (`uicore/commands.cpp`). Never a hard-coded `Shortcut` or menu item in QML.
  A command is triggered with `App.commands.trigger(key)` and handled in
  `CommandController::trigger`.
- **Settings through `SettingsController`** (keys at the top of its .cpp,
  defaults, `resetSection`, a row in `SettingsDialog.qml` if users edit it).
  The new GUI never writes legacy's `.burrtools.rc`; it may seed from it once
  (`seedFromLegacy`).
- **Environment variables** go into `../tools/envvars.h` (the `--help` table);
  `test_envvars.cpp` fails on one the code reads but the table lacks.

### C++ and Qt

- C++20, the repository's style (prose comments, `f(void)`, two-space
  indent). Match the file you are in.
- `Q_PROPERTY`: `FINAL`; `double`, not `qreal`; a `NOTIFY` for anything that
  changes; setters compare before they emit (a needless `changed` re-evaluates
  every binding on it and can redraw the 3D view). Compare the whole state
  when one setter can change several things (`LayoutController::changeLayout`).
- An enum QML sees: `Q_ENUM`, and switches over it without `default:` so the
  compiler flags a new value.
- Library factories (`getMesh`, `getDrawingMesh`, `getSTLMesh`,
  `getStlExporter`, `getVoxel`) return owning raw pointers: wrap them in
  `std::unique_ptr` at once.
- **An invokable that reaches the library runs inside `guarded()`**
  (`guarded.h`): a failed `bt_assert` or `bad_alloc` must not unwind through
  the QML engine.
- **Files:** write beside the target and rename over it (`QSaveFile`, or as
  `SettingsStore::save` / `DocumentSession::writePuzzle` do); check every
  result; never report success after a failed write. Turn a file dialog's
  URL into a path with `localPath()`.
- **Threads.** The 3D view renders on Qt's render thread: it may read GUI
  objects only in `synchronize()` (the GUI thread is blocked then), and only
  the render thread may touch the window's `QRhi`. Meshes cross over as
  `std::shared_ptr<const ShapeMesh>`, built completely before they are
  published and never changed after. Work on a worker thread touches only
  its own shared state, which the GUI thread polls or receives queued
  (`UpdateController` is the pattern); no QObject is used from two threads.
- No network in a scripted run or a test: see how `main.cpp` gates the
  launch update check.

### QML

- **Every panel and every `*Dialog.qml` opens with a wireframe**: a
  `// Wireframe (each number marks its code below):` comment block drawing
  the UI, its parts numbered ① ② …, each number marking exactly once, as a
  trailing `// ①`, the line that opens that part. Keep drawing and numbers in
  step when parts move. A new panel also goes into the list in
  `everyPanelAndDialogHasAWireframeNumberedToItsCode`
  (`test/qtgui/test_qtgui.cpp`); dialogs are found by name. README,
  "Reading the QML", has an example.
- **`objectName` on every item a user sees or uses** (the spec's element
  ids, dotted: `entities.shapes.row.2.label`). Tests find items by it; the
  outline overlay labels them with it.
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
  `BURRTOOLS_STARTUP_TRACE`; the performance guide has the method.
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

### Accessibility and keyboard

The accessibility tree is what screen readers and UI automation use; treat
it as part of the UI. WCAG 2.1 AA is the yardstick.

- **Role, name, state on every control built from primitives** (a Rectangle
  with a TapHandler is invisible to assistive tech otherwise):
  `Accessible.role`, `Accessible.name` (what a sighted user reads, e.g.
  "S2 Cage, weight 2"), state (`checkable`/`checked`, `selectable`/`selected`),
  and `Accessible.onPressAction` doing what a click does. Mark decorative or
  duplicated texts `Accessible.ignored: true`. Toggle-like buttons set
  `toggle: true` (`IconButton`, `ToolbarButton`); workspace tabs are
  `PageTab`s.
- **Names for fields and groups** — a `NumberField`, `Segmented`,
  `ProblemList` takes its visible label as `Accessible.name`; a label beside
  it is not linked automatically.
- **Tooltips are not hover-only**: `shown: hovered || visualFocus`, and the
  same text as `Accessible.description` on the control it explains.
- **Focus:** buttons take focus on click too (`focusPolicy: Qt.StrongFocus`),
  so Tab goes on from what was last used; the ring shows only for keyboard
  focus (`visualFocus`). Read-only things are not Tab stops. When a control
  disappears under the user's hands (collapse, a popup closing), give the
  focus to its counterpart (`Main.qml` `keepToggleFocus`, `DisplayMenu`'s
  `onClosed`). Popups that the keyboard should drive take `focus: true`.
- **Keys:** Esc closes the innermost thing first (dialogs and menus count
  themselves in `openPopups`); Enter presses a dialog's default button;
  arrows move within lists and menus. New single-letter shortcuts need care
  (WCAG 2.1.4): prefer them scoped to the view they act on.
- **Contrast and colour:** text ≥ 4.5:1, borders, focus rings and state
  indicators ≥ 3:1 against what they sit on, in both themes; never colour or
  hue alone for a state (add text, a mark, a weight or a bar).
  `tst_quality.qml` computes contrast — extend its pairs when you add a
  surface.
- **Status changes are announced:** a message to the user goes through
  `App.status.flash(text)`, which the status bar announces
  (`Accessible.announce`) and keeps as its description.
- The accessibility work still open (text scaling, high contrast, small
  screens, the screen-reader pass, …) is in README's deferred table; don't
  re-open fixed items (the keyboard 3D view and reduced motion are done),
  and add new gaps there with a reason.
- **Motion through `Theme`:** durations come from its tokens
  (`durCollapse`, `durSwitch`, `motion()`), which are 0 while the system
  asks for reduced motion; a literal duration or an animation that ignores
  them breaks that.

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

Update `README.md` when a phase item lands, is deferred (the deferred
table: item, spec reference, why, when) or deviates from the spec. Keep the
wireframes, the controller table and `--help` true.

## Reviewing a change (for an agent reviewing a hand-written PR)

Read the diff against the rules above; for each gap, name the rule, point to
the file that already does it right, and suggest the smallest fix. Ask for
a test when behaviour changed without one.

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

## Gotchas found the hard way

- An `id` on a `Popup`'s `contentItem` (any name) broke its layout (rows
  102 px wide, clicks missing) in `DisplayMenu.qml`; reach the rows through
  `root.contentItem.children` instead.
- `x as Popup` is null for `QtQuick.Controls.Basic` popups (a separate QML
  type); cast to `Dialog` or the concrete type.
- A JavaScript line starting with `(` continues the previous statement
  (automatic semicolon insertion); assign to a `const` first.
- Menus open as their own windows (`Popup.Window`): a click in the main
  window does not close them by itself (`KeyboardCues::windowPressed`), and
  the application's activation, not the window's, tells when focus left.
- `forceActiveFocus` in a test window can leave focus nowhere; Tab works.
- The Qt Quick message dialogs are in-window and modal on Windows and Linux:
  count them in `modalOpen`, and dismiss them in tests.
- A C++ enum whose values do not count up from 0 reads as `undefined` in
  ahead-of-time compiled QML; hand QML the numbers as a map
  (`InspectorController::tasks`).
- A `HoverHandler` on a parent sees no hover over the 3D view (a
  `QQuickRhiItem` that takes hover itself): put a strip item with a
  non-blocking `HoverHandler` over the part to watch (`ViewportCard`'s
  reveal zone).
- A `ListView` whose height depends on its `count` resizes inside a model
  reset and lays out rows the reset just took ("DelegateModel::cancel"
  warnings): size it from outside, `cacheBuffer: 0`.
- Positioners (`Row`, `Column`, `Flow`) lay their children out on the next
  polish: a test reads their geometry after `waitForRendering` or
  `tryVerify`, not at once.
- Windows draws the title bar dark or light from
  `QStyleHints::setColorScheme`; ask for the theme's scheme before the
  first window (`Theme`'s constructor), not only when it changes.
