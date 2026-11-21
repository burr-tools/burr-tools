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
#include "solversession.h"

#include "../progressmodel.h"

#include <chrono>
#include <cmath>
#include <iterator>

namespace {

long long steadyNowNs(void) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

resumeData_t resumeData_t::clone(void) const {
  resumeData_t copy;
  copy.engine = engine;
  copy.state = state;
  copy.nextSeq = nextSeq;
  copy.pending.reserve(pending.size());
  for (const auto & a : pending) {
    if (a)
      copy.pending.push_back(std::make_unique<assembly_c>(a.get()));
  }
  return copy;
}

solverSession_c::solverSession_c(std::shared_ptr<const solverSpec_t> spec,
                                 solverOptions_t options,
                                 std::function<void()> onWake)
  : spec_(std::move(spec)),
    options_(std::move(options)),
    onWake_(std::move(onWake))
{
  if (!spec_)
    bt_te("solverSession_c: null solverSpec_t");

  if (options_.disassemble && options_.rotationMoves)
    rotationContext_ = rotationContextFor(spec_, options_.settings.rotation, true);
}

solverSession_c::~solverSession_c() {
  abort();
  join();
}

void solverSession_c::setResume(resumeData_t data) {
  std::lock_guard<std::mutex> lock(queueMutex_);
  if (state_ != solverState_e::Idle)
    bt_te("solverSession_c::setResume: only legal in Idle state");
  initialResume_ = std::move(data);
}

void solverSession_c::adoptPreparedAssembler(std::unique_ptr<assembler_c> existing,
                                             std::vector<std::unique_ptr<assembly_c>> pending) {
  std::lock_guard<std::mutex> lock(queueMutex_);
  if (state_ != solverState_e::Idle)
    bt_te("solverSession_c::adoptPreparedAssembler: only legal in Idle state");
  assm_ = std::move(existing);
  prepared_ = (assm_ != nullptr);
  pending_ = std::move(pending);
  assmPtr_.store(assm_.get(), std::memory_order_release);
}

std::unique_ptr<assembler_c> solverSession_c::releaseAssembler(void) {
  const solverState_e st = sessionState();
  if (st == solverState_e::Running)
    bt_te("solverSession_c::releaseAssembler: cannot release while Running");
  join();
  {
    std::lock_guard<std::mutex> lock(activeMutex_);
    activeAssm_ = nullptr;
  }
  assmPtr_.store(nullptr, std::memory_order_release);
  prepared_ = false;
  return std::move(assm_);
}

void solverSession_c::setPoolDrainedHook(std::function<void()> hook) {
  poolDrainedHook_ = std::move(hook);
  if (assm_ && sessionState() != solverState_e::Running)
    assm_->setPoolDrainedHook(poolDrainedHook_);
}

void solverSession_c::setPoolSeedHook(std::function<void()> hook) {
  poolSeedHook_ = std::move(hook);
  if (assm_ && sessionState() != solverState_e::Running)
    assm_->setPoolSeedHook(poolSeedHook_);
}

void solverSession_c::setAssemblyPhaseEndedHook(std::function<void()> hook) {
  assemblyPhaseEndedHook_ = std::move(hook);
}

void solverSession_c::setPreparedHook(std::function<void()> hook) {
  preparedHook_ = std::move(hook);
}

void solverSession_c::applyHooksToAssembler(void) {
  if (!assm_)
    return;
  if (poolDrainedHook_)
    assm_->setPoolDrainedHook(poolDrainedHook_);
  if (poolSeedHook_)
    assm_->setPoolSeedHook(poolSeedHook_);
}

void solverSession_c::wakeHost(void) {
  if (onWake_)
    onWake_();
}

bool solverSession_c::start(bool stopAfterPreparation) {
  std::optional<resumeData_t> resume;
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (state_ != solverState_e::Idle)
      bt_te("solverSession_c::start: session already started");
    resume.swap(initialResume_);
  }

  // Validate resume data synchronously before launching any worker threads
  // (design/2026-11-20-core-api.md, sections 4.1 and 4.4).
  bool hadResume = false;
  std::vector<std::unique_ptr<assembly_c>> restoredPending;
  if (resume) {
    hadResume = true;
    const auto t0 = std::chrono::steady_clock::now();
    auto elapsed = [&]() {
      return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    };
    resumeData_t res = std::move(*resume);
    resume.reset();

    try {
      const unsigned int numPieces = spec_->getNumberOfPieces();
      for (const auto & a : res.pending) {
        if (!a || a->placementCount() != numPieces) {
          finishPhase(solverState_e::Error, ACT_ERROR,
                      assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX, 0, elapsed());
          return false;
        }
      }

      if (!prepared_) {
        action_.store(ACT_PREPARATION, std::memory_order_release);
        std::unique_ptr<assembler_c> newAssm = gridType_c::findAssembler(spec_, options_.settings);
        if (!newAssm) {
          finishPhase(solverState_e::Error, ACT_ERROR,
                      assembler_c::ERR_PUZZLE_UNHANDABLE, 0, elapsed());
          return false;
        }
        assmPtr_.store(newAssm.get(), std::memory_order_release);

        assembler_c::errState err = newAssm->createMatrix(options_.keepRotations, options_.completeRotations);
        if (err != assembler_c::ERR_NONE) {
          int param = newAssm->getErrorsParam();
          assmPtr_.store(nullptr, std::memory_order_release);
          finishPhase(solverState_e::Error, ACT_ERROR, err, param, elapsed());
          return false;
        }

        if (options_.reduce) {
          action_.store(ACT_REDUCE, std::memory_order_release);
          newAssm->reduce();
        }

        assm_ = std::move(newAssm);
        prepared_ = true;
      }

      assembler_c::errState err = assm_->setPosition(res.state.c_str(), res.engine.c_str());
      if (err != assembler_c::ERR_NONE) {
        assmPtr_.store(nullptr, std::memory_order_release);
        assm_.reset();
        prepared_ = false;
        finishPhase(solverState_e::Error, ACT_ERROR, err, 0, elapsed());
        return false;
      }

      applyHooksToAssembler();
      restoredPending = std::move(res.pending);
      nextSeq_.store(res.nextSeq, std::memory_order_relaxed);
      assmPtr_.store(assm_.get(), std::memory_order_release);

      if (stopAfterPreparation) {
        {
          std::lock_guard<std::mutex> lock(queueMutex_);
          if (state_ != solverState_e::Idle)
            return false; // abort() ran during the validation: stays Stopped
          pending_ = std::move(restoredPending);
        }
        finishPhase(solverState_e::Paused, ACT_PAUSING,
                    assembler_c::ERR_NONE, 0, elapsed());
        return true;
      }
    } catch (const assert_exception & a) {
      assertEx_ = a;
      assmPtr_.store(nullptr, std::memory_order_release);
      assm_.reset();
      prepared_ = false;
      finishPhase(solverState_e::Error, ACT_ASSERT,
                  assembler_c::ERR_NONE, 0, elapsed());
      return false;
    } catch (...) {
      assmPtr_.store(nullptr, std::memory_order_release);
      assm_.reset();
      prepared_ = false;
      failPhase(std::current_exception(), elapsed());
      return false;
    }
  }

  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    // abort() may have run while the resume data was validated above (it
    // finalises an Idle session to Stopped itself): never revive it.
    if (state_ != solverState_e::Idle)
      return false;
    if (hadResume)
      pending_ = std::move(restoredPending);
    pauseRequested_.store(false, std::memory_order_release);
    abortRequested_.store(false, std::memory_order_release);
    state_ = solverState_e::Running;
    stateAtomic_.store(solverState_e::Running, std::memory_order_release);
  }

  std::lock_guard<std::mutex> tlock(threadMutex_);
  workerThread_ = std::jthread([this, stopAfterPreparation]() {
    runPhase(stopAfterPreparation);
  });
  return workerThread_.joinable();
}

void solverSession_c::requestPause(void) {
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (state_ != solverState_e::Running)
      return;
    pauseRequested_.store(true, std::memory_order_release);
    // SPEC-SESS-2: lift the soft queueCapacity cap so workers blocked in
    // enqueueResult() can push their bounded O(num_threads) in-flight items
    // and park without deadlocking even if the host stopped calling pop().
    cvNotFull_.notify_all();
  }

  unsigned int cur = action_.load(std::memory_order_acquire);
  if (cur == ACT_PREPARATION || cur == ACT_REDUCE ||
      cur == ACT_ASSEMBLING || cur == ACT_DISASSEMBLING) {
    action_.store(ACT_WAIT_TO_STOP, std::memory_order_release);
  }

  {
    // Both calls are non-blocking. stopSticky(): a stop landing between
    // runPhase's pauseRequested_ check and assemble()'s beginRun() must not
    // be lost with the stop source beginRun() replaces.
    std::lock_guard<std::mutex> alock(activeMutex_);
    if (activeAssm_)
      activeAssm_->stopSticky();
    if (activePool_)
      activePool_->requestStop();
  }
}

void solverSession_c::pause(void) {
  requestPause();
  join();
}

bool solverSession_c::resume(void) {
  // Ensure the previous phase's driver thread has joined before reusing it.
  join();

  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (state_ != solverState_e::Paused)
      return false;
    if (!prepared_ || !assm_)
      return false;
    pauseRequested_.store(false, std::memory_order_release);
    abortRequested_.store(false, std::memory_order_release);
    state_ = solverState_e::Running;
    stateAtomic_.store(solverState_e::Running, std::memory_order_release);
  }

  applyHooksToAssembler();

  std::lock_guard<std::mutex> tlock(threadMutex_);
  workerThread_ = std::jthread([this]() {
    runPhase(false);
  });
  return workerThread_.joinable();
}

void solverSession_c::abort(void) {
  bool finalisedHere = false;
  bool wasPaused = false;
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    abortRequested_.store(true, std::memory_order_release);
    pauseRequested_.store(true, std::memory_order_release);
    queue_.clear();
    // Idle and Paused have no driver thread deciding the end state: finalise
    // here. A Running session is finalised by finishPhase(), which reads
    // abortRequested_ under this same mutex (SPEC-SESS-6), so no driver can
    // leave the session Paused once this block has run.
    if (state_ == solverState_e::Paused || state_ == solverState_e::Idle) {
      wasPaused = (state_ == solverState_e::Paused);
      state_ = solverState_e::Stopped;
      stateAtomic_.store(solverState_e::Stopped, std::memory_order_release);
      pending_.clear();
      initialResume_.reset();
      finalisedHere = true;
    }
    cvNotFull_.notify_all();
    cvNotEmpty_.notify_all();
  }

  {
    // Non-blocking calls only: the driver may be inside the pool's finish(),
    // holding the pool's lifecycle mutex while it drains, so the pool's own
    // abort() would wait out the whole queue here (and stall every poller
    // of activeMutex_). requestStop() drops the queued jobs at once; the
    // driver then finishes (or aborts) the pool and ends the phase Stopped.
    std::lock_guard<std::mutex> alock(activeMutex_);
    if (activeAssm_)
      activeAssm_->stopSticky();
    if (activePool_)
      activePool_->requestStop();
  }

  join();

  if (finalisedHere) {
    {
      std::lock_guard<std::mutex> rlock(runInfoMutex_);
      solverRunInfo_t info;
      // From Idle, start() may still be validating resume data on another thread.
      info.seconds = wasPaused ? accumulatedSeconds_ : 0.0;
      info.iterations = (wasPaused && assm_) ? assm_->getIterations() : 0;
      info.assembliesFound = assembliesFound_.load(std::memory_order_relaxed);
      info.solutionsFound = solutionsFound_.load(std::memory_order_relaxed);
      info.complete = false;
      if (rotationContext_) {
        info.rotationModel = spec_->getRotationModel().value_or(options_.settings.rotation.model);
        info.rotationsSkipped = rotationContext_->getSkipped();
      }
      runInfo_ = info;
    }
    wakeHost();
  }
}

void solverSession_c::join(void) {
  std::jthread toJoin;
  {
    std::lock_guard<std::mutex> tlock(threadMutex_);
    if (workerThread_.joinable() && workerThread_.get_id() != std::this_thread::get_id())
      toJoin = std::move(workerThread_);
  }
  if (toJoin.joinable())
    toJoin.join();
}

resumeData_t solverSession_c::state(void) const {
  if (sessionState() != solverState_e::Paused)
    bt_te("solverSession_c::state: only legal while Paused");
  if (!assm_)
    bt_te("solverSession_c::state: no prepared assembler");

  // Workers are parked/joined while Paused, so reading assm_ and pending_ is race-free.
  auto [engine, engineState] = assm_->saveState();
  resumeData_t out;
  out.engine = std::move(engine);
  out.state = std::move(engineState);
  out.nextSeq = nextSeq_.load(std::memory_order_relaxed);
  out.pending.reserve(pending_.size());
  for (const auto & a : pending_) {
    if (a)
      out.pending.push_back(std::make_unique<assembly_c>(a.get()));
  }
  return out;
}

std::optional<solverResult_t> solverSession_c::pop(void) {
  std::unique_lock<std::mutex> lock(queueMutex_);
  // abort() empties the queue and sets abortRequested_ under this mutex: a
  // blocked pop() returns at once instead of waiting for the driver to wind
  // down in-flight disassembly jobs.
  cvNotEmpty_.wait(lock, [this]() {
    return !queue_.empty() || state_ != solverState_e::Running ||
           abortRequested_.load(std::memory_order_acquire);
  });
  if (queue_.empty())
    return std::nullopt;
  solverResult_t r = std::move(queue_.front());
  queue_.pop_front();
  cvNotFull_.notify_one();
  return r;
}

size_t solverSession_c::drain(std::vector<solverResult_t> & out, size_t maxItems) {
  if (maxItems == 0)
    return 0;
  size_t taken = 0;
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (taken < maxItems && !queue_.empty()) {
      out.push_back(std::move(queue_.front()));
      queue_.pop_front();
      taken++;
    }
    if (taken > 0)
      cvNotFull_.notify_all();
  }
  return taken;
}

// SPEC-SESS-2: bounded pull queue; the soft cap is lifted on pauseRequested_
// (and abort) so in-flight producers never wedge against a full queue.
//
// No producer holds a ThreadBudget token here, so none is yielded across the
// cvNotFull_ wait (SPEC-SESS-5): assembly-only runs have no budget at all,
// the inline pool runs without one and a pool worker returns its token
// before it delivers (disassemblerPool_c::worker_loop).
bool solverSession_c::enqueueResult(solverResult_t r) {
  bt_assert(!threadBudget_ || !ThreadBudget::holdsHere(threadBudget_.get()));
  bool wasEmpty = false;
  {
    std::unique_lock<std::mutex> lock(queueMutex_);
    if (options_.queueCapacity > 0) {
      cvNotFull_.wait(lock, [this]() {
        return queue_.size() < options_.queueCapacity ||
               pauseRequested_.load(std::memory_order_acquire) ||
               abortRequested_.load(std::memory_order_acquire);
      });
    }
    if (abortRequested_.load(std::memory_order_acquire))
      return false;
    wasEmpty = queue_.empty();
    queue_.push_back(std::move(r));
  }
  cvNotEmpty_.notify_one();
  // May run on a solver thread under solver-internal locks (the assembler's
  // callback mutex, the inline pool's mutex): see the onWake contract.
  if (wasEmpty)
    wakeHost();
  return true;
}

bool solverSession_c::assembly(std::unique_ptr<assembly_c> a) {
  if (abortRequested_.load(std::memory_order_acquire))
    return false;

  if (options_.disassemble) {
    bt_assert(disasmPool_);
    disasmPool_->submit(std::move(a));
    return !disasmPool_->isAborted() &&
           !disasmPool_->isStopRequested() &&
           !pauseRequested_.load(std::memory_order_acquire) &&
           !abortRequested_.load(std::memory_order_acquire);
  }

  // Assembly-only mode
  assembliesFound_.fetch_add(1, std::memory_order_relaxed);
  if (!options_.countOnly) {
    const uint64_t seq = nextSeq_.fetch_add(1, std::memory_order_relaxed);
    if (!enqueueResult(solverResult_t{seq, std::move(a), nullptr}))
      return false;
  }
  return !pauseRequested_.load(std::memory_order_acquire) &&
         !abortRequested_.load(std::memory_order_acquire);
}

void solverSession_c::onDisassemblyResult(uint64_t seqNo,
                                          std::unique_ptr<assembly_c> a,
                                          std::unique_ptr<separation_c> s) {
  if (abortRequested_.load(std::memory_order_acquire))
    return;

  const bool isSolution = (a && a->placementCount() <= 1) || (s != nullptr);
  assembliesFound_.fetch_add(1, std::memory_order_relaxed);
  if (isSolution)
    solutionsFound_.fetch_add(1, std::memory_order_relaxed);
  phaseDelivered_.fetch_add(1, std::memory_order_relaxed);

  if (!options_.countOnly) {
    // The pool numbers assemblies in submit order (pending re-submissions
    // first) and delivers them in any order. A pause salvages a suffix of
    // that numbering (the never-started queue), so the delivered numbers of
    // a phase are exactly phaseSeqBase_ .. phaseSeqBase_ + delivered - 1.
    enqueueResult(solverResult_t{phaseSeqBase_ + seqNo, std::move(a), std::move(s)});
  }
}

void solverSession_c::finishPhase(solverState_e nextState, unsigned int nextAction,
                                  assembler_c::errState err, int errParam,
                                  double phaseSeconds) {
  bool publish = true;
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    accumulatedSeconds_ += phaseSeconds;

    if (state_ == solverState_e::Stopped) {
      // abort() finalised this session itself (from Idle, while start() was
      // validating resume data): keep Stopped and its run summary.
      publish = false;
    } else {
      // SPEC-SESS-1 / SPEC-SESS-6: the end-of-phase decision and abort()
      // are ordered by queueMutex_. An abort that arrived after the
      // driver's last check still wins: a session that would end Paused or
      // Finished ends Stopped, with its queue and pending assemblies
      // discarded, so abort() never returns with a resumable session.
      if (abortRequested_.load(std::memory_order_acquire) &&
          (nextState == solverState_e::Paused || nextState == solverState_e::Finished)) {
        nextState = solverState_e::Stopped;
        nextAction = ACT_PAUSING;
      }
      if (nextState == solverState_e::Stopped) {
        queue_.clear();
        pending_.clear();
      }

      {
        std::lock_guard<std::mutex> rlock(runInfoMutex_);
        solverRunInfo_t info;
        info.seconds = accumulatedSeconds_;
        info.iterations = assm_ ? assm_->getIterations() : 0;
        info.assembliesFound = assembliesFound_.load(std::memory_order_relaxed);
        info.solutionsFound = solutionsFound_.load(std::memory_order_relaxed);
        info.complete = (nextState == solverState_e::Finished);
        if (rotationContext_) {
          info.rotationModel = spec_->getRotationModel().value_or(options_.settings.rotation.model);
          info.rotationsSkipped = rotationContext_->getSkipped();
        }
        info.error = err;
        info.errorParam = errParam;
        runInfo_ = info;
      }

      action_.store(nextAction, std::memory_order_release);
      state_ = nextState;
      stateAtomic_.store(nextState, std::memory_order_release);
    }
    cvNotEmpty_.notify_all();
    cvNotFull_.notify_all();
  }

  if (publish)
    wakeHost();
}

void solverSession_c::failPhase(std::exception_ptr ex, double phaseSeconds) {
  {
    std::lock_guard<std::mutex> rlock(runInfoMutex_);
    exception_ = std::move(ex);
  }
  finishPhase(solverState_e::Error, ACT_ERROR,
              assembler_c::ERR_NONE, 0, phaseSeconds);
}

void solverSession_c::teardownAfterFailure(void) {
  {
    // Lift the soft cap first: a pool worker may sit in enqueueResult()
    // on a full queue, and the pool's abort() below joins it.
    std::lock_guard<std::mutex> lock(queueMutex_);
    pauseRequested_.store(true, std::memory_order_release);
    cvNotFull_.notify_all();
  }
  if (disasmPool_) {
    // The pool may have died with the exception being handled; abort()
    // joins what is left and must not throw again.
    try {
      disasmPool_->abort();
    } catch (...) {
    }
    poolPtr_.store(nullptr, std::memory_order_release);
    {
      std::lock_guard<std::mutex> alock(activeMutex_);
      activePool_ = nullptr;
    }
    disasmPool_.reset();
    threadBudget_.reset();
  }
  {
    std::lock_guard<std::mutex> alock(activeMutex_);
    activeAssm_ = nullptr;
  }
}

std::exception_ptr solverSession_c::exception(void) const {
  std::lock_guard<std::mutex> rlock(runInfoMutex_);
  return exception_;
}

std::string solverSession_c::exceptionMessage(void) const {
  std::exception_ptr ex = exception();
  if (!ex)
    return {};
  try {
    std::rethrow_exception(ex);
  } catch (const std::exception & e) {
    return e.what();
  } catch (...) {
    return "Unknown C++ exception occurred during solving";
  }
}

void solverSession_c::runPhase(bool stopAfterPrep) {
  const auto phaseStart = std::chrono::steady_clock::now();
  auto elapsed = [&]() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - phaseStart).count();
  };

  try {
    // 1. Preparation (fresh run only; skipped on in-place resume or setResume)
    if (!prepared_) {
      action_.store(ACT_PREPARATION, std::memory_order_release);
      std::unique_ptr<assembler_c> newAssm = gridType_c::findAssembler(spec_, options_.settings);
      if (!newAssm) {
        finishPhase(solverState_e::Error, ACT_ERROR,
                    assembler_c::ERR_PUZZLE_UNHANDABLE, 0, elapsed());
        return;
      }

      assmPtr_.store(newAssm.get(), std::memory_order_release);
      {
        std::lock_guard<std::mutex> alock(activeMutex_);
        activeAssm_ = newAssm.get();
      }

      assembler_c::errState err = newAssm->createMatrix(options_.keepRotations, options_.completeRotations);
      if (err != assembler_c::ERR_NONE) {
        int param = newAssm->getErrorsParam();
        {
          std::lock_guard<std::mutex> alock(activeMutex_);
          activeAssm_ = nullptr;
        }
        assmPtr_.store(nullptr, std::memory_order_release);
        finishPhase(solverState_e::Error, ACT_ERROR, err, param, elapsed());
        return;
      }

      if (options_.reduce && !abortRequested_.load(std::memory_order_acquire)) {
        if (!pauseRequested_.load(std::memory_order_acquire))
          action_.store(ACT_REDUCE, std::memory_order_release);
        newAssm->reduce();
      }

      assm_ = std::move(newAssm);
      prepared_ = true;
      applyHooksToAssembler();
      assmPtr_.store(assm_.get(), std::memory_order_release);
      if (preparedHook_)
        preparedHook_();
    } else {
      std::lock_guard<std::mutex> alock(activeMutex_);
      activeAssm_ = assm_.get();
      // A sticky stop left over from the previous phase's pause must not
      // stop this one; a pause from here on is either seen by the checks
      // below or re-armed by requestPause() through activeAssm_.
      assm_->clearStickyStop();
      applyHooksToAssembler();
    }

    if (abortRequested_.load(std::memory_order_acquire)) {
      {
        std::lock_guard<std::mutex> alock(activeMutex_);
        activeAssm_ = nullptr;
      }
      finishPhase(solverState_e::Stopped, ACT_PAUSING,
                  assembler_c::ERR_NONE, 0, elapsed());
      return;
    }

    if (stopAfterPrep || pauseRequested_.load(std::memory_order_acquire)) {
      {
        std::lock_guard<std::mutex> alock(activeMutex_);
        activeAssm_ = nullptr;
      }
      finishPhase(solverState_e::Paused, ACT_PAUSING,
                  assembler_c::ERR_NONE, 0, elapsed());
      return;
    }

    // 2. Set up disassembly pool for this phase (reusing rotationContext_)
    if (options_.disassemble) {
      phaseSeqBase_ = nextSeq_.load(std::memory_order_relaxed);
      phaseDelivered_.store(0, std::memory_order_relaxed);
      disasmPool_ = std::make_unique<disassemblerPool_c>(
        spec_,
        options_.settings,
        [this](uint64_t seqNo, std::unique_ptr<assembly_c> a, std::unique_ptr<separation_c> s) {
          onDisassemblyResult(seqNo, std::move(a), std::move(s));
        },
        rotationContext_
      );
      if (options_.settings.threadBudget && !disasmPool_->isInline()) {
        threadBudget_ = std::make_unique<ThreadBudget>(disasmPool_->threadCount());
        disasmPool_->setBudget(threadBudget_.get());
      }
      {
        std::lock_guard<std::mutex> alock(activeMutex_);
        activePool_ = disasmPool_.get();
        // If pause/abort raced pool creation, forward it immediately.
        if (abortRequested_.load(std::memory_order_acquire))
          activePool_->abort();
        else if (pauseRequested_.load(std::memory_order_acquire))
          activePool_->requestStop();
      }
      poolPtr_.store(disasmPool_.get(), std::memory_order_release);
    }

    // 3. Pending assemblies FIRST: they take the lowest seq numbers of the
    //    phase, so a pause's salvage stays a suffix (gap-free seq)
    if (!pending_.empty() &&
        !pauseRequested_.load(std::memory_order_acquire) &&
        !abortRequested_.load(std::memory_order_acquire)) {
      std::vector<std::unique_ptr<assembly_c>> toSubmit;
      toSubmit.swap(pending_);
      for (size_t i = 0; i < toSubmit.size(); ++i) {
        if (pauseRequested_.load(std::memory_order_acquire) ||
            abortRequested_.load(std::memory_order_acquire)) {
          for (size_t j = i; j < toSubmit.size(); ++j)
            if (toSubmit[j]) pending_.push_back(std::move(toSubmit[j]));
          break;
        }
        if (disasmPool_) {
          if (!disasmPool_->submit(std::move(toSubmit[i]))) {
            // submit() salvaged toSubmit[i] into disasmPool_; keep remaining items
            // in order after the pool's salvage.
            for (size_t j = i + 1; j < toSubmit.size(); ++j)
              if (toSubmit[j]) pending_.push_back(std::move(toSubmit[j]));
            break;
          }
        } else {
          // No disassembly this phase (a run that stopped with queued
          // disassembly work, continued without disassembling): deliver the
          // stash as plain assemblies, in order, as the assembler would.
          assembliesFound_.fetch_add(1, std::memory_order_relaxed);
          if (!options_.countOnly) {
            const uint64_t seq = nextSeq_.fetch_add(1, std::memory_order_relaxed);
            if (!enqueueResult(solverResult_t{seq, std::move(toSubmit[i]), nullptr}))
              break; // aborted: everything is discarded anyway
          }
        }
      }
    }

    // 4. Assembly phase
    if (!pauseRequested_.load(std::memory_order_acquire) &&
        !abortRequested_.load(std::memory_order_acquire)) {
      assm_->setNumThreads(options_.settings.threads);
      const unsigned int threads = assm_->getRunThreads();
      assemblyThreads_.store(threads ? threads : 1, std::memory_order_relaxed);

      const float base = assm_->getFinished();
      assemblyBaseFraction_.store((base > 0.0f && base < 1.0f) ? base : 0.0f,
                                  std::memory_order_relaxed);

      assemblyEndNs_.store(0, std::memory_order_relaxed);
      assemblyStartNs_.store(steadyNowNs(), std::memory_order_release);

      action_.store(ACT_ASSEMBLING, std::memory_order_release);
      assm_->assemble(assemblyCb_ ? assemblyCb_ : this);

      if (assemblyPhaseEndedHook_)
        assemblyPhaseEndedHook_();

      assemblyEndNs_.store(steadyNowNs(), std::memory_order_release);
    }

    // 5. Drain / tear down disassembly pool and collect salvaged assemblies
    if (disasmPool_) {
      if (!pauseRequested_.load(std::memory_order_acquire) &&
          !abortRequested_.load(std::memory_order_acquire))
        action_.store(ACT_DISASSEMBLING, std::memory_order_release);

      if (abortRequested_.load(std::memory_order_acquire)) {
        disasmPool_->abort();
      } else {
        disasmPool_->finish();
        auto salvaged = disasmPool_->takeSalvaged();
        if (!salvaged.empty()) {
          if (pending_.empty()) {
            pending_ = std::move(salvaged);
          } else {
            salvaged.insert(salvaged.end(),
                            std::make_move_iterator(pending_.begin()),
                            std::make_move_iterator(pending_.end()));
            pending_ = std::move(salvaged);
          }
        }
      }
      nextSeq_.store(phaseSeqBase_ + phaseDelivered_.load(std::memory_order_relaxed),
                     std::memory_order_relaxed);

      // Roll this phase's disassembly metrics into cumulative totals before destroying pool
      priorDisasmSubmitted_.fetch_add(disasmPool_->submittedCount(), std::memory_order_relaxed);
      priorDisasmCompleted_.fetch_add(disasmPool_->completedCount(), std::memory_order_relaxed);
      priorDisasmCost_.store(
          priorDisasmCost_.load(std::memory_order_relaxed) + disasmPool_->accumulatedCostSeconds(),
          std::memory_order_relaxed);

      poolPtr_.store(nullptr, std::memory_order_release);
      {
        std::lock_guard<std::mutex> alock(activeMutex_);
        activePool_ = nullptr;
      }
      disasmPool_.reset();
      threadBudget_.reset();
    }

    {
      std::lock_guard<std::mutex> alock(activeMutex_);
      activeAssm_ = nullptr;
    }

    // 6. Decide terminal or Paused state (LIFECYCLE-1 / SPEC-SESS-1). An
    // abort() after this read is still honoured: finishPhase() re-decides
    // under queueMutex_.
    if (abortRequested_.load(std::memory_order_acquire)) {
      finishPhase(solverState_e::Stopped, ACT_PAUSING,
                  assembler_c::ERR_NONE, 0, elapsed());
    } else if (assm_->getFinished() >= 1.0f && pending_.empty()) {
      finishPhase(solverState_e::Finished, ACT_FINISHED,
                  assembler_c::ERR_NONE, 0, elapsed());
    } else {
      finishPhase(solverState_e::Paused, ACT_PAUSING,
                  assembler_c::ERR_NONE, 0, elapsed());
    }
  } catch (const assert_exception & a) {
    assertEx_ = a;
    teardownAfterFailure();
    finishPhase(solverState_e::Error, ACT_ASSERT,
                assembler_c::ERR_NONE, 0, elapsed());
  } catch (...) {
    // Anything else (std::bad_alloc, a worker's exception rethrown by the
    // pool's finish()) must not escape this jthread: that would terminate
    // the process. Keep it for the host (exception()).
    std::exception_ptr ex = std::current_exception();
    teardownAfterFailure();
    failPhase(std::move(ex), elapsed());
  }
}

unsigned int solverSession_c::currentActionParameter(void) const {
  const unsigned int act = action_.load(std::memory_order_acquire);
  if (act == ACT_REDUCE || act == ACT_PREPARATION) {
    assembler_c * a = assmPtr_.load(std::memory_order_acquire);
    return a ? a->getReducePiece() : 0;
  }
  return 0;
}

unsigned long solverSession_c::getIterations(void) const {
  assembler_c * a = assmPtr_.load(std::memory_order_acquire);
  return a ? a->getIterations() : 0;
}

std::optional<solverRunInfo_t> solverSession_c::runInfo(void) const {
  std::lock_guard<std::mutex> lock(runInfoMutex_);
  return runInfo_;
}

unsigned int solverSession_c::rotationsSkipped(void) const {
  return rotationContext_ ? rotationContext_->getSkipped() : 0;
}

bool solverSession_c::workOutstanding(void) const {
  if (!assemblyEndNs_.load(std::memory_order_acquire))
    return true;
  std::lock_guard<std::mutex> alock(activeMutex_);
  return activePool_ &&
         activePool_->completedCount() < activePool_->submittedCount();
}

float solverSession_c::getProgress(void) const {
  float f;

  if (action_.load(std::memory_order_acquire) == ACT_FINISHED) {
    f = 1.0f;
  } else {
    const long long start = assemblyStartNs_.load(std::memory_order_acquire);
    if (!start)
      return reportedProgress_.load(std::memory_order_relaxed);

    assembler_c * a = assmPtr_.load(std::memory_order_acquire);
    if (!a)
      return reportedProgress_.load(std::memory_order_relaxed);

    const long long end = assemblyEndNs_.load(std::memory_order_acquire);

    progressModel_c::Input in;
    in.assemblyFraction = a->getFinished();

    if (!end && in.assemblyFraction >= 1.0f)
      in.assemblyFraction = std::nextafter(1.0f, 0.0f);

    float base = assemblyBaseFraction_.load(std::memory_order_relaxed);
    if (in.assemblyFraction < base) {
      base = in.assemblyFraction;
      assemblyBaseFraction_.store(base, std::memory_order_relaxed);
    }

    {
      const long long upto = end ? end : steadyNowNs();
      const long long ns = (upto > start) ? (upto - start) : 0;
      const double sessionCost = 1e-9 * static_cast<double>(ns)
          * static_cast<double>(assemblyThreads_.load(std::memory_order_relaxed));
      in.assemblyCostSeconds =
          progressModel_c::projectAssemblyCost(sessionCost, in.assemblyFraction, base);
    }

    if (options_.disassemble) {
      uint64_t sub = priorDisasmSubmitted_.load(std::memory_order_relaxed);
      uint64_t comp = priorDisasmCompleted_.load(std::memory_order_relaxed);
      double cost = priorDisasmCost_.load(std::memory_order_relaxed);
      {
        std::lock_guard<std::mutex> alock(activeMutex_);
        if (activePool_) {
          sub += activePool_->submittedCount();
          comp += activePool_->completedCount();
          cost += activePool_->accumulatedCostSeconds();
        }
      }
      in.assembliesFound = sub;
      in.disassembled = comp;
      in.disassemblyCostSeconds = cost;
    }

    f = progressModel_c::evaluate(in).fraction;

    if (in.disassembled == 0 && in.assembliesFound > 0)
      f = noEvidenceProgress(in.assemblyFraction,
                             reportedProgress_.load(std::memory_order_relaxed));

    if (f > runningCap)
      f = runningCap;
  }

  float prev = reportedProgress_.load(std::memory_order_relaxed);
  while (f > prev &&
         !reportedProgress_.compare_exchange_weak(prev, f,
                                                  std::memory_order_relaxed))
    ;

  return reportedProgress_.load(std::memory_order_relaxed);
}
