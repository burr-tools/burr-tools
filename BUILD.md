# BurrTools Build Instructions

BurrTools uses the Meson build system. This document describes how to build the project on Linux and how to cross-compile for Windows.

## Prerequisites

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

## Building on Linux

1. Setup the build directory:

```bash
meson setup build
```

2. Compile the project:

```bash
ninja -C build
```

3. The executables will be created in the `build/` directory:
   - `build/burrtools` - Main GUI application
   - `build/burrTxt` - Command-line tool
   - `build/burrTxt2` - Command-line tool

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
- C++ standard: C++11
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

