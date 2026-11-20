/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02110-1307, USA.
 */
#ifndef __SOLVER_SOLVERSESSION_H__
#define __SOLVER_SOLVERSESSION_H__

/** @file
 * Solver session API: lifecycle controls (start, pause, state, resume, abort)
 * and a bounded pull queue of results (design/2026-11-19-ng-merge-plan.md,
 * sections 4.4 to 4.7; design/2026-11-20-core-api.md, sections 3 and 4).
 *
 * Pure core computation: no I/O, no XML, no problem_c or puzzle_c.
 */

#include "resumedata.h"
#include "solverspec.h"
#include "../assembler.h"
#include "../assembly.h"
#include "../bt_assert.h"
#include "../disassemblerpool.h"
#include "../disassembly.h"
#include "../rotationgeometry.h"
#include "../rotationtables.h"
#include "../solver_config.h"
#include "../thread_budget.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

/** Solving options for a session (no host storage/limit/sorting policies). */
struct solverOptions_t {
  bool reduce            = true;
  bool disassemble       = false;
  bool keepRotations     = false;
  bool completeRotations = false;
  bool rotationMoves     = false;
  /** count assemblies/solutions without enqueuing results. The engines still
   *  build each assembly internally; only the queue and host storage are
   *  skipped */
  bool countOnly         = false;
  /** soft backpressure bound while Running (0 = unbounded); lifted on pause()
   *  for the bounded O(num_threads) in-flight items so workers never deadlock */
  size_t queueCapacity   = 256;
  solverSettings_t settings;
};

/** One result produced by a session and owned by the consuming host. */
struct solverResult_t {
  /** discovery index: unique and gap-free over the whole search, also
   *  across pause/resume and setResume() (resumeData_t::nextSeq). The order
   *  of results is unspecified: with several threads the assembler hands
   *  assemblies on, and the disassembly pool delivers them, in no particular
   *  order, and the queue holds results as they arrive. Only the set and
   *  count of assemblies and solutions are promised. */
  uint64_t seq = 0;
  std::unique_ptr<assembly_c> assembly;
  /** null when disassemble is false, for single-piece assemblies, or when
   *  the assembly does not come apart */
  std::unique_ptr<separation_c> separation;
};

/** Summary published when a session enters Paused, Finished, Stopped, or Error. */
struct solverRunInfo_t {
  double seconds = 0.0;
  unsigned long iterations = 0;
  uint64_t assembliesFound = 0;
  uint64_t solutionsFound = 0;
  bool complete = false;
  std::optional<rotationGeometry::model_t> rotationModel;
  unsigned int rotationsSkipped = 0;
  assembler_c::errState error = assembler_c::ERR_NONE;
  int errorParam = 0;
};

enum class solverState_e {
  Idle,
  Running,
  Paused,
  Finished,
  Stopped,
  Error
};

/**
 * Self-contained solver session driving an assembler and optional
 * disassembler pool, publishing owned solverResult_t items into a bounded
 * thread-safe pull queue.
 */
class solverSession_c : private assembler_cb {
public:
  /** Fine-grained activity phases for progress display. */
  enum {
    ACT_PREPARATION,
    ACT_REDUCE,
    ACT_ASSEMBLING,
    ACT_DISASSEMBLING,
    ACT_PAUSING,
    ACT_FINISHED,
    ACT_ERROR,
    ACT_ASSERT,
    ACT_WAIT_TO_STOP
  };

  /** Optional wake hook: called with no data when the queue transitions
   *  empty -> non-empty or the session leaves Running. It may be called from
   *  solver threads, possibly while solver-internal locks are held (the
   *  assembler's callback mutex, the inline disassembler's mutex): it must
   *  be thread-safe, must not block, and must not call back into the
   *  session. Post an event to the host's own thread instead. */
  solverSession_c(std::shared_ptr<const solverSpec_t> spec,
                  solverOptions_t options,
                  std::function<void()> onWake = {});
  ~solverSession_c() override;

  solverSession_c(const solverSession_c &) = delete;
  solverSession_c & operator=(const solverSession_c &) = delete;

  /** Optional, only in Idle (before start()): supply a saved search to resume.
   *  Validated by start(); incompatible or damaged data causes start() to
   *  return false before any worker starts, leaving the session in Error. */
  void setResume(resumeData_t data);

  /** Start the session (Idle -> Running). Throws assert_exception if called
   *  outside Idle. Returns false (session in Error) if resume data cannot be
   *  restored, or false (session Stopped) if abort() ran meanwhile.
   *
   *  With stopAfterPreparation, start() returns at once on a fresh run (the
   *  session is Running while the matrix is built and reduced) and the
   *  session reaches Paused once preparation is done (onWake fires); hosts
   *  join() or wait for Paused before calling state(). With resume data the
   *  preparation runs synchronously inside start(), so the session may
   *  already be Paused when start() returns. */
  bool start(bool stopAfterPreparation = false);

  /** Stop new task dispatch, lift the soft queue cap so in-flight workers
   *  drain without deadlocking, join workers and pool, and transition
   *  Running -> Paused (or Finished if the search already completed).
   *  Retains the built matrix, reductions, rotation context and pending
   *  assemblies for in-place resume() or state(). */
  void pause(void);

  /** Non-blocking pause request: lifts the soft queue cap and signals the
   *  assembler and disassembler pool to stop. Safe from any thread, including
   *  worker test hooks. The session transitions to Paused (or Finished) once
   *  its worker thread unwinds (wait via join() or pop()). */
  void requestPause(void);

  /** Continue a Paused session in place (Paused -> Running) with its built
   *  matrix, reductions, rotation context and pending assemblies intact.
   *  Returns false if not Paused. */
  bool resume(void);

  /** Immediately cancel any running or paused work, clear queued and pending
   *  items, wake blocked producers and consumers, join workers if called from
   *  another thread, and transition to Stopped. Total and idempotent: unless
   *  the session already ended Finished or Error, it is Stopped when abort()
   *  returns (called from another thread than the session's own). In-flight
   *  disassembly jobs run to their end (they are not interruptible), queued
   *  ones are dropped at once. */
  void abort(void);

  /** Wait for the current worker thread phase (if any) to exit. */
  void join(void);

  /** Snapshot resumeData_t: ONLY legal while Paused (throws via bt_te
   *  otherwise). Clones pending assemblies, serialises remaining engine
   *  work and records the next seq number without consuming live state, so
   *  resume() stays valid. */
  resumeData_t state(void) const;

  /** Pull the next result on the host's thread. Returns an item immediately
   *  if one is queued (in any state); blocks while empty and Running; returns
   *  std::nullopt once empty and not Running, or at once after abort(). */
  std::optional<solverResult_t> pop(void);

  /** Timed pull: returns a queued item immediately if one is ready, waits up
   *  to timeout while Running, or returns std::nullopt on timeout or once
   *  empty and not Running. */
  std::optional<solverResult_t> popFor(std::chrono::milliseconds timeout);

  /** Non-blocking batch pull: appends up to maxItems queued results to out
   *  and returns how many were moved. */
  size_t drain(std::vector<solverResult_t> & out, size_t maxItems = SIZE_MAX);

  /** \name Lock-free polled status and progress (safe from any thread) */
  //@{
  solverState_e sessionState(void) const { return stateAtomic_.load(std::memory_order_acquire); }
  unsigned int currentAction(void) const { return action_.load(std::memory_order_acquire); }
  unsigned int currentActionParameter(void) const;
  float getProgress(void) const;
  unsigned long getIterations(void) const;
  uint64_t assembliesFound(void) const { return assembliesFound_.load(std::memory_order_relaxed); }
  uint64_t solutionsFound(void) const { return solutionsFound_.load(std::memory_order_relaxed); }
  std::optional<solverRunInfo_t> runInfo(void) const;

  bool rotationMoves(void) const { return rotationContext_ != nullptr; }
  const rotationContext_c * rotationContext(void) const { return rotationContext_.get(); }
  unsigned int rotationsSkipped(void) const;
  float getAssemblyBaseFraction(void) const { return assemblyBaseFraction_.load(std::memory_order_relaxed); }
  bool workOutstanding(void) const;
  /** The internal assertion that ended the session (Error with ACT_ASSERT). */
  const assert_exception & getAssertException(void) const { return assertEx_; }
  /** Any other exception that ended the session (Error with ACT_ERROR and
   *  runInfo()->error == ERR_NONE), e.g. std::bad_alloc from a worker; null
   *  otherwise. Hosts report it or rethrow it on their own thread. */
  std::exception_ptr exception(void) const;
  /** what() of exception(), or a generic text for non-std exceptions;
   *  empty when exception() is null. */
  std::string exceptionMessage(void) const;
  //@}

  static constexpr float runningCap = 0.999f;

  static float noEvidenceProgress(float assemblyFraction, float held) {
    return (assemblyFraction >= runningCap)
             ? held
             : ((assemblyFraction < 0.0f) ? 0.0f : assemblyFraction);
  }

  /** \name Deterministic test hooks (set while Idle or Paused) */
  //@{
  void setPoolDrainedHook(std::function<void()> hook);
  void setPoolSeedHook(std::function<void()> hook);
  void setAssemblyPhaseEndedHook(std::function<void()> hook);
  /** Read-only access to the live assembler pointer for test inspections. */
  assembler_c * assemblerForTest(void) const { return assmPtr_.load(std::memory_order_acquire); }
  /** Optional assembler_cb interceptor for deterministic phase/progress tests. */
  void setAssemblyInterceptor(assembler_cb * cb) { assemblyCb_ = cb; }
  /** Forward an intercepted assembly into this session's pipeline. */
  bool submitAssemblyForTest(std::unique_ptr<assembly_c> a) { return assembly(std::move(a)); }
  /** Expose the active phase's thread budget to test interceptors. */
  ThreadBudget * threadBudgetForTest(void) const { return threadBudget_.get(); }
  //@}

  /** \name Host problem_c / FLTK PlacementBrowser integration */
  //@{
  /** Adopt an already-prepared assembler (and any pending assemblies) in Idle. */
  void adoptPreparedAssembler(std::unique_ptr<assembler_c> existing,
                              std::vector<std::unique_ptr<assembly_c>> pending = {});
  /** Release ownership of the internal assembler when not Running. */
  std::unique_ptr<assembler_c> releaseAssembler(void);
  /** Optional hook invoked right after fresh preparation succeeds (before assembling). */
  void setPreparedHook(std::function<void()> hook);
  //@}

private:
  // assembler_cb overrides
  bool assembly(std::unique_ptr<assembly_c> a) override;
  ThreadBudget * threadBudget(void) override { return threadBudget_.get(); }

  void onDisassemblyResult(uint64_t seqNo, std::unique_ptr<assembly_c> a, std::unique_ptr<separation_c> s);
  bool enqueueResult(solverResult_t r);
  void runPhase(bool stopAfterPrep);
  void finishPhase(solverState_e nextState, unsigned int nextAction,
                   assembler_c::errState err, int errParam, double phaseSeconds);
  void failPhase(std::exception_ptr ex, double phaseSeconds);
  void teardownAfterFailure(void);
  void applyHooksToAssembler(void);
  void wakeHost(void);

  std::shared_ptr<const solverSpec_t> spec_;
  solverOptions_t options_;
  std::function<void()> onWake_;

  // Rotation context shared across pause/resume (nullptr when rotation moves off)
  std::shared_ptr<rotationContext_c> rotationContext_;

  // Saved resume data handed to setResume() before start()
  std::optional<resumeData_t> initialResume_;

  // Owned assembler, kept alive across in-place pause/resume
  std::unique_ptr<assembler_c> assm_;
  bool prepared_ = false;

  // Assemblies found before a pause that have not been disassembled yet
  std::vector<std::unique_ptr<assembly_c>> pending_;

  // Active phase machinery (created per Running phase when disassemble is true)
  std::unique_ptr<ThreadBudget> threadBudget_;
  std::unique_ptr<disassemblerPool_c> disasmPool_;

  // Guards active pointers so requestPause()/abort() never race teardown
  mutable std::mutex activeMutex_;
  assembler_c * activeAssm_ = nullptr;
  disassemblerPool_c * activePool_ = nullptr;

  // Lock-free published pointers for polling (written only on worker / start)
  std::atomic<assembler_c *> assmPtr_{nullptr};
  std::atomic<disassemblerPool_c *> poolPtr_{nullptr};

  // Lifecycle & bounded result queue
  mutable std::mutex queueMutex_;
  std::condition_variable cvNotEmpty_;
  std::condition_variable cvNotFull_;
  std::deque<solverResult_t> queue_;
  solverState_e state_ = solverState_e::Idle;
  std::atomic<solverState_e> stateAtomic_{solverState_e::Idle};
  std::atomic<unsigned int> action_{ACT_PREPARATION};
  std::atomic<bool> pauseRequested_{false};
  std::atomic<bool> abortRequested_{false};

  // Counters & timing. nextSeq_ is the next discovery index to hand out;
  // with a pool, seq = phaseSeqBase_ + the pool's submit number, and
  // nextSeq_ advances by the results delivered in the phase once it ends.
  std::atomic<uint64_t> nextSeq_{0};
  uint64_t phaseSeqBase_ = 0;
  std::atomic<uint64_t> phaseDelivered_{0};
  std::atomic<uint64_t> assembliesFound_{0};
  std::atomic<uint64_t> solutionsFound_{0};
  double accumulatedSeconds_ = 0.0;

  // Progress model state
  std::atomic<long long> assemblyStartNs_{0};
  std::atomic<long long> assemblyEndNs_{0};
  std::atomic<unsigned int> assemblyThreads_{1};
  mutable std::atomic<float> assemblyBaseFraction_{0.0f};
  mutable std::atomic<float> reportedProgress_{0.0f};
  // Cumulative completed/submitted counts across prior paused phases
  std::atomic<uint64_t> priorDisasmSubmitted_{0};
  std::atomic<uint64_t> priorDisasmCompleted_{0};
  std::atomic<double> priorDisasmCost_{0.0};

  // Run summary & error info
  mutable std::mutex runInfoMutex_;
  std::optional<solverRunInfo_t> runInfo_;
  assert_exception assertEx_;
  std::exception_ptr exception_;  // guarded by runInfoMutex_

  // Test & transitional hooks
  std::function<void()> poolDrainedHook_;
  std::function<void()> poolSeedHook_;
  std::function<void()> assemblyPhaseEndedHook_;
  std::function<void()> preparedHook_;
  assembler_cb * assemblyCb_ = nullptr;

  // Single driver thread (spawns assembler & pool workers)
  std::mutex threadMutex_;
  std::jthread workerThread_;
};

#endif // __SOLVER_SOLVERSESSION_H__
