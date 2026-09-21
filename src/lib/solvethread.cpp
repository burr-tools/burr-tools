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
#include "solution.h"

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

    if (!stopPressed) {

      action = solveThread_c::ACT_ASSEMBLING;
      a->assemble(this);

      if (disasm_pool) {
        if (!stopPressed)
          action = solveThread_c::ACT_DISASSEMBLING;
        disasm_pool->finish();
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

solveThread_c::solveThread_c(problem_c & puz, int par) :
action(ACT_PREPARATION),
puzzle(puz),
parameters(par),
sortMethod(SRT_COMPLETE_MOVES),
liveSort(-1),
solutionLimit(10),
solutionDrop(1),
disasm_pool(nullptr),
assm(0)
{

  if (par & PAR_DISASSM) {
    /* Thread budget architecture:
     * Disassembler pool is instantiated with 0 (defaulting to BURRTOOLS_THREADS or hardware_concurrency).
     * The Tier 1 assembler and Tier 2 disassembler pool concurrently run up to N workers each.
     * This overlap is deliberate: disassembly is memory/movement-closure bound while assembly is
     * CPU/search bound. Dynamic backpressure via bounded queues (MAX_QUEUE_SIZE = 64) prevents
     * queue bloat and coordinates CPU utilization (see design/2026-09-19-threading-model-assessment.md).
     */
    disasm_pool = std::make_unique<disassemblerPool_c>(
      puz,
      0,
      [this](uint64_t seqNo, std::unique_ptr<assembly_c> a, std::unique_ptr<separation_c> s) {
        onDisassemblyResult(seqNo, std::move(a), std::move(s));
      }
    );
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
