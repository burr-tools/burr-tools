**Test suite timings.** The test recipes build first, so what you wait for is
compilation plus test execution. Test execution alone is about 1.6s for `just
test` and about 8.9s for `just test-all`; the difference is almost entirely the
one Minkowski random-shapes stress case. Compilation is extra and can dominate:
re-running with nothing changed is 1.6s against 8.9s, editing a single file is
about 3.4s against 10.0s, and editing a widely-included header is about 13.6s
against 21.0s. Use `just test` while iterating; what else to run before
calling a task done is in `llm_docs/verification.md`.

`just docs` requires `doxygen` and `graphviz` (`brew install doxygen graphviz` on macOS,
`apt-get install doxygen graphviz` on Linux). Without graphviz it still produces a complete
site, minus the diagrams. It fails on any doxygen warning, so a stale `@param` or a broken
`\ref` is a build error, not a silently mangled page; CI runs the same recipe and publishes
the result from `master` to https://burr-tools.github.io/burr-tools/.

Coverage requires `gcovr` (`brew install gcovr` on macOS, `apt-get install gcovr` on Linux).
On macOS the recipes pass `--gcov-executable "xcrun llvm-cov gcov"` automatically, because
Apple Clang emits coverage data that plain `gcov` cannot parse.

**Coverage is reported per area.** The library (`src/lib`, `src/tools`, `src/halfedge`)
is the figure compared across PRs; `src/uicore` (Catch2 `[ui]` cases) and `src/qtgui`
(the Qt suites, C++ only — gcov does not see QML) are reported beside it. When Qt ≥ 6.8
is found, the coverage build includes the Qt GUI and runs its suites too. The library
figure comes from the non-`[ui]` cases alone, captured before the GUI suites run, so it
does not depend on whether Qt was found. CI measures in two jobs: `coverage` builds
without Qt (`-Dqt_gui=disabled`) for the library and `src/uicore`, `coverage-qt` builds
the Qt GUI for `src/qtgui`, and `coverage-report` comments both; a broken Qt install
costs only the Qt GUI's figure.

**First `just coverage` run is slow.** It configures a fresh `build-cov` directory and
compiles all subprojects under instrumentation from scratch — expect several minutes,
not the sub-second/few-second times above. Subsequent runs are incremental and much
faster.

**Coverage numbers are toolchain-specific.** The line/branch denominator itself is
compiler-dependent, so the macOS/Apple Clang percentage and the Linux/gcc CI percentage
will differ. The Linux CI number is the canonical one for comparing coverage across PRs.

For running filtered or verbose test cases directly:
```bash
./build/test_burrtools "[solver]"             # Run all solver tests
./build/test_burrtools "Pelikan Burr*"        # Run specific puzzle test case
./build/test_burrtools -s                     # Run with verbose assertion output
```

For sanitizer builds when diagnosing memory corruption or concurrency issues:
```bash
just test-asan      # AddressSanitizer & UndefinedBehaviorSanitizer over test_burrtools (when to run: verification.md)
just test-asan-qt   # the same over the Qt GUI's suites
just build-tsan     # ThreadSanitizer (critical for solver data races)
```
