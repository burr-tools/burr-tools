# burrtools-qt — Performance Guide

**Date:** 2026-10-08, start-up work 2026-10-09/10
**Status:** a guide and a record. Start-up ("first paint" and "app ready")
is reworked and measured on Windows; macOS and Linux have a plan (below).
Rendering and interaction: P1, P2.4, P2.5, P2.7 and the cheap half of P2.1
are done; the rest of P2 (P2.1's cheaper sort, P2.2, P2.3, P2.6) and P3 is
open. The checks under "Measuring" exist.
**Scope:** `src/qtgui`, `src/uicore`, the QML module `BurrTools.Ui`.

How to use this guide: measure first ("Measuring"); the start-up section
says what costs what and the rules that keep start-up fast; the backlog
says what is known to be left.

## Measuring

All headless (offscreen, or Xvfb with lavapipe on Linux) unless said
otherwise, no external service. Gates fail CI; reports never do.

| Check | Where | Runs | Gate or report |
| :--- | :--- | :--- | :--- |
| Start-up, first paint and app ready | `BURRTOOLS_STARTUP_TRACE=1 burrtools-qt <puzzle>` prints each step from process creation (BUILD.md); `=quit` quits once the app is ready | by hand, on the real display | report |
| Start-up, wall time | `scripts/profile-qt.sh … startup`, cold and warm; `just startup-time` (includes `--screenshot`'s fixed 1.5 s) | Qt profile workflow | report |
| UI text needs no fallback font | `TestTheme::uiTextNeedsNoFallbackFont`: every character in the QML's and the command table's strings is in Segoe UI | every Qt job (Windows; skipped elsewhere) | **gate** |
| Work counts | `anEditKeepsTheRowsAndBuildsTheSceneOnce`, `theTargetsOutgrowTheViewInSteps`, `theVoxelStyleSettingRebuildsTheMesh` (Qt Test) | every Qt job | **gate** |
| Graphics memory of the scene targets | `TestRender::theTargetsMemoryStaysInBudget`: bytes from size × format × samples (`SceneRenderer::targetBytes`); a 2048×1920 view at 4× under 256 MB, 8× near double | every Qt job | **gate** |
| Allocations per frame | `TestRender::framesDoNotAllocatePerTriangle` (orbiting a 12³ variable cube, ~20k translucent triangles: 0 B a frame from our code; the old sort took 414 720 B), `TestViewportController::aFrameReadsNoSettings` and `[depthsort][alloc]` (Catch2), through a counting `operator new` ([test/alloccount.h](../test/alloccount.h)) | every Qt job; `just test` | **gate** |
| Bindings left to the JS engine | `qtgui_qml_aot` meson test: `qmlcachegen --verbose -I <the import tree>` per QML file as the build runs it, against [test/qtgui/qml_aot_baseline.json](../test/qtgui/qml_aot_baseline.json) (Qt 6.11: 408; 1936 before P2.5); `just qml-aot [--update]` | every Qt job (compared where the Qt matches the baseline's, reported elsewhere) | **gate** (may only go down) |
| Sanitizers | `sanitizers` job: ASan + UBSan build; `test_burrtools` with leak checks, the Qt suites without (Qt and Mesa are not instrumented) | every push | **gate** |
| Micro-benchmarks | [test/bench_ui.cpp](../test/bench_ui.cpp), hidden `[bench]`: mesh building (8³, 20³, 20³ shell with a variable core, 50³), the depth sort (80k), vector export; `just bench-ui` | `build-linux` (run summary), Qt profile workflow | report |
| QML profile | `scripts/profile-qt.sh … qml` (qmlprofiler; a `-Dqml_debug=true` build); `just profile-qml` | Qt profile workflow (artifact `qt-profile`) | report |
| Heap profile | `scripts/profile-qt.sh … heap` (heaptrack, Linux); `just heap-qt` | Qt profile workflow | report |
| GPU timing | RenderDoc, PIX, Xcode | **no** — needs a real GPU and a person | local only |

Not done: `QRhi::statistics()` (the allocator's view of graphics memory) is
only filled on Vulkan and D3D12; the computed bytes cover what the renderer
owns on every backend.

**Timing start-up by hand.** Release build (`just build-release`), on the
real display -- offscreen hides the platform's and the GPU's costs. Discard
the first run of a newly built or downloaded executable: Microsoft Defender
scans it (1.2-1.5 s before `main()`). *Cold* here is a first start (no
settings file, so no pipeline cache), *warm* a start with both. Compare
builds run by run, interleaved, with medians of 8 or more; a laptop varies
±20 % between sessions. Let each run quit by itself (`=quit`): killing it
and starting the next at once slows that next start while the driver tears
the last one down. Qt's own timings help:
`QT_LOGGING_RULES="qt.scenegraph.time.renderloop=true;qt.rhi.general=true;qt.qpa.fonts=true"`
with `QT_FORCE_STDERR_LOGGING=1` (burrtools-qt is a GUI program, so Qt
logs to the debugger otherwise).

## Start-up: first paint and app ready

**First paint** is the first frame on screen. **App ready** is the first
moment the GUI thread is idle with the workspace on screen and the puzzle
named on the command line opened and drawn in the 3D view -- when the UI
answers input at once.

### Results (Windows)

Release build, Direct3D 11, PelikanBurr, Windows 11, RTX 3080 Ti laptop,
676 installed fonts; medians of 8 cold and 8 warm runs of each build,
interleaved (2026-10-10):

| Build | First paint, warm / cold | App ready, warm / cold |
| :--- | :--- | :--- |
| before (2026-10-09) | 1130 / 1236 ms | 1131 / 1236 ms |
| round 1: the window first, dialogs on demand, fonts warmed on a thread | 555 / 700 ms | 892 / 966 ms |
| round 2: the device made early, one font family (no fallback scan), the puzzle opened after the first frame, dialog files loaded on demand | **349 / 486 ms** | **624 / 917 ms** |

Where the time goes now (warm, ms from `main()`; process start ~25 ms before):

| Step | ms | Thread |
| :--- | ---: | :--- |
| `QGuiApplication` | ~70 | GUI |
| `App` (controllers, settings) | ~2 | GUI |
| `Main.qml` and its types (the shell only) | ~35 | GUI |
| Qt's first native window (`QWindow::create`) | ~150 | GUI |
| `show()`, the device handed over | ~20 | GUI |
| first frame (the outline) | ~20 | render |
| **first paint** | **~330** | |
| the puzzle opened, the workspace made | ~100-150 | GUI |
| the 3D view's mesh built (queued after the workspace) | ~50 | GUI |
| **app ready** | **~530-630** | |

Cold starts are slower mostly in their first frames: Direct3D compiles the
shaders the pipeline cache holds on later starts (P2.7 covers ours in CI
builds; Qt Quick's own come as the Qt build packaged them).

### What was done, and why

1. **The window first, the workspace after the first frame** (`Main.qml`,
   round 1). The window comes up with the workspace's outline -- menu
   strip, rail, cards and status bar in the theme's colours, no text -- and
   `startContent()` makes the workspace and the menu bar once that frame is
   on screen (`frameSwapped`; a 1 s timer for a window never drawn).
   Messages (a puzzle's comment, an unfinished search) wait for the
   workspace too. QML tests wait for `win.contentReady`.
2. **Dialogs made when first opened** (round 1), and **their files loaded
   only then** (round 2): each is a `Loader { source: "X.qml" }` opened
   through `showDialog()`; `modalOpen` reads through the loaders. Loading
   `Main.qml` went from ~50 to ~35 ms; each dialog's first open costs
   ~5-10 ms. `BtDialog` parents itself to the window's overlay, so its size
   follows the window wherever it was made.
3. **The window in the theme before its first frame** (`WindowBackground`
   in `main.cpp`, Windows). Qt's window classes have no background brush:
   until the first frame (~0.2-0.3 s) the window was white, a flash in the
   dark theme. The erase is answered in the Qt Quick window's own colour.
4. **No font fallback scan** (`App`, Windows; round 2). Qt's fallback list
   for a font on Windows is every installed family, each read under one
   lock: 0.3-0.8 s, at every start (Qt keeps no cache of it between runs).
   It is built at the first text when the font names fallback families (the
   design tokens' stack did) or when a character is missing from the font
   (the voxel editor's 🔒, a tooltip's ▸). The app font is now one family,
   Segoe UI; the UI's own text uses only characters it has (the padlock is
   drawn; the gate above keeps it so). The list is built only if a name in
   another script needs it -- once, then. Another font would not shorten
   the list, nor would bundling one. (Round 1 had built the list on a
   thread instead: it overlapped the window's creation but still held the
   GUI thread's first text.)
5. **The graphics device made at once** (`EarlyGraphicsDevice`,
   [earlydevice.h](../src/qtgui/earlydevice.h), Windows; round 2). Qt
   Quick made the Direct3D 11 device on its render thread once the window
   was shown, the GUI thread waiting (0.2-0.3 s; `blockedForSync` in the
   render-loop log). It is now made at the start of `main()` on a thread of
   its own, as Qt would make it (`QT_D3D_ADAPTER_INDEX`, else the first
   adapter), and handed over with `QQuickGraphicsDevice::fromDeviceAndContext`;
   Qt logs "Using imported device". It applies only where Qt would use
   Direct3D 11 itself; a lost device sends the window back to one of Qt's
   own. `BURRTOOLS_EARLY_DEVICE=0` turns it off.
6. **The puzzle opened after the first frame** (round 2):
   `DocumentController::loadStartupFiles`, called by `startContent()` just
   before the workspace is made, so the workspace is made with it.
   `--command` and `--screenshot` wait for it (`startupFilesLoaded`).
7. **A start-up trace** (`StartupTrace` in `main.cpp`): the steps above,
   with app ready detected as the event loop's first idle moment after the
   puzzle's frame.

### Rules that keep start-up fast

- Nothing before the first frame that the first frame does not need. New
  work goes after it (`startContent()`), into a dialog's own file, or is
  made when first used.
- A new dialog: its own QML file, a `Loader { source: … }` in `Main.qml`,
  opened with `showDialog()`; add it to `modalOpen` if it is modal.
- The outline (`Main.qml`, `id: outline`) has no text and no images.
- UI text: only characters the app font has. Icons, not emoji or symbol
  characters (`TestTheme::uiTextNeedsNoFallbackFont` fails otherwise).
- The app font stays one family on Windows.
- Measure before and after (above), interleaved, on the real display.

### Tried and dropped (measured)

| Idea | Result |
| :--- | :--- |
| Warming the font fallback list on a thread (round 1) | −80 ms on its own; the scan still holds Qt's font lock, which the first text waits for. Replaced by not scanning (4). |
| `QFont::NoFontMerging` for the app font | Not faster: Qt Quick Controls take their font from the style, not from `setFont`, and still scan; and every character Segoe UI lacks (CJK names, emoji) would be a box. |
| Opening every font face in parallel through DirectWrite | Slower (first paint +70 ms); Qt's pass gained nothing. |
| Preloading the first window's DLLs on a thread | No change to `QWindow::create`. |
| A throwaway Win32 window on another thread | Windows' own first-window cost (~60 ms) is already paid in `QGuiApplication`; no change. |
| An early DXGI factory | 13 ms; not the first window's cost. |
| Windows platform options (`darkmode=0`, `nowmpointer`, `dpiawareness=1`) | No change. |
| Creating the device early and throwing it away (driver warm-up only) | −20 ms; keeping it (5) is what pays. |
| Another graphics API | D3D11 first frame 1510-1722 ms; OpenGL 1860-2011, D3D12 1839-2115, Vulkan 2013-2115 (before round 1) -- D3D11 stays. |

### Open

- **Qt's first native window: ~150 ms** on the GUI thread (a plain Win32
  window: ~60 ms; the Quick window after a plain one: ~10 ms). It loads a
  signature check's crypto DLLs and the shell's `twinapi.appcore` and
  `dataexchange`; finding the call needs a sampling profiler (Windows
  Performance Recorder, an administrator's).
- **The mesh after the workspace (~50 ms of app ready):** building meshes
  off the GUI thread (P3) would overlap the two.
- `QGuiApplication` (~70 ms) and the time before `main()` (~25 ms; DLL
  loading) are Qt's; the standalone (static) build loads fewer DLLs --
  not measured.

## Windows-only, and the plan for macOS and Linux

Items 1, 2, 6 and 7 above are shared QML and C++ and apply on every
platform; nobody has measured macOS or Linux start-up yet. The rest is
Windows-only:

| Windows-only | Why Windows-only | macOS | Linux |
| :--- | :--- | :--- | :--- |
| 5. The device made early | Direct3D 11 code | Metal: `MTLCreateSystemDefaultDevice` and a command queue on a thread, handed over with `QQuickGraphicsDevice::fromDeviceAndCommandQueue`. Metal devices are usually quick to make; measure first. | Qt Quick's default there is OpenGL: a context made on another thread must be moved to the render thread (`QOpenGLContext::moveToThread`, `fromOpenGLContext`) -- fragile. Vulkan (`BURRTOOLS_RHI=vulkan`): the instance (loading the drivers) and the device on a thread, `fromDeviceObjects`, the instance set on the window. |
| 4. One font family, no fallback scan | The all-families scan is the Windows (DirectWrite) font database's | Core Text's fallback list is the system's cascade list, not every font: probably cheap. The stack (system font, then Segoe UI, Inter, … that are not installed) may still cost lookups -- count them. | Fontconfig sorts the fonts for each family and script (`FcFontSort`), from its disk cache: likely tens of ms, more with many fonts; the stack's missing families each ask. |
| 3. Window colour before the first frame | `WM_ERASEBKGND` | The window shows its system background (follows the light/dark appearance Qt is told) until the first Metal frame: a near match. Setting `NSWindow.backgroundColor` to the theme's (Objective-C++) would make it exact. | X11: no background pixel is set, so the window may show black or stale pixels until the first frame (compositor-dependent); `XCB_CW_BACK_PIXEL` would set the theme's. Wayland: a window appears only with its first frame -- nothing to do. |
| The trace's "process created" | `GetProcessTimes` | `sysctl(KERN_PROC_PID)` start time | `/proc/self/stat` start time against `/proc/uptime` |
| The no-fallback gate | Checks Segoe UI | Check against the app font as Qt resolves it on each platform (`QRawFont::fromFont(QGuiApplication::font())`), not a named font. | Same; CI's fonts differ from users', so the gate would check the CI image's font. |
| P2.7 precompiled shaders | DXBC is Direct3D's | Metal compiles MSL when pipelines are made; the pipeline cache covers later starts. Whether this Qt's `qsb` can store a compiled Metal library is to be checked. | SPIR-V is compiled already; OpenGL drivers keep their own shader caches. Nothing to do. |
| Qt's first-window cost | Found on Windows | Unmeasured | Unmeasured |

**Plan**

1. **Measure (both platforms).** Add the trace's "process created" marker
   for macOS and Linux; make `scripts/profile-qt.sh … startup` report
   first paint and app ready from the trace (medians, interleaved) instead
   of wall time; run it in the Qt profile workflow on the macOS and Linux
   runners, with Qt's render-loop, RHI and font logs. CI runners are
   virtual machines without real GPUs, so also take one run on a real Mac
   and a real Linux desktop (X11 and Wayland). Read: `blockedForSync` at the
   first frame (the device), the number and cost of `fallbacksForFamily`
   calls (fonts), and what the window shows before its first frame.
2. **Cheap, where measured worth it** (more than ~20 ms, or a visible
   flash): the no-fallback gate against each platform's resolved app font;
   one font family where the fallback lookups cost; the X11 background
   pixel; `NSWindow.backgroundColor`.
3. **The early device, where measured worth it** (the first frame's
   `blockedForSync` over ~50 ms): Metal first (the simplest: no threading
   rule stands in the way), then Vulkan; OpenGL only if its numbers demand
   it.

Each step behind its own switch, measured before and after as above.

## Rendering and interaction backlog

From a structural, performance and memory review of the first cut of the Qt
GUI (PR #140, issue #139). It read the render path (`scenerenderer.cpp`,
`voxelviewport.cpp`), frame construction (`scenecontroller.cpp`,
`viewportcontroller.cpp`), mesh building (`uicore/scenemesh.cpp`), the
controllers' signal wiring and the QML. **It was a code reading, not a
profiled run**: sizes and timings in the open items are worked out from the
code, estimates until measured. The layering is sound -- toolkit-free logic
in `src/uicore`, Qt adapters in `src/qtgui`, an immutable mesh shared with
the render thread, RAII-owned QRhi resources -- so every item is local;
none needs a redesign.

### Done in P1 (commit `956841cc`)

| Item | What changed |
| :--- | :--- |
| One mesh per change | `ViewportController` looks the shape up at once and builds its mesh once per burst of signals (`scheduleMesh`); an edit used to build it two or three times. |
| Shapes list keeps its rows | `ShapesModel::refresh` inserts or removes rows at the end and updates the rest in place; voxel counts are cached per row; `countChanged` only when the count changes. |
| 4× MSAA default, a setting | Settings ▸ 3D view ▸ Anti-aliasing (Off/2×/4×/8×). 8× on the RGBA16F scene target was ~410 MB at 2025×1875 device px. |
| Targets grow in steps | Scene targets in 64 px steps, kept while the view fits and uses half of them; the composite pass reads the view's corner. The offscreen renderer keeps one render pass layout across sizes. |
| Settings saves | Debounced 500 ms, flushed on quit; written to `<file>.tmp` and renamed over the file. |

Tests guard each: one rebuild and no model reset per edit and undo
(`TestShapesAndStatus::anEditKeepsTheRowsAndBuildsTheSceneOnce`), target
sizes and corner rendering on D3D11/OpenGL/Vulkan
(`TestRender::theTargetsOutgrowTheViewInSteps`), the setting
(`antialiasingIsFourTimesByDefault`, `test_antialiasingIsASegmentedChoice`),
atomic and debounced saves.

### P2.1 Re-sort translucent triangles only when the view turns — partly done

*Done:* `btui::DepthSorter` ([depthsort.h](../src/uicore/depthsort.h))
keeps its storage and the renderer sorts only when the view's rotation
changes (`btui::sameRotation`); a frame allocates nothing per triangle
(`TestRender::framesDoNotAllocatePerTriangle`).

*Open:* a cheaper sort for small turns -- sort the previous order again
(insertion sort is near-linear when the view turned a little), or per voxel
cell (six times fewer keys) where the cell is the unit that cannot
interpenetrate. A 20³ variable core is ~80k triangles, an estimated 4-6 ms
per sort. Verify with the depth-sort benchmark (80k) and the translucency
render tests (`innerVariableVoxelsShowThroughTheOuterOnes`).

### P2.2 Build only the visible faces

- **Where:** `addFlatVoxels`, [scenemesh.cpp](../src/uicore/scenemesh.cpp).
- **Problem:** every face of every non-empty voxel is computed
  (`getConnectionFace`) and copied into a freshly allocated
  `std::vector<Vec3>` before the neighbour is checked; interior faces are
  then dropped. About six allocations and face computations per voxel for
  nothing in a solid shape.
- **Approach:** check the neighbour's state first; orient the normal from the
  voxel-to-neighbour direction instead of the centroid of all faces; corners
  in a fixed array (a grid's face has at most a handful).
- **Verify:** `test_ui_scenemesh.cpp` (same vertices out, every grid type);
  a benchmark of `buildShapeMesh` at 8³, 20³ and 50³. It also shortens app
  ready (the start-up mesh, ~50 ms for PelikanBurr).

### P2.3 Animation in step with the display

- **Where:** `SceneController`, [scenecontroller.cpp](../src/qtgui/scenecontroller.cpp) (`m_timer`, 16 ms).
- **Problem:** a coarse 16 ms `QTimer` drives camera animation. Windows'
  coarse timers fire on its ~15.6 ms tick, so animation judders at 60 Hz and
  cannot exceed ~60 fps on 120/144 Hz displays.
- **Approach:** advance the animation from the window's frame
  (`QQuickWindow::afterAnimating`, or `QQuickItem::update()` from the item
  while animating, reading the elapsed time); at the least `Qt::PreciseTimer`.
- **Verify:** the camera's existing animation tests (they call `tick(dt)`).

### P2.4 No string-keyed settings reads per frame — done

`SceneController::readSettings` keeps lighting, the voxel style (an enum)
and the anti-aliasing samples in members, refreshed on
`SettingsController::changed`; `ViewportController` keeps the four Display
options (`m_display`), refreshed with them and set by its own setters.
`frame()` and `wantedSamples()` read only members (they ran while the render
thread waited in `synchronize()`). Gate:
`TestViewportController::aFrameReadsNoSettings` (no allocation while
building a frame with nothing but the mesh to draw; each string-keyed read
allocated a `std::string` -- counted on Linux and macOS, where the test's
`operator new` also serves the standard library's strings; MinGW's shared
libstdc++ allocates them inside its DLL, past the count).

### P2.5 Type the QML — done

Bindings left to the JavaScript engine (Qt 6.11) **1936 → 398** (408 after
the start-up work's `Loader` reads, which qmlcachegen cannot type):

| Step | Total |
| :--- | ---: |
| before | 1936 |
| qmlcachegen sees the module's C++ types (the import tree, below) | 1527 |
| every `Q_PROPERTY` in `src/qtgui` `FINAL` (168; "can be shadowed") | 745 |
| `qreal` properties declared `double` (the qmltypes' "qreal" is no type to qmlcachegen), and `pragma ComponentBehavior: Bound` in the 13 files whose delegates use the file's ids | 466 |
| typed function parameters and returns; `ImageExportController`, `StlExportController`, `ShapeStatusModel` and the validators typed; constant arrays `list<var>` | 398 |

The import tree: `qml_module(cachegen: false)`, then
`scripts/qml_import_tree.py` lays out `import/BurrTools/Ui/` (the module's
qmldir with its components pointed at the sources, and the registrar's
.qmltypes), and our own `qmlcachegen -I import` targets compile each file
and the cache loader ([src/qtgui/qml/meson.build](../src/qtgui/qml/meson.build)).
The tree changes with the C++ types only, so a QML edit recompiles that
file alone (~8 s). `qtgui_qml_aot` and `just qml-aot` pass the same
`--import`. (Before, meson's `qt.qml_module` wrote the qmldir and
.qmltypes flat in the build directory and qmlcachegen warned "Failed to
import BurrTools.Ui".)

One behaviour change compiling exposed: a binding that read a property
only for its dependency (`{ root.stl.revision; return … }`) lost the
dependency once compiled -- the bare read is dropped. It now uses the value
(`revision >= 0 ? … : …`), as `CommandMenu.qml` already did.

What is left is mostly by design: field reads on JavaScript objects and
maps (`modelData` of the menus, Settings rows and shortcut tables, built in
C++ as `QVariantMap`s), `App.settings[row.prop]` in the Settings rows,
calls into JavaScript built-ins, and Qt's own non-`FINAL` members
(`Loader`). Measured effect: the density switch −33 % (median, 2.5 →
1.67 ms); the theme switch and dialog creation within noise; the stripped
executable +1.3 MB (5.8 → 7.0 MB).

### P2.6 Image export off the GUI thread

- **Where:** `ImageExportController::writePages`, [imageexportcontroller.cpp](../src/qtgui/imageexportcontroller.cpp).
- **Problem:** all pages are composed and PNG-encoded in one go on the GUI
  thread, every picture held until then; a large multi-page export freezes
  the window.
- **Approach:** compose and encode each page with `QtConcurrent::run` as soon
  as its pictures are drawn; release pictures once placed.
- **Verify:** `TestImageExport` (pages, cancel, unwritable place).

### P2.7 Precompiled Direct3D shaders — done

*Measured first* (`QRhi::statistics().totalPipelineCreationTime` of our
pipelines, no pipeline cache, PelikanBurr drawn flat then Classic, three
runs):

| Backend | Pipeline creation |
| :--- | :--- |
| D3D11, WARP | 47–51 ms |
| D3D11, RTX 3080 Ti | 39–55 ms |
| Vulkan, RTX 3080 Ti | 11 ms first run, ~1 ms once the driver cached them |
| OpenGL, RTX 3080 Ti | 24 ms first run, ~0 ms once the driver cached them |

On Direct3D the GPU makes no difference: it is the HLSL compiler on the CPU,
about 50 ms of every start without a pipeline cache (the first, or after an
update). `-Ddxbc` (a feature, auto): on Windows the build looks for the
Windows SDK's `fxc` (on the PATH, else the newest SDK under
`Windows Kits/10/bin`) and runs `qsb -c`, storing DXBC instead of HLSL;
without it, HLSL source as before (`meson setup` prints which). The Windows
Qt CI job builds with `-Ddxbc=enabled`, so its build has DXBC and its D3D11
(WARP) render tests draw with it. Gate:
`TestRender::theShadersCarryEveryGraphicsApi` (DXBC exactly when the build
says so). Qt Quick's own shaders stay as the Qt build packaged them.

### P3 — later

| Item | Where | Problem | Approach |
| :--- | :--- | :--- | :--- |
| Indexed, smaller vertices | `uicore/scenemesh`, `scenerenderer` | Plain triangles at 56 B per vertex: ~336 B per square face. | An index buffer (−26 %); `Half`/`UNormByte` formats for normal, cell and edge (56 → ~40 B). |
| Meshes built off the GUI thread | `ViewportController::buildMesh` | Large shapes block the window while building; at start-up the mesh follows the workspace (~50 ms of app ready). | `QtConcurrent` with the revision as the result's key; the view keeps the last mesh meanwhile. |
| Finer change signals | `SettingsController`, `Theme` | One `changed` for 15+ settings and the whole theme: every binding re-evaluates on any change. | A signal per property (or per group: view, theme, general). |
| A command model | `CommandMenu.qml`, `CommandController` | Every menu item's enabled/checked binds to one global revision: any change re-evaluates ~80 bindings. | A `QAbstractListModel` of commands (or one QObject per command) with its own change signals. |
| Camera by composition | `SceneController`, `ViewportController`, `ImageExportController` | Inheritance to share a camera; vector export lives in the viewport controller. | A camera/gesture object both hold; export in its own controller. |
| No hidden singletons | 17 `App::instance()` / `Theme::instance()` | Hidden dependencies; tests share state. | Pass them in at construction. |
| Settings keys in one place | `viewportcontroller.cpp` and others | `"view.display.axes"` etc. scattered as strings. | One header of keys (or typed accessors on `SettingsController`). |
| Undo memory | `puzzleHistory_c` | Each edit stores a copy of the edited shape: 500 steps on a 100³ shape ≈ 500 MB worst case. | Say so in the Undo depth setting; store run-length diffs for large shapes. |
| Small cleanups | `SceneRenderer::release` (raw `delete`), per-frame `std::vector<LineVertex>` | Style; an allocation per frame. | `d = std::make_unique<Impl>()`; keep the line buffer as a member. |

## History: start-up, measured 2026-10-08

Before any start-up work, the dev build, `burrtools-qt PelikanBurr.xmpuzzle
--screenshot` (which waits a fixed 1.5 s before quitting), wall time:

| Build | Cold (fresh settings, no pipeline cache) | Warm |
| :--- | :--- | :--- |
| before P1 (`3494281b`) | 7848\*, 5461\*, 2510, 3877, 2513 ms | 2544, 3791, 2535, 3864, 2503 ms |
| after P1 (`956841cc`) | 3873, 2516, 3858, 2549, 3885 ms | 3814, 2493, 3850, 2517, 3762 ms |

\* the first runs of a binary Windows had not seen: Microsoft Defender
scanning new content and Qt's DLLs coming from a cold disk cache. Every
rebuild of the dev build pays it once; so does a downloaded build, once.
The ~1.3 s that alternated between runs was later traced (2026-10-09) to
the font fallback scan, `Main.qml`'s creation and the device -- the
start-up section above.
