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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "solvethread.h"

#include "problem.h"
#include "solver/solverspec.h"

#include <chrono>
#include <vector>

namespace {

solverOptions_t makeSessionOptions(int par, const solverSettings_t & s) {
  solverOptions_t opts;
  opts.reduce            = (par & solveThread_c::PAR_REDUCE) != 0;
  opts.disassemble       = (par & solveThread_c::PAR_DISASSM) != 0;
  opts.keepRotations     = (par & solveThread_c::PAR_KEEP_ROTATIONS) != 0;
  opts.completeRotations = (par & solveThread_c::PAR_COMPLETE_ROTATIONS) != 0;
  opts.rotationMoves     = (par & solveThread_c::PAR_ROTATION_MOVES) != 0;
  // With disassembly the collector does the counting (and keeps single-piece
  // assemblies as solutions, as before); the session enqueues every result.
  opts.countOnly         = (par & solveThread_c::PAR_JUST_COUNT) != 0 &&
                           (par & solveThread_c::PAR_DISASSM) == 0;
  opts.settings          = s;
  return opts;
}

problemCollectorOptions_t makeCollectorOptions(int par) {
  problemCollectorOptions_t opts;
  opts.disassemble       = (par & solveThread_c::PAR_DISASSM) != 0;
  opts.countOnly         = (par & solveThread_c::PAR_JUST_COUNT) != 0;
  opts.dropDisassemblies = (par & solveThread_c::PAR_DROP_DISASSEMBLIES) != 0;
  opts.sortMethod        = problemCollectorOptions_t::SRT_COMPLETE_MOVES;
  opts.solutionLimit     = 10;
  opts.solutionDrop      = 1;
  return opts;
}

} // namespace

solveThread_c::solveThread_c(problem_c & puz, int par, const solverSettings_t & s)
  : puzzle(puz),
    spec(makeSolverSpec(puz)),
    parameters(par),
    settings(s),
    session_(std::make_unique<solverSession_c>(spec, makeSessionOptions(par, s))),
    collector_(puz, makeCollectorOptions(par))
{}

solveThread_c::~solveThread_c(void) {
  stopInternal();
  joinThread();
  session_.reset();
}

bool solveThread_c::assembly(std::unique_ptr<assembly_c> a) {
  return session_->submitAssemblyForLegacyBridge(std::move(a));
}

unsigned int solveThread_c::currentAction(void) const {
  if (finalized_.load(std::memory_order_acquire))
    return terminalAction_.load(std::memory_order_acquire);
  if (!started_.load(std::memory_order_acquire))
    return stopPressed_.load(std::memory_order_acquire) ? ACT_WAIT_TO_STOP : ACT_PREPARATION;
  unsigned int act = session_->currentAction();
  // Never report a terminal action before run() has finished draining
  // results and updating problem_c (LIFECYCLE-1/2).
  if (act == ACT_PAUSING || act == ACT_FINISHED ||
      act == ACT_ERROR || act == ACT_ASSERT) {
    return stopPressed_.load(std::memory_order_acquire)
             ? ACT_WAIT_TO_STOP
             : ACT_DISASSEMBLING;
  }
  return act;
}

unsigned int solveThread_c::currentActionParameter(void) {
  if (finalized_.load(std::memory_order_acquire))
    return 0;
  return session_->currentActionParameter();
}

float solveThread_c::getProgress(void) const {
  if (finalized_.load(std::memory_order_acquire) &&
      terminalAction_.load(std::memory_order_acquire) == ACT_FINISHED)
    return 1.0f;

  float p = session_->getProgress();
  if (!finalized_.load(std::memory_order_acquire) && p > runningCap)
    p = runningCap;
  return p;
}

void solveThread_c::stopInternal(void) {
  if (finalized_.load(std::memory_order_acquire))
    return;

  unsigned int act = currentAction();
  if (act != ACT_ASSEMBLING &&
      act != ACT_REDUCE &&
      act != ACT_DISASSEMBLING &&
      act != ACT_PREPARATION)
    return;

  stopPressed_.store(true, std::memory_order_release);
  if (session_)
    session_->requestPause();
}

void solveThread_c::stop(void) {
  stopInternal();
}

bool solveThread_c::start(bool stop_after_prep) {
  if (running.load(std::memory_order_acquire) || worker_thread.joinable())
    bt_te("solveThread_c::start: thread already started, use one object per run");

  stopPressed_.store(false, std::memory_order_release);
  finalized_.store(false, std::memory_order_release);
  return_after_prep = stop_after_prep;
  startTime = time(0);

  collector_.resetDropMultiplicator();

  started_.store(true, std::memory_order_release);
  running.store(true, std::memory_order_release);
  worker_thread = std::jthread([this]() {
    run();
    running.store(false, std::memory_order_release);
  });

  return worker_thread.joinable();
}

void solveThread_c::run(void) {
  try {
    // LIFECYCLE-4: reject unstartable problems at worker entry, untouched.
    if (!puzzle.canStartSolving()) {
      errState = assembler_c::ERR_CAN_NOT_START;
      errParam = static_cast<int>(puzzle.getSolveState());
      terminalAction_.store(ACT_ERROR, std::memory_order_release);
      finalized_.store(true, std::memory_order_release);
      return;
    }

    session_->setAssemblyInterceptor(this);
    session_->setAssemblyPhaseEndedHook([this]() { assemblyPhaseEnded(); });

    bool usedSavedResume = false;

    if (puzzle.getAssembler()) {
      // Path 1: continue an in-memory paused assembler.
      std::unique_ptr<assembler_c> existing = puzzle.takeAssembler();
      std::vector<std::unique_ptr<assembly_c>> stashed = puzzle.takeStashedAssemblies();
      session_->adoptPreparedAssembler(std::move(existing), std::move(stashed));
    } else if (puzzle.hasResumeData()) {
      // Path 2: restore from saved resumeData_t (e.g. loaded from XML).
      usedSavedResume = true;
      resumeData_t res = puzzle.cloneResumeData();
      const unsigned int pieces = puzzle.getNumberOfPieces();
      std::erase_if(res.pending, [pieces](const std::unique_ptr<assembly_c> & a) {
        return !a || a->placementCount() != pieces;
      });
      session_->setResume(std::move(res));
    } else {
      // Path 3: fresh solve. Flip SS_UNSOLVED -> SS_SOLVING only after
      // createMatrix() + reduce() succeed so preparation errors leave the
      // problem SS_UNSOLVED.
      session_->setPreparedHook([this]() { puzzle.beginSolving(); });
    }

    const bool startedOk = session_->start(return_after_prep);
    // Exceptions other than assertions ended the process before the session
    // caught them; the FLTK GUI keeps that behaviour (rethrown on this
    // thread, which std::terminate()s as it always did).
    if (auto ex = session_->exception())
      std::rethrow_exception(ex);
    if (!startedOk) {
      if (session_->currentAction() == ACT_ASSERT) {
        ae = session_->getAssertException();
        terminalAction_.store(ACT_ASSERT, std::memory_order_release);
        finalized_.store(true, std::memory_order_release);
        return;
      }

      auto info = session_->runInfo();
      errState = info ? info->error : assembler_c::ERR_NONE;
      errParam = info ? info->errorParam : 0;

      if (usedSavedResume) {
        if (errState == assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED) {
          puzzle.removeAllSolutions();
        } else if (errState == assembler_c::ERR_CAN_NOT_RESTORE_VERSION ||
                   errState == assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX) {
          puzzle.discardResumeEngineState();
        }
      }

      terminalAction_.store(ACT_ERROR, std::memory_order_release);
      finalized_.store(true, std::memory_order_release);
      return;
    }

    if (usedSavedResume)
      puzzle.clearResumeData();

    // Forward any stop() that arrived while start() was validating resume data.
    if (stopPressed_.load(std::memory_order_acquire))
      session_->requestPause();

    if ((parameters & PAR_JUST_COUNT) && !(parameters & PAR_DISASSM)) {
      // In countOnly mode the session emits no queued items; sync counters
      // periodically so live observers see progress, then do a final sync.
      while (session_->sessionState() == solverState_e::Running) {
        if (puzzle.getSolveState() == SS_SOLVING)
          collector_.syncCountOnly(session_->assembliesFound(), session_->solutionsFound());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      session_->join();
      if (puzzle.getSolveState() == SS_SOLVING)
        collector_.syncCountOnly(session_->assembliesFound(), session_->solutionsFound());
    } else {
      collector_.consumeAll(*session_);
      session_->join();
    }

    const solverState_e st = session_->sessionState();
    if (auto ex = session_->exception())
      std::rethrow_exception(ex);

    if (st == solverState_e::Finished) {
      if (puzzle.getSolveState() == SS_UNSOLVED)
        puzzle.beginSolving();
      puzzle.installAssembler(session_->releaseAssembler());
      puzzle.addTime(time(0) - startTime);
      puzzle.finishedSolving();
      terminalAction_.store(ACT_FINISHED, std::memory_order_release);
      finalized_.store(true, std::memory_order_release);
    } else if (st == solverState_e::Paused) {
      if (puzzle.getSolveState() == SS_UNSOLVED)
        puzzle.beginSolving();
      resumeData_t snap = session_->state();
      puzzle.stashAssemblies(std::move(snap.pending));
      puzzle.installAssembler(session_->releaseAssembler());
      if (!return_after_prep)
        puzzle.addTime(time(0) - startTime);
      terminalAction_.store(ACT_PAUSING, std::memory_order_release);
      finalized_.store(true, std::memory_order_release);
    } else if (st == solverState_e::Error) {
      if (session_->currentAction() == ACT_ASSERT) {
        ae = session_->getAssertException();
        if (auto a = session_->releaseAssembler()) {
          puzzle.installAssembler(std::move(a));
          puzzle.removeAllSolutions();
        }
        terminalAction_.store(ACT_ASSERT, std::memory_order_release);
        finalized_.store(true, std::memory_order_release);
      } else {
        auto info = session_->runInfo();
        errState = info ? info->error : assembler_c::ERR_NONE;
        errParam = info ? info->errorParam : 0;
        terminalAction_.store(ACT_ERROR, std::memory_order_release);
        finalized_.store(true, std::memory_order_release);
      }
    } else {
      terminalAction_.store(ACT_PAUSING, std::memory_order_release);
      finalized_.store(true, std::memory_order_release);
    }
  } catch (assert_exception & a) {
    ae = a;
    if (session_) {
      session_->abort();
      if (auto released = session_->releaseAssembler()) {
        puzzle.installAssembler(std::move(released));
        puzzle.removeAllSolutions();
      } else if (puzzle.getAssembler()) {
        puzzle.removeAllSolutions();
      }
    }
    terminalAction_.store(ACT_ASSERT, std::memory_order_release);
    finalized_.store(true, std::memory_order_release);
  }
}
