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
| P0 Foundations | QML module, theme from the design tokens, QRhi 3D host, Qt Quick Test harness, primitives, component gallery with reference images | Done, except what later phases need first: the Stepper, GlyphButton, ActionButton, Accordion and Chip primitives, and the composed edit cursors (see *Deferred*) |
| P1 Shell and layout (C00, C10, C11) | Native window and title, menus, cards with rails and Focus modes, status bar | Done; the cursor readout waits for the 2D grid (P4) |
| P1b New puzzle voxel type (C14) | File ▸ New's five types, the read-only type chip | Done |
| P7 Settings (C12), P9 Workspace rail (C15), P12c Help window (C22) | The Settings dialog with search, the rail, Help ▸ Keyboard shortcuts | Done |
| P12a/b Density and keyboard | Standard and Minimal geometry, global keys, Alt to reach the menus (also with the menu bar hidden) | Done, except Minimal's auto-hiding 3D toolbar (see *Deferred*) |
| — Legacy dialogs | Comment, About, Convert, Import assemblies, Status, STL / vector / image export | Ported |
| — 3D view (P5 without editing) | Camera, gestures with eased wheel zoom, view cube with animated snaps, Home / F, Display menu, three voxel styles | Done; editing in 3D (Edit button, quick tools, ghosts, cursors) arrives with P4 |
| P2 onward | Shapes sidebar (its collapsed rail's chips are in already), voxel editor, Puzzle and Solver workspaces | Next; the Puzzle and Solver cards are placeholders |

### Deferred to the next phase

Work that belongs to the phases above but waits, each for the reason given.
Pick these up with the phase named.

| Item | Spec | Why it waits | Pick up with |
| :--- | :--- | :--- | :--- |
| Composed edit cursors (OS pointer + tool badge) | P0 exit criteria, C06 | Only the voxel editor and 3D editing use them | P4 |
| Stepper, GlyphButton, ActionButton, Accordion, Chip primitives | PR-08 and others | First used by the Shapes sidebar, the editor and the inspector | P2, P4, P6 |
| Status bar cursor readout | AC-C10-02 | Shows the hovered cell of the 2D grid, which does not exist yet; `StatusController::setCursorText` is ready, nothing calls it | P4 |
| Minimal density's auto-hiding 3D toolbar | AC-DEN-05, density §3 | The toolbar only goes compact in Minimal; showing it on pointer-at-top / Tab / O P E F and hiding it again is not built | P12a, before cut-over |
| `E` toggles 3D edit mode | AC-C06-07 | Edit mode itself arrives with 3D editing; the Edit button is shown disabled, its tooltip names E; the key is not bound and not in the shortcuts window | P4 |
| Layer slab and Dim other layers | AC-C06-05, AC-C11-08 | Drawn only while the voxel editor is on screen, so not visible yet; every voxel style (STL included) carries the voxel per vertex the dimming needs -- check them on screen then | P4 |
| Quick tool 2 (Alt+click) | AC-C06-06 | Safe with the Alt menu keys (a tap opens menus; Alt held through a click does not). Open question: KDE moves windows on Alt+drag by default, so on that desktop the window manager may take the click | P4 design |
| Picking in use | C06 3D editing | `buildPickMesh` / `pick` and `SceneController::clicked` exist and are tested; nothing in the app uses them yet | P4 |
| STL export writes in place | legacy `stlExporter_c::write` (`src/lib`) | It opens the target before building the mesh and ignores write errors: a failing export can leave a truncated file. The library's writer, shared with legacy; the document save no longer has this flaw | before cut-over |
| Image export memory | `ImageExportController::drawPicture` | Large pages with supersampling need gigabytes and fail late with "could not be drawn"; meshes are built twice per picture (performance guide, P3) | P8 polish |
| STL dialog after the document changes under it | `StlExportController` | Keeps the exporter of the grid it opened with; only reachable when a file opens while the modal dialog is up (macOS Finder) | before cut-over |
| Settings and modes as strings | `SettingsController`, `ImageExportController` | Voxel style, rotation, density, colour view, image mode: parsed from strings in several places each; one typed table per vocabulary would keep them in step | P3 cleanup |
| Hardening from the final review | `earlydevice.cpp`, `scenerenderer.cpp`, `main.cpp` | A device loss's reset of the early D3D device is queued to the GUI thread (could race the render thread re-creating its QRhi); the composite pipeline is matched to the render pass by pointer (an address reused after a resize would pass); the early device and pipeline-cache settings are handed to the window after it is shown (safe while its first expose arrives later, as on Windows). None reproduced; each wants a device-loss or resize test under the debug layer | P8 polish |
| macOS and Linux start-up | performance guide | Measured on Windows only; the plan is in the guide | before cut-over |

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
`src/uicore/puzzlehistory`, the undo history, which gained a depth limit
both use.

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

qmlcachegen compiles the QML to C++ ahead of time where it knows the types.
It sees the module's own C++ types through an import tree the build lays
out (`scripts/qml_import_tree.py`), so: give new C++ properties `FINAL` and
declare reals `double` (not `qreal`), type QML properties and function
parameters, and use `pragma ComponentBehavior: Bound` where delegates refer
to the file's ids. `just qml-aot` counts what is left to the JavaScript
engine; `qtgui_qml_aot` fails when the count grows.

### Reading the QML

Two aids tie what is on screen to the code that draws it.

**Wireframes.** Every panel (`Main`, `WorkspaceRail`, `CollapsedRail`, the
cards, `StatusBar`, `AppMenuBar`, `DisplayMenu`) and every dialog
(`*Dialog.qml` but the shared frame `BtDialog`) opens with a drawing of
itself, under its description comment:

```qml
// C10 status bar: status text (or a flash message) and the cursor readout.
//
// Wireframe (each number marks its code below):
//
//   ┌──────────────────────────────────────────────────┐
//   │ ① Shape S1 has 512 voxels (296 fixed, ...) ② x y z │
//   └──────────────────────────────────────────────────┘
Rectangle {
    ...
    Text { // ①
        objectName: "status.text"
```

Each circled number in the drawing (① to ⑳) marks, once, the line that opens
the code for that part, as a trailing `// ①`. Search the file for the number
to jump from the drawing to the code. The drawing shows the layout and the
parts, not exact sizes; plain characters (`[x]`, `(•)`, `[ ]`, `▸`) suit it
better than emoji.

A **new panel or dialog file** gets such a header from the start: add the
file to the list in `everyPanelAndDialogHasAWireframeNumberedToItsCode`
(`test/qtgui/test_qtgui.cpp`) if it is a panel; a `*Dialog.qml` is found by
its name. When a change adds, moves or removes a part, update the drawing
and its numbers with it. The test fails when a listed file has no header,
or when a number in the drawing does not mark exactly one place in the code
(or a marker in the code is not in the drawing). Primitives (`BtButton`,
`Segmented`, ...) need none; the gallery (`--gallery`) shows them.

**Outlines at run time.** `BURRTOOLS_QML_OUTLINES=1 burrtools-qt` outlines
every item the QML gave an `objectName`. The item under the mouse pointer is
highlighted and labelled with that name and the file and line that declare
it (`ShapesCard.qml:103`), and its chain of named parents is printed on
stderr as the pointer moves onto it:

```
> entities.shapes.row.2.label  ShapesCard.qml:103
  entities.shapes.row.2  ShapesCard.qml:63
  entities.shapes.list  ShapesCard.qml:47
  entities.shapes  Main.qml:284
  workspace.left  Main.qml:277
```

The overlay takes no input, so the window works as usual underneath
(`qmloutlines.cpp`). `burrtools-qt --help` lists this and every other
environment variable.

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
  of them at build time by `qsb` and embedded -- for Direct3D as DXBC when
  the build finds the Windows SDK's `fxc` (`-Ddxbc`), else as HLSL source.
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
  checker, variable voxels opaque with a black marker. *STL* is the shape as
  Export ▸ STL makes it -- the library's chamfer meshers
  (`voxel_c::getSTLMesh`), no outlines, the shape's colour -- and leaves the
  variable voxels out, as an exported piece does. One rule decides how each
  style is drawn (back faces, translucent layers) for the view, the export
  previews and the exported pictures alike (`SceneController::applyStyle`).
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
| Catch2 `[ui]` (`test/test_ui_*.cpp`, 113 cases) | `src/uicore`: command table vs legacy menus, camera, view cube, meshes and picking for every grid type, layout model, settings store, undo session, vector export, … | `just test`, no window |
| Qt Test `test_qtgui` (17 classes, ~165 functions) | the controllers, theme, settings, icons; real renders through an offscreen QRhi (shading, outlines, linear blending, layered translucency, tiling, the 3D view item resized, the pipeline caches) | headless; renders need a graphics backend (below) |
| Qt Quick Test `test_qtgui_qml` (`test/qtgui/qml/tst_*.qml`, ~90 functions) | the shell, menus, settings, dialogs, layout at 960 dp, tooltips, accessibility and contrast, the component gallery | headless (`offscreen`), Qt Quick's software renderer |
| Gallery snapshots | each gallery row and the glyph / icon sheets against reference images, light and dark, at device-pixel ratios 1, 1.5 and 2 | `test/qtgui/snapshots/<os>/`; tolerant diff |
| Performance | work counts per edit, the scene targets' graphics memory, allocations per frame (none per triangle), QML bindings left to the JS engine (`qtgui_qml_aot`, against a baseline); benchmarks and profiles as reports | Qt suites and `just qml-aot`; `just bench-ui`, `startup-time`, `profile-qml`, `heap-qt`; see `design/2026-10-08-qtgui-performance-guide.md` |

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
  through (spec: 50 %, outer surface only). The **Classic** and **STL**
  voxel styles are additions.
- **Workspace rail**: the selected workspace's fill runs to the rail's right
  edge, square there, with the indicator bar on that edge -- a tab joined to
  the workspace (spec AC-C15-02: the bar at the left). Minimal density's rail
  buttons are 40 dp high.
- **Menus by keyboard**: an Alt tap underlines the access keys and arms the
  menus (Windows' convention); with the menu bar hidden it opens the rail's
  menu button instead. Alt held through a click or another key arms
  nothing, so the planned Alt+click quick tool keeps working.
- **Collapsed Shapes card**: its rail lists the shapes as clickable chips
  (colour and id) -- C01's rail chips, ahead of P2.
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
  demand by the *Qt standalone build* workflow, which can attach it to a
  given release.
- Shared: the undo history moved from `src/gui` to
  `src/uicore/puzzlehistory` and gained an undo-depth limit, with tests.
  Lowering the limit now drops the oldest steps at once, in legacy too.
- `burrtools-qt` builds into the build folder itself (`build/burrtools-qt`),
  beside `burrtools` and `burrTxt`.
- Every environment variable is in one table, `src/tools/envvars.h`;
  `burrtools-qt --help`, `burrTxt --help` and `burrTxt2 --help` print the
  ones each reads, and `test_envvars.cpp` fails on one the code reads but
  the table does not list.
- Reading the QML: wireframe headers in every panel and dialog, checked by
  a test, and the `BURRTOOLS_QML_OUTLINES` overlay (above).
- Start-up: the window first, its content after the first frame, dialogs
  loaded when first opened, the Direct3D device made early on a thread;
  first paint ~0.3 s and app ready ~0.5 s warm on Windows (the performance
  guide has the measurements and what was tried).
- Saving a puzzle writes beside the file and renames it over, so a save
  that fails part way leaves the old file intact (it used to truncate it).

## Next

P2 — the Shapes sidebar (C01, C02), with a reusable Chip (PR-08); then the
voxel editor (P4: the 2D grid, edit cursors, the Stepper, editing in 3D),
the Puzzle and Solver workspaces, and the remaining primitives as their
components arrive. The phase plan is in the spec's "Implementation plan"
section.
