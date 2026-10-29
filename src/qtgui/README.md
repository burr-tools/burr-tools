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
| P0 Foundations | QML module, theme from the design tokens, QRhi 3D host, Qt Quick Test harness, primitives, component gallery with reference images | Done. The Chip came with P2; the GlyphButton, the Stepper and the composed edit cursors with P4; the ActionButton and AccordionSection with P6 |
| P1 Shell and layout (C00, C10, C11) | Native window and title, menus, cards with rails and Focus modes, status bar | Done; the cursor readout since P4 |
| P1b New puzzle voxel type (C14) | File ▸ New's five types, the read-only type chip | Done |
| P7 Settings (C12), P9 Workspace rail (C15), P12c Help window (C22) | The Settings dialog with search, the rail, Help ▸ Keyboard shortcuts | Done |
| P12a/b Density and keyboard | Standard and Minimal geometry, global keys, Alt to reach the menus (also with the menu bar hidden), Minimal's auto-hiding 3D toolbar | Done; the auto-hiding toolbar since 2026-10-28. An audit on 2026-10-27 found the Voxel editor ignoring the density and five smaller gaps (rail captions, "+", button and status bar sizes, the toolbar's height); fixed, and `qtgui_density_tokens` now fails on a density token no component reads |
| — Legacy dialogs | Comment, About, Convert, Import assemblies, Status, STL / vector / image export | Ported |
| — 3D view | Camera, gestures with eased wheel zoom, view cube with animated snaps, Home / F, Display menu, three voxel styles, the layer slab and Dim other layers | Done; editing in it since P5 |
| P2 Shapes sidebar (C01) | New, duplicate, delete (it asks only when solutions would be lost), inline rename, weight, reorder by drag, menu or `Alt+↑/↓`, the row context menu, the list's keys, the rail's `+` and keyboard, the Chip (PR-08) | Done (second cut) |
| P3 Drawing colour (C09) | The voxel editor's footer: current colour and name, palette (one Tab stop, ← →), the colour menu (Edit, Remove), Add at once with the next preset colour (Edit changes it), the collapsed rail's swatch | Done; the 2D grid paints with it since P4, 3D editing with P5 |
| P4 Voxel editor (C07, C08, C02) | The tools 1–4 (keys, a one-stop strip), the quick tools (Shift, Alt, Ctrl; right-click erases), Mirror and Span, the planes (XZ and YZ for all but prisms), the layer strip with its dots, the 2D grid of every voxel type with rulers, hover outline, readout and composed cursors, strokes as one undo step, Grid size with Apply to all and the blocked shrink, the GlyphButton and Stepper | Done (third cut) |
| P5 3D editing (rest of C06) | Edit mode (the Edit button, `E`) with its on and override states, the quick tools in 3D, picking: a voxel's face, the active layer's slab as a 2D grid, the origin of an empty shape; ghosts and the edit cursors under the 3D pointer, the rejected face outlined; the status line naming the target; the two-line hint; the canvas by keyboard (Tab, ring, orbit, pan, zoom, the cube's turns, rolls and faces); Settings ▸ Single-key shortcuts | Done |
| P6 Inspector (C03, C04, C05) | Under the Shapes list: the selected shape's head, then three sections that remember whether they are open -- Transform (each voxel type's buttons: the brick matrix, the prism's and the spheres' nudge pads), Fit & scale (Prune, Center, To origin, Minimize, ×2, ×3), Repair & surface (Fill holes, the inner / outer table); each press one undo step, a blocked one saying why; a Transform button under the pointer or focus previews its result in 3D; the ActionButton and AccordionSection | Done |
| — Entities review (2026-10-29) | The Shapes tab against the spec: every AC of C00–C14 and C22, the element ids both ways, the docs; saving after every kind of edit tested (`TestSaving`) | Done; findings in *Spec review of the Entities workspace* below |
| P10 onward | Puzzle and Solver workspaces | Next (see *Next phases*); their cards are placeholders |

### Deferred to the next phase

Work that belongs to the phases above but waits, each for the reason given,
and the phase that takes it on (the plan's *Deferred work by phase*,
below, lists them per phase).

| Item | Spec | Why it waits | Phase |
| :--- | :--- | :--- | :--- |
| Quick tool 2 (Alt+click) on KDE | AC-C06-06 | In the 2D grid and the 3D view since P5, and safe with the Alt menu keys (a tap opens menus; Alt held through a click does not). KDE moves windows on Alt+drag by default, so there the window manager may take the click; untested on that desktop | P8 |
| STL export writes in place | legacy `stlExporter_c::write` (`src/lib`) | It opens the target before building the mesh and ignores write errors: a failing export can leave a truncated file. The library's writer, shared with legacy; the document save no longer has this flaw | P8 |
| Image export memory | `ImageExportController::drawPicture` | Large pages with supersampling need gigabytes and fail late with "could not be drawn"; meshes are built twice per picture (performance guide, P3) | P8 |
| Hardening from the final review | `earlydevice.cpp`, `scenerenderer.cpp`, `main.cpp` | A device loss's reset of the early D3D device is queued to the GUI thread (could race the render thread re-creating its QRhi); the composite pipeline is matched to the render pass by pointer (an address reused after a resize would pass); the early device and pipeline-cache settings are handed to the window after it is shown (safe while its first expose arrives later, as on Windows). None reproduced; each wants a device-loss or resize test under the debug layer | P8 |
| Linux start-up | performance guide | Measured on Windows and macOS only (macOS 15, Apple Silicon, 2026-10-29: `just startup-time` ~0.4 s cold and warm beyond its fixed 1.5 s wait); the plan is in the guide | P8 |
| **Accessibility** (review of 2026-10-13) | WCAG 2.1 AA, spec P8 | Fixed then: shape list and rail chips as named, selectable list items; status bar role, plain-text name, announcements and kept last message; Display menu focus, arrow keys, Esc and radio rows; Settings named; one contrast-based chip-text rule (the 3D tag too); Status problem cells say "no"; tooltips on keyboard focus and as descriptions; dialogs' initial focus and Enter; toggle and tab states; export / import fields named; Alt kept from the menus while a dialog is up. The rest: | |
| — Text size and high contrast | WCAG 1.4.4, 1.4.11 | Fonts are constant dp sizes: Windows' "Make text bigger" changes nothing, and chips/buttons have fixed heights that would clip. High-contrast themes are not detected; the tokens override them. Wants a text-scale factor on the font tokens and a high-contrast token set | P8 |
| — Small screens | WCAG 1.4.10 | The 960 × 640 dp minimum is taller than 1920 × 1080 at 200 % or 1366 × 768 at 150 %; placement lets the minimum win, so the bottom falls off-screen. Clamp to the screen; scroll or collapse to rails below the design minimum | P8 |
| — Dark-theme button text | WCAG 1.4.3 | White on the dark danger (2.71:1) and accent (2.77:1, a known XFAIL) buttons: a design-token decision (dark text, or darker tokens) | P8 |
| — Remaining contrast | WCAG 1.4.3, 1.4.11 | Light theme: muted text on the canvas, bg and accentSoft (4.05–4.32:1), danger on the canvas 3.93; control borders and unchecked switch/checkbox 1.3–1.6:1 (want a ≥ 3:1 border token); toggled toolbar icons differ by hue only (the list now rings its selection when focused) | P8 |
| — Progress and errors announced | WCAG 4.1.3 | The Status dialog's progress is drawn rectangles (no ProgressBar), image export's progress text and the STL preview's error are not announced | P8 (the solver's progress: P11) |
| — Lists by keyboard | WCAG 2.1.1 | The Shortcuts window and off-screen Settings rows cannot be scrolled to by keyboard (the collapsed rail's chips are a Tab stop since P2) | P8 |
| — Shape row actions by keyboard | WCAG 2.1.1 | The row's buttons are no Tab stops (C01: the list is one); their actions are the list's keys and its context menu (Menu key, Shift+F10). Check with a screen reader that this is found | P8 |
| Double-click to rename, tested | AC-C01-03 | QtTest's double-click reaches a TapHandler as two single taps, so `tst_shapes` opens the field where the handler does instead of through the gesture; the colour swatches use a MouseArea, whose double-click QtTest can send -- the row could too, beside its DragHandler | P8 |
| QML left to the JavaScript engine | `qtgui_qml_aot` | P2–P6's QML was measured only with Qt 6.12 locally, where the report does not compare; on CI's 6.11 it left 731 bindings to the engine against the first cut's 441 (the baseline, raised 2026-10-29 to what is). The rest is typing work: `QVariantMap` / `var` data read by property (the inspector's buttons, the tools' and planes' models -- typed C++ value types or `required` properties), colour bindings 6.11 cannot populate, fonts read as `double` into `int` pixel sizes, untyped functions. Biggest: Inspector 107, VoxelEditorCard 83, Main 71, ShapesCard 33 | P8 |
| Command enabled-state bumps | performance guide | Selection and count changes now bump `CommandController::revision` too (~80 bindings re-read, < 0.1 ms); coalesce to one per event loop if it shows | P8 |
| Edits while the solver runs | legacy `threadStopped()` | Shape and colour edits change data an assembler reads; nothing in the new GUI solves yet. `DocumentController::edit` must refuse them while a solve runs | P11 |
| Screen reader pass for P2 / P3 | WCAG 4.1.2 | The lists' arrow-key selection (List / ListItem, RadioButton swatches), the menus and the confirmation's text read out -- check with a real screen reader | P8 |
| — Small items | | Duplicate names (both "Collapse sidebar"), fragmented texts in the tree, 9.5–10.5 px captions and badges, a drag cut short by Alt+Tab leaving the 3D gesture running (no `mouseUngrabEvent`), view-cube labels below contrast | P8 |
| — To verify on a real system | | The accessibility tree itself (UI Automation: Accessibility Insights or a `QAccessible` test — the QML tests see the attached properties only), custom controls' checked state while assistive tech is active, focus return after dialogs, tooltips dismissable / hoverable, dialog body text read out, Popup.Window menus in UIA, targets under 24 × 24 dp | P8 |

## Architecture

```
  src/lib, src/halfedge     the solver library: puzzles, voxels, meshes (unchanged)
          ^
  src/uicore                toolkit-free UI logic, unit-tested with Catch2:
          ^                 command table, camera, view cube, scene meshes,
          |                 layout model, settings store, undo session,
          |                 window placement, image pages, vector export ...
  src/qtgui                 Qt: controllers (C++) + the QML module BurrTools.Ui,
                            the 3D renderer on QRhi, the program's main();
                            reads the library's classes directly as well
```

Everything that decides *what* happens — which menu holds which command,
how the camera turns, which faces a voxel shows, how wide a card is — sits in
`src/uicore`, free of Qt and FLTK, so it is tested without a window and
could serve another front end. `src/qtgui` turns it into Qt objects and
pixels. The legacy GUI (`src/gui`) shares the library and two files with
it: `src/uicore/puzzlehistory`, the undo history, which gained a depth limit
both use, and `src/uicore/palette`, the piece colours, which legacy's
`piececolor` wraps.

So the interface `src/qtgui` builds on has two parts: uicore, and the
library's own classes (`puzzle_c`, `voxel_c`, `problem_c`, `disassembly_c`,
the STL exporters, `Polyhedron`), which the controllers include to read and
show the puzzle and hand to uicore or to the library's operations. Only the
build enforces uicore's side (`libburr_uicore` links no toolkit); the rule
for the other is the same: a decision that needs no window — a limit, a
check, what an import records — goes into uicore, or into `src/lib` when it
is a property of a library object (an exporter's parameter range), where the
legacy GUI can use it too. uicore's API is in-tree only and will change a
lot through the later phases (the Puzzle and Solver workspaces, the voxel
editor); it is not a stable interface yet.

### Controllers and the QML module

`main()` creates one `App`, the QML singleton every QML file reaches as
`App`. It owns one controller per concern:

| Controller | Job |
| :--- | :--- |
| `SettingsController` | the new GUI's own settings file `.burrtools-qt.rc`, seeded once from legacy's `.burrtools.rc` and never writing it |
| `DocumentController` | open, save, new, the window title, the undo / redo session |
| `CommandController` | the command table (`uicore/commands`) as menus and shortcuts, per platform |
| `LayoutController` | workspaces, card collapse, Focus 2D / 3D, the column widths |
| `ColoursModel` | the drawing colour and the puzzle's colours: add and edit through the colour chooser, remove (asks first when solutions would go), each one `AK_COLOR_PALETTE` undo step through `btui::drawcolours` |
| `ShapesModel`, `StatusController` | the shape list and its actions (new, duplicate, delete, rename, weight, reorder, each one undo step through `btui::shapelist`), and the status bar |
| `EditorController` | the voxel editor: tool and quick tool, Mirror and Span, plane and layer, the 2D grid's strokes (one undo step each, `DocumentSession::beginStroke`), grid size; the cursor readout |
| `ViewportController` (a `SceneController`) | the 3D view: camera, gestures, display options, the frame to draw (the slab from the editor's plane and layer, the ghosts, a Transform button's preview) |
| `InspectorController` | the inspector: its sections' open state, the voxel type's Transform buttons, whether each operation may run and why not, running it (one undo step) through `btui::shapeops`, the 3D preview of a Transform button |
| `StlExportController`, `ImageExportController`, `ToolsController` | the export dialogs and the puzzle tools |
| `KeyboardCues`, `PopupWindowStyle` | Windows' access-key underlines; Windows 11 menu corners and borders |
| `UpdateController` | Help ▸ Check for updates and the launch check (GitHub Releases, as legacy) |

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
engine; `qtgui_qml_aot` fails when the count grows. A C++ enum whose
values do not count up from 0 reads as undefined in compiled QML (the
generated type information lists its names only): hand QML the numbers
another way (`InspectorController::tasks`).

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
- **Pipeline cache.** Compiled pipelines are kept between runs in the
  user's cache folder (`QStandardPaths::CacheLocation`: `window.pipelines`;
  the image export's `offscreen.pipelines`), or beside a scratch settings
  file (`BURRTOOLS_QT_SETTINGS`), so a second start skips compiling
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
| Catch2 `[ui]` (`test/test_ui_*.cpp`, ~200 cases) | `src/uicore`: command table vs legacy menus, camera, view cube, meshes and picking for every grid type, layout model, settings store, undo session, vector export, … | `just test`, no window |
| Qt Test `test_qtgui` (23 classes, ~255 functions) | the controllers, theme, settings, icons; every voxel type edited end to end and saved (`TestVoxelTypes`, `TestSaving`); real renders through an offscreen QRhi (shading, outlines, linear blending, layered translucency, tiling, the 3D view item resized, the pipeline caches) | headless; renders need a graphics backend (below) |
| Qt Quick Test `test_qtgui_qml` (`test/qtgui/qml/tst_*.qml`, ~160 functions) | the shell, menus, settings, dialogs, the Shapes list, the Voxel editor, the inspector, the 3D view's toolbar and hint, layout at 960 dp, tooltips, accessibility and contrast, the component gallery | headless (`offscreen`), Qt Quick's software renderer |
| Gallery snapshots | each gallery row and the glyph / icon sheets against reference images, light and dark, at device-pixel ratios 1, 1.5 and 2 | `test/qtgui/snapshots/<os>/`; tolerant diff |
| Density tokens (`qtgui_density_tokens`) | every density token of `design-tokens.json` read by a component (through `Theme`, or the layout's metrics, which must match), or listed with the phase that builds it; Standard and Minimal with the same tokens | `scripts/check_density_tokens.py`, no build needed |
| Performance | work counts per edit, the scene targets' graphics memory, allocations per frame (none per triangle), QML bindings left to the JS engine (`qtgui_qml_aot`, against a baseline); benchmarks and profiles as reports | Qt suites and `just qml-aot`; `just bench-ui`, `startup-time`, `profile-qml`, `heap-qt`; see `design/2026-10-08-qtgui-performance-guide.md` |

Render tests use Direct3D's WARP rasteriser on Windows and Mesa's lavapipe
(software Vulkan) on Linux CI, so they need no GPU. Where no backend can be
made they skip, unless `BURRTOOLS_REQUIRE_GPU_TESTS` is set, as in CI.
Reference images exist for Windows and macOS (CI fails on a missing one
there); on Linux the snapshot checks skip and CI uploads the grabs, to be
committed as its references (`just update-snapshots` writes them locally).
The macOS ones match a local Apple Silicon Mac and the CI runner alike, so
`just update-snapshots` on a Mac writes them. After an intended visual
change, `just update-snapshots button` rewrites that row's references --
both themes, all three ratios -- and only where they no longer match, so the
commit holds just the images that changed.

Every voxel type is edited end to end in `TestVoxelTypes` (the 2D grid in
each plane the type has, by pointer and keyboard; the 3D view's slab, face
and Erase clicks), on top of uicore's per-type rules. **Saving** is
tested the same way (`TestSaving`): edits through the controllers -- the
2D grid in every plane (Fixed, Variable, a stroke, Paint, Erase, Mirror,
an area), the 3D view (the slab, a face, quick tools, Edit mode, another
layer), the inspector, grid size, several shapes renamed, weighted,
duplicated, reordered and deleted, a problem's result and pieces --
are saved, the file opened in a new App, and the whole puzzle compared
line by line with what was there before the save; undone edits are not
saved and a save leaves the document clean. A new kind of edit gets a
case there. Time budgets
(AC-C08-08's 16 ms and the like) use `VERIFY_BUDGET`: checked in every
build but the coverage and sanitizer builds, whose counters and checks
slow the hot loops several-fold, where they are only reported. The update check takes its GitHub reply
through `UpdateController::setFetch`, so its outcomes are tested without
the network.

## Deviations from the spec, by decision

- **Settings** live in File ▸ Settings (macOS: the application menu's
  Settings / Preferences item, with ⌘,), not
  behind a gear in the top bar; the keyboard-shortcuts page moved to
  Help ▸ Keyboard shortcuts. There is no top bar: the window has the
  system's title bar and a menu bar, which can be hidden (View ▸ Show menu bar).
- **Theme** defaults to Light, as legacy looks (spec: System).
- **Element ids**: the collapsed side cards' expand buttons are the
  shell's, `workspace.left.rail.expand` / `workspace.right.rail.expand`, one
  per side for all three workspaces (C01: `entities.shapes.rail.expand`).
  Every card's header is `<card>.header` (`entities.shapes.header`).
- **3D view hint** (C06): each pair reads key + gesture, then its effect in
  the text colour -- "Shift +click fixed voxel", "Drag orbit", "Shift +drag
  or middle-drag pan" -- without the tool numbers (spec: "Shift click 1
  fixed"; user feedback 2026-10-28).
- **Variable voxels** (Flat) are 60 % opaque and the inner ones show
  through (spec: 50 %, outer surface only). The **Classic** and **STL**
  voxel styles are additions.
- **Workspace rail**: the selected workspace's fill runs to the rail's right
  edge, square there, with the indicator bar on that edge -- a tab joined to
  the workspace (spec AC-C15-02: the bar at the left). In Minimal a rail
  button is as tall as its turned caption; the menu button (menu bar
  hidden) shows its icon alone, "All menus" in its tooltip.
- **Density** (density.md §1 over the component sections, as §1 says):
  buttons 28 (PR-04: 30); the status bar's 11.5 dp text is drawn at 12, a
  font's pixel size being whole.
- **Menus by keyboard**: an Alt tap underlines the access keys and arms the
  menus (Windows' convention); with the menu bar hidden it opens the rail's
  menu button instead. Alt held through a click or another key arms
  nothing, so the planned Alt+click quick tool keeps working.
- **Collapsed Shapes card**: its rail's selected chip has the card's row
  highlight, not C01's 2 dp panel and 4 dp accent ring.
- **Shapes list** (P2, product decisions 2026-10-27): **F2 stays Save**
  (legacy's key); rename is Enter in the focused list, a double-click, the
  pencil or the context menu (AC-C01-03 names F2). **Delete asks first**
  when a problem using the shape has solutions or a search in progress,
  which undo cannot restore. Legacy never asks (OQ-14), and the spec asks
  only if legacy does. **Weight** stops at 0. Chip text is black or white by
  contrast (`prefersWhiteText`), not white with a shadow (PR-08).
  **Dragging a row** moves the other rows aside (150 ms, `motion.reorderMs`)
  to open the gap it drops into, in place of AC-C01-07's 2 dp drop line
  (product decision, 2026-10-27).
- **Drawing colour** (P3): **Remove colour asks first** when a problem
  whose shapes use the colour has solutions or a search in progress
  (legacy clears all of those problems' solutions without asking; the same
  rule as the shape Delete). After a remove the drawing colour is Default,
  as C09 says (legacy kept the next colour). **Add** is refused at the
  library's 63 colours, where legacy's Add reached an assert. **Add is
  quick**: a preset colour at once, as the mock does (C09 asked for the
  chooser); Edit colour… opens the platform's chooser (`ColorDialog`). The
  palette has a row of its own under the current colour, as wide as the
  card (C09 put it beside the name, where it had room for one swatch); in
  a card with room for it (Focus 2D) it sits on the colour's line, between
  the name and the buttons.
- **Voxel editor** (P4): **XZ and YZ for every voxel type but the
  triangular prism** (product decisions 2026-10-27 and -28): spheres,
  rhombic and tetra-octa grids look the same along X, Y and Z -- a test
  maps every cell's corners onto each other under the six axis orders --
  so their XZ and YZ layers are drawn as XY ones. Prisms are edited one XY
  layer at a time, as legacy does; their XZ / YZ buttons are disabled with
  a tooltip saying why (C08 offers all three planes for every type). The
  tool strip and the plane row are **one Tab stop each** (the selected
  button, ← → move), as C07 asks of the strip. The layer chips are no Tab
  stops: PgUp / PgDn step the layer anywhere in Entities. The grid has no
  elevation shadow. Rulers number square cells only; the other voxel
  types' cells are not in rows and columns of one cell each.
- **Ghosts behind voxels** show at 35 % of their strength, drawn last
  with the depth test reversed, so a cell inside the shape still shows
  where it is (C06 does not say).
- **Rendering** is gamma-correct; the layer slab's tint is scaled so it
  still reads like the mock's sRGB mix. **Dim other layers** draws the
  whole shape back to front while it is on, so the dimmed voxels show
  what is behind them, the active layer among them.
- **3D editing in empty space** (P5, product decision 2026-10-27): the
  active layer's slab acts as a 2D grid in the 3D view, and an empty shape
  with the slab hidden takes its first voxel at the origin; C06 targets
  voxel faces only, which leaves an empty shape unreachable from 3D.
- **The Edit button's override** (P5, product decision 2026-10-27): a
  quick tool held over the view lights the button as Edit mode does (the
  glyph and badge the quick tool's), not with C06's dashed outline.
- **The view's keys** (P5): Alt+1 … 6 for the cube's faces, Ctrl+arrows
  for its turns (Option on macOS); the spec asks for them without naming keys.
- **Inspector** (P6, product decisions 2026-10-28): **Fit & scale follows
  Apply to all shapes** (Grid size): Prune, Center, To origin, Minimize,
  ×2 and ×3 act on every shape while it is on, as legacy's Size tab does
  (C03: the selected shape only); "All shapes" shows beside the section's
  first heading then. Transform and Repair stay per shape. **Center and
  Minimize are legacy's**: Center on bricks grows the grid by a cell
  where the free space is odd, so the piece sits exactly in the middle
  (C03: floor the offset); Minimize divides by every prime that divides
  the whole shape, as often as it does, on bricks only (legacy has no
  scale-down for the other types; C03: the largest k ≤ 8). **A nudge out
  of the grid is blocked** with its reason (C04, OQ-7; legacy loses the
  voxels). **Scale** stops at the 1000-step grid limit (OQ-6; the mock:
  64) and is hidden for spheres (C14); rhombic and tetra-octa shapes scale
  too (the library can). **The surface sets** are the spec's (AC-C05-04:
  the walls of an enclosed cavity are inner; legacy counts them as outer).
  **A Transform button previews its result** in the 3D view while hovered
  or focused -- the result as a ghost in the shape's colour, legacy's arc,
  double arrow or arrow in the colour of the axis, danger when blocked --
  as legacy's tool tabs do (the spec has no preview). **A greyed button
  still takes a press** and the status bar says why; its hint (Standard)
  or tooltip (Minimal) says so beforehand, which a disabled button could
  not show. An open section's buttons are one Tab stop (see the feedback
  of 2026-10-29). The nudge pad's row labels are 72 dp (the mock's 62 cut "Down layer"). No
  nudge auto-repeat (C04: optional).
- **Feedback of 2026-10-28** (user decisions): the **Segmented** option on
  is accent-tinted with an accent edge and text (PR-05: panel fill and a
  shadow, which hardly showed on the light track). The **Shapes list**
  takes what the inspector's open sections leave, at least four rows (the
  mock: 60 to 250 dp). The Voxel editor's **tools and Mirror / Span** have
  fixed widths and flow, the placement group beside the tools in a wide
  card; Mirror and Span sit under a **Voxel placement** heading with a hint
  each, and beside them in a wide card the tools take an "Editing tool" heading so
  the two button rows line up; the **planes** sit below the grid under an
  "Editing plane" heading. In a card wide enough for both (Focus 2D) each
  pair -- the tools and the placement group, the planes and the grid size
  -- shares a row, the second block at the right edge, and the Drawing
  colour's palette moves onto the colour's own line, between its name and
  the buttons. Focus 2D gives the
  editor 80 % of what the 3D view's 360 dp leave and the 3D view the rest
  (C11: the editor took all of it; `kFocus2dEditorShare`). The inspector's sections **slide** open and
  shut when toggled (Theme.durCollapse; PR-09 shows and hides them). **Area mode** (legacy's rubber
  band, not in the spec): a toggle after the tools -- a drag spans an area
  of the type's own cells, placed on release as one undo step, Esc drops
  it; remembered, seeded from legacy's `rubberband`, off by default.
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
- Help ▸ Check for updates, and the same check at launch (at most daily;
  Settings ▸ General ▸ Check for updates at launch), as legacy has since
  #137: the shared rules in `src/tools/updatecheck`, the platform's HTTPS
  (`src/tools/httpget`), `UpdateController` and `UpdateDialog.qml` here.
  Scripted runs (a screenshot, a command, the gallery, a scratch settings
  file) never check.
- Focus follows use: a clicked button is where Tab goes on from (no ring
  until the keyboard moves), and collapsing or expanding a sidebar keeps the
  keyboard on the button that undoes it.

## Next phases

The spec's phase plan (`migration/00-phase-plan.md`), in the order its
dependencies allow, less what the first cut already built. Each phase is
one pull request, stacked on the one before until that merges. The right
column is what the review of #140 asks of the phase.

| Phase | Content | From the #140 review |
| :--- | :--- | :--- |
| **P2** Shapes sidebar (C01) — done | New, duplicate, delete, rename inline, weight, reorder (drag, menu, `Alt+↑/↓`), the row context menu, the rail's `+`, the Chip primitive. Each action is one undo step. The status bar flashes when shapes are renumbered | The rules go in a new `uicore/shapelist`, legacy's `cb_NewShape` and the rest stay as they are, and the drift table gets a row for them. The list's keys come from the command table, with a scope |
| **P3** Drawing colour (C09) — done | The footer, palette and menu, adding, editing and removing colours, and the `drawColour` state that P4 and P5 use | Palette edits are `AK_COLOR_PALETTE` steps. `DocumentController::editStructure` takes the kind instead of assuming `AK_ENTITIES_STRUCTURAL` |
| **P4** Voxel editor (C07, C08, C02) — done | The 2D grid, tools, mirror and span, plane and layer, grid size, and the deferred work below | Every mutation path goes through `DocumentSession::edit`. First, a stroke API on the session, so that a paint drag is one coalesced step and no caller touches `puzzleHistory_c` directly. Rules (`expand()`, plane mapping, cell size, blocked shrink) go in uicore with tests |
| **P5** 3D editing (rest of C06) — done | Edit mode and `E`, quick tools, ghosts under the 3D pointer (the hover preview's second source), picking in use, the keyboard 3D view | The same edit and stroke paths as P4 |
| **P6** Inspector (C03–C05) — done | Transform per grid type, fit and scale, repair and surface | A blocked operation's reason is decided in uicore (`btui::shapeops`), so that legacy can share it |
| **P10, P11** Puzzle, Solver | C16–C21 | uicore's API changes here the most. Solver results go through `editWithoutUndo` |
| **P12a** — done, **P8** | Minimal's auto-hiding toolbar; then polish and cut-over, with the deferred work below | Legacy is removed only at cut-over. Until then the rules in `gui-migration.md` apply |

### Deferred work by phase

What each phase takes on besides its own components: the rows of
*Deferred to the next phase* (above, where each row says why it waits),
by the phase named there. A phase is done when these are, too; a row that
moves to a later phase says why in that table.

- **P4 Voxel editor** — done: the composed edit cursors; the Stepper and
  GlyphButton primitives; the status bar's cursor readout; the layer slab
  and Dim other layers on screen (which found Dim other layers drawn into
  an opaque pass, so the window never showed it in the Classic style);
  quick tool 2 (Alt+click) in the 2D grid; settings and modes as typed
  tables (`uicore/vocabulary`); edits that change no geometry no longer
  rebuilding the 3D mesh (`puzzleEdited` says what changed).
- **P5 3D editing** — done: Edit mode and its `E` key; picking in use;
  quick tool 2 in 3D; the 3D view and view cube by keyboard (a focusable
  canvas, arrows, the cube's turns, rolls and faces); the single-key
  shortcuts as a setting (product decision 2026-10-27: window-wide, with
  Settings ▸ General ▸ Single-key shortcuts to turn them off).
- **P6 Inspector** — done: the ActionButton and AccordionSection
  primitives (in the gallery, with reference images).
- **P11 Solver:** shape and colour edits refused while a solve runs
  (legacy `threadStopped()`); the solver's progress announced.
- **P12a Density** — done: Minimal's auto-hiding 3D toolbar (density.md
  section 3): hidden faded and 10 dp up with a 40 x 4 handle; shown for the
  pointer within 72 dp of the view's top (a strip over the canvas, whose
  own hover events would hide it from the card), keyboard focus, its open
  Display menu, and O P E F (1.4 s, `CommandController::toolbarModeUsed`);
  hidden 700 ms after the pointer leaves the strip, 500 ms when it leaves
  the view; Esc or a press in the view lets go of its focus.
- **P8 Polish and cut-over:** the OS pointer in the edit cursors (done
  2026-10-29: the system's arrow and pointer size, `systempointer`); Dim
  other layers in the STL style (done 2026-10-29 for every voxel type);
  STL export writing in place; image export
  memory; the
  hardening from the final review (device loss, resize); Linux
  start-up measured; the accessibility rows -- reduced motion (done
  2026-10-28: the system's QAccessibilityHints::motionPreference from Qt
  6.12, before it macOS's and Windows' own switch (`systemmotion`), sets every
  Theme duration, the toolbar's fade and the 3D view's snaps to none), text
  size and high contrast, small screens, dark-theme button text, remaining
  contrast, progress and errors announced, lists by keyboard, small items
  -- and the screen reader and real-system checks (the shape row's
  actions, the P2 / P3 lists and menus, the accessibility tree); the
  double-click rename test; command enabled-state bumps.

Alongside these, and independent of them, are the two **legacy follow-ups**
from [`gui-migration.md`](../../design-spec/gui-migration.md#the-legacy-gui-while-both-exist):
legacy saves through the atomic write, and legacy's Convert, Import
assemblies and Status call `btui::puzzletools`. Each is a small pull request
of its own (rule 4).

Every phase's pull request follows what #140 settled:

- a short series of area commits, each of which builds and passes the tests;
- a change that reaches legacy in a commit of its own, listed in the
  description;
- no merges from master inside the series.

### P2 decisions

- **Ids and colours by position** (OQ-16). The library keeps the problems'
  references when shapes are exchanged or removed
  (`puzzle_c::exchangeShapes`, `removeShape`), so a reorder from *i* to *j*
  is done as adjacent exchanges, and every reference follows its shape, as
  legacy's ←/→ did.
- **New** takes the selected shape's dimensions, and the grid's default size
  when nothing is selected, as legacy `cb_NewShape` does (OQ-14). **New and
  Duplicate append** to the end of the list. A copy keeps the voxels, the
  label and the weight.
- **Delete** is disabled at one shape (AC-C01-05). Legacy could delete the
  last one. A file with no shapes still shows the empty row. Delete asks
  first only when it would lose data that undo cannot restore: the
  solutions, or a search in progress, of a problem that uses the shape.
  Otherwise it deletes at once, as legacy does, and undo brings the shape
  back.
- **Weight** stops at 0 (legacy had no lower bound).
- **F2 stays Save.** It is legacy's save key, kept in the first cut. Rename
  is Enter in the focused list, a double-click, the pencil button or the
  context menu (a deviation from AC-C01-03, product decision 2026-10-27).
- **The list's keys** (`↑ ↓`, `Alt+↑ ↓`, `Enter`, `Del`, `Ctrl+D`) work
  only while the list has focus (spec Rev 6.0, C01 *Keyboard*). They are in
  the command table with a scope, so they appear in Help ▸ Keyboard
  shortcuts and never fire from elsewhere.

### P3 decisions

- **Colours by id**, as voxels store them: 0 is Default (the piece's own
  colour, named "Default", C0), 1 … n the puzzle's colours, named C1, C2, …
  by their id (product decision 2026-10-27; legacy's chips made Default C1,
  its colours C2, C3, …; OQ-8). The rules are `uicore/drawcolours`.
- **Add is quick** (the mock's, product decision 2026-10-27): + appends the
  first of a set of distinct preset colours the puzzle lacks (the mock's
  five, then five more), with no chooser, and draws with it; Edit colour…
  then changes it in the platform's chooser. As legacy's `cb_AddColor`, the
  new colour is allowed in every problem's colour matrix. Edit keeps the
  drawing colour.
- **Remove** does what legacy's does (solutions of the problems using a
  shape with that colour, then `removeColor`), asks first only when that
  loses solutions, and leaves Default to draw with.
- Colour edits are `AK_COLOR_PALETTE` undo steps, through
  `DocumentController::edit(kind, …)`, which `editStructure` now calls.
- The drawing colour is not used yet: the 2D grid and 3D editing paint with
  it from P4 and P5 (AC-C09-02 is checked there).

### P4 decisions

- **The rules** are in uicore: `voxeledit` (the tools and quick tools,
  Mirror and Span as C07's expansion -- Span, then Mirror, sorted, no
  duplicates --, what a tool does to a cell, the planes, the cell size, the
  grid size in steps) and `gridcells` (each voxel type's 2D cells, ported
  from legacy's `gridEditor_0` … `_4`, and their hit test). Each is tested
  for every voxel type.
- **A tool's effect** is legacy's: Fixed and Variable set the state and
  the colour, Erase empties and clears the colour, Paint recolours a voxel
  that is there and leaves empty cells alone (AC-C07-02). Coordinates that
  name no cell of the voxel type are skipped.
- **Strokes**: `DocumentSession::beginStroke` / `strokeEdit` /
  `endStroke` make a drag one undo step (AC-C08-05) without the caller
  touching `puzzleHistory_c`; an undo, an edit or another document during
  a stroke ends it first, so nothing painted is lost. The tool is captured
  at the press (AC-C08-10); a fast drag paints every cell it crosses.
- **The quick tools** follow C06 (Ctrl > Alt / ⌘ > Shift). On macOS,
  Command picks Variable and Option erases (product decision 2026-10-28:
  Control-click is macOS's secondary click, overview/04 §6).
- **Grid size** counts in steps: 5 voxels for rhombic shapes, 3 for
  tetra-octa ones (legacy's fields), 1 … 1000. A shrink that would cut
  voxels is refused (OQ-5); with Apply to all, every shape that allows it
  changes by the same steps and the others are left as they are, one undo
  step, with the message if any was refused.
- **What an edit changed** is on `puzzleEdited` (`DocumentController::Change`:
  Structure, Voxels, Label, Palette): a rename re-reads one row and no
  mesh, a colour rebuilds the mesh only in the voxel colour view.
- **Plane and layer** moved from `ViewportController` to the new
  `EditorController`; the 3D view reads them for its slab.
- **Settings and modes** are read and written through one table each
  (`uicore/vocabulary`); the QML still sees words.
- **The hover preview** (AC-C08-10, AC-C06-11; added after P4's review,
  which had missed it): `EditorController::preview()` is what a click on
  the hovered cell would do -- `btui::previewTool` over the same Mirror /
  Span expansion, with the tool in effect, so modifiers change it at once
  -- and the 3D view draws it as C06's ghosts: placed cubes in the drawing
  colour at 55 % with an accent outline (dashed for Variable), a faint
  dashed overlay on a voxel that would change, Erase in danger, Paint at
  50 %. A ghost is its voxel type's shape (`btui::addGhost`, the flat
  builder). It clears off the grid and while a stroke paints. Only cells
  that would change get a ghost (Paint over a voxel of that colour: none).
- **Sizes** are density.md §1's, through `Theme` (tool strip 48, Mirror /
  Span 44, planes 54 × 46, layer chips 32 × 24, stepper 28, the drawing
  colour 32; Minimal 36, 32, 38 × 34, 30 × 22, 26, 26), not C07 / C08 /
  C09's own older numbers (58, 54, 62 × 56, 36 × 28, 30, 40): §1 says it
  wins. Minimal shows the tools, Mirror / Span and the planes as icons
  alone and hides the group labels, "Z layer" and "Drawing colour"; the
  tooltips name them (Mirror and Span's say which, the layer chips their
  axis, the drawing colour's swatch the colour). `tst_editor` checks every
  size in both densities.

### P5 decisions

- **One rule for a press in the 3D view** (C06): the quick tool the
  modifiers pick, else in Edit mode the tool, else nothing
  (`EditorController::viewportAction`). Edit mode lives with the tool in
  `EditorController`, so the Edit button, the cursor and the ghost read
  one place.
- **Where it acts** (`btui::target3d`; product decision 2026-10-27): on a
  voxel's face, Fixed and Variable the cell beyond it and Erase and Paint
  the voxel -- a voxel under the pointer always decides, even behind or
  below the slab (2026-10-28); on the active layer's slab where the
  pointer is on no voxel, the layer is a 2D grid and the cell under the
  pointer is the target; in an empty shape with the slab hidden, the
  first voxel goes to the origin (the voxel type's nearest cell) wherever
  the pointer is. A Fixed or Variable press whose cell is outside the grid
  outlines the face in dashed danger and does nothing. Picking runs on
  two meshes kept with the mesh and the slab: the shape's voxels and the
  active layer's cells.
- **Telling the user**: the ghost at the target; the status line naming
  it ("in the active layer (Z1)", "the first voxel goes to the origin",
  "Outside the grid"); the empty-shape message saying what a Shift-click
  does in either case.
- **One hover preview**: the 2D grid's cell or the 3D view's target,
  whichever pointer is over its view; a drag in 3D, a stroke in 2D, or
  leaving clears it. Modifiers held over the 3D view are watched while
  the pointer is there, so a quick tool's cursor, ghost and the Edit
  button's override appear the moment its key goes down.
- **A click is one undo step** (`AK_ENTITIES_CLICK_3D`, which never
  coalesces: quick clicks were merged as a 2D stroke's cells until P6),
  Mirror and Span included; 3D edits leave the plane and layer alone.
- **The canvas by keyboard** (WCAG 2.1.1): a Tab stop after the toolbar,
  ringed on the card's edge; a click takes no focus (C20). Its keys are
  the command table's viewport scope, listed in Help: arrows orbit 15°,
  Shift+arrows pan 40 dp, + / − zoom a wheel notch, Ctrl+arrows the
  cube's 90° turns (Option+arrows on macOS, whose Control+arrows switch
  spaces), Ctrl+Shift+Left / Right its rolls, Alt+1 … 6 its faces (front
  +Z, back −Z, left −X, right +X, top +Y, bottom −Y, as the cube labels
  them).
- **The review of 2026-10-28** (spec and guide against P1–P5; product
  decisions): the **3D toolbar is one roving Tab stop** (← → move the
  ring, Enter or Space press; its buttons take no focus on a click, C20),
  as overview/04 §5 asks. The **2D grid is a Tab stop** with a cell cursor
  -- the hover, so the outline, readout and 3D ghost follow -- moved by the
  arrows (`btui::stepCell`, every voxel type), Space or Enter applying the
  tool in effect (Shift+Space Fixed), Delete or Backspace erasing; a click
  focuses it too. Alt+Space and Ctrl+Space are the system's (window menu,
  input source), so no quick tool rides on them. **macOS**: Command picks
  Variable and Option erases (Control-click is the secondary click there).
  **Performance**: a stroke segment to the 3D view's next frame is tested
  under AC-C08-08's 16 ms (16³ half full: ~7 ms); the pick mesh is made only
  when a pick needs it, from the cells' faces (~2 ms there, against ~170 ms
  through the library's flat mesh, which spheres keep), and the 3D hover
  is picked at most once a frame (`VoxelViewport::updatePolish`).
- **Single-key shortcuts** (WCAG 2.1.4): O, P, F, E and 1–4 are marked in
  the command table (`btui::isSingleCharacter`) and Settings ▸ General ▸
  Single-key shortcuts turns them all off; the shortcuts window says so.

### P8 decisions (the first)

- **The system's arrow in the edit cursors** (PR-20, OQ-17;
  `systempointer`): Fixed and Variable draw their badge on the arrow the
  system shows, snug against its upper right, with its hotspot. Windows:
  the arrow of the user's pointer scheme or pointer colour (the `Arrow`
  file under `HKCU\Control Panel\Cursors`, else the system's), at
  Settings ▸ Accessibility ▸ Mouse pointer's size (`CursorBaseSize`; a
  custom cursor is shown at its own size there), drawn on black and on
  white to get its colour and transparency (`QImage::fromHICON` gives
  nothing for a cursor). macOS: `NSCursor.arrowCursor`; the system
  enlarges every cursor, ours too, by its Pointer size, so ours is drawn
  at the default size. Linux and the BSDs: the Xcursor theme's arrow and
  size (`XCURSOR_THEME` / `XCURSOR_SIZE`, then KDE's `kcminputrc`, then
  GTK's `settings.ini`), read by `uicore/xcursor`. Erase and Paint follow
  the pointer size. Anything unreadable falls back to the bundled arrow.
  The arrow is read again when its settings change (the scheme, the size,
  the screen's scale).
- **Dim other layers in the STL style, every voxel type** (AC-C06-05):
  the prism, rhombic and tetra-octa meshers merge faces across cells
  as the brick exporter does; each face is cut by the walls of the
  filled cells it crosses (`splitAtCellWalls`: a cell is the convex
  polyhedron of its connection faces), each piece taking its cell.
  Spheres' faces are each ball's own already. The cuts cost nothing
  measurable beside the mesher, which takes ~28 s for a 10³ prism shape
  on its own (the library's Minkowski mesher; not changed here).

### P6 decisions

- **The rules are uicore's** (`btui::shapeops`): every operation is
  legacy's tool-tab command for the voxel type (`ToolTab_0` … `_4`,
  `applyTask`), keyed by legacy's button number, so a button runs exactly
  what its legacy twin runs -- the lattice-parity rules of Center and To
  origin included. The inspector adds the checks: a transform that would
  lose voxels is refused, Minimize and the scales say why they cannot,
  Fill and the surface tools count what they changed. `opsDescriptor`
  gives each type's Transform buttons with their captions, tooltips and
  ids (C14), from which the QML builds the matrix or the pads.
- **One undo step a press** (`AK_ENTITIES_STRUCTURAL`, as legacy records
  its tool tabs), checked again when pressed; the states the buttons show
  are worked out again 120 ms after edits pause (a Minimize over every
  shape tries each prime on a copy), at once for a new selection or file.
- **The preview** (`InspectorController::preview`): one button at a time
  owns it -- moving straight from one to the next does not clear the new
  one -- and it follows the shape while shown (a press shows the next
  step). Its hint is drawn over everything at full strength
  (`SceneFrame::hintLines`), as legacy draws it with the depth test off.
- **The Shapes list's height** follows the inspector, not its rows: it
  takes what the open sections leave, at least four rows (feedback of
  2026-10-28). A list that resized with its own rows laid out, inside a
  model change, rows the change had just taken away (Qt's
  "DelegateModel::cancel" warnings); no rows are made ahead off screen.
- **QML gets the task numbers as `tasks`**, not the `Task` enum: the
  compiled QML read the enum's values (15, 16, … 40) as undefined.

## Spec review of the Entities workspace (2026-10-29)

The Shapes tab -- C00 to C14 and C22, the density and keyboard parts --
read against what is built: each acceptance criterion against the code
and the tests, the spec's element ids against the QML's `objectName`s
both ways, and this file against the code.

**Changed on purpose.** Everything under *Deviations from the spec, by
decision* above, each with its date. In short: no top bar (the system's
title bar, Settings in File, Light by default); C01's F2, delete
confirmation, weight floor, chip text and drag gap; the Voxel editor's
planes (XZ / YZ but prisms; below the grid), headings, fixed-width blocks
and two-block rows in a wide card, Area mode, the grid's keyboard cursor;
C09's quick Add, Remove confirmation and palette placement; the 3D view's
slab as a 2D grid, ghosts behind voxels, the Edit button's override look
and cursor glyph, the hint's wording, the view's keys, the Classic and STL
styles; the inspector following Apply to all, legacy's Center and
Minimize, blocked nudges, the 3D preview, greyed buttons that still say
why; Focus 2D's 80 / 20 split; the rail's indicator side; density.md
§1's sizes over the component sections'.

**Spec conflicts, settled.** AC-C03-01 says the inspector's sections are
closed by default; layout §2 opens Transform. All three start closed, as
AC-C03-01 has it (user decision 2026-10-29; Transform first opened, as §2
and the mock have it).

**Left out unintentionally, found by this review.**

| Item | Spec | Now |
| :--- | :--- | :--- |
| No `entities.shapes.header` (no card header had an id) | C01 ids | Fixed: every card's header is `<card>.header`; `tst_shapes` checks the header's parts |
| No test that edits survive a save -- per voxel type, 2D and 3D, several shapes, the problems that use them | AC-C01-07 "order persists after save/load", AC-C14-02, test plan | Added `TestSaving` (3 cases × 5 voxel types, 3 more); it found no fault |
| This README: reduced motion still deferred, the list's 238 dp height, the palette's place, the "Tool" heading, a shape name's tooltip, the test counts | -- | Brought up to date; the agents' guide too |

**Still open, knowingly.** The rows of *Deferred to the next phase*
(KDE's Alt+click, text size and high contrast, small screens, the
screen-reader pass; the OS pointer in the edit cursors and the STL
style's dimming for every voxel type were done right after, see *P8
decisions*). AC-C08-07's "recolouring the shape in the app" has
no control in Entities -- a piece's colour is the Puzzle workspace's
(C17, P10) -- and is checked there.

**Added unintentionally.** None found. The ids the QML has beyond the
spec's are parts of a specified component (headings, the colour menu's
items, the Display menu's rows, focus rings) or of a decision listed
above (`entities.editor.tool.area`, `entities.viewport.toolbar.handle`,
`entities.fit.allShapes`).
