# Benchmarking & Performance Verification

Always use the standardized benchmark infrastructure in [`bench/`](bench/) to validate optimizations across the full puzzle corpus. **Never benchmark on a single puzzle in isolation and extrapolate results.** All AI agents working on performance optimizations in this repository MUST follow this protocol.

### Standardized Tooling

- **[`bench/bench_solve.py`](bench/bench_solve.py)**: Interleaved, multi-run A/B testing measuring wall time, user/sys CPU time, multi-core utilization, and peak RSS across the 10-puzzle curated corpus.
- **[`bench/run_suite.sh`](bench/run_suite.sh)**: Shell wrapper for automated full-suite regression and speedup reporting.
- **[`bench/run_snapshot.sh`](bench/run_snapshot.sh)** (`just bench`): Single-commit snapshot over the fixed corpus (including the Jack Krijnen Supernova problems), always with disassembly. Stores point-in-time measurements as `bench/results/results_<timestamp>_<hash>[-dirty]_<subject-slug>.csv` for later comparison.

### Point-in-Time Snapshots and Regression Checks Without Old Code

`just bench` records the current commit's solver performance (3 runs per puzzle by default) into a self-identifying CSV under [`bench/results/`](bench/results/). Because each file carries its timestamp, git hash, and commit subject in its name, a later commit can be regression-checked by diffing its snapshot against a stored older one — no checkout, rebuild, or re-run of the old code needed. Each CSV additionally starts with `#`-prefixed provenance lines (UTC timestamp, mode/binaries, full git commit + subject, clean/dirty worktree state, hostname + CPU count, runs/timeout/threads/disassemble settings) written by `bench_solve.py`, so a stored file is interpretable on its own:

```bash
just bench                                   # snapshot current HEAD (builds first), then compare against previous snapshot
just bench --runs 5                          # more runs per puzzle
just bench --threads 1                       # single-thread throughput snapshot
just bench --runs 1 <puzzle>...              # one-off subset (replaces corpus)
just bench --no-compare                      # snapshot only, skip the comparison

# Compare two snapshots (CSVs share the same header/column layout):
diff bench/results/results_20260923_114837_4280734-*.csv \
     bench/results/results_20260924_090112_*.csv
```

Caveats: snapshots from different machines are not comparable (absolute times depend on hardware); correctness columns (assemblies/solutions/iterations) in the CSV double as the regression signal and are machine-independent. A `-dirty` suffix marks runs from a worktree with uncommitted changes.

**Benchmarks must use the release binary.** `just bench` builds and measures `build-rel/burrTxt` (`--buildtype=release -Db_ndebug=true` via `just build-release`). Never publish timings from the default `build/` dir: it carries `_GLIBCXX_ASSERTIONS` and live `bt_assert` checks, which cost ~15–30% solver time in the exact-cover hot loops (every `vector::operator[]` gains a bounds check). See `design/2026-09-24-benchmark-speedup-analysis.md` §7. The same applies to hand-rolled A/B comparisons: build both binaries with `-Db_ndebug=true`, or the numbers measure assertion overhead instead of your change.

### Solver Settings and Their Environment Variables

Solver hyperparameters and A/B toggles live in one configuration object, `solverConfig_t` (`src/lib/solver_config.h`, which also has the field-to-variable table). A front end sets the fields it chooses (burrTxt from its flags, the GUI from its preferences, Python from `solve()` arguments) and passes the result of `resolve()`, a `solverSettings_t`, when it creates the solver; every field it leaves unset comes from the environment variable below, else the default. The settings are a required constructor argument; the solver itself never reads the environment, all parsing is in `solver_config.cpp`. Tests create solvers with `bttest::settings()` (the defaults, changed per scope by `bttest::ScopedSettings`), never through the environment; only `test_solver_config.cpp` sets variables. Values are `0|1` for switches (no `NO_` prefixes), an unparsable value is an error, and so is a name these variables had before 2026-10-28 (`BURRTOOLS_NO_SIMD` and friends), with the new name in the message. So a typo never silently benchmarks the default.

The engine-selection toggles (`BURRTOOLS_BITPARALLEL`, `BURRTOOLS_HUANG_MEM_MB`) apply to fresh runs only: a resumed search keeps the engine that saved its remainder, because the remainder's tasks are only valid for that engine.

| Environment Variable | Effect | Purpose |
| :--- | :--- | :--- |
| `BURRTOOLS_BITPARALLEL=0\|1` | `0` disables the SIMD bit-parallel solver in **both** assemblers (`assembler_0_c` and `assembler_1_c`), forcing classical DLX. | Measure pure speedup of SIMD bit-parallel exact cover against the Knuth DLX baseline. |
| `BURRTOOLS_VECTOR=auto\|scalar\|avx2\|avx512\|neon` | The most of the vector instructions a run may use, in every exact cover engine and the disassembler closure; the CPU decides what of that exists. `scalar` is the portable 64-bit word loop on every architecture; `avx2` keeps AVX2 but not AVX-512. | `scalar` isolates algorithmic gains (0-cost backtracking, cache locality) from intrinsics on x86 and ARM alike; `avx2` isolates AVX-512's gain. |
| `BURRTOOLS_DISASM_VECTOR=0\|1` | `0` disables vector instructions in the disassembler's Roy-Floyd-Warshall closure only. | Measure pure disassembler vector speedup. |
| `BURRTOOLS_DISASM_POOL=0\|1` | `0` disables the multi-threaded disassembly pool, running disassemblies synchronously. | Measure speedup and scaling of the parallel disassembly pool against the synchronous baseline. |
| `BURRTOOLS_THREADS=N` | Worker threads (default: `hardware_concurrency`, clamped to `assembler_c::MAX_THREADS`). **Note:** the assembler and the disassembly pool each get N, so `N` may yield `2N` workers overall. burrTxt `-t`, the GUI's setting and Python's `threads=` override it. | Measure thread scaling curves (e.g. 1, 2, 4, 8 cores). |
| `BURRTOOLS_HUANG_MEM_MB=N` | The Huang-SIMD memory budget in MB (default 256; `0` disables the SIMD path via the budget). | A/B the SIMD-vs-DLX tradeoff near the refusal threshold without rebuilding. |
| `BURRTOOLS_SPLIT_FANOUT=N` | Fan-out cap for dividing one search level into tasks (default 16, 1 to 4096, see `src/lib/split_fanout.h`): assembler_0 runtime splits and the stop-time remainders of every engine emit at most N singletons plus one rest. Any N >= 1 is exact. | Tests set it to 1-2 so small puzzles exercise every rest path that only wide levels reach at the default. |
| `BURRTOOLS_SPLIT_ORDER=depth\|breadth` | `breadth` queues assembler_0's dynamically split child tasks at the back of the pool instead of the front (depth-first, the default). | A/B the depth-first order: queue/remainder size (breadth-first grows towards every node of the split depth on wide puzzles) and long-tail wall time. |
| `BURRTOOLS_DONATE=0\|1` | `0` disables split on demand in every engine: running tasks no longer hand their shallowest open level to waiting siblings. assembler_0 falls back to its eager pop-time splitting (shallow prefixes, starving siblings); assembler_1's tasks then run as seeded. See `design/2026-10-17-split-on-demand.md`. | A/B donation against pop-time splitting: tail idle time and wall time on skewed puzzles. |
| `BURRTOOLS_DONATE_EVERY=N` | Tests only: additionally donate every N search nodes whether or not a sibling is waiting, so donation paths run deterministically, even single-threaded. | Exercise the donation/cut/divide interplay on small puzzles. |
| `BURRTOOLS_THREAD_BUDGET=0\|1` | `0` runs the assembly and disassembly pools without the shared thread budget (`src/lib/thread_budget.h`). | A/B capped vs uncapped scheduling. |
| `BURRTOOLS_ROT_PLANNER=0\|1` | Turns the rotation planner off or on where the front end does not choose (`burrTxt -Q` and `-M` do). See `design/2026-10-19-rotation-moves.md`. | A/B the planner's extra turns and cost. |
| `BURRTOOLS_ROT_FAMILY=compass\|wide` | The fixed rotation path family: `wide` (the default: 16 directions, wiggles, slides and ramps, amounts to 0.55: 881 paths per sense) or `compass` (8 directions, wiggles and slides: 145 paths, about 5 times faster standalone). | A/B the wide family's extra turns against its table-build cost. |

### Running an Interleaved A/B Benchmark

New solver optimizations MUST ship a runtime toggle (a `solverConfig_t` field with its `BURRTOOLS_` variable, e.g. `BURRTOOLS_BITPARALLEL=0`) enabling clean, interleaved A/B benchmarking from the exact same build without recompilation.

To compare an optimization against the baseline using `bench/bench_solve.py`:

```bash
# 1. Create a wrapper for the baseline (e.g. DLX fallback)
cat << 'EOF' > build/burrTxt-base
#!/bin/sh
exec env BURRTOOLS_BITPARALLEL=0 $(dirname "$0")/burrTxt "$@"
EOF
chmod +x build/burrTxt-base

# 2. Run interleaved comparison across the entire curated corpus
#    --no-disassemble: isolate assembler performance
#    --threads 1: test single-thread throughput (omit for all-core parallel test)
python3 bench/bench_solve.py --ab build/burrTxt-base build/burrTxt --no-disassemble --runs 3

# 3. Clean up the wrapper
rm build/burrTxt-base
```
