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
#ifndef __SOLVETHREAD_H__
#define __SOLVETHREAD_H__

#include "assembler.h"
#include "bt_assert.h"
#include "problemcollector.h"
#include "solver/solversession.h"

#include <time.h>
#include <atomic>
#include <memory>
#include <thread>

class problem_c;
struct solverSpec_t;

/**
 * Transitional B4 wrapper over solverSession_c + problemCollector_c preserving
 * the legacy solveThread_c API for FLTK, burrTxt2, and existing tests until
 * B5 migrates those front ends directly onto solverSession_c.
 */
class solveThread_c : public assembler_cb {

  public:

    enum {
      ACT_PREPARATION   = solverSession_c::ACT_PREPARATION,
      ACT_REDUCE        = solverSession_c::ACT_REDUCE,
      ACT_ASSEMBLING    = solverSession_c::ACT_ASSEMBLING,
      ACT_DISASSEMBLING = solverSession_c::ACT_DISASSEMBLING,
      ACT_PAUSING       = solverSession_c::ACT_PAUSING,
      ACT_FINISHED      = solverSession_c::ACT_FINISHED,
      ACT_ERROR         = solverSession_c::ACT_ERROR,
      ACT_ASSERT        = solverSession_c::ACT_ASSERT,
      ACT_WAIT_TO_STOP  = solverSession_c::ACT_WAIT_TO_STOP
    };

    static const int PAR_REDUCE             = 0x01;
    static const int PAR_KEEP_ROTATIONS     = 0x04;
    static const int PAR_DROP_DISASSEMBLIES = 0x08;
    static const int PAR_DISASSM            = 0x10;
    static const int PAR_JUST_COUNT         = 0x20;
    static const int PAR_COMPLETE_ROTATIONS = 0x40;
    static const int PAR_ROTATION_MOVES     = 0x80;

    enum {
      SRT_UNSORT         = problemCollectorOptions_t::SRT_UNSORT,
      SRT_COMPLETE_MOVES = problemCollectorOptions_t::SRT_COMPLETE_MOVES,
      SRT_LEVEL          = problemCollectorOptions_t::SRT_LEVEL
    };

    solveThread_c(problem_c & puz, int par, const solverSettings_t & settings);
    virtual ~solveThread_c(void);

    const problem_c & getProblem(void) const { return puzzle; }

    unsigned int currentAction(void) const;
    unsigned int currentActionParameter(void);

    assembler_c::errState getErrorState(void) {
      bt_assert(currentAction() == ACT_ERROR);
      return errState;
    }
    int getErrorParam(void) {
      bt_assert(currentAction() == ACT_ERROR);
      return errParam;
    }

    unsigned long getTime(void) { return time(0) - startTime; }

    bool rotationMoves(void) const { return session_->rotationMoves(); }
    unsigned int rotationsSkipped(void) const { return session_->rotationsSkipped(); }

    void setSortMethod(int sort) { collector_.setSortMethod(sort); }
    void setLiveSort(int method) { collector_.setLiveSort(method); }
    void setSolutionLimits(unsigned int limit, unsigned int drop = 1) {
      collector_.setSolutionLimits(limit, drop);
    }

    const assert_exception & getAssertException(void) { return ae; }

    bool isRunning(void) const { return running.load(std::memory_order_relaxed); }

    void joinThread(void) {
      if (worker_thread.joinable())
        worker_thread.join();
    }

    bool start(bool stop_after_prep = false);
    void stop(void);

    bool stopped(void) const { return finalized_.load(std::memory_order_acquire); }

    float getProgress(void) const;

    static constexpr float runningCap = solverSession_c::runningCap;

    static float noEvidenceProgress(float assemblyFraction, float held) {
      return solverSession_c::noEvidenceProgress(assemblyFraction, held);
    }

    float getAssemblyBaseFraction(void) const {
      return session_->getAssemblyBaseFraction();
    }

    bool workOutstanding(void) const {
      return session_->workOutstanding();
    }

    void run(void);

  protected:

    bool assembly(std::unique_ptr<assembly_c> a) override;
    virtual void assemblyPhaseEnded(void) {}

  private:

    ThreadBudget * threadBudget(void) override {
      return session_->threadBudgetForLegacyBridge();
    }

    void stopInternal(void);

    problem_c & puzzle;
    std::shared_ptr<const solverSpec_t> spec;
    int parameters;
    const solverSettings_t settings;

    std::unique_ptr<solverSession_c> session_;
    problemCollector_c collector_;

    assembler_c::errState errState = assembler_c::ERR_NONE;
    int errParam = 0;
    assert_exception ae;
    time_t startTime = 0;
    bool return_after_prep = false;

    std::atomic<bool> started_{false};
    std::atomic<bool> stopPressed_{false};
    std::atomic<bool> finalized_{false};
    std::atomic<unsigned int> terminalAction_{ACT_PREPARATION};
    std::atomic<bool> running{false};
    std::jthread worker_thread;

    solveThread_c(const solveThread_c &) = delete;
    void operator=(const solveThread_c &) = delete;
};

#endif
