# Thread configuration: one contract for two concurrent stages

*2026-09-21*

## The problem

Solving a puzzle runs two stages, and they run **at the same time**:

- the **assembler** (`assembler_0_c` / `assembler_1_c`) searches for assemblies;
- the **disassembly pool** (`disassemblerPool_c`) takes each assembly as it is
  found and tries to take it apart.

`solveThread_c` creates the pool in its *constructor*, so the pool's workers are
alive for the whole solve rather than starting after assembly finishes. That
overlap is deliberate — assembly is search bound, disassembly is
movement-closure bound, and the pool's bounded queue (`MAX_QUEUE_SIZE`) provides
backpressure — but it has a consequence that used to be easy to miss:

> The two counts **add up**. A single "use N threads" setting produced `2N`
> compute-hot threads.

Before this change each stage independently resolved its own count, and each
defaulted to `hardware_concurrency()`. On a 14-core machine a default solve ran
14 assembler threads + 14 pool workers + 1 merger. Setting a single knob to 9
still produced 18 hot threads.

Three separate implementations of "how many threads?" had also accumulated —
`assembler_c::getEffectiveThreads()`, the `disassemblerPool_c` constructor, and
the GUI's own `hardware_concurrency()` call — each with its own clamp.

## The contract

One resolver, `threadConfig` ([`src/lib/threadconfig.h`](../src/lib/threadconfig.h)),
shared by every front end: `burrTxt`, `burrTxt2`, the GUI and the Python module.

The two stages are configured **separately**, because one number cannot express
a sensible split between two concurrent consumers.

Resolution order, applied per stage, first match wins:

| | Assembler | Disassembler |
|---|---|---|
| 1. explicit request (non-zero) | `-t N`, slider, `setNumThreads()` | `-T N`, slider, pool ctor |
| 2. stage variable | `BURRTOOLS_ASSEMBLER_THREADS` | `BURRTOOLS_DISASSEMBLER_THREADS` |
| 3. shared variable | `BURRTOOLS_THREADS` | `BURRTOOLS_THREADS` |
| 4. default | 60% of cores, min 1 | **1 (inline)** |

Every path is clamped into `[1, threadConfig::MAX_THREADS]`, so a resolved count
is always directly usable to size a thread vector. `0` everywhere means
"not specified", never "zero threads".

## Why these defaults

**Assembler: 60% of cores.** The assembler runs for the whole solve, so it
should get the bulk of the machine — but not all of it. It shares the box with
the disassembly stage and, in the GUI, with an interactive main thread that
polls progress roughly once a second. Leaving headroom keeps the machine usable
while a long solve runs.

**Disassembler: 1, meaning inline.** A count of 1 is not "one worker" — it is a
distinct mode. `disassemblerPool_c` creates neither workers nor a merger thread
and disassembles on whichever thread calls `submit()`, which is an assembler
worker. This is the right default because the pool only pays off for puzzles
with very many assemblies, and it is not free: each worker owns a private
`disassembler_0_c` with its own movement cache, so a pool of *N* costs *N*
movement caches. Users who know their puzzle benefits can raise it.

## The budget rule

Because the stages overlap, `threadConfig` treats them as a shared budget:

```
assemblerThreads + disassemblerThreadCost(disassemblerThreads) <= maxThreads()
```

`disassemblerThreadCost()` returns **0** when the count is 1, because inline
spawns nothing. This is what makes `(maxThreads(), 1)` a legal pair — the
default on a 14-core machine is `(8, 1)`, and even `(14, 1)` fits.

`fitToBudget()` shrinks an over-subscribed pair. The pool gives way first: the
assembler runs for the entire solve, while dropping the pool back to inline
costs no threads at all. A pool of exactly 1 is meaningless, so when fewer than
two threads are spare the disassembler goes inline rather than keep one useless
worker. Neither value drops below 1, so on a single-core machine the pair stays
`(1, 1)` — which costs nothing: serial assembly plus inline disassembly.

## Where the rule is enforced

| Front end | Behaviour |
|---|---|
| **GUI** | Two sliders, coupled. Dragging one lowers the other so the pair always fits. `configuration_c`'s accessors also run `fitToBudget()` on whatever is in `~/.burrtools.rc`, so a hand-edited or copied-over file cannot oversubscribe. |
| **`burrTxt` / `burrTxt2`** | `-t` and `-T` are honoured **as typed** — an explicit flag is explicit intent — but the tool prints a warning when the pair exceeds the machine. |
| **Python** | `Problem.solve(threads=…)` sets the assembler count. The module disassembles serially with a single `disassembler_0_c` and never builds a pool, so there is no second count to set. |

The asymmetry is intentional: a GUI slider is an exploratory control and should
not let you shoot yourself in the foot, whereas a command-line flag typed by a
benchmarker is a deliberate instruction and should be obeyed.

## Consequences worth knowing

- **The merger thread is one beyond the budget.** When the pool is active it
  also runs a merger that reorders results by sequence number. It is near-idle,
  but `(4, 4)` really means 9 OS threads.
- **`BURRTOOLS_THREADS=N` still yields up to `2N`**, by design — it sets both
  stages. It is kept for the existing benchmark scripts. Use the per-stage
  variables when the distinction matters.
- **There is still no shared thread pool.** The assemblers spawn a transient
  `std::vector<std::thread>` inside `parallelMultiSearch` and join it before
  returning; the disassembly pool is the only persistent pool. They share the
  *budget*, not the *threads*. Unifying them onto one executor would let an idle
  stage lend capacity to a busy one, which this design does not do.

## Testing

`test/test_threadconfig.cpp` covers the module directly.

*Unit* (`[threads][unit]`) — the defaults, the environment precedence chain,
rejection of malformed values, the inline cost model, `fitToBudget()` and the
shared CLI argument parser.

*Dispatch* (`[threads][disasm][pool]`) — that a disassembler count of 1 really
selects inline rather than a one-worker pool. This is observed by which thread
the result callback arrives on: inline calls back on the submitting thread, a
pool calls back on its merger. `BURRTOOLS_NO_DISASM_POOL` and the default of 0
are checked the same way.

*Thread usage* (`[threads][solver][stress]`) — that a resolved count reaches the
workers, which is the failure mode that matters: a number computed correctly
and then ignored. It solves `examples/SolidSixPieceBurrs.xmpuzzle`, the largest
puzzle in `examples/` (588 assemblies over ~4.3M search-tree nodes — enough work
for the parallel search to split into several tasks; a small puzzle finishes
inside one task and would prove nothing). The assembler callback records
`std::this_thread::get_id()`, so the number of distinct worker threads is
observable portably, without mach / proc / Win32 thread enumeration. It asserts
that 1 thread stays on exactly one thread, that 4 threads spread across more
than one and never more than four, and that the assembly count is identical
either way. Tagged `[stress]` because the serial run alone takes about 7s.
