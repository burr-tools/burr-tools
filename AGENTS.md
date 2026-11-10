This repository contains **BurrTools**, an application for designing, solving, and analyzing 3D interlocking burr puzzles.
It is primarily written in C++20 using the FLTK GUI library, OpenGL, and the Meson build system.

## Build System & Common Commands

Use [`just`](justfile) to execute tasks. Do not invoke ad-hoc build steps when a target exists in the `justfile`. Run bare `just` for the authoritative, up-to-date recipe list.

For targeted information, consult these:

`llm_docs/coverage_tests.md` -- coverage and test suite docs
`llm_docs/architecture.md` -- code architecture


## Rules

1. **Assertions:** use `bt_assert(cond)` (`src/lib/bt_assert.h`) for runtime checks. It vanishes under `NDEBUG`, so the expression must be side-effect-free; use `bt_assert2` for mutating checks. Never use assert macros for control flow: validation callers rely on throws unconditionally via `bt_te()`.
2. **Third-party code:** never edit `subprojects/` or `src/lua/`; keep tools and regexes ignoring them.
3. **Tests** live in `test/` (Catch2 v3). Tests asserting debug-only behavior (`bt_assert` throwing, `assert_log`, bounds checks) need `#ifndef NDEBUG` (CI also tests release). Precedent: `test_halfedge.cpp`.
4. **Verify** with `just build` and `just test`; leave the rest to CI.

## Consult when the task touches...

- FLTK widgets (`src/gui/`) -> `llm_docs/fltk_gui.md`
- Solver threads, shared solver state, or solver tests -> `llm_docs/solver_rules.md`
- File formats, saved search state, CLI/API changes -> `llm_docs/compatibility.md`
- Qt GUI / QML / new `BURRTOOLS_*` env var -> `llm_docs/qt_gui_rules.md`
- Performance work, benchmarks, solver env-var toggles -> `llm_docs/benchmarking.md`
- CI failures, SIMD/intrinsics, TSan, `spec/` TLA+ models, `src/lib` pool/budget headers -> `llm_docs/ci_gates.md`
- What to run before pushing/opening a PR (restacks, `just test-regression`) -> `llm_docs/verification.md`
