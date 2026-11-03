# Continuous Integration Gates

Beyond the standard Linux/Windows/macOS build jobs, two CI jobs exist specifically to catch defect classes the other jobs are structurally blind to. Do not weaken or skip them.

| Job | Catches | Why the other jobs miss it |
| :--- | :--- | :--- |
| `clang-x86-64` | GCC-only constructs that Clang ignores or rejects, most importantly `#pragma GCC target(...)`. | Linux and Windows build with GCC; the macOS runner is arm64, where the x86 intrinsic blocks are excluded by the preprocessor. An x86-64 Clang build is otherwise untested. |
| `tsan-parallel` | Data races in the parallel assembler and disassembler. | The only ThreadSanitizer run (ASan and UBSan: `sanitizers`). |
| `qt-linux`, `qt-windows`, `qt-macos` | Breakage in `burrtools-qt`: controllers, QML, renders (WARP on Windows, lavapipe Vulkan on Linux — required, not skipped), gallery reference images (Windows, macOS). Each uploads a preview build. `qt-linux` runs for every PR; `qt-macos` for PRs touching Qt GUI or library (`core`) paths; `qt-windows`, `coverage-qt` and the Qt suites under the sanitizers run on master, tags and PRs touching Qt GUI paths. | The legacy jobs build without Qt. |
| `sanitizers` | Memory errors and undefined behaviour (ASan + UBSan) in `test_burrtools` (leaks too) and the Qt suites (renders on lavapipe). | Leaks are not checked in the Qt suites: Qt and Mesa are not instrumented. |
| `qt-profile.yml` (on demand) | Nothing: reports only — start-up times, a QML trace, a heap profile, the `src/uicore` micro-benchmarks (artifact `qt-profile`). | Not a check: shared runners are too noisy to fail on time. |
| `qt-standalone.yml` (on demand only) | A static `burrtools-qt.exe` that still loads an MSYS2 library, or fails its self-check. Attaches the standalone program to a release when given its tag. | Too slow for every push (550 MB static Qt download). |

### Rules for SIMD and intrinsics

- Use `__attribute__((target("avx2")))` on individual functions. **Never** use `#pragma GCC target(...)`: Clang parses it as an unknown pragma, ignores it, and then fails on every intrinsic in the block.
- Gate every ISA path behind runtime detection (`__builtin_cpu_supports`), and keep a working scalar fallback.

### Rules for the ThreadSanitizer job

- The job runs the `[parallel]` tag. **At least one test in that tag must use a puzzle whose result shape has a symmetry breaker** (currently `examples/Bermuda.xmpuzzle`).
- This is not incidental. `avoidTransformedAssemblies` is only enabled by `checkForTransformedAssemblies()`, called from `createMatrix()` and only when a symmetry breaker is found. Puzzles without one never call `assembly_c::smallerRotationExists()` from a worker thread, so a TSan run over only those puzzles reports **zero races while real ones remain** — a false negative that has already let races ship.
- Compare assembly *multisets* between serial and parallel runs, never just counts. A run that loses one assembly and duplicates another passes a count check.

### Rules for the TLA+ protocol specs (`spec/`)

- The PlusCal models in `spec/` check protocol shape (token accounting, pop/finish pairing, quiescence, ordered merge) over all small-instance interleavings. They are the backstop for the defect class TSan cannot see (lost wakeups, hangs, token leaks).
- Any PR touching `src/lib/thread_budget.h`, `src/lib/assembler_pool.h`, `src/lib/disassemblerpool.h`, `src/lib/assembler.h` or `src/lib/solvethread.h` must state "spec unaffected" or update the spec.
- `just spec-check` reads only `spec/` (it translates and model-checks the `.tla`/`.cfg` files, never the C++), so run it only when something under `spec/` changed. Re-running it after a C++-only change re-proves the unchanged models and costs minutes for nothing.
- Invariant names (`SPEC-BUDGET-1`, `SPEC-POOL-1`, …) are stable IDs quoted in both the spec and the `src/lib` comments. Rename on both sides or neither.
- Never hand-edit the generated TLA+ translation in `spec/*.tla` — edit the `--algorithm` PlusCal and re-run `pcal.trans` via `just spec-check`.
