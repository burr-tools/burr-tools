# src/qtgui — guide for AI agents and contributors

`burrtools-qt`, the redesigned GUI: Qt 6 Quick (QML) on C++ controllers, the
3D view drawn through Qt's GPU layer (QRhi). It runs beside the FLTK program
(`src/gui`) until it replaces it. Read, as the task needs:

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

## Commands

```sh
just build-qt          # or: ninja -C build   (the program is build/burrtools-qt)
just run-qt [file]     # run it;  just run-gallery: every primitive in every state
just test-qt           # the Qt suites;  just test-all: everything CI runs
just qml-aot           # bindings left to the JavaScript engine vs the baseline
just update-snapshots <row>   # rewrite a gallery row's reference images, after a deliberate visual change
burrtools-qt --help    # options and every environment variable;  --self-check: invariants
BURRTOOLS_QML_OUTLINES=1 burrtools-qt   # outlines named items; hover shows objectName and File.qml:line
BURRTOOLS_STARTUP_TRACE=1 burrtools-qt  # start-up timings (quit: also quit at app ready)
```

On Windows these run in an MSYS2 UCRT64 shell. Close a running
`burrtools-qt.exe` before building: Windows locks it and the link fails. A
stale `libburr_qtgui.a` ("thin archive" errors) is fixed by deleting it.

## Rules

1. **Decisions in uicore, presentation here.** Logic that needs no window
   goes to `src/uicore` with a Catch2 test; this folder adapts it to Qt and
   draws it.
2. **The puzzle changes through the document** (`DocumentController::editStructure`,
   `editVoxels`), never through `session().puzzle()`, which is read-only.
3. **Every menu entry and shortcut comes from the command table**
   (`uicore/commands.cpp`); never a hard-coded `Shortcut` or menu item in QML.
4. **An invokable that reaches the library runs inside `guarded()`**
   (`guarded.h`).
5. **A behaviour change comes with a test.** Verify with `just build-qt` and
   `just test-qt` (run the Qt suites serially); leave the rest to CI
   (`llm_docs/verification.md`).

## Consult when the task touches...

- Where a file or concern lives in this folder, where its tests are -> `llm_docs/qt_gui_map.md`
- Controllers, settings, environment variables, `Q_PROPERTY`s, files, the render thread or worker threads -> `llm_docs/qt_gui_rules.md`
- Any QML file: wireframes, `objectName`, strings, `Theme`, ahead-of-time typing, dialogs, start-up, primitives, density -> `llm_docs/qt_gui_qml.md`
- Accessibility, keyboard, focus, tooltips, contrast, announcements, motion -> `llm_docs/qt_gui_accessibility.md`
- Qt Test / Qt Quick Test cases, save cases, gallery reference images, README upkeep -> `llm_docs/qt_gui_tests.md`
- Reviewing a hand-written PR -> `llm_docs/qt_gui_review.md`
- Qt or QML behaving oddly (popup layout, casts, focus in tests, hover over the 3D view, positioners, title bar) -> `llm_docs/qt_gui_gotchas.md`
