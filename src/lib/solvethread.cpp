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

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "solvethread.h"

#include "disassembly.h"
#include "problem.h"
#include "puzzle.h"
#include "assembly.h"
#include "disassembler_0.h"
#include "progressmodel.h"
#include "solution.h"

#include <chrono>
#include <cmath>

namespace {

  long long steadyNowNs(void) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  /* the cap getProgress() reports up to while the solve is running; see the
   * declaration on solveThread_c, where the tests reach it by name
   */
  constexpr float runningCap = solveThread_c::runningCap;

}

void solveThread_c::run(void){

  try {

    /* local pointer for this thread's own use; the shared member `assm` is
     * only written (published) here and read by the GUI thread
     */
    assembler_c * a = 0;

    /* first check, if there is an assembler available with the
     * problem, if there is one take that
     */
    if (puzzle.getAssembler()) {
      a = puzzle.getAssembler();
      assm.store(a, std::memory_order_release);
    }

    else {

      /* otherwise we have to create a new one
       */
      action = solveThread_c::ACT_PREPARATION;
      std::unique_ptr<assembler_c> new_assm = puzzle.getPuzzle().getGridType()->findAssembler(puzzle);
      a = new_assm.get();
      assm.store(a, std::memory_order_release);

      errState = a->createMatrix(parameters & PAR_KEEP_MIRROR, parameters & PAR_KEEP_ROTATIONS, parameters & PAR_COMPLETE_ROTATIONS);
      if (errState != assembler_c::ERR_NONE) {

        errParam = a->getErrorsParam();

        action = solveThread_c::ACT_ERROR;

        assm.store(0, std::memory_order_release);
        return;
      }

      if (parameters & PAR_REDUCE) {

        if (!stopPressed)
          action = solveThread_c::ACT_REDUCE;

        a->reduce();
      }

      /* set the assembler to the problem as soon as it is finished
       * with initialisation, NOT EARLIER as the function
       * also restores the assembler state to a state that might
       * be saved within the problem
       */
      assm.store(0, std::memory_order_release);
      errState = puzzle.setAssembler(std::move(new_assm));
      if (errState != assembler_c::ERR_NONE) {
        action = solveThread_c::ACT_ERROR;
        return;
      }
      a = puzzle.getAssembler();
      assm.store(a, std::memory_order_release);
    }

    if (return_after_prep) {
      action = solveThread_c::ACT_PAUSING;
      return;
    }

    // Re-submit assemblies salvaged by an earlier stop (see
    // disassemblerPool_c::requestStop): found but never disassembled, and
    // suppressed by the assembler's dedup on re-search, so without this
    // they would be lost on pause/continue. First in, so order stays stable.
    if (disasm_pool && !stopPressed) {
      for (auto &stashed : puzzle.takeStashedAssemblies())
        disasm_pool->submit(std::move(stashed));
    }

    if (!stopPressed) {

      /* publish the phase's cost basis before announcing the phase, so a GUI
       * poll that sees ACT_ASSEMBLING can never see a zero thread count.
       *
       * getRunThreads(), not getNumThreads(): the latter reports what was
       * *configured*, where 0 means "decide at run time" -- the default, and
       * what the GUI leaves it at. A resumed assembler also runs serially
       * however many threads are configured. Either way the configured number
       * is not the number of worker-seconds a wall second buys.
       */
      const unsigned int threads = a->getRunThreads();
      assemblyThreads.store(threads ? threads : 1, std::memory_order_relaxed);

      /* How far the assembler already was when we picked it up -- 0 for a
       * fresh solve, the carried-over fraction for a resumed one. The seconds
       * that bought that head start were spent by a previous solveThread_c
       * and died with it, so the phase's cost has to be projected from this
       * one; see progressModel_c::projectAssemblyCost().
       *
       * Read here, on the worker thread, with nothing else running. It is a
       * starting estimate rather than the last word: assemble() re-seeds the
       * progress source for the run it is about to make, so a fraction read
       * before the call can describe a source the call is about to discard.
       * getProgress() lowers the base if the run ever reports less than this;
       * see the note there.
       */
      const float base = a->getFinished();
      assemblyBaseFraction.store((base > 0.0f && base < 1.0f) ? base : 0.0f,
                                 std::memory_order_relaxed);

      assemblyEndNs.store(0, std::memory_order_relaxed);
      /* release: this is the store that opens getProgress()'s door to the
       * assembler, so everything preparation wrote must be visible first
       */
      assemblyStartNs.store(steadyNowNs(), std::memory_order_release);

      action = solveThread_c::ACT_ASSEMBLING;
      a->setNumThreads(numThreads);
      a->assemble(this);

      /* freeze the assembly cost: from here on the machine is draining the
       * disassembly queue, and charging that time to assembly too would make
       * the blend approach 1 without any of the remaining work being done
       */
      assemblyEndNs.store(steadyNowNs(), std::memory_order_relaxed);

      if (disasm_pool) {
        if (!stopPressed)
          action = solveThread_c::ACT_DISASSEMBLING;
        disasm_pool->finish();
        // Move anything the pool salvaged (stop with a full queue, or
        // submits rejected after stop) to the problem for the next run.
        // Normally empty; finish() already delivered everything filed.
        puzzle.stashAssemblies(disasm_pool->takeSalvaged());
      }

      puzzle.addTime(time(0)-startTime);

      if (a->getFinished() >= 1) {
        action = solveThread_c::ACT_FINISHED;
        puzzle.finishedSolving();
      } else
        action = solveThread_c::ACT_PAUSING;

    } else {
      action = solveThread_c::ACT_PAUSING;
      puzzle.addTime(time(0)-startTime);
    }

  }

  catch (assert_exception & a) {

    ae = a;
    action = solveThread_c::ACT_ASSERT;
    if (disasm_pool)
      disasm_pool->abort();
    if (puzzle.getAssembler())
      puzzle.removeAllSolutions();
  }
}

solveThread_c::solveThread_c(problem_c & puz, int par, unsigned int threads) :
action(ACT_PREPARATION),
puzzle(puz),
parameters(par),
numThreads(threads),
sortMethod(SRT_COMPLETE_MOVES),
liveSort(-1),
solutionLimit(10),
solutionDrop(1),
disasm_pool(nullptr),
assm(0)
{

  if (par & PAR_DISASSM) {
    /* Thread budget architecture (see the concurrency architecture note
     * in thread_budget.h):
     * One ThreadBudget shared by the assembly task pool(s) and this pool
     * caps concurrently *working* solver threads at the pool size; idle
     * threads on either side hold nothing. Created only for real
     * (non-inline) pools with budgeting enabled -- otherwise null and all
     * budget call sites behave exactly as without any cap.
     */
    disasm_pool = std::make_unique<disassemblerPool_c>(
      puz,
      numThreads,
      [this](uint64_t seqNo, std::unique_ptr<assembly_c> a, std::unique_ptr<separation_c> s) {
        onDisassemblyResult(seqNo, std::move(a), std::move(s));
      }
    );
    if (threadBudgetEnabled() && !disasm_pool->isInline()) {
      threadBudget_ = std::make_unique<ThreadBudget>(disasm_pool->threadCount());
      disasm_pool->setBudget(threadBudget_.get());
    }
  }
}

solveThread_c::~solveThread_c(void) {

  /* signal the worker to stop and wait for it to actually finish before we
   * free anything it might still be using.
   */
  stopInternal();
  joinThread();

  disasm_pool.reset();
}

bool solveThread_c::assembly(std::unique_ptr<assembly_c> a) {

  if (parameters & PAR_DISASSM) {
    bt_assert(disasm_pool);
    disasm_pool->submit(std::move(a));
    return !disasm_pool->isAborted() && !disasm_pool->isStopRequested() && !stopPressed;
  }

  // Assembly-only mode
  if (!(parameters & PAR_JUST_COUNT)) {
    if (puzzle.getNumAssemblies() % (solutionDrop * dropMultiplicator) == 0)
      puzzle.addSolution(a.release());
  }

  puzzle.incNumAssemblies();

  // this is the case for assembly only
  // we need to thin out the list
  if (solutionLimit && (puzzle.getNumberOfSavedSolutions() > solutionLimit)) {
    unsigned int idx = puzzle.getNumAssemblies() - 1;
    idx = (idx % (solutionLimit * solutionDrop * dropMultiplicator)) / (solutionDrop * dropMultiplicator);

    if (idx == solutionLimit - 1)
      dropMultiplicator *= 2;

    puzzle.removeSolution(idx + 1);
  }

  int ls = liveSort.load(std::memory_order_relaxed);
  if (ls >= 0 && puzzle.getNumberOfSavedSolutions() >= 2)
    puzzle.sortSolutions(ls);

  return true;
}

void solveThread_c::onDisassemblyResult(uint64_t /*seqNo*/, std::unique_ptr<assembly_c> a, std::unique_ptr<separation_c> s) {

  // when the assembly has only 1 piece, we don't need
  // to disassemble, the disassembler will return 0 anyway
  if (a->placementCount() <= 1) {
    // only one piece, that is always a solution, so increment number
    // of solutions but save only the assembly
    puzzle.addSolution(a.release());
    puzzle.incNumSolutions();
    puzzle.incNumAssemblies();
    return;
  }

  // check if we found a disassembly sequence
  if (!s) {
    // no disassembly sequence found
    puzzle.incNumAssemblies();
    return;
  }

  // if the user wants to save the solution, do it
  if (!(parameters & PAR_JUST_COUNT)) {
    // find the place to insert and insert the new solution so that
    // they are sorted by the complexity of the disassembly

    bool ins = false;

    {
      std::unique_lock<std::recursive_mutex> solGuard = puzzle.lockSolutions();

      switch(sortMethod) {
        case SRT_COMPLETE_MOVES:
          {
            unsigned int lev = s->sumMoves();

            for (unsigned int i = 0; i < puzzle.getNumberOfSavedSolutions(); i++) {

              const disassembly_c * s2 = puzzle.getSavedSolution(i)->getDisassembly();

              if (s2 && s2->sumMoves() > lev) {
                if (parameters & PAR_DROP_DISASSEMBLIES) {
                  puzzle.addSolution(a.release(), new separationInfo_c(s.get()), i);
                } else
                  puzzle.addSolution(a.release(), s.release(), i);
                ins = true;
                break;
              }
            }

            if (!ins) {
              if (parameters & PAR_DROP_DISASSEMBLIES) {
                puzzle.addSolution(a.release(), new separationInfo_c(s.get()));
              } else
                puzzle.addSolution(a.release(), s.release());
            }

            if (solutionLimit && (puzzle.getNumberOfSavedSolutions() > solutionLimit))
              puzzle.removeSolution(0);
          }
          break;

        case SRT_LEVEL:
          {
            for (unsigned int i = 0; i < puzzle.getNumberOfSavedSolutions(); i++) {

              const disassembly_c * s2 = puzzle.getSavedSolution(i)->getDisassemblyInfo();

              if (s2 && (s2->compare(s.get()) > 0)) {
                if (parameters & PAR_DROP_DISASSEMBLIES) {
                  puzzle.addSolution(a.release(), new separationInfo_c(s.get()), i);
                } else
                  puzzle.addSolution(a.release(), s.release(), i);
                ins = true;
                break;
              }
            }

            if (!ins) {
              if (parameters & PAR_DROP_DISASSEMBLIES) {
                puzzle.addSolution(a.release(), new separationInfo_c(s.get()));
              } else
                puzzle.addSolution(a.release(), s.release());
            }

            if (solutionLimit && (puzzle.getNumberOfSavedSolutions() > solutionLimit))
              puzzle.removeSolution(0);
          }
          break;

        case SRT_UNSORT:
          if (puzzle.getNumSolutions() % (solutionDrop * dropMultiplicator) == 0) {
            if (parameters & PAR_DROP_DISASSEMBLIES) {
              puzzle.addSolution(a.release(), new separationInfo_c(s.get()));
            } else
              puzzle.addSolution(a.release(), s.release());
          }
          break;
      }
    }

    // yes, the puzzle is disassemblable, count solutions
    puzzle.incNumSolutions();
  } else {
    puzzle.incNumSolutions();
  }

  puzzle.incNumAssemblies();

  if (solutionLimit && (puzzle.getNumberOfSavedSolutions() > solutionLimit)) {
    unsigned int idx = puzzle.getNumSolutions() - 1;

    idx = (idx % (solutionLimit * solutionDrop * dropMultiplicator)) / (solutionDrop * dropMultiplicator);

    if (idx == solutionLimit - 1)
      dropMultiplicator *= 2;

    puzzle.removeSolution(idx + 1);
  }

  int ls = liveSort.load(std::memory_order_relaxed);
  if (ls >= 0 && puzzle.getNumberOfSavedSolutions() >= 2)
    puzzle.sortSolutions(ls);
}

void solveThread_c::stopInternal(void) {

  if ((action != ACT_ASSEMBLING) &&
      (action != ACT_REDUCE) &&
      (action != ACT_DISASSEMBLING) &&
      (action != ACT_PREPARATION)
     )
    return;

  action = ACT_WAIT_TO_STOP;

  if (puzzle.getAssembler())
    puzzle.getAssembler()->stop();

  if (disasm_pool)
    disasm_pool->requestStop();

  stopPressed = true;
}

void solveThread_c::stop(void) {
  stopInternal();
}

bool solveThread_c::start(bool stop_after_prep) {

  stopPressed = false;
  return_after_prep = stop_after_prep;
  startTime = time(0);

  // calculate dropMultiplicator

  dropMultiplicator = 1;

  unsigned int a;

  // only when we save count the possible assemblies we use the assembly counter, in all
  // other cases we use the solution counter
  if ((parameters & (PAR_JUST_COUNT | PAR_DISASSM)) == 0) {

    if (!puzzle.numAssembliesKnown())
      a = 0;
    else
      a = puzzle.getNumAssemblies();
  } else {
    if (!puzzle.numSolutionsKnown())
      a = 0;
    else
      a = puzzle.getNumSolutions();
  }

  while (a+solutionDrop > 2 * solutionLimit * solutionDrop) {
    dropMultiplicator *= 2;
    a = (a+1) / 2;
  }

  running.store(true, std::memory_order_release);
  worker_thread = std::jthread([this]() {
    run();
    running.store(false, std::memory_order_release);
  });

  return worker_thread.joinable();
}

unsigned int solveThread_c::currentActionParameter(void) {

  switch(action.load()) {
  case ACT_REDUCE:
  case ACT_PREPARATION:
    {
      assembler_c * a = assm.load(std::memory_order_acquire);
      if (a)
        return a->getReducePiece();
      else
        return 0;
    }

  default:
    return 0;
  }
}

float solveThread_c::getProgress(void) const {

  float f;

  if (action.load(std::memory_order_acquire) == ACT_FINISHED) {

    /* Reported rather than computed. The blend does land on exactly 1.0 here
     * in the ordinary case, but it is built out of a float fraction and two
     * measured costs, and the GUI's "finished" state must not depend on those
     * agreeing to the last bit.
     */
    f = 1.0f;

  } else {

    /* Nothing to report before the assembly phase begins, and nothing may be
     * read off the assembler either: it is published to `assm` before
     * createMatrix() and reduce() run, and those build the very matrix
     * getFinished() reads. Acquiring here pairs with the release store the
     * worker makes in run(), so everything preparation built is visible
     * before the first getFinished() call.
     */
    const long long start = assemblyStartNs.load(std::memory_order_acquire);
    if (!start)
      return reportedProgress.load(std::memory_order_relaxed);

    assembler_c * a = assm.load(std::memory_order_acquire);
    if (!a)
      return reportedProgress.load(std::memory_order_relaxed);

    const long long end = assemblyEndNs.load(std::memory_order_relaxed);

    progressModel_c::Input in;
    in.assemblyFraction = a->getFinished();

    /* The parallel assemblers sum float terms that approach 1 asymptotically,
     * so getFinished() can round to exactly 1.0f with workers still live:
     * `a < 1` does not mean "still running". While assembly has demonstrably
     * not finished, take the input at its word only up to the largest float
     * below 1, so downstream arithmetic cannot read it as completion.
     *
     * This makes the input honest; on its own it does not change what is
     * reported. With a drained pool, evaluate() returns 1.0f for a == 1.0f and
     * 0.99999994f for the clamped value -- both above runningCap, so both are
     * reported as runningCap. What keeps that from latching the bar for the
     * whole disassembly tail is the no-evidence branch further down, not this.
     */
    if (!end && in.assemblyFraction >= 1.0f)
      in.assemblyFraction = std::nextafter(1.0f, 0.0f);

    /* Keep the cost basis below the live fraction, lowering it if the run ever
     * reports less than what was captured before assemble() was called.
     *
     * That capture is a guess about a number only assemble() settles: it
     * re-seeds the progress source for the run it is about to make, and a
     * paused parallel run leaves behind task counters that describe a run
     * being abandoned rather than the one about to start. Captured from those,
     * the base can sit ABOVE everything the new run reports -- and then
     * `gained` is negative for the whole solve, projectAssemblyCost() returns
     * 0, evaluate() cannot blend, and the bar silently reverts to reporting
     * assembly alone with disassembly never weighed. That is the exact case
     * the projection exists to handle, so it must not be the case that breaks
     * it.
     *
     * Correcting it here rather than by having the assembler expose its reset
     * keeps the fix inside the one class that has the problem, holds for any
     * back end whatever its reset ordering, and needs no public mutator on
     * assembler_c. The run's real starting point is the lowest fraction it has
     * been seen to report, and getFinished() is monotone within a run, so this
     * settles on the first poll and never drifts afterwards.
     */
    float base = assemblyBaseFraction.load(std::memory_order_relaxed);
    if (in.assemblyFraction < base) {
      base = in.assemblyFraction;
      assemblyBaseFraction.store(base, std::memory_order_relaxed);
    }

    {
      const long long upto = end ? end : steadyNowNs();
      const long long ns = (upto > start) ? (upto - start) : 0;

      /* worker-seconds, the same currency the pool's cost is measured in: it
       * sums the wall time of completed tasks across all of its workers. A
       * wall-clock figure here would be the same quantity divided by the
       * thread count, and weighing one phase against the other with a constant
       * factor between their units biases the blend by exactly that factor.
       *
       * KNOWN LIMITATION, deliberately not modelled here. Matching the units
       * does not make the two costs accrue at the same rate per wall second.
       * The assembler and the pool each default to hardware_concurrency(), so
       * while the phases overlap the pair accrues roughly 2N worker-seconds
       * per wall second, and in the disassembly tail -- after assemble() has
       * returned and only the pool is live -- roughly N. The bar therefore
       * changes speed at that transition, and projectedRemainingSeconds is
       * biased across it. Correcting it means modelling the phase-dependent
       * accrual rate, which is a design change rather than part of this fix;
       * it is recorded here rather than papered over.
       */
      const double sessionCost = 1e-9 * static_cast<double>(ns)
          * static_cast<double>(assemblyThreads.load(std::memory_order_relaxed));

      /* What the model needs is the cost of the whole of assemblyFraction, and
       * on a resume this run only paid for the tail of it. Project the rest
       * from the rate this run measured rather than leaving the phase charged
       * for a fraction it did not buy -- which under-projects the assembly
       * phase by exactly the ratio of the two, so the blend collapses towards
       * the disassembly fraction and the monotone guard pins it there while
       * assembly does the remaining hours of work.
       */
      in.assemblyCostSeconds =
          progressModel_c::projectAssemblyCost(sessionCost, in.assemblyFraction, base);
    }

    if (disasm_pool) {
      in.assembliesFound        = disasm_pool->submittedCount();
      in.disassembled           = disasm_pool->completedCount();
      in.disassemblyCostSeconds = disasm_pool->accumulatedCostSeconds();
    }

    f = progressModel_c::evaluate(in).fraction;

    /* Until the pool completes its first task there is no disassembly evidence
     * to blend with, so evaluate() reports the assembly fraction alone -- and
     * once assembly is complete with assemblies still outstanding, that is its
     * "counted but not done" sentinel, std::nextafter(1.0f, 0.0f).
     *
     * That value must not reach the monotone guard. It caps to runningCap and
     * latches there, and every genuine blended value that follows is lower --
     * the first completion of a long disassembly queue puts the blend far
     * below 1 and it climbs from there -- so the guard locks all of them out
     * and the bar sits at 99.9% for the whole tail. Measured on
     * PelikanBurr.xmpuzzle, whose assembly finishes in ~4 ms against a ~185 ms
     * tail: two distinct values over 187 samples, 0 then 0.999.
     *
     * The trigger is precisely "the assembler finished before the pool
     * completed its first task", not "disassembly dominates": once anything
     * has been disassembled, d = completed/submitted and the blend
     * (A + D)/(A + D/d) rises strictly as d approaches 1, with no latch.
     *
     * So while there is no evidence to blend, report the assembly fraction
     * alone, and when even that has run out -- the fraction has reached the
     * cap, nothing is disassembled, nothing is yet known about the phase that
     * is left -- hold the bar where it is. The threshold is the cap rather
     * than 1.0f deliberately; see noEvidenceProgress(), which is where the
     * rule lives so that it can be tested without driving a solve.
     */
    if (in.disassembled == 0 && in.assembliesFound > 0)
      f = noEvidenceProgress(in.assemblyFraction,
                             reportedProgress.load(std::memory_order_relaxed));

    /* The solve is still running, so it is not complete, whatever the inputs
     * rounded to. Two ways they get there: progressModel_c signals "everything
     * counted is accounted for but work remains" with a value a hair under 1
     * that the GUI's %.4f renders as 100.0000%, and getFinished() can round to
     * exactly 1.0f on the parallel assemblers while their workers are live.
     * Capping here is also what keeps run()'s `getFinished() >= 1` terminal
     * check the thing that decides ACT_FINISHED.
     */
    if (f > runningCap) f = runningCap;
  }

  /* monotone: raise the published value, never lower it */
  float prev = reportedProgress.load(std::memory_order_relaxed);
  while (f > prev &&
         !reportedProgress.compare_exchange_weak(prev, f,
                                                 std::memory_order_relaxed))
    ;

  return reportedProgress.load(std::memory_order_relaxed);
}
