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

