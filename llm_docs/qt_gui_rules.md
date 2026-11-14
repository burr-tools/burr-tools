# Qt GUI: where code goes, C++ and Qt

For `src/qtgui/`. QML rules are in `llm_docs/qt_gui_qml.md`.

### Where code goes

- **Decisions in uicore, presentation here.** What a command does, which
  faces a voxel shows, how a camera turns, how wide a card is, how a file is
  saved: `src/uicore`, unit-tested without a window. `src/qtgui` adapts it to
  Qt and draws it. If logic there needs a window only to be tested, move it.
- **The library directly, for data.** Controllers may include `src/lib` and
  `src/halfedge` headers to read puzzles, shapes, problems and meshes and to
  call the library's operations. A rule about those objects still goes
  below: into uicore, or into `src/lib` when it is the object's own (an
  exporter's parameter limits), so the legacy GUI gets it too.
- **The puzzle changes through the document.** A controller changes the
  puzzle with `DocumentController::editStructure(shape, f)`, which makes
  it one undo step and tells the views; `session().puzzle()` is
  read-only (`llm_docs/uicore_rules.md`). A change to shapes' voxels goes
  through `editVoxels(kind, shape, selected, shapes, f)`, so the problems
  using them lose their stale solutions (undo brings them back).
- **State in controllers, not QML.** QML binds to controller properties and
  calls `Q_INVOKABLE`s; it does not keep the truth. New concern, new
  controller owned by `App` (constructor in `app.cpp`, a `CONSTANT`
  property in `app.h`, the controller table in `src/qtgui/README.md`).
- **Every menu entry and shortcut comes from the command table**
  (`uicore/commands.cpp`). Never a hard-coded `Shortcut` or menu item in QML.
  A command is triggered with `App.commands.trigger(key)` and handled in
  `CommandController::trigger`.
- **Settings through `SettingsController`** (keys at the top of its .cpp,
  defaults, `resetSection`, a row in `SettingsDialog.qml` if users edit it).
  The new GUI never writes legacy's `.burrtools.rc`; it may seed from it once
  (`seedFromLegacy`).
- **Environment variables:** a new `BURRTOOLS_*` variable goes into
  [`src/tools/envvars.h`](../src/tools/envvars.h), which the programs'
  `--help` prints; `test_envvars.cpp` fails on one the code reads but the
  table does not list.

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
