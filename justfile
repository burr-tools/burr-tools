# BurrTools Task Runner (just)

# Default recipe: list available recipes
default: help

# List all available recipes
help:
    @just --list

# Configure Meson build directory if not already set up
setup:
    @if [ ! -d "build" ]; then meson setup build; fi

# Reconfigure existing Meson build directory
reconfigure:
    meson setup --reconfigure build

# Build BurrTools binaries with Ninja
build: setup
    ninja -C build

# Clean build artifacts
clean:
    ninja -C build clean

# Delete build directory and rebuild from scratch
rebuild:
    rm -rf build
    meson setup build
    ninja -C build

# Run test suite
test: build
    ninja -C build test

# Run fast static analysis (cppcheck) on BurrTools source files
check-cppcheck: setup
    cppcheck --project=build/compile_commands.json \
             -i subprojects \
             --suppress="*:*subprojects*" \
             --enable=warning,performance,portability \
             --inline-suppr \
             --quiet \
             -j$(getconf _NPROCESSORS_ONLN)

# Run clang-tidy across BurrTools source files (excluding subprojects and lua)
check-tidy pattern="burr-tools/src/(?!lua/).*": setup
    run-clang-tidy -p build "{{pattern}}"

# Run Clang Static Analyzer (scan-build) and output report
check-scan: setup
    ninja -C build scan-build

# Build with GCC -fanalyzer static analysis enabled
check-analyzer:
    @if [ ! -d "build-analyzer" ]; then meson setup build-analyzer -Dcpp_args="-fanalyzer" -Dc_args="-fanalyzer"; fi
    ninja -C build-analyzer

# Run default static check (cppcheck)
check: check-cppcheck

# Run full static check suite (cppcheck + clang-tidy)
check-all: check-cppcheck check-tidy

# Configure the coverage build directory if not already set up
setup-cov:
    @if [ ! -d "build-cov" ]; then meson setup build-cov -Db_coverage=true; fi

# Shared gcovr filters, used by both `coverage` and `coverage-html` so the two
# recipes can never silently drift and report different numbers. Excluding
# build-cov/subprojects skips walking vendored coverage data the filters would
# discard anyway (~44s saved); it must not change the reported TOTAL.
gcovr_flags := "--root . " + \
    "--filter 'src/lib/' --filter 'src/tools/' --filter 'src/halfedge/' " + \
    "--exclude 'src/lua/' " + \
    "--exclude-directories 'build-cov/subprojects' " + \
    ( if os() == "macos" { '--gcov-executable "xcrun llvm-cov gcov"' } else { "" } )

# Report test coverage for BurrTools sources (excludes subprojects and lua)
coverage: setup-cov
    ninja -C build-cov
    ./build-cov/test_burrtools
    gcovr {{ gcovr_flags }} --print-summary build-cov

# Write an HTML coverage report to coverage-html/index.html
coverage-html: setup-cov
    ninja -C build-cov
    ./build-cov/test_burrtools
    mkdir -p coverage-html
    gcovr {{ gcovr_flags }} --print-summary --html-details coverage-html/index.html

# Build with AddressSanitizer and UndefinedBehaviorSanitizer
build-asan:
    @if [ ! -d "build-asan" ]; then meson setup build-asan -Db_sanitize=address,undefined; fi
    ninja -C build-asan

# Build with ThreadSanitizer (detect concurrency data races)
build-tsan:
    @if [ ! -d "build-tsan" ]; then meson setup build-tsan -Db_sanitize=thread; fi
    ninja -C build-tsan

# Cross-compile for Windows using MinGW
build-win:
    @if [ ! -d "build-win" ]; then meson setup build-win --cross-file cross-mingw64.txt; fi
    ninja -C build-win

# Build with warnings treated as errors (excluding vendored code and subprojects)
build-werror:
    @if [ ! -d "build-werror" ]; then meson setup build-werror --werror; fi
    ninja -C build-werror

