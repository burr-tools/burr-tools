# Qt GUI: map of `src/qtgui/`

| Where | What |
| :--- | :--- |
| `main.cpp` | the program: options (`--help`), start-up order and trace, the early Direct3D device, the launch update check |
| `app.{h,cpp}` | `App`, the QML singleton: owns every controller, the status sentence, internal-error rescue |
| `*controller.{h,cpp}`, `shapesmodel`, `keyboardcues`, `popupwindowstyle` | one QObject per concern (table in README); QML reaches them as `App.document`, `App.layout`, … |
| `scenecontroller`, `viewportcontroller`, `stl/imageexportcontroller` | camera, gestures and the `SceneFrame` each view draws; `SceneController::applyStyle` is the one rule for how a voxel style is drawn |
| `editorcontroller`, `voxelgriditem`, `editcursor` | the voxel editor: tool, Mirror and Span, plane and layer, strokes, Area mode and grid size (`App.editor`); the 2D grid (QPainter); the composed edit cursors and the Edit button's glyph |
| `systempointer`, `systempointer_mac.mm` | the system's own arrow and pointer size for the edit cursors: Windows (registry scheme, `DrawIconEx`), macOS (`NSCursor`), Linux (Xcursor themes through `uicore/xcursor`) |
| `systemmotion`, `systemmotion_mac.mm` | the system's reduce-motion switch before Qt 6.12 reads it (`QAccessibilityHints`): macOS (`NSWorkspace`, watched), Windows (`SPI_GETCLIENTAREAANIMATION`); `Theme::reducedMotion` asks it |
| `windowframe` | Linux and the BSDs: the window manager's title bar dark with a dark theme -- X11's `_GTK_THEME_VARIANT`, KDE's `KDE_COLOR_SCHEME_PATH`; a light theme leaves it to the desktop. GNOME on Wayland cannot be reached (Qt's Adwaita decoration reads the portal alone) |
| `inspectorcontroller` | the inspector (C03–C05): sections' open state, each voxel type's Transform buttons, whether an operation may run and why not, running it through `btui::shapeops`, its 3D preview |
| `voxelviewport`, `viewcubeitem`, `scenerenderer`, `offscreenrenderer`, `pipelinecache`, `earlydevice`, `shaders/` | the 3D view on QRhi: render thread, offscreen renders, pipeline cache, shaders compiled by `qsb` |
| `theme.{h,cpp}`, `resources/design-tokens.json` | every colour, size and duration of the spec, light and dark, Standard and Minimal |
| `qml/` | the QML module `BurrTools.Ui`: `Main.qml` (the window), panels, `*Dialog.qml`, primitives (`Bt*`, `IconButton`, `Segmented`, …), `Gallery.qml` |
| `qmloutlines`, `selfcheck`, `guarded.h`, `localpath.h` | developer overlay, `--self-check`, exception fence for QML entry points, file-dialog URLs to paths |
| `meson.build`, `qml/meson.build` | sources, the moc list (`qtgui_moc_headers`: every `Q_OBJECT` header), QML files, shaders, the AOT import tree |

Tests live in `test/qtgui/` (`test_qtgui.cpp`; `test_viewport.cpp`,
`test_editor.cpp`, `test_inspector.cpp`, `test_voxeltypes.cpp` -- every
voxel type edited end to end --, `test_saving.cpp` -- every kind of edit
saved and read back --; `qml/tst_*.qml`; snapshots) and
`test/test_ui_*.cpp` for uicore.
