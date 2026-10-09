# burrtools-qt — Performance and Maintainability Backlog

**Date:** 2026-10-08
**Status:** Backlog. P1 is done (commit `956841cc`), and so is the cheap
half of P2.1 (the sort keeps its storage and runs only when the view turns);
P2.4, P2.5 and P2.7 are done (2026-10-09); the checks under "Measuring"
exist. The rest of P2 (P2.1's cheaper sort, P2.2, P2.3, P2.6) and P3 is
open.
**Scope:** `src/qtgui`, `src/uicore`, the QML module `BurrTools.Ui`.

## Where this comes from

A structural, performance and memory review of the first cut of the Qt GUI
(PR #140, issue #139). It read the render path (`scenerenderer.cpp`,
`voxelviewport.cpp`), frame construction (`scenecontroller.cpp`,
`viewportcontroller.cpp`), mesh building (`uicore/scenemesh.cpp`), the
controllers' signal wiring and the QML. **It was a code reading, not a
profiled run**: the sizes and timings below are worked out from the code and
are estimates until measured (see "Measuring" at the end).

The layering is sound — toolkit-free logic in `src/uicore`, Qt adapters in
`src/qtgui`, an immutable mesh shared with the render thread, RAII-owned QRhi
resources — so every item here is local; none needs a redesign.

## Done in P1 (for reference)

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

## P2 — next

### P2.1 Re-sort translucent triangles only when the view turns

*Partly done:* `btui::DepthSorter` ([depthsort.h](../src/uicore/depthsort.h))
keeps its storage and the renderer sorts only when the view's rotation
changes (`btui::sameRotation`); a frame allocates nothing per triangle
(`TestRender::framesDoNotAllocatePerTriangle`). Open: a cheaper sort for
small turns (below).

- **Where:** `SceneRenderer::render`, [scenerenderer.cpp](../src/qtgui/scenerenderer.cpp) — the
  `layers && (!sorted || memcmp(view…))` block.
- **Problem:** the back-to-front sort of every translucent triangle runs on
  any change of the view matrix — panning and zooming included, which do not
  change the order — and allocates two vectors per sort on the render thread.
  A 20³ variable core is ~80k triangles: an estimated 4–6 ms per frame while
  orbiting.
- **Approach:** compare only the view's 3×3 rotation (the order is by depth
  along the view direction, which translation does not change); keep `order`
  and `idx` as members so their capacity is reused; sort the previous order
  again (insertion sort is near-linear when the view turned a little), or sort
  per voxel cell (six times fewer keys) when the cell is the unit that cannot
  interpenetrate.
- **Verify:** a Catch2 benchmark of the sort on a synthetic 80k-triangle
  list (see Measuring); the render tests' translucency cases
  (`innerVariableVoxelsShowThroughTheOuterOnes`) must still pass.

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
  a benchmark of `buildShapeMesh` at 8³, 20³ and 50³.

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

*Done:* `SceneController::readSettings` keeps lighting, the voxel style (an
enum) and the anti-aliasing samples in members, refreshed on
`SettingsController::changed`; `ViewportController` keeps the four Display
options (`m_display`), refreshed with them and set by its own setters.
`frame()` and `wantedSamples()` read only members. Gate:
`TestViewportController::aFrameReadsNoSettings` (no allocation while
building a frame with nothing but the mesh to draw; each string-keyed read
allocated a `std::string` -- counted on Linux and macOS, where the test's
`operator new` also serves the standard library's strings; MinGW's shared
libstdc++ allocates them inside its DLL, past the count).


- **Where:** `ViewportController::frame`, `boolSetting`,
  [viewportcontroller.cpp](../src/qtgui/viewportcontroller.cpp).
- **Problem:** every frame re-reads four or five display options by string
  key (QString → std::string → map → back) and compares the voxel-style
  string, while the render thread waits in `synchronize()`.
- **Approach:** cache the display options in members, refreshed on
  `SettingsController::changed`; an enum for the voxel style.
- **Verify:** existing viewport tests (display options).

### P2.5 Type the QML — done

*Done:* bindings left to the JavaScript engine (Qt 6.11) **1936 → 398**:

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
file alone (~8 s), as before. `qtgui_qml_aot` and `just qml-aot` pass the
same `--import`.

One behaviour change compiling exposed: a binding that read a property
only for its dependency (`{ root.stl.revision; return … }`) lost the
dependency once compiled -- the bare read is dropped. It now uses the value
(`revision >= 0 ? … : …`), as `CommandMenu.qml` already did.

What is left is mostly by design: field reads on JavaScript objects and
maps (`modelData` of the menus, Settings rows and shortcut tables, built in
C++ as `QVariantMap`s), `App.settings[row.prop]` in the Settings rows, and
calls into JavaScript built-ins.


- **Where:** 44 `property var` in `src/qtgui/qml` (9 in `SettingsDialog.qml`).
- **Problem:** untyped properties keep `qmlcachegen` from compiling the
  bindings that use them ahead of time; they fall back to the JS engine.
- **Approach:** concrete types (`list<…>`, `QtObject` types, value types);
  run `qmllint --compiler` (or read `qmlcachegen`'s warnings) to list the
  bindings still interpreted, and bring the count down.
- **Found while adding that count:** the build's qmlcachegen cannot import
  the module's own types. meson's `qt.qml_module` writes `BurrTools_Ui_qmldir`
  and `BurrTools_Ui.qmltypes` flat in the build directory, not in an
  importable `BurrTools/Ui/` tree, and passes qmlcachegen no `-I`; it warns
  "Failed to import BurrTools.Ui" and compiles without knowing `App`, `Theme`
  or the controllers. Given such a tree, `SettingsDialog.qml` alone goes from
  48 unqualified lookups to 11 and from 175 uncompiled bindings to 147.
  Fix: an import tree in the build directory (qmldir, qmltypes, the QML files
  or links), generated before the cache step, and
  `qmlcachegen_extra_arguments: ['-I', <its root>]`, with the cache step
  depending on the type registrar's output.
- **Verify:** `qtgui_qml_aot` (see Measuring): the baseline goes down.

### P2.6 Image export off the GUI thread

- **Where:** `ImageExportController::writePages`, [imageexportcontroller.cpp](../src/qtgui/imageexportcontroller.cpp).
- **Problem:** all pages are composed and PNG-encoded in one go on the GUI
  thread, every picture held until then; a large multi-page export freezes
  the window.
- **Approach:** compose and encode each page with `QtConcurrent::run` as soon
  as its pictures are drawn; release pictures once placed.
- **Verify:** `TestImageExport` (pages, cancel, unwritable place).

### P2.7 Precompiled Direct3D shaders (start-up) — done

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
update). *Done:* `-Ddxbc` (a feature, auto) -- on Windows the build looks
for the Windows SDK's `fxc` (on the PATH, else the newest SDK under
`Windows Kits/10/bin`) and runs `qsb -c`, storing DXBC instead of HLSL;
without it, HLSL source as before (`meson setup` prints which). The Windows
Qt CI job builds with `-Ddxbc=enabled`, so its build has DXBC and its D3D11
(WARP) render tests draw with it. Gate:
`TestRender::theShadersCarryEveryGraphicsApi` (DXBC exactly when the build
says so). Qt Quick's own shaders stay as the Qt build packaged them.


- **Where:** the `qsb` call, [src/qtgui/meson.build](../src/qtgui/meson.build).
- **Problem:** shaders are packaged for Direct3D as HLSL source
  (`--hlsl 50`), so D3D compiles them when the pipelines are made; only the
  pipeline cache spares later starts, and any shader change (or a fresh
  install) misses it.
- **Measured:** on a fast GPU (RTX 3080 Ti, D3D11) a cold start (no pipeline
  cache) and a warm one take the same time to within noise — see "Start-up"
  below — so this matters on slow GPUs and WARP, if anywhere. Measure there
  before doing it.
- **Approach:** `qsb -c` compiles HLSL to DXBC at build time; it needs
  `fxc.exe` (Windows SDK), which an MSYS2 build does not have — so either a
  build step that finds `fxc`, or prebuilt `.qsb` files regenerated when a
  shader changes.
- **Verify:** start-up time cold (no pipeline cache) before and after.

### Start-up (measured 2026-10-08)

Windows 11, RTX 3080 Ti, the dev build, `burrtools-qt PelikanBurr.xmpuzzle
--screenshot` (which waits a fixed 1.5 s before quitting), wall time:

| Build | Cold (fresh settings, no pipeline cache) | Warm |
| :--- | :--- | :--- |
| before P1 (`3494281b`) | 7848\*, 5461\*, 2510, 3877, 2513 ms | 2544, 3791, 2535, 3864, 2503 ms |
| after P1 (`956841cc`) | 3873, 2516, 3858, 2549, 3885 ms | 3814, 2493, 3850, 2517, 3762 ms |

\* the first runs of a binary Windows had not seen. P1 changed nothing
measurable. The one-off cost is the **first launch of a newly built
executable** — about 4–5 s more, most likely Microsoft Defender scanning new
content and Qt's DLLs coming from a cold disk cache; a copy of a known
binary under a new name costs only ~0.3 s more on its first run. Every
rebuild of the dev build pays it once; so does a downloaded preview, once.

Open question: runs alternate between ~2.5 s and ~3.8 s, in both builds, cold
and warm alike — about 1.3 s that is not the shader compiler. Worth a trace
(`qmlprofiler`, or `QT_LOGGING_RULES=qt.scenegraph.time.*`) before P2.7.

## P3 — later

| Item | Where | Problem | Approach |
| :--- | :--- | :--- | :--- |
| Indexed, smaller vertices | `uicore/scenemesh`, `scenerenderer` | Plain triangles at 56 B per vertex: ~336 B per square face. | An index buffer (−26 %); `Half`/`UNormByte` formats for normal, cell and edge (56 → ~40 B). |
| Meshes built off the GUI thread | `ViewportController::buildMesh` | Large shapes block the window while building. | `QtConcurrent` with the revision as the result's key; the view keeps the last mesh meanwhile. |
| Finer change signals | `SettingsController`, `Theme` | One `changed` for 15+ settings and the whole theme: every binding re-evaluates on any change. | A signal per property (or per group: view, theme, general). |
| A command model | `CommandMenu.qml`, `CommandController` | Every menu item's enabled/checked binds to one global revision: any change re-evaluates ~80 bindings. | A `QAbstractListModel` of commands (or one QObject per command) with its own change signals. |
| Camera by composition | `SceneController`, `ViewportController`, `ImageExportController` | Inheritance to share a camera; vector export lives in the viewport controller. | A camera/gesture object both hold; export in its own controller. |
| No hidden singletons | 17 `App::instance()` / `Theme::instance()` | Hidden dependencies; tests share state. | Pass them in at construction. |
| Settings keys in one place | `viewportcontroller.cpp` and others | `"view.display.axes"` etc. scattered as strings. | One header of keys (or typed accessors on `SettingsController`). |
| Undo memory | `puzzleHistory_c` | Each edit stores a copy of the edited shape: 500 steps on a 100³ shape ≈ 500 MB worst case. | Say so in the Undo depth setting; store run-length diffs for large shapes. |
| Small cleanups | `SceneRenderer::release` (raw `delete`), per-frame `std::vector<LineVertex>` | Style; an allocation per frame. | `d = std::make_unique<Impl>()`; keep the line buffer as a member. |

## Measuring

All headless (offscreen, or Xvfb with lavapipe on Linux), no external
service. Gates fail CI; reports never do.

| Check | Where | Runs | Gate or report |
| :--- | :--- | :--- | :--- |
| Work counts | `anEditKeepsTheRowsAndBuildsTheSceneOnce`, `theTargetsOutgrowTheViewInSteps`, `theVoxelStyleSettingRebuildsTheMesh` (Qt Test) | every Qt job | **gate** |
| Graphics memory of the scene targets | `TestRender::theTargetsMemoryStaysInBudget`: bytes from size × format × samples (`SceneRenderer::targetBytes`); a 2048×1920 view at 4× under 256 MB, 8× near double | every Qt job | **gate** |
| Allocations per frame | `TestRender::framesDoNotAllocatePerTriangle` (orbiting a 12³ variable cube, ~20k translucent triangles: 0 B a frame from our code; the old sort took 414 720 B) and `[depthsort][alloc]` (Catch2), through a counting `operator new` ([test/alloccount.h](../test/alloccount.h)) | every Qt job; `just test` | **gate** |
| Bindings left to the JS engine | `qtgui_qml_aot` meson test: `qmlcachegen --verbose -I <the import tree>` per QML file as the build runs it, against [test/qtgui/qml_aot_baseline.json](../test/qtgui/qml_aot_baseline.json) (Qt 6.11: 398, was 1936 before P2.5); `just qml-aot [--update]` | every Qt job (compared where the Qt matches the baseline's, reported elsewhere) | **gate** (may only go down) |
| Sanitizers | `sanitizers` job: ASan + UBSan build; `test_burrtools` with leak checks, the Qt suites without (Qt and Mesa are not instrumented) | every push | **gate** |
| Micro-benchmarks | [test/bench_ui.cpp](../test/bench_ui.cpp), hidden `[bench]`: mesh building (8³, 20³, 20³ shell with a variable core, 50³), the depth sort (80k), vector export; `just bench-ui` | `build-linux` (run summary), Qt profile workflow | report |
| Start-up time | `scripts/profile-qt.sh … startup`, cold and warm; `just startup-time` | Qt profile workflow | report |
| QML profile | `scripts/profile-qt.sh … qml` (qmlprofiler; a `-Dqml_debug=true` build); `just profile-qml` | Qt profile workflow (artifact `qt-profile`) | report |
| Heap profile | `scripts/profile-qt.sh … heap` (heaptrack, Linux); `just heap-qt` | Qt profile workflow | report |
| GPU timing | RenderDoc, PIX, Xcode | **no** — needs a real GPU and a person | local only |

Not done: `QRhi::statistics()` (the allocator's view of graphics memory) is
only filled on Vulkan and D3D12; the computed bytes cover what the renderer
owns on every backend.
