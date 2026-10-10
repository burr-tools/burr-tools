# BurrTools Build Instructions

BurrTools uses the Meson build system. This document describes how to build the project on Linux and macOS, and how to cross-compile for Windows.

## Prerequisites

### macOS

Install the required build dependencies using [Homebrew](https://brew.sh/):

```bash
brew install meson ninja cmake
```

- **Xcode Command Line Tools**: If not already installed, run `xcode-select --install` to obtain Apple Clang (with C++20 support) and Git.
- **CMake**: Required by Meson because FLTK is built from source as a CMake subproject.
- **Libraries**: Dependencies (FLTK 1.4, Catch2, libpng, zlib) are automatically fetched and built as subprojects, and linked against native macOS frameworks (`Cocoa`, `OpenGL`, `ScreenCaptureKit`, `UniformTypeIdentifiers`).
- **Minimum macOS**: the binaries run on macOS 14 Sonoma and later (`-Dmacos_deployment_target=14.0`, the default).

#### Optional Static Analysis Tools (macOS)

To run the static analysis and code quality checks (`just check` and `just check-tidy`):

```bash
brew install cppcheck llvm just
```

### Linux

Install the required dependencies:

```bash
sudo apt-get update
sudo apt-get install -y meson ninja-build build-essential \
    libboost-all-dev libgl-dev libglu1-mesa-dev freeglut3-dev \
    libfltk1.3-dev libpng-dev zlib1g-dev
```

### Windows Cross-Compilation (from Linux)

Install the MinGW cross-compiler:

```bash
sudo apt-get install -y mingw-w64
```

## Building on Linux and macOS

1. Setup the build directory:

```bash
meson setup build
```

2. Compile the project:

```bash
ninja -C build
```

3. The executables and modules will be created in the `build/` directory:
   - `build/burrtools` - Main GUI application
   - `build/burrTxt` - Command-line tool
   - `build/burrTxt2` - Command-line tool
   - `build/burrtools*.so` - Python wrapper extension module

## Cross-Compiling for Windows

A cross-compilation configuration file `cross-mingw64.txt` is provided for building Windows binaries from Linux.

1. Setup the build directory for Windows:

```bash
meson setup build-win --cross-file cross-mingw64.txt
```

2. Compile the project:

```bash
ninja -C build-win
```

3. The Windows executables will be created in the `build-win/` directory:
   - `build-win/burrtools.exe` - Main GUI application
   - `build-win/burrTxt.exe` - Command-line tool
   - `build-win/burrTxt2.exe` - Command-line tool

## Python Wrapper

BurrTools includes a Python C++ extension module that provides an iterator for streaming solutions as they are found.

### Running Python Tests
```bash
just test-py
```
Or directly:
```bash
PYTHONPATH=build python3 -m unittest discover -s test/python -v
```

### Usage Examples

#### 1. Loading and Solving an Existing Puzzle
```python
import burrtools

# Load puzzle from file
puzzle = burrtools.load("examples/PelikanBurr.xmpuzzle")
problem = puzzle.problems[0]

# Stream solutions via iterator
for sol in problem.solve(disassemble=True):
    print(f"Solution #{sol.solution_number}: level {sol.level}, moves: {sol.moves_text}")
    for p in sol.placements:
        if p.is_placed:
            print(f"  piece {p.piece_id} at ({p.x}, {p.y}, {p.z}) rot {p.transformation}")
```

#### 2. Creating and Solving a Puzzle Entirely in Python
```python
import burrtools

# Create an empty 3D cubic grid puzzle
puzzle = burrtools.Puzzle()
puzzle.comment = "2x2x2 cube from two 1x2x2 blocks"

# Define target result shape: 2x2x2 cube
target = puzzle.add_shape(2, 2, 2, name="cube")
target.fill([(x, y, z) for x in range(2) for y in range(2) for z in range(2)])

# Define piece shape: 1x2x2 slab
piece = puzzle.add_shape(1, 2, 2, name="slab")
piece.fill([(0, y, z) for y in range(2) for z in range(2)])

# Create problem and set piece constraints
problem = puzzle.add_problem(name="assemble cube")
problem.set_result(target)
problem.set_piece_count(piece, 2)

# Solve in-memory without saving to file!
for sol in problem.solve(disassemble=False):
    print(f"Assembly found with {len(sol.placements)} pieces")

# Optional: save to .xmpuzzle file
puzzle.save("my_puzzle.xmpuzzle")
```

## Cleaning Build Directories

To clean and rebuild from scratch:

```bash
rm -rf build
meson setup build
ninja -C build
```

For Windows builds:

```bash
rm -rf build-win
meson setup build-win --cross-file cross-mingw64.txt
ninja -C build-win
```

## Build Configuration

The project is configured to use:
- C standard: C11
- C++ standard: C++20
- Build type: Debug (by default)

To change the build type to release:

```bash
meson setup build --buildtype=release
```

Or for Windows:

```bash
meson setup build-win --cross-file cross-mingw64.txt --buildtype=release
```

## Task Automation & Static Code Checking

A [`justfile`](justfile) is provided for common build, testing, and static analysis workflows using [`just`](https://github.com/casey/just):

```bash
just               # Show available recipes (default)
just build         # Build binaries
just check         # Run fast static analysis (cppcheck) on src/
just check-tidy    # Run clang-tidy across src/
just check-scan    # Run Clang Static Analyzer (scan-build)
just check-analyzer# Build with GCC -fanalyzer
just check-all     # Run both cppcheck and clang-tidy
just test          # Run tests
just clean         # Clean build artifacts
```

### Runtime Sanitizers
To build with memory or thread sanitizers:
```bash
just build-asan    # AddressSanitizer & UndefinedBehaviorSanitizer
just build-tsan    # ThreadSanitizer (useful for diagnosing solver data races)
```

## Qt GUI (burrtools-qt)

`burrtools-qt` is the redesigned GUI, a preview next to the FLTK
`burrtools`; what it is and how it is built inside is described in
[`src/qtgui/README.md`](src/qtgui/README.md). It is built when Qt is found
(the `qt_gui` meson option is `auto`; `-Dqt_gui=enabled` makes a missing Qt
an error).

**Needs:** Qt ≥ 6.8 with the Declarative (Qt Quick), ShaderTools, Svg and
Tools modules, and meson ≥ 1.7.

| Platform | Getting Qt |
| :--- | :--- |
| Windows | MSYS2 UCRT64: `pacman -S mingw-w64-ucrt-x86_64-qt6-{base,declarative,shadertools,svg,tools} zip` |
| macOS | the Qt online installer or `aqtinstall` (as CI does), whose Qt runs on macOS 13 and later; `brew install qt` works too, but Homebrew builds it for your own macOS, so set `-Dmacos_deployment_target` to that version (the linker warns otherwise) |
| Linux | Distributions shipping Qt ≥ 6.8 (e.g. Ubuntu 25.10+, Debian `forky`/`sid`; check with `apt-cache policy qt6-base-dev`) via apt, else the Qt online installer or `aqtinstall` (e.g. Ubuntu 22.04 ships 6.2, 24.04 ships 6.4), always with `qtshadertools`; the render tests also use `mesa-vulkan-drivers` |

```bash
# Distro Qt ≥ 6.8 only (Debian/Ubuntu):
sudo apt-get install -y qmake6 qt6-base-dev qt6-base-private-dev \
    qt6-declarative-dev qt6-declarative-dev-tools \
    qt6-svg-dev qt6-shadertools-dev \
    qt6-tools-dev qt6-tools-dev-tools \
    mesa-vulkan-drivers libvulkan-dev
# Older distributions: Qt online installer or `pip install aqtinstall`,
# module `qtshadertools` (as CI does with jurplel/install-qt-action).
```

```bash
just build-qt        # configure with -Dqt_gui=enabled and build
just run-qt [file]   # run it, optionally on a puzzle file
just run-gallery     # the component gallery: every primitive in every state
just test-qt         # the self-check and the Qt tests (controllers, renders, QML)
just update-snapshots [rows] # rewrite the gallery reference images that changed (all rows, or e.g. button,switch)
just deploy-qt       # a self-contained build in artifacts/qt (zip / .app / AppImage)
just build-qt-static # Windows: one standalone burrtools-qt.exe (static Qt) in artifacts/qt-static
just qml-aot [--update] # QML bindings left to the JS engine, against test/qtgui/qml_aot_baseline.json
just bench-ui        # micro-benchmarks of src/uicore's hot paths (release build)
just startup-time    # cold and warm start-up times (artifacts/profile)
just profile-qml     # a qmlprofiler trace (a -Dqml_debug=true build of its own, build-prof)
just heap-qt         # a heaptrack profile (Linux)
```

**Standalone Windows program.** `just build-qt-static` (an MSYS2 UCRT64
shell with `mingw-w64-ucrt-x86_64-qt6-static` installed, a 2.8 GB package)
links Qt and the C/C++ runtimes into one `burrtools-qt.exe` of about 50 MB
(20 MB zipped) that needs nothing beside it on Windows 10 and later. It
configures a separate `build-static` directory with `-Dqt_static=true`;
`scripts/qt_static_link.py` works out the static link line and the plugin
imports. CI builds it only on demand and for releases (the *Qt standalone
build* workflow), not on every push.

`burrtools-qt` also takes `--gallery`, `--screenshot=<file.png>` (draw the
window once, save it, quit), `--command=<key>` (run a command, e.g.
`export.stl`, once the window is up) and `--self-check`.

Environment variables:

| Variable | Effect |
| :--- | :--- |
| `BURRTOOLS_QT_SETTINGS` | use this settings file instead of `.burrtools-qt.rc`; the pipeline caches go beside it and Qt's own disk cache is switched off — for scripted runs that must not touch the user's files |
| `BURRTOOLS_RHI` | the 3D view's graphics API: `d3d11`, `d3d12`, `vulkan`, `opengl` or `metal` |
| `BURRTOOLS_OFFSCREEN_RHI` | the same for the image export and the render tests |
| `BURRTOOLS_TEST_QPA` | the platform plugin for `test_qtgui` (default `offscreen`; Linux CI uses `xcb` on Xvfb for Vulkan) |
| `BURRTOOLS_REQUIRE_GPU_TESTS` | render tests fail instead of skipping when no graphics backend can be made |
| `BURRTOOLS_REQUIRE_SNAPSHOTS` | a missing gallery reference image fails instead of skipping |
| `BURRTOOLS_UPDATE_SNAPSHOTS=1` | gallery checks write their grabs as the new references, only where missing or no longer matching |
| `BURRTOOLS_SNAPSHOTS_ONLY` | comma-separated gallery rows (`button,switch`) to check or update; unset: all |
| `BURRTOOLS_PROFILE_PUZZLE` | the puzzle `scripts/profile-qt.sh` opens (default `examples/PelikanBurr.xmpuzzle`) |

Performance checks and profiles (what each measures, which fail CI and which
only report): `design/2026-10-08-qtgui-performance-backlog.md`, "Measuring".
`-Dqml_debug=true` lets qmlprofiler and the QML debugger attach; it is for
profiling builds, never a release. `-Ddxbc` (auto) ships the Direct3D
shaders compiled, sparing the HLSL compile (~50 ms) of a start without a
pipeline cache; it needs the Windows SDK's `fxc`, which an MSYS2 install
alone does not have -- without it the build ships HLSL source, as before
(`meson setup` says which).

The tests run headless. Render tests draw through Direct3D's WARP software
rasteriser on Windows and need no GPU; on Linux they need Vulkan, which
Mesa's lavapipe provides (`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json`
and `BURRTOOLS_TEST_QPA=xcb` under `xvfb-run`, as CI does).

CI builds and tests the Qt GUI on Linux, Windows and macOS and uploads a
preview build of each as the workflow artifact `burrtools-qt-<os>`. The
*Qt standalone build* workflow (`.github/workflows/qt-standalone.yml`) makes
the standalone Windows program: run it by hand from the Actions tab
(optionally naming a release tag to attach to), and it runs by itself for
every `v*.*.*` tag, attaching the program to the release.
