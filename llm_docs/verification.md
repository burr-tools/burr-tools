# Quality verification: what to run locally, what CI covers

- Locally, verify what CI cannot, or not cheaply: `just build` and `just test` in the dev `build/` dir (assertions live), plus the cases your change touches, run as often as their risk warrants. A change to threading or timing deserves a stress loop (hundreds of runs, under load, and pinned to one CPU where the OS allows it: `taskset -c 0` on Linux, `start /affinity 1` on Windows; macOS cannot pin a process): CI runs a test once, so a flaky test is found there one failure at a time.
- Leave the rest to CI rather than repeating it: `build-linux` (GCC) and `build-macos` (Apple Clang) build with `--werror -Db_ndebug=true` and run `just test-all`, `build-linux` also `just check` (so `just build-release`/`just test-release` add nothing before a push); Windows is MinGW cross-built and run under wine (`build-windows`) and built natively with MSYS2 UCRT64 (`qt-windows`, for pushes to master, tags and PRs touching the Qt GUI; `qt-macos` is the Qt GUI's job for every PR, `qt-linux` runs as `qt-windows` does); `coverage` runs `test_burrtools` with assertions live. The ASan/UBSan job (`sanitizers`) runs on master, tags and manual runs only, never for a pull request: see below. A toolchain CI does not use (MSVC, say) is yours to check. Run any of them locally when CI reported a failure there, or when you need the answer before pushing.
- Before pushing a restack (every PR of a stack at once), at least build and run `just test` at the stack's top and at every PR that changes an interface the PRs above it use: one failure there costs a CI run on each PR above it.
- `just test-regression` is not run by CI. Run it before opening or updating a PR that can change solver output (assembler, disassembler, grids, puzzle loading), to check that it still matches the known-good 0.7.1 output across all example puzzles.

## Sanitizers: run them when the code calls for it

CI does not run ASan and UBSan for pull requests, so for a PR that check is yours. Decide by what your diff does, not by which paths it touches: a change in `src/lib` can be plain logic, and a small change in the Qt GUI can free an object another one still points to.

Run `just test-asan` (the unit tests, leaks included) when the change:

- allocates or frees by hand (`new`/`delete`, `malloc`/`free`), or changes who owns an object or when it is destroyed;
- indexes raw arrays or buffers, does pointer arithmetic, or computes sizes, offsets or strides;
- uses `memcpy`/`memset`/`memmove`, `reinterpret_cast`, type punning, or SIMD loads and stores;
- keeps pointers, references or iterators into containers that may grow, shrink or be freed underneath them;
- does shift or integer arithmetic where an overflow, a narrowing or a signed/unsigned conversion is possible (UBSan);
- changes a pool, an arena, or a hand-written container's growth or reuse.

Run `just test-asan-qt` (the Qt GUI's suites) when the Qt GUI's own C++ changes in one of those ways, or changes the lifetime of objects shared with QML. QML-only and layout changes don't need it; it is slow on a small machine.

Not needed: logic inside safe abstractions (containers with checked access, value types), algorithm changes that leave memory layout and ownership alone, docs, and tests that add cases without new memory handling. When unsure, run it.

Say in the PR description what you ran, or why nothing was needed, in one line: "ASan: `just test-asan` clean", or "Sanitizers: not run, no change to ownership or indexing". A reviewer can then question the reasoning instead of guessing. Anyone can still have CI check a PR before merging: a manual run of Build and Release on the PR's branch ("Run workflow") runs the `sanitizers` job.

ThreadSanitizer is unchanged: CI's `tsan-parallel` runs for every PR touching the library. Run `just build-tsan` and `./build-tsan/test_burrtools "[parallel]"` yourself when you change data shared between threads (workers, atomics, locks, the thread budget) and want the answer before pushing.
