# Solver Settings in One Object

**Date:** 2026-10-28
**Status:** Implemented on branch `rotations`.
**Scope:** `src/lib/solver_config.{h,cpp}` (new), `src/lib/simd_config.h`
(removed), the assemblers, SIMD covers, disassembler, disassembly pool,
`solveThread_c`, burrTxt, burrTxt2, the GUI, the Python bindings, tests,
AGENTS.md.

## Problem

Solver hyperparameters and A/B toggles were read from the environment
where they were used: `SimdConfig::` predicates in the cover constructors
and the disassembler's kernel choice, `SplitFanout::*FromEnv()` at the start
of each parallel run, `getenv` in the disassembly pool, the assembler's
thread count, the Huang memory budget, the thread budget (a function-local
static) and the rotation planner. Eleven variables, each with its own
parsing (`atoi`, `strtol`, presence only), its own idea of what an invalid
value means (mostly: silently the default), and `NO_` names whose value
was ignored. A front end could not choose a setting except through the
environment, and tests changed process-wide state to pick an engine.

## Design

* `solverConfig_t` holds what a front end chooses, as `std::optional`
  fields, plus a `rotationConfig_t` for the rotation settings.
  `resolve()` turns it into a `solverSettings_t` of plain values: each
  field the caller set, else its environment variable, else the default.
  It is the only code that reads the solver's environment, and only front
  ends call it (burrTxt, burrTxt2, the GUI's `mainWindow_c::solverSettings`,
  the Python iterator).
* Defaults are the initializers of `solverSettings_t` and
  `rotationSettings_t`, so `solverSettings_t{}` is the defaults with no
  environment involved; `resolve()` only overlays choices and variables.
* The settings are a required argument wherever a solver is created, and
  fixed from then on: `gridType_c::findAssembler(problem, settings)` and the
  assembler constructors, `disassemblerPool_c` (which takes its worker
  count from them), `disassembler_0_c` / `movementAnalysator_c`,
  `solveThread_c`. No constructor defaults to the environment, so nothing
  can read it behind the caller's back. Components the solver creates get
  the settings from their creator (assembler -> SIMD cover, pool -> worker
  disassemblers).
* The one exception is the thread count: `assembler_c::setNumThreads`
  writes `settings.threads` (the only copy), because a paused search may be
  continued with another count; `solveThread_c` applies it before a run.
  The engines of a continued search stay those it was created with.
* Naming: `BURRTOOLS_<SETTING>=<value>`, `0|1` for switches, no `NO_`
  prefixes; one `BURRTOOLS_VECTOR` cap replaces four per-ISA switches.
  `solver_config.h` holds the one field-to-variable table; AGENTS.md says
  what each variable is for.

| Before | Now |
|---|---|
| `NO_SIMD=1` | `BITPARALLEL=0` |
| `NO_VECTOR=1`, `NO_AVX2=1`, `NO_NEON=1` | `VECTOR=scalar` |
| `NO_AVX512=1` | `VECTOR=avx2` |
| `NO_DISASM_SIMD=1` | `DISASM_VECTOR=0` |
| `NO_DISASM_POOL=1` | `DISASM_POOL=0` |
| `NO_DONATE=1` | `DONATE=0` |
| `SPLIT_BREADTH_FIRST=1` | `SPLIT_ORDER=breadth` |
| `NO_BUDGET=1` | `THREAD_BUDGET=0` |
| `NO_ROT_PLANNER=1` | `ROT_PLANNER=0` |
| `THREADS`, `HUANG_MEM_MB`, `SPLIT_FANOUT`, `DONATE_EVERY` | unchanged |

* Errors: an unparsable value, and any of the old names, throw
  `std::invalid_argument` naming the variable (and the replacement). Not
  `bt_te`: `assert_exception` keeps only a pointer to its message, which
  must therefore be a literal. An A/B run that silently measures the
  default twice is the failure this prevents; the old names are rejected
  rather than read, because nothing here was in a release (AGENTS.md
  "Compatibility").

## Behaviour changes

* `BURRTOOLS_NO_SIMD=1` used to also switch every vector kernel to scalar,
  the disassembler's closure included. `BITPARALLEL=0` only selects DLX;
  an A/B of DLX against SIMD with disassembly now keeps the disassembler's
  vector kernels on both sides, which isolates the assembler. For the old
  combination set `BITPARALLEL=0 VECTOR=scalar`.
* Toggles that were read at the start of every run (split fan-out,
  donation, the Huang budget, the bit-parallel switch, the disassembler's
  kernels) are now fixed when the solver is created. Changing the
  environment between two runs of one assembler no longer has an effect;
  a test that needs two settings creates two assemblers.
* `BURRTOOLS_ROT_PLANNER` is below `-Q`; the old `NO_ROT_PLANNER` overrode
  it.

## Tests

`test/test_solver_config.cpp` is the only test that touches the
environment: defaults, environment below explicit choices, malformed
values and old names rejected with the variable named, the vector cap, the
rotation settings. Every other test creates solvers with
`bttest::settings()`, `solverSettings_t{}` unless a `bttest::ScopedSettings`
in scope changes it (`ScopedSettings dlx; dlx->bitParallel = false;`), so a
`BURRTOOLS_` variable in the developer's shell cannot change what a test
checks.

Spec unaffected: the change moves where settings come from; no protocol
in `spec/` (budget, pool, assembler task accounting) changed.
