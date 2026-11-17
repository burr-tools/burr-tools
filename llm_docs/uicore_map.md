# uicore: map of `src/uicore/`

| File | What it decides |
| :--- | :--- |
| `commands` | **the command table**: every menu item and shortcut once — key, label (`&` access key), menu, divider, shortcuts per platform, the legacy callback it replaces, the macOS/Windows label and menu differences |
| `layoutmodel` | workspaces, card collapse per workspace, Focus 2D / 3D (the editor's share, `kFocus2dEditorShare`), the column widths for a window width and density |
| `documentsession` | open, save (beside the file, then renamed over it), import, new, the modified state, undo / redo; a voxel edit drops the solutions of the problems using the shape into its undo step (`editVoxels`, strokes), a change to what a problem asks for its own (`editProblems`) |
| `drawcolours` | the puzzle's colours as the Drawing colour footer edits them: names, add (up to the library's 63), edit, remove (and whether it loses solutions) |
| `shapecolours` | what a shape is shown in: Classic (the `palette` table) or Muted, per theme; not compiled into legacy |
| `puzzlehistory` | the undo history (snapshots, stroke batching, depth limit, the Puzzle selection around a step; the solve records a step dropped, swapped back by its undo, to one memory budget for the process; problems a step did not change are left alone, solutions and all) — **shared with the legacy GUI** |
| `settingsstore` | the settings file: `key = value` lines, typed reads with defaults, an atomic save |
| `camera`, `viewcube`, `vecmath` | the 3D view's camera (orbit, pan, zoom, eased animations, projections) and the view cube's faces, edges, corners and hit testing |
| `scenemesh` | the vertices the 3D view draws for a shape, per voxel style (Flat, Classic, STL; STL faces cut at the cells' walls, so they dim by layer), each vertex carrying its voxel; ghosts, a Transform button's result and hint; picking meshes and ray picks |
| `assemblyscene`, `depthsort` | whole puzzles and assemblies as scenes (image export); back-to-front order for translucent layers |
| `palette` | the shape colours, and black or white text on them (WCAG contrast) — the colours **shared with the legacy GUI** |
| `imagepages`, `vectorexport` | the image export's page layout; vector export (PS, EPS, TeX, PDF, SVG, PGF); a shape's small picture for lists (`shapePicture`) |
| `puzzletools` | the status table's checks, convert, import assemblies |
| `problemlist` | the Puzzle workspace's operations: the problems list, what a problem asks for (its kind, goal slots, the right card's sections), the result, counts and ranges, groups, colour rules, the totals and the derived holes, whether a change costs solutions |
| `puzzlescene` | the Puzzle 3D scene: the goal zone and the pieces' runs (a shape's copies side by side, groups first) and where they lie, the selected piece's emphasis, one mesh with each object around its own centre, its place carried in `cell` |
| `shapelist` | the Shapes list's operations: new, duplicate, delete (and whether it loses solutions), rename, weight, reorder; positional ids |
| `voxeledit` | the voxel editor's rules: tools 1–4 and the modifiers' quick tools, Mirror and Span, the 2D planes and layers, what a tool does to a voxel, the 2D cell size, grid size (steps, apply to all, the blocked shrink), Area mode's cells, where a 3D click acts (`target3d`) |
| `shapeops` | the inspector's operations per voxel type (legacy's tool tabs): Transform, Fit & scale, Repair & surface, whether each may run and why not, the Transform buttons' descriptors |
| `gridcells` | the 2D grid's cells per voxel type (squares, prism triangles, sphere circles, rhombic and tetra-octa super-cells) and hit testing |
| `vocabulary` | the settings' and modes' fixed vocabularies (theme, density, voxel style, projection, colour view, image mode, …): one table of values and words each |
| `windowplacement` | where the window opens: the saved place if still on a screen, else centred |
| `xcursor` | the desktop's cursor theme on Linux and the BSDs: which theme and size (environment, KDE's and GTK's settings), where its files are (inherited themes too), and an Xcursor file's image nearest a size |

Tests: `test/test_ui_<name>.cpp`, tagged `[ui]`, compiled into
`test_burrtools` (`uicore_test_src` in the root `meson.build`); benchmarks in
`test/bench_ui.cpp` (`[bench]`, hidden); `test/alloccount.h` counts
allocations for "no allocation per …" checks.
