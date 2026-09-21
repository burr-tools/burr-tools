# Concurrency Architecture & Threading Model: "Should-Be" vs. "Is" Assessment

**Date:** 2026-09-19  
**Scope:** BurrTools Concurrency Model across `src/lib/{solvethread,disassemblerpool,assembler_0,assembler_1,movementcache,gridtype}.{h,cpp}` and `src/gui/mainwindow.cpp`  
**PR Stack:** #78 → #79 → #81 → #82 → #83 → #84 → #87  
**Status:** Canonical Architecture Specification & Remediation Plan  

---

## 1. Executive Summary & Context

BurrTools is transitioning from a single-threaded solver pipeline (GUI thread + single background solver thread running inline assembly and disassembly) to a **two-tier concurrent pipeline**:
1. **Tier 1 (Assembly):** Multi-threaded exact cover search (DLX and SIMD bit-parallel) distributing independent subtrees across worker threads.
2. **Tier 2 (Disassembly):** Concurrent producer-consumer thread pool with a reorder buffer, allowing assemblies to be disassembled across worker threads as they are found, while preserving deterministic solution ordering.

Recent code reviews by `@tomburns` across the PR stack (#78 through #84) correctly identified critical concurrency, lifecycle, and data-race hazards. This document defines:
1. The **"Should-Be"** threading model: the formal contract for thread roles, shared-state ownership, synchronization primitives, thread budgeting, cancellation, and exception safety.
2. The **"Is"** assessment: an exact gap analysis of where the current implementations in PRs #79, #81, and #84 diverge from this model.
3. The **Actionable Remediation Plan**: a prioritized checklist to fix every identified issue before merging the PR stack.

---

## 2. The "Should-Be" Concurrency Model

```
+---------------------------------------------------------------------------------------------------+
|                                       FLTK GUI Thread                                             |
|  - Owns widgets, event loop, viewports                                                            |
|  - Polls atomics (iterations, assemblies, solutions) at ~10 Hz                                    |
|  - Triggers start(), stop(), setPosition()                                                        |
|  - NEVER blocks on worker joins or mutexes                                                        |
+---------------------------------------------------------------------------------------------------+
                                         │ Start / Stop
                                         ▼
+---------------------------------------------------------------------------------------------------+
|                                  Solver Orchestrator Thread                                       |
|                                       (solveThread_c)                                             |
|  - Warm lazy caches (getSymmetries, voxel BbHsCache) BEFORE spawning any workers                  |
|  - Allocates thread budget: N = N_asm + N_dis                                                     |
|  - Instantiates Assembler (Tier 1) and Disassembler Pool (Tier 2)                                 |
|  - Catches worker exceptions; coordinates clean join on completion/abort                          |
+---------------------------------------------------------------------------------------------------+
                  │                                                               │
                  ▼ (N_asm workers)                                               ▼ (N_dis workers)
+----------------------------------------+                      +-----------------------------------+
|       Tier 1: Assembler Workers        |                      |    Tier 2: Disassembler Workers   |
|  - Independent subtree search          |                      |  - Read from bounded work_queue   |
|  - Private DLX/SIMD search context     |                      |  - Private disassembler_0_c       |
|  - Zero inter-worker communication     |                      |  - Shared movementCache (mutex)   |
|  - Periodic check of abort flag        |                      |  - Periodic check of abort flag   |
+----------------------------------------+                      +-----------------------------------+
                  │                                                               │
                  │ submit(assembly, seqNo)                                       │ push(seqNo, result)
                  ▼                                                               ▼
+---------------------------------------------------------------------------------------------------+
|                              Bounded Reorder Buffer & Merger Thread                               |
|  - Enforces strict deterministic seqNo ordering (0, 1, 2, ...)                                    |
|  - Bounded: blocks Tier 2 workers if buffer exceeds MAX_REORDER_SIZE                              |
|  - SOLE WRITER to problem_c::solutions (under lockSolutions())                                    |
+---------------------------------------------------------------------------------------------------+
```

---

### 2.1 Thread Roles & Hierarchy

1. **Thread 0: GUI Main Thread (FLTK)**
   - **Responsibilities:** Renders UI, responds to user events, updates progress bars and 3D viewports.
   - **Synchronization Rule:** Must **never** execute blocking operations (no `std::thread::join()`, no waiting on condition variables, no holding locks while workers compute).
   - **Communication:** Communicates with the solver exclusively via `std::atomic` flags (`running`, `finished`, `abort`) and non-blocking lock acquisitions (`pr->lockSolutions()`).

2. **Thread 1: Solver Orchestrator Thread (`solveThread_c`)**
   - **Responsibilities:** Background thread that runs the outer solve workflow (`solveThread_c::run()`).
   - **Synchronization Rule:** Responsible for the lifecycle of all worker threads. Spawns workers, waits for them via `join()`, catches any propagated exceptions, and updates problem status.

3. **Threads 2..$K+1$: Assembler Workers (Tier 1)**
   - **Responsibilities:** Explore disjoint subtrees of the exact cover matrix (via `assembler_0_c` or `assembler_1_c`).
   - **Synchronization Rule:** 100% thread-isolated search state. Shared problem data is strictly read-only. Worker threads communicate out only via thread-safe callbacks to submit found assemblies.

4. **Threads $K+2..M$: Disassembler Workers (Tier 2)**
   - **Responsibilities:** Consume assemblies from a thread-safe queue and run 3D movement analysis (`disassembler_0_c::disassemble()`).
   - **Synchronization Rule:** Each worker owns a **private** `disassembler_0_c` and `movementAnalysator_c`. Shared `movementCache_c` is protected by a mutex.

5. **Thread $M+1$: Disassembly Merger Thread**
   - **Responsibilities:** Drains the reorder buffer in strict `seqNo` order (0, 1, 2, ...) and executes the single-threaded solution insertion, drop heuristics, and UI notification.
   - **Synchronization Rule:** Sole writer to `problem_c::solutions`. Guarantees bit-for-bit deterministic solution ordering regardless of completion order.

---

### 2.2 Global Thread Budgeting ($N$ Cores) & Dynamic Backpressure

Rather than statically partitioning CPU cores (e.g. 60% assembler, 40% disassembler) — which starves the assembler when disassembly is trivial and starves disassembly when assembly finishes early — BurrTools implements a **demand-driven, dynamic backpressure pipeline**:

```
[Assembler: Up to N Threads] ──(submit)──> [Bounded Queue: Max 2N] ──(consume)──> [Disassembler: Up to N Workers]
            ▲                                       │                                         │
            │                                       ▼                                         ▼
   (Queue full: blocks                    (Queue empty: sleeps                      (Active workers scale
    and yields CPU to disassemblers)       on condition_variable,                    dynamically with
                                           consuming 0% CPU)                         queue backlog)
```

1. **Zero-Cost Idle Workers:**
   - Disassembler pool workers wait on `std::condition_variable cv_worker`.
   - When `work_queue` is empty (no assemblies found yet), workers consume **0% CPU**. The assembler utilizes **100% of all $N$ cores**.
2. **Tiered Worker Wake-up:**
   - When a solution is submitted, `submit()` notifies one worker (`cv_worker.notify_one()`).
   - For trivial disassemblies (taking microseconds), 1 worker drains the queue continuously. The other $N-1$ assembler threads run uninterrupted.
3. **Bounded Queue Backpressure (Eliminating Oversubscription):**
   - The input queue is bounded at `MAX_QUEUE_SIZE = 2 * N` (default 64) and the reorder buffer is bounded at `MAX_REORDER_SIZE = 2 * N` (default 64).
   - If disassemblies are computationally heavy and fall behind assembly, the queue fills to capacity.
   - When `work_queue` is full, assembler threads block in `cv_producer.wait()`.
   - While assembler threads are blocked waiting for queue space, the OS scheduler naturally gives **100% of the CPU cores to the active disassembler workers**.
4. **Post-Assembly Core Handoff:**
   - When the assembler finishes searching its subtrees, its threads terminate.
   - The disassembler pool automatically has access to all $N$ cores to finish processing any remaining backlog.

| Scenario | Assembler Workers ($N_{\text{asm}}$) | Disassembler Workers ($N_{\text{dis}}$) | Merger Thread | Effective CPU Allocation |
| :--- | :---: | :---: | :---: | :---: |
| **Assembly Only (`disassemble == false`)** | $N$ | 0 | 0 | $N$ cores (100% assembler) |
| **Assembly + Trivial Disassembly** | $N$ | Up to $N$ (only 1 active, $N-1$ sleeping) | 1 (event-driven) | $\approx N-1$ assembler, $\approx 1$ disassembler |
| **Assembly + Heavy Disassembly** | $N$ (throttled by backpressure) | Up to $N$ active | 1 (event-driven) | $\approx N$ cores total (naturally time-shared via backpressure) |
| **Post-Assembly Backlog Drain** | 0 (terminated) | Up to $N$ active | 1 (event-driven) | $N$ cores (100% disassembler) |
| **Explicit `BURRTOOLS_THREADS = K`** | $K$ max | $K$ max | 1 (event-driven) | Automatically throttled to $\le K$ active runnable threads |

**CLI Parsing Contract:**
- `burrTxt -t N`: Must clamp negative values: `int t = atoi(arg); unsigned threads = t > 0 ? (unsigned)t : 0;`.
- Never pass unvalidated or wrapped `unsigned` values to thread constructors.

---

### 2.3 Shared State & Lazy-Initialization Invariant

**The Law of Immutability:** Any data structure accessed concurrently by multiple threads without a mutex must be **strictly immutable**.

**The Pre-Warming Invariant:** All lazily initialized `mutable` fields in shared objects must be initialized on the orchestrator thread **before** worker threads are launched:
1. `gridType_c::getSymmetries()`: Constructing a single instance touches `sym`. The orchestrator must call `puzzle.getGridType()->getSymmetries()` prior to worker creation.
2. `voxel_c::symmetries` and `voxel_c::BbHsCache`: The orchestrator must iterate over all puzzle shapes and call `s->initHotspot()` and warm bounding-box caches prior to worker creation.
3. Once workers are running, these pointers are read-only and require zero locks.

---

### 2.4 Bounded Queues & Dual-Sided Backpressure

To prevent unbounded memory growth when the assembler produces assemblies faster than the disassemblers can process them:
1. **`work_queue` (Input Queue):** Bounded at `MAX_QUEUE_SIZE = 64`. When full, `pool->submit()` blocks the assembler thread.
2. **`reorder_buffer` (Output Buffer):** Bounded at `MAX_REORDER_SIZE = 128`. If assembly $K$ takes 10 seconds while assemblies $K+1 \dots K+200$ finish in 1 ms, workers must **not** continue accumulating completed results in memory. When `reorder_buffer.size() >= MAX_REORDER_SIZE`, disassembler workers must wait on a condition variable `cv_reorder` until the merger drains earlier sequence numbers.

---

### 2.5 Lifecycle, Cancellation, and Exception Safety

1. **Clean Shutdown (`finish()`):**
   ```cpp
   void finish() {
     {
       std::lock_guard<std::mutex> lock(queue_mutex);
       finished = true;
     }
     cv_worker.notify_all(); // Wakes all idle workers
     for (auto &w : workers) if (w.joinable()) w.join();
     // Signal merger that all workers are done
     {
       std::lock_guard<std::mutex> lock(reorder_mutex);
       merger_finished = true;
     }
     cv_merger.notify_all();
     if (merger.joinable()) merger.join();
   }
   ```
   - **Lost-Wakeup Prevention:** `finished = true` MUST be set under `queue_mutex`, and `cv_worker.notify_all()` must be called to wake threads waiting in `cv_worker.wait(lock, [&]{ return !queue.empty() || finished || aborted; })`.

2. **Asynchronous Cancellation (`abort()`):**
   - Must set `aborted = true` under lock and broadcast to `cv_worker`, `cv_merger`, and `cv_reorder`.
   - Disassembler workers must check `aborted.load(std::memory_order_relaxed)` inside their inner BFS loop (`disassemble_rec`).
   - `abort()` returns immediately. Worker joins occur inside `solveThread_c`, never on the GUI thread.

3. **Exception Safety:**
   - Every worker thread entry point (`worker_fn`) must wrap its body in:
     ```cpp
     try {
       worker_loop();
     } catch (...) {
       std::lock_guard<std::mutex> lock(exception_mutex);
       if (!stored_exception) stored_exception = std::current_exception();
       aborted = true;
       cv_worker.notify_all();
     }
     ```
   - The orchestrator rethrows `stored_exception` upon `join()`, preventing `std::terminate` and allowing BurrTools to display an error modal cleanly.

---

## 3. The "Is" Assessment (Gap Analysis across PR Stack)

| PR | Component | Current Implementation ("Is") | Architectural Hazard | Severity |
| :--- | :--- | :--- | :--- | :---: |
| **#84** | `disassemblerpool.cpp:189` | `finished` set and `cv_worker.notify_all()` called **without holding `queue_mutex`**. | **Lost Wakeup:** Worker checks `finished` before lock, sleeps, misses signal, hangs solver forever. | **CRITICAL** |
| **#84** | `disassemblerpool.cpp:137` | `reorder_buffer` is an unbounded `std::map<uint64_t, Result>`. | **OOM / Unbounded Memory:** If one assembly is slow, fast assemblies pile up indefinitely. | **HIGH** |
| **#84** | `disassemblerpool.cpp:67` | Workers concurrently call `puzzle.getGridType()->getSymmetries()`. | **Data Race / Use-After-Free:** Unsynchronized lazy init on `mutable std::unique_ptr<symmetries_c> sym`. | **CRITICAL** |
| **#84** | `solvethread.cpp:152` | Assembler pool claims $N$ threads, disassembler pool claims $N$ threads. | **2x Oversubscription:** $2N+1$ threads spawned on an $N$-core system; invalidates benchmarks. | **HIGH** |
| **#84** | `solvethread.cpp:231` | `stopInternal()` calls `pool->abort()`, then blocks waiting for worker drain. | **GUI Hang:** Canceling a slow solve freezes the GUI until all active disassemblies finish. | **HIGH** |
| **#84** | `burrTxt.cpp:173` | `threads = atoi(args[i+1])` without negative check; passed to `unsigned int`. | **Crash / Denial of Service:** `-t -1` tries to spawn $2^{32}-1$ threads, calling `std::terminate`. | **MEDIUM** |
| **#84** | `meson.build:100` | Zero tests for `disassemblerPool_c` in `test/`. | **Unverified Concurrency:** Deterministic reorder buffer and lifecycle untested under TSAN. | **HIGH** |
| **#79/#81** | `assembler_0.cpp:1994` | Workers concurrently access shared `voxel_c` shapes. | **Data Race:** Concurrent first-touch of `symmetries` and `BbHsCache` in `voxel_c`. | **HIGH** |
| **#79/#81** | `assembler_0/1.cpp` | Workers lack `try/catch` handlers. | **Crash on Assertion:** A failed `bt_assert` calls `std::terminate` rather than showing GUI error. | **MEDIUM** |
| **#82/#83** | `assembler_0/1.cpp` | `canUseSimd()` returns true even when resuming a search (`setPosition`). | **Search Corruption:** Resuming a saved search restarts from scratch under SIMD, losing state. | **CRITICAL** |
| **#78** | `movementanalysator.cpp:50` | `moved` diff in `prepare()` compares only x/y/z, omitting `getTrans()`. | **Silent Miscalculation:** If piece orientation changes without position change, stale matrix used. | **MEDIUM** |
| **#78** | `movementanalysator.cpp:42` | `pieces == prevPieces` compares raw pointer that dangles after `checkSubproblem`. | **Stale Pointer Hazard:** Subproblem frame reuse at same stack address could falsely match. | **MEDIUM** |

---

## 4. Actionable Remediation Plan

We address these issues systematically across the stack:

### Step 1: Fix Core Concurrency Hazards in PR #84 (`disassembler-pool`)
1. **Fix Lost Wakeup in `finish()`:**
   - Enclose `finished = true` within `std::lock_guard<std::mutex> lock(queue_mutex);`.
2. **Bound `reorder_buffer`:**
   - Add `std::condition_variable cv_reorder;` with `MAX_REORDER_SIZE = 128`. Disassembler workers wait if `reorder_buffer.size() >= MAX_REORDER_SIZE`.
3. **Pre-warm `getSymmetries()`:**
   - In `disassemblerPool_c` constructor, explicitly call `puzzle.getPuzzle().getGridType()->getSymmetries()` before spawning worker threads.
4. **Fix Thread Budget Oversubscription:**
   - In `solveThread_c::run()`, split effective thread count:
     `unsigned int total_threads = getEffectiveThreads();`
     `unsigned int disasm_threads = total_threads > 1 ? std::max(1u, total_threads / 3) : 0;`
     `unsigned int asm_threads = total_threads > disasm_threads ? total_threads - disasm_threads : 1;`
5. **Fix CLI Negative Thread Argument:**
   - Clamp in `burrTxt.cpp`: `threads = t > 0 ? (unsigned)t : 0;`.
6. **Add Comprehensive Concurrency Tests:**
   - Add Catch2 tests in `test/test_disassembler_pool.cpp` verifying:
     - Equivalence with synchronous baseline (`BURRTOOLS_NO_DISASM_POOL=1`).
     - Backpressure under heavy load (`MAX_QUEUE_SIZE` and `MAX_REORDER_SIZE`).
     - Abort mid-stream without deadlock or leak.

### Step 2: Fix Data Races & Exception Safety in PR #79 & #81 (`assembler-parallel`, `huang-parallel`)
1. **Pre-warm Voxel Caches:**
   - In `assembler_c::createMatrix()`, pre-warm bounding box and symmetry caches for all puzzle shapes before starting worker threads.
2. **Worker Exception Boundaries:**
   - Wrap assembler and disassembler worker loops in `try/catch` blocks storing `std::exception_ptr`.

### Step 3: Fix Resume Search & Clang Portability in PR #82 & #83 (`assembler-simd`, `more-simd`)
1. **Guard Resume against SIMD:**
   - In `assembler_0_c::canUseSimd()` and `assembler_1_c::canUseSimd()`, add `if (isResume()) return false;` so that resumed searches continue via DLX without restarting.
2. **Clang Portability:**
   - Ensure all target-specific intrinsics use proper function attributes (`__attribute__((target("avx2")))`) rather than `#pragma GCC target` which Clang ignores.

### Step 4: Fix Incremental Move Invariants in PR #78 (`disassembler-optimizations`)
1. **Include `getTrans()` in Moved Diff:**
   - Check `(searchnode->getTrans(i) != prevSearch->getTrans(i))` in `movementAnalysator_c::prepare()`.
2. **Harden `pieces == prevPieces`:**
   - Clear `prevPieces` and `prevSearch` in `init_find()` whenever subproblem context changes.
