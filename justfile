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

# Run the fast test suite (everything but the stress cases)
test: build
    meson test -C build --suite fast --print-errorlogs

# Run only the slow/stress cases
test-slow: build
    meson test -C build --suite slow --print-errorlogs

# Run every test, fast and slow. This is what CI runs.
# Named suites rather than "everything", so a vendored subproject's own test
# suite (libpng ships one) cannot gate this project's CI.
test-all: build
    meson test -C build --suite fast --suite slow --print-errorlogs

# Run Python wrapper test suite
test-py: build
    PYTHONPATH=build python3 -m unittest discover -s test/python -v

# Run regression test comparing burrTxt and burrTxt2 against known-good 0.7.1 release output
test-regression: build
    python3 test/test_examples_regression.py

# Run fast static analysis (cppcheck) on BurrTools source files
#
# Every suppression here is scoped to third-party or system code. Deliberately
# absent are blanket `--suppress=syntaxError` / `--suppress=unknownMacro`: those
# hide cppcheck *parse* failures anywhere in the tree, so a file cppcheck can no
# longer parse reports zero warnings and sails through the --error-exitcode=1
# gate below -- precisely the failure that gate exists to catch. Should a single
# file ever genuinely need one, use an inline `// cppcheck-suppress <id>` comment
# (--inline-suppr is on) instead of re-adding a global suppression.
#
# Also absent is `--suppress="*:*test*"`, which was a substring glob matching any
# path merely containing "test", not the test directory; `-i test` above already
# excludes that directory.
check-cppcheck: setup
    cppcheck --project=build/compile_commands.json \
             -i subprojects \
             -i src/lua \
             -i test \
             --suppress="*:*subprojects*" \
             --suppress="*:*src/lua*" \
             --suppress="*:*/usr/include/*" \
             --suppress="preprocessorErrorDirective:*python*" \
             --enable=warning,performance,portability \
             --inline-suppr \
             --error-exitcode=1 \
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

# Headless GUI invariant check (menu table consistency)
check-gui: build
    ./build/burrtools --self-check

# Generate the Doxygen API reference into gendoc/html
#
# Two settings are appended to Doxyfile rather than stored in it, because both
# depend on the machine rather than the project: the version stamp comes from
# `git describe`, and HAVE_DOT is switched off when graphviz is absent so the
# docs still build (without diagrams) on a machine that lacks it. Doxygen reads
# its config from stdin when given "-", and later assignments win, so piping
# the file plus the overrides needs no temporary file.
docs:
    #!/usr/bin/env bash
    set -euo pipefail
    if ! command -v doxygen >/dev/null 2>&1; then
        echo "doxygen not found. Install it with 'brew install doxygen graphviz'" >&2
        echo "on macOS, or 'apt-get install doxygen graphviz' on Debian/Ubuntu." >&2
        exit 1
    fi
    version=$(git describe --tags --always --dirty 2>/dev/null || echo unknown)
    if command -v dot >/dev/null 2>&1; then
        have_dot=YES
    else
        have_dot=NO
        echo "graphviz not found; generating without diagrams." >&2
    fi
    # Created up front: doxygen opens WARN_LOGFILE before it creates
    # OUTPUT_DIRECTORY, so the log's directory has to exist already.
    rm -rf gendoc
    mkdir -p gendoc
    # Doxygen reports broken doc comments but still exits 0, so the warning log
    # is captured and checked explicitly. Without this the CI job would go green
    # over a reference full of mangled documentation.
    #
    # The exit status is collected rather than left to `set -e` so that a
    # doxygen that fails outright still gets its warning log printed -- that log
    # usually says why.
    status=0
    {
        cat Doxyfile
        echo "PROJECT_NUMBER = \"$version\""
        echo "HAVE_DOT = $have_dot"
        echo "WARN_LOGFILE = gendoc/doxygen-warnings.log"
    } | doxygen - || status=$?
    if [ -s gendoc/doxygen-warnings.log ]; then
        echo "Doxygen reported warnings:" >&2
        cat gendoc/doxygen-warnings.log >&2
        status=1
    fi
    if [ "$status" -ne 0 ]; then
        exit "$status"
    fi
    echo "Documentation written to gendoc/html/index.html"

