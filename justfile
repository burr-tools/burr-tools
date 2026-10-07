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
# Also absent is `--suppress="*:*test*"`: as a substring glob it matches any path
# merely containing "test", not the test directory; `-i test` above already
# excludes that directory, so don't add it back.
#
# Where Qt is found the build has the Qt GUI: --library=qt teaches cppcheck
# Qt's macros (Q_PROPERTY, QT_CONFIG, ...), `-i build` skips what moc and
# qmlcachegen generate, and the framework glob covers macOS Qt's headers
# (Homebrew's and the official), which /usr/include does not.
check-cppcheck: setup
    cppcheck --project=build/compile_commands.json \
             -i subprojects \
             -i src/lua \
             -i test \
             -i build \
             --library=qt \
             --suppress="*:*subprojects*" \
             --suppress="*:*src/lua*" \
             --suppress="*:*/usr/include/*" \
             --suppress="*:*/Qt*.framework/*" \
             --suppress="preprocessorErrorDirective:*python*" \
             --suppress="preprocessorErrorDirective:*qt6*" \
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

# Shared gcovr options, used by both `coverage` and `coverage-html` so the two
# recipes can never silently drift and report different numbers. Excluding
# build-cov/subprojects skips walking vendored coverage data the filters would
# discard anyway (~44s saved); it must not change the reported TOTAL.
gcovr_base := "--root . " + \
    "--exclude 'src/lua/' " + \
    "--exclude-directories 'build-cov/subprojects' " + \
    ( if os() == "macos" { '--gcov-executable "xcrun llvm-cov gcov"' } else { "" } )

# What is measured, reported as one figure per area. The library is the
# canonical figure compared across pull requests; the redesigned GUI's two
# layers stand beside it, so adding them never shifts the library's number.
# src/qtgui counts its C++ only: QML is not compiled code gcov sees.
gcovr_lib := "--filter 'src/lib/' --filter 'src/tools/' --filter 'src/halfedge/'"
gcovr_uicore := "--filter 'src/uicore/'"
gcovr_qtgui := "--filter 'src/qtgui/'"

# Build the coverage build and run the suites that feed it: test_burrtools
# (the library and, as its [ui] cases, src/uicore), then the Qt GUI's suites
# when the coverage build has the Qt GUI (Qt >= 6.8 found). Headless Linux
# needs the render tests' platform set up as the qt-linux CI job does.
_coverage-tests: setup-cov
    #!/usr/bin/env bash
    set -euo pipefail
    ninja -C build-cov
    ./build-cov/test_burrtools
    if [ -d build-cov/test/qtgui ]; then
        meson test -C build-cov --print-errorlogs qtgui qtgui_qml qtgui_gallery_150 qtgui_gallery_200
    fi

# One gcov pass into a JSON tracefile (and an HTML report of every area when
# html names its index file), then a summary per area read back from it
_coverage-summary html="":
    #!/usr/bin/env bash
    set -euo pipefail
    html_args=()
    if [ -n "{{ html }}" ]; then
        mkdir -p "$(dirname "{{ html }}")"
        html_args=(--html-details "{{ html }}")
    fi
    gcovr {{ gcovr_base }} {{ gcovr_lib }} {{ gcovr_uicore }} {{ gcovr_qtgui }} \
        --json build-cov/coverage.json ${html_args[@]+"${html_args[@]}"} build-cov
    area() {
        echo "== $1"     # the CI report picks these lines out of the build's output
        shift
        # the summary is what is wanted; the full text report goes to a file
        # (gcovr refuses /dev/null as an output)
        gcovr --root . --add-tracefile build-cov/coverage.json "$@" --print-summary --output build-cov/coverage-area.txt
    }
    area "library: src/lib, src/tools, src/halfedge -- compared across pull requests" {{ gcovr_lib }}
    area "UI core: src/uicore" {{ gcovr_uicore }}
    if [ -d build-cov/test/qtgui ]; then
        area "Qt GUI: src/qtgui, C++ only" {{ gcovr_qtgui }}
    fi

# Report test coverage per area (library, UI core, Qt GUI)
coverage: _coverage-tests _coverage-summary

# Per-area coverage summary plus an HTML report of every area in coverage-html/
coverage-html: _coverage-tests (_coverage-summary "coverage-html/index.html")

# Run the single-commit snapshot benchmark over the fixed puzzle corpus
# (bench/run_snapshot.sh); extra args are forwarded, e.g. `just bench --runs 5`
# Always measures the release+ndebug binary (dev-build assertion overhead
# would pollute every snapshot), then prints a comparison against the
# previous snapshot so one command shows the change's impact.
bench *args: build-release
    ./bench/run_snapshot.sh --binary build-rel/burrTxt {{args}}

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

# Build an optimized release binary (assertions off) for benchmarking.
# The default `build` dir carries _GLIBCXX_ASSERTIONS and live bt_assert
# checks, which cost ~15-30% solver time in the exact-cover hot loops
# (see design/2026-09-24-benchmark-speedup-analysis.md). Benchmarks must
# use this binary, never the dev build.
# --werror is deliberate: this mirrors the CI ship jobs, so warnings that
# would fail a PR (including NDEBUG-gated ones invisible to `just build`)
# fail here first.
build-release:
    @if [ ! -d "build-rel" ]; then meson setup build-rel --buildtype=release -Db_ndebug=true --werror; else meson configure build-rel --buildtype=release -Db_ndebug=true -Dwerror=true; fi
    ninja -C build-rel

# Run the test suites against the release binary (the configuration CI
# ships and gates on). Required before pushing: the dev build neither
# treats warnings as errors nor disables assertions, so `just test-all`
# alone cannot catch what CI will fail on.
test-release:
    meson test -C build-rel --suite fast --suite slow --print-errorlogs

# Headless GUI invariant check (menu table consistency)
check-gui: build
    ./build/burrtools --self-check

# --- burrtools-qt, the redesigned GUI (src/qtgui) ---------------------------
# Built by `just build` whenever Qt >= 6.8 and meson >= 1.7 are installed
# (-Dqt_gui=auto); these recipes insist on it and fail clearly when it is not.

# Configure the build directory with the Qt GUI required, then build
build-qt: setup
    meson configure build -Dqt_gui=enabled
    ninja -C build

# Run burrtools-qt, optionally with a puzzle file: just run-qt examples/PelikanBurr.xmpuzzle
run-qt *args: build-qt
    ./build/src/qtgui/burrtools-qt {{args}}

# Open the component gallery: every primitive in every state (light/dark toggle)
run-gallery: build-qt
    ./build/src/qtgui/burrtools-qt --gallery

# Run only the Qt GUI tests (controllers, QML shell, gallery at each dp ratio)
# plus its self-check
test-qt: build-qt
    ./build/src/qtgui/burrtools-qt --self-check
    meson test -C build --print-errorlogs qtgui qtgui_qml qtgui_gallery_150 qtgui_gallery_200

# Rewrite changed gallery references (test/qtgui/snapshots/<os>), all rows or e.g. `button,switch`
update-snapshots rows="": build-qt
    BURRTOOLS_UPDATE_SNAPSHOTS=1 BURRTOOLS_SNAPSHOTS_ONLY="{{rows}}" meson test -C build --print-errorlogs qtgui_qml qtgui_gallery_150 qtgui_gallery_200

# Self-contained burrtools-qt preview in artifacts/qt: zip (Windows), .app (macOS), AppImage (Linux); as CI
deploy-qt:
    @if [ ! -d "build-rel" ]; then meson setup build-rel --buildtype=release -Db_ndebug=true -Dqt_gui=enabled; else meson configure build-rel -Dqt_gui=enabled; fi
    ninja -C build-rel src/qtgui/burrtools-qt
    bash scripts/package-qt.sh build-rel artifacts/qt

# Standalone burrtools-qt.exe with static Qt in artifacts/qt-static (MSYS2 qt6-static; as qt-standalone.yml)
build-qt-static:
    bash scripts/build-qt-static.sh build-static artifacts/qt-static

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

