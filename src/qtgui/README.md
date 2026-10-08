# burrtools-qt — the redesigned GUI (Qt 6 Quick)

`burrtools-qt` is a second program next to the FLTK `burrtools`. It shows
the same puzzles through a redesigned interface, built with Qt 6 Quick and
drawn through Qt's GPU layer. The legacy GUI is unchanged and still the
released program; this one is a **preview** that grows phase by phase until
it can replace it.

- **What it should become:** [`design-spec/BurrTools-SPEC-FULL-FINAL-v6.md`](../../design-spec/BurrTools-SPEC-FULL-FINAL-v6.md),
  the whole spec in one file: components C00–C22, primitives PR-01…PR-32,
  design tokens, density, phases P0…P12, the test plan. It is the spec
  bundle's documents one after another, each marked `<!-- FILE: path -->`,
  so a reference such as `foundations/density.md` or "design-tokens §7" in
  the code is found by searching for it there. (The bundle's assets and its
  HTML mock are not in the repository; the GUI's own copies of the glyphs,
  icons and tokens are in `resources/`.)
- **Why Qt Quick, and how:** [`design-spec/gui-migration.md`](../../design-spec/gui-migration.md)
  and the spec's `foundations/qtquick-implementation-guide.md` part.
- **How to build, run, test and package it:** [`BUILD.md`](../../BUILD.md#qt-gui-burrtools-qt).

## Status: the first cut

| Phase | Content | State |
| :--- | :--- | :--- |
| P0 Foundations | QML module, theme from the design tokens, QRhi 3D host, Qt Quick Test harness, primitives, component gallery with reference images | Done, except primitives first needed later: Stepper, GlyphButton, ActionButton, Accordion, Chip (P2/P4/P6), edit cursors (P4) |
| P1 Shell and layout | Native window and title, menus, workspace rail, cards with rails and Focus modes, status bar, densities | Done; the cursor readout waits for the 2D grid (P4) |
| — Legacy dialogs | Settings, Keyboard shortcuts, Comment, About, Convert, Import assemblies, Status, STL / vector / image export | Ported |
| — 3D view | The Entities 3D view: camera, gestures, view cube, display options, two voxel styles | Done; editing in 3D arrives with P4 |
| P2 onward | Shapes sidebar, voxel editor, Puzzle and Solver workspaces | Next; the Puzzle and Solver cards are placeholders |

## Architecture

```
  src/lib, src/halfedge     the solver library: puzzles, voxels, meshes (unchanged)
          ^
  src/uicore                toolkit-free UI logic, unit-tested with Catch2:
          ^                 command table, camera, view cube, scene meshes,
          |                 layout model, settings store, undo session,
          |                 window placement, image pages, vector export ...
  src/qtgui                 Qt: controllers (C++) + the QML module BurrTools.Ui,
                            the 3D renderer on QRhi, the program's main()
```

Everything that decides *what* happens — which menu holds which command,
how the camera turns, which faces a voxel shows, how wide a card is — sits in
`src/uicore`, free of Qt and FLTK, so it is tested without a window and
could serve another front end. `src/qtgui` turns it into Qt objects and
pixels. The legacy GUI (`src/gui`) shares the library and one file with it:
`puzzlehistory` (the undo history) gained a depth limit both use.

### Controllers and the QML module

`main()` creates one `App`, the QML singleton every QML file reaches as
`App`. It owns one controller per concern:

| Controller | Job |
| :--- | :--- |
| `SettingsController` | the new GUI's own settings file `.burrtools-qt.rc`, seeded once from legacy's `.burrtools.rc` and never writing it |
| `DocumentController` | open, save, new, the window title, the undo / redo session |
| `CommandController` | the command table (`uicore/commands`) as menus and shortcuts, per platform |
| `LayoutController` | workspaces, card collapse, Focus 2D / 3D, the column widths |
| `ShapesModel`, `StatusController` | the shape list and the status bar |
| `ViewportController` (a `SceneController`) | the 3D view: camera, gestures, display options, the frame to draw |
| `StlExportController`, `ImageExportController`, `ToolsController` | the export dialogs and the puzzle tools |
| `KeyboardCues`, `PopupWindowStyle` | Windows' access-key underlines; Windows 11 menu corners and borders |

`Theme` (a singleton too) reads `resources/design-tokens.json` and gives QML
every colour, size and duration of the spec in the light or dark set.

The QML files in `qml/` are compiled into the program as the module
`BurrTools.Ui` (`qml/meson.build`). `Main.qml` is the window; the primitives
(`IconButton`, `BtButton`, `Segmented`, `Dropdown`, `BtToolTip`, …) re-skin
Qt Quick Controls' Basic style to the design tokens. `Gallery.qml` shows
each primitive in each state (`burrtools-qt --gallery`). Items carry the
spec's element ids as `objectName`s, which is how the tests find them.

### The command table

`uicore/commands.cpp` holds every command once: its key, label, menu,
shortcuts and the legacy callback it replaces. The menus, the shortcuts,
the Help ▸ Keyboard shortcuts window and the Settings search all come from
it, and a test checks it against legacy's menu tables, so no legacy command
gets lost. Platforms differ where their conventions do — macOS gets Close
(⌘W), Status under File (⌘I) and "BurrTools Help" (⌘?), and its system menu
bar; Windows says Exit and Linux Quit.

### Platform integration

- A native window: the system's title bar ("File name - BurrTools" on
  Windows, the document name on macOS), placement remembered and kept on a
  screen.
- macOS uses its system menu bar and native menus. Elsewhere the menu bar
  is drawn in the theme, its popups are windows of their own (rounded and
  bordered by the system on Windows 11), and View ▸ Show menu bar can move
  the menus into a button on the workspace rail.
- Dialogs for files, colours and messages are the system's.

### The 3D view

`VoxelViewport` (a `QQuickRhiItem`) draws a `SceneFrame` — everything one
frame shows, gathered by a `SceneController` — with `SceneRenderer`, which
only knows a `QRhi` and a render target. The same renderer draws the image
export and the render tests through `OffscreenRenderer`.

- **Graphics API.** Qt's QRhi: Direct3D 11 on Windows, Metal on macOS,
  Vulkan or OpenGL elsewhere. The shaders (`shaders/`) are compiled for all
  of them at build time by `qsb` and embedded.
- **Gamma-correct.** The scene is drawn into a linear-light RGBA16F target,
  multisampled and resolved there, so edges and translucent voxels blend
  evenly; a last pass encodes it to sRGB. Faces are still shaded in sRGB, so
  their colours are exactly the mock's and legacy's.
- **Anti-aliasing** (Settings ▸ 3D view ▸ Anti-aliasing): 4× by default, as
  far as the GPU goes; 8× smooths little more for about twice the graphics
  memory (several hundred MB for a large view on a high-DPI screen). The
  targets grow in 64 px steps and are kept while the view fits, so resizing
  the window does not make them anew for every pixel.
- **One mesh per change.** An edit reaches the view through several signals
  (the document's, the shapes list's count and selection); the shape is
  looked up at once, its mesh built once after the burst. The shapes list
  keeps its rows through edits (insert, remove, update in place) and counts
  each shape's voxels once per change.
- **Voxel styles** (Settings ▸ 3D view ▸ Voxel style).
  *Flat*, the default, is the redesign's: plain faces, each voxel face
  outlined in the fragment shader (no extra geometry, no depth fighting),
  variable voxels see-through with dashed outlines and every translucent
  layer blended back to front, so inner variable voxels show through the
  outer ones. *Classic* is legacy's look: bevelled voxels in a light and dark
  checker, variable voxels opaque with a black marker.
- **Meshes** come from `uicore/scenemesh` — the library's own voxel geometry
  for every grid type (cubes, prisms, spheres, both tetrahedral grids).
- **Pipeline cache.** Compiled pipelines are kept between runs next to the
  settings file (`.burrtools-qt.pipelines`; the image export's
  `.burrtools-qt.offscreen.pipelines`), so a second start skips compiling
  them. A missing, broken or foreign cache is ignored without a word.
- **View cube** (`ViewCubeItem`, `uicore/viewcube`): faces, edges and
  corners to snap to, 90° steps, roll, Home, in the main view's projection.

### Settings

The new GUI never writes legacy's settings. On its first start it copies
what legacy has (tooltips, lighting, rotation method, …) into
`.burrtools-qt.rc`; from then on that file is its own. `BURRTOOLS_QT_SETTINGS`
names another file — the tests and scripted runs use a scratch one, and the
pipeline caches follow it, so they never touch the user's.

Changes apply at once and reach the file half a second after the last of a
burst, or when the program quits. The file is written beside itself and
renamed over the old one, so a crash part way leaves the old settings.

## Testing

| Suite | What | How it runs |
| :--- | :--- | :--- |
| Catch2 `[ui]` (`test/test_ui_*.cpp`, 101 cases) | `src/uicore`: command table vs legacy menus, camera, view cube, meshes and picking for every grid type, layout model, settings store, undo session, vector export, … | `just test`, no window |
| Qt Test `test_qtgui` (16 classes, ~140 functions) | the controllers, theme, settings, icons; real renders through an offscreen QRhi (shading, outlines, linear blending, layered translucency, tiling, the 3D view item resized, the pipeline caches) | headless; renders need a graphics backend (below) |
| Qt Quick Test `test_qtgui_qml` (`test/qtgui/qml/tst_*.qml`, ~80 functions) | the shell, menus, settings, dialogs, layout at 960 dp, tooltips, accessibility and contrast, the component gallery | headless (`offscreen`), Qt Quick's software renderer |
| Gallery snapshots | each gallery row and the glyph / icon sheets against reference images, light and dark, at device-pixel ratios 1, 1.5 and 2 | `test/qtgui/snapshots/<os>/`; tolerant diff |
| Performance | work counts per edit, the scene targets' graphics memory, allocations per frame (none per triangle), QML bindings left to the JS engine (`qtgui_qml_aot`, against a baseline); benchmarks and profiles as reports | Qt suites and `just qml-aot`; `just bench-ui`, `startup-time`, `profile-qml`, `heap-qt`; see `design/2026-10-08-qtgui-performance-backlog.md` |

Render tests use Direct3D's WARP rasteriser on Windows and Mesa's lavapipe
(software Vulkan) on Linux CI, so they need no GPU. Where no backend can be
made they skip, unless `BURRTOOLS_REQUIRE_GPU_TESTS` is set, as in CI.
Reference images exist for Windows; on other platforms the snapshot checks
skip and CI uploads the grabs, to be committed as that platform's references
(`just update-snapshots` writes them locally). After an intended visual
change, `just update-snapshots button` rewrites that row's references --
both themes, all three ratios -- and only where they no longer match, so the
commit holds just the images that changed.

## Deviations from the spec, by decision

- **Settings** live in File ▸ Settings (macOS: the application menu's
  Settings / Preferences item, with ⌘,), not
  behind a gear in the top bar; the keyboard-shortcuts page moved to
  Help ▸ Keyboard shortcuts. There is no top bar: the window has the
  system's title bar and a menu bar, which can be hidden (View ▸ Show menu bar).
- **Theme** defaults to Light, as legacy looks (spec: System).
- **Variable voxels** (Flat) are 60 % opaque and the inner ones show
  through (spec: 50 %, outer surface only). The **Classic** voxel style is
  an addition.
- **Rendering** is gamma-correct; the layer slab's tint is scaled so it
  still reads like the mock's sRGB mix.
- **Edit-tool glyphs**: our copies declare the size of their drawing (the
  design files declare 48 × 48 round a 28 × 24 drawing, which Qt stretches).

## What changed in this first cut

- New: `src/uicore` (toolkit-free UI logic), `src/qtgui` (controllers, QML
  module, renderer, resources), `test/qtgui` and `test/test_ui_*.cpp`.
- Build: the `qt_gui` meson option (auto; needs Qt ≥ 6.8), `just build-qt`,
  `run-qt`, `run-gallery`, `test-qt`, `update-snapshots`, `deploy-qt`.
- CI: Qt jobs on Linux, Windows and macOS that build, test (renders
  included) and upload a self-contained preview build of `burrtools-qt`
  for each platform (`scripts/package-qt.sh`: stripped, with only the Qt
  parts it loads); the release job takes only the legacy builds.
- A standalone Windows program: `-Dqt_static=true` links a static Qt
  (MSYS2's qt6-static) into one ~50 MB `burrtools-qt.exe`
  (`scripts/build-qt-static.sh`, `scripts/qt_static_link.py`), built on
  demand and for release tags by the *Qt standalone build* workflow.
- Shared: `src/gui/puzzlehistory` gained an undo-depth limit (legacy keeps
  its behaviour), with tests.

## Next

P2 — the Shapes sidebar (C01, C02), with a reusable Chip (PR-08); then the
voxel editor (P4: the 2D grid, edit cursors, the Stepper, editing in 3D),
the Puzzle and Solver workspaces, and the remaining primitives as their
components arrive. The phase plan is in the spec's "Implementation plan"
section.
