# AGENTS.md — BurrTools Guidelines for AI Coding Assistants

This repository contains **BurrTools**, an application for designing, solving, and analyzing 3D interlocking burr puzzles. It is primarily written in C++20 using the FLTK GUI library, OpenGL, and the Meson build system.

---

## 1. Build System & Common Commands

Always use [`just`](justfile) to execute build, test, and quality control tasks. Do not invoke ad-hoc build steps when a target exists in the `justfile`.

```bash
just                # Show available recipes (default)
just build          # Compile BurrTools binaries (build/burrtools, build/burrTxt, build/burrTxt2, build/test_burrtools)
just test           # Fast test suite: Catch2 (minus stress cases) + Python wrapper
just test-slow      # Stress cases only, chiefly the Minkowski random-shapes case
just test-all       # Everything, fast and slow. This is what CI runs
just test-regression # Regression test comparing burrTxt/burrTxt2 against known-good 0.7.1 release output (run before creating a PR)
just check          # Fast static code analysis with cppcheck (~5s, always run before finishing tasks)
just check-tidy     # Deep static analysis with clang-tidy on BurrTools sources
just check-scan     # Clang Static Analyzer (scan-build)
just check-analyzer # GCC -fanalyzer static analysis
just coverage       # Report test coverage for BurrTools sources (gcovr)
just coverage-html  # Write an HTML coverage report to coverage-html/index.html
just docs           # Generate the Doxygen API reference into gendoc/html
just clean          # Clean build artifacts
just rebuild        # Rebuild from scratch (removes build/ and re-runs meson setup)
just build-werror   # Build with warnings treated as errors (excluding vendored code)
```

**Test suite timings.** The recipes above build first, so what you wait for is
compilation plus test execution. Test execution alone is about 1.6s for `just
test` and about 8.9s for `just test-all`; the difference is almost entirely the
one Minkowski random-shapes stress case. Compilation is extra and can dominate:
re-running with nothing changed is 1.6s against 8.9s, editing a single file is
about 3.4s against 10.0s, and editing a widely-included header is about 13.6s
against 21.0s. Use `just test` while iterating and `just test-all` before
calling a task done.

`just docs` requires `doxygen` and `graphviz` (`brew install doxygen graphviz` on macOS,
`apt-get install doxygen graphviz` on Linux). Without graphviz it still produces a complete
site, minus the diagrams. It fails on any doxygen warning, so a stale `@param` or a broken
`\ref` is a build error, not a silently mangled page; CI runs the same recipe and publishes
the result from `master` to https://burr-tools.github.io/burr-tools/.

Coverage requires `gcovr` (`brew install gcovr` on macOS, `apt-get install gcovr` on Linux).
On macOS the recipes pass `--gcov-executable "xcrun llvm-cov gcov"` automatically, because
Apple Clang emits coverage data that plain `gcov` cannot parse.

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
just build-asan     # AddressSanitizer & UndefinedBehaviorSanitizer
just build-tsan     # ThreadSanitizer (critical for solver data races)
```

---

## 2. Codebase Architecture

- **`src/lib/`**: Core domain logic. Contains voxel grids (`voxel*.cpp`), polycube symmetries (`symmetries_*.cpp`), puzzle definitions (`puzzle.cpp`, `problem.cpp`), assembly/disassembly algorithms, and solver engines (`assembler_*.cpp`, `disassembler_*.cpp`, `solvethread.cpp`).
- **`src/gui/`**: FLTK-based graphical user interface and OpenGL 3D viewports (`mainwindow.cpp`, `view3dgroup.cpp`, `arcball.cpp`, `viewcube.cpp`).
- **`src/halfedge/`**: Half-edge data structure for 3D polyhedron mesh manipulation and STL export.
- **`src/tools/`**: XML parser/writer (`xml.cpp`), file existence helpers, and gzip stream wrappers (`gzstream.cpp`).
- **`test/`**: Catch2 v3 automated regression test suite
- **`design/`**: Durable design docs and specs (e.g. the test coverage stack design).
- **`src/lua/`**: Bundled Lua 5.x C interpreter. **Do not modify.**
- **`subprojects/`**: External dependencies managed by Meson (`fltk`, `catch2`). **Do not modify.**
- **`build/compile_commands.json`**: Compilation database generated by Meson, consumed by clang-tidy, cppcheck, and IDE language servers.

---

## 3. Important Development & Architecture Rules

1. **Assertions & Control Flow:**
   - BurrTools uses custom assertions defined in [`src/lib/bt_assert.h`](src/lib/bt_assert.h).
   - Use `bt_assert(condition)` for runtime checks. In debug builds, it throws `assert_exception` through the `[[noreturn]]` function `bt_te()`.
2. **FLTK Widget Ownership:**
   - FLTK widgets constructed between an `Fl_Group`'s `begin()` and `end()` are automatically adopted by the enclosing group.
   - Do not manually `delete` FLTK child widgets owned by groups; the parent destructor handles them.
3. **Thread Safety in Solvers:**
   - BurrTools solvers run worker threads in the background while the FLTK GUI polls metrics on the main thread.
   - Any state shared across threads (e.g., `iterations`, `finished`, solutions list) must use `std::atomic` or be guarded with explicit mutex locks (`pr->lockSolutions()`). Never strip concurrency synchronization.
4. **Testing & Solver Invariants:**
   - All regression tests live in `test/` using Catch2 v3.
   - Test solver correctness using rotation- and permutation-invariant properties: assembly count, disassemblable solution count, disassembly move levels (`da->movesText()`), and piece placement counts.
   - **Do not assert exact iteration counts in regression tests**; iterations measure internal search-tree node visits and will break on valid search heuristic optimizations.
5. **Third-Party Boundaries:**
   - Never edit files inside `subprojects/` or `src/lua/`.
   - Ensure tools and regexes ignore these directories so static analysis and formatting stay focused on BurrTools sources (`burr-tools/src/(?!lua/).*`).
6. **Quality Verification:**
   - After making code modifications, always verify that `just build`, `just test-all` (regression tests, fast and slow), and `just check` (static analysis) pass cleanly. `just test` is the quick loop to use while iterating; run `just test-all` before calling a task done, since it is what CI runs.
   - Always run `just test-regression` before creating a PR to verify that solver output matches the known-good 0.7.1 release output across all example puzzles.
7. **Benchmarking & Optimization Work:**
   - When modifying solver algorithms or proposing optimizations, agents MUST use the standardized benchmark infrastructure in [`bench/bench_solve.py`](bench/bench_solve.py) across the curated 10-puzzle corpus.
   - Never evaluate optimizations on a single puzzle in isolation.
   - Always implement runtime environment variable toggles (e.g. `BURRTOOLS_NO_SIMD=1`) to allow clean, interleaved A/B benchmarking from the exact same build without recompilation.

---

## 4. Benchmarking & Performance Verification

Always use the standardized benchmark infrastructure in [`bench/`](bench/) to validate optimizations across the full puzzle corpus. **Never benchmark on a single puzzle in isolation and extrapolate results.** All AI agents working on performance optimizations in this repository MUST follow this protocol.

### Standardized Tooling

- **[`bench/bench_solve.py`](bench/bench_solve.py)**: Interleaved, multi-run A/B testing measuring wall time, user/sys CPU time, multi-core utilization, and peak RSS across the 10-puzzle curated corpus.
- **[`bench/run_suite.sh`](bench/run_suite.sh)**: Shell wrapper for automated full-suite regression and speedup reporting.
- **[`bench/run_snapshot.sh`](bench/run_snapshot.sh)** (`just bench`): Single-commit snapshot over the fixed corpus (including the Jack Krijnen Supernova problems), always with disassembly. Stores point-in-time measurements as `bench/results/results_<timestamp>_<hash>[-dirty]_<subject-slug>.csv` for later comparison.

### Point-in-Time Snapshots and Regression Checks Without Old Code

`just bench` records the current commit's solver performance (3 runs per puzzle by default) into a self-identifying CSV under [`bench/results/`](bench/results/). Because each file carries its timestamp, git hash, and commit subject in its name, a later commit can be regression-checked by diffing its snapshot against a stored older one — no checkout, rebuild, or re-run of the old code needed. Each CSV additionally starts with `#`-prefixed provenance lines (UTC timestamp, mode/binaries, full git commit + subject, clean/dirty worktree state, hostname + CPU count, runs/timeout/threads/disassemble settings) written by `bench_solve.py`, so a stored file is interpretable on its own:

```bash
just bench                                   # snapshot current HEAD (builds first)
just bench --runs 5                          # more runs per puzzle
just bench --threads 1                       # single-thread throughput snapshot
just bench --runs 1 <puzzle>...              # one-off subset (replaces corpus)

# Compare two snapshots (CSVs share the same header/column layout):
diff bench/results/results_20260923_114837_4280734-*.csv \
     bench/results/results_20260924_090112_*.csv
```

Caveats: snapshots from different machines are not comparable (absolute times depend on hardware); correctness columns (assemblies/solutions/iterations) in the CSV double as the regression signal and are machine-independent. A `-dirty` suffix marks runs from a worktree with uncommitted changes.

### Benchmarking Alternatives via Environment Variables

Solver engines support runtime feature toggles via environment variables to allow clean, side-by-side A/B benchmarking from the exact same build:

| Environment Variable | Effect | Purpose |
| :--- | :--- | :--- |
| `BURRTOOLS_NO_SIMD=1` | Disables the SIMD bit-parallel solver in **both** assemblers (`assembler_0_c` and `assembler_1_c`), forcing classical DLX. Also read by the `SimdExactCover` / `SimdHuangCover256` constructors. | Measure pure speedup of SIMD bit-parallel exact cover against the Knuth DLX baseline. |
| `BURRTOOLS_NO_VECTOR=1` | Disables architecture-specific vector SIMD instructions (AVX2, AVX-512 on x86-64, NEON on ARM) across all exact cover solvers and disassembler closure, falling back to portable 64-bit scalar word loops. Replaces needing platform-specific flags. | Clean, architecture-agnostic benchmark isolation of algorithmic gains (0-cost backtracking, cache locality) from vector intrinsics. |
| `BURRTOOLS_NO_AVX2=1` | Disables the AVX2 and AVX-512 kernels **on x86-64**, falling back to the portable 64-bit word scalar loop. No effect on ARM -- use `BURRTOOLS_NO_NEON` there. | Isolate the algorithmic gain (0-cost backtracking, cache locality) from x86 vector intrinsics. |
| `BURRTOOLS_NO_AVX512=1` | Disables AVX-512 vector instructions in SIMD solver and disassembler closure. | Isolate AVX-512 vector performance gains from AVX2. |
| `BURRTOOLS_NO_NEON=1` | Disables the NEON kernels **on ARM**, falling back to the same scalar loop. Honoured by `SimdExactCover` and `SimdHuangCover256`. | The ARM equivalent of `BURRTOOLS_NO_AVX2`; without it an A/B on Apple Silicon silently measures the same code twice. |
| `BURRTOOLS_NO_DISASM_SIMD=1` | Disables vector instructions in disassembler Roy-Floyd-Warshall closure. | Measure pure disassembler vector speedup. |
| `BURRTOOLS_NO_DISASM_POOL=1` | Disables multi-threaded disassembly pool, running disassemblies synchronously. | Measure speedup and scaling of parallel disassembly pool against synchronous baseline. |
| `BURRTOOLS_THREADS=N` | Forces solver to use $N$ worker threads (default: `hardware_concurrency`, clamped to `assembler_c::MAX_THREADS`). **Note:** read independently by the assembler and, once the disassembly pool lands, by that pool too, so `N` may yield `2N` workers overall. | Measure thread scaling curves (e.g. 1, 2, 4, 8 cores). |

### Running an Interleaved A/B Benchmark

To compare an optimization against the baseline using `bench/bench_solve.py`:

```bash
# 1. Create a wrapper for the baseline (e.g. DLX fallback)
cat << 'EOF' > build/burrTxt-base
#!/bin/sh
exec env BURRTOOLS_NO_SIMD=1 $(dirname "$0")/burrTxt "$@"
EOF
chmod +x build/burrTxt-base

# 2. Run interleaved comparison across the entire curated corpus
#    --no-disassemble: isolate assembler performance
#    --threads 1: test single-thread throughput (omit for all-core parallel test)
python3 bench/bench_solve.py --ab build/burrTxt-base build/burrTxt --no-disassemble --runs 3

# 3. Clean up the wrapper
rm build/burrTxt-base
```


---

## 5. Continuous Integration Gates

Beyond the standard Linux/Windows/macOS build jobs, two CI jobs exist specifically to catch defect classes the other jobs are structurally blind to. Do not weaken or skip them.

| Job | Catches | Why the other jobs miss it |
| :--- | :--- | :--- |
| `clang-x86-64` | GCC-only constructs that Clang ignores or rejects, most importantly `#pragma GCC target(...)`. | Linux and Windows build with GCC; the macOS runner is arm64, where the x86 intrinsic blocks are excluded by the preprocessor. An x86-64 Clang build is otherwise untested. |
| `tsan-parallel` | Data races in the parallel assembler and disassembler. | No other job runs a sanitizer. |

### Rules for SIMD and intrinsics

- Use `__attribute__((target("avx2")))` on individual functions. **Never** use `#pragma GCC target(...)`: Clang parses it as an unknown pragma, ignores it, and then fails on every intrinsic in the block.
- Gate every ISA path behind runtime detection (`__builtin_cpu_supports`), and keep a working scalar fallback.

### Rules for the ThreadSanitizer job

- The job runs the `[parallel]` tag. **At least one test in that tag must use a puzzle whose result shape has a symmetry breaker** (currently `examples/Bermuda.xmpuzzle`).
- This is not incidental. `avoidTransformedAssemblies` is only enabled by `checkForTransformedAssemblies()`, called from `createMatrix()` and only when a symmetry breaker is found. Puzzles without one never call `assembly_c::smallerRotationExists()` from a worker thread, so a TSan run over only those puzzles reports **zero races while real ones remain** — a false negative that has already let races ship.
- Compare assembly *multisets* between serial and parallel runs, never just counts. A run that loses one assembly and duplicates another passes a count check.
