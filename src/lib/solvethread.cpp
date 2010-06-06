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
#include "assembly.h"
#include "disassembler_0.h"
#include "solution.h"

void solveThread_c::run(void){

  try {

    /* first check, if there is an assembler available with the
     * problem, if there is one take that
     */
    if (puzzle->getAssembler())
      assm = puzzle->getAssembler();

    else {

      /* otherwise we have to create a new one
       */
      action = solveThread_c::ACT_PREPARATION;
      assm = puzzle->getGridType()->findAssembler(puzzle);

      errState = assm->createMatrix(puzzle, parameters & PAR_KEEP_MIRROR, parameters & PAR_KEEP_ROTATIONS, parameters & PAR_COMPLETE_ROTATIONS);
      if (errState != assembler_c::ERR_NONE) {

        errParam = assm->getErrorsParam();

        action = solveThread_c::ACT_ERROR;

        delete assm;
        return;
      }

      if (parameters & PAR_REDUCE) {

        if (!stopPressed)
          action = solveThread_c::ACT_REDUCE;

        assm->reduce();
      }

      /* set the assembler to the problem as soon as it is finished
       * with initialisation, NOT EARLIER as the function
       * also restores the assembler state to a state that might
       * be saved within the problem
       */
      errState = puzzle->setAssembler(assm);
      if (errState != assembler_c::ERR_NONE) {
        action = solveThread_c::ACT_ERROR;
        return;
      }
    }

    if (return_after_prep) {
      action = solveThread_c::ACT_PAUSING;
      return;
    }

    if (!stopPressed) {

      action = solveThread_c::ACT_ASSEMBLING;
      assm->assemble(this);
      puzzle->addTime(time(0)-startTime);

      if (assm->getFinished() >= 1) {
        action = solveThread_c::ACT_FINISHED;
        puzzle->finishedSolving();
      } else
        action = solveThread_c::ACT_PAUSING;

    } else {
      action = solveThread_c::ACT_PAUSING;
      puzzle->addTime(time(0)-startTime);
    }

  }

  catch (assert_exception *a) {

    ae = a;
    action = solveThread_c::ACT_ERROR;
    if (puzzle->getAssembler())
      puzzle->removeAllSolutions();
  }
}

solveThread_c::solveThread_c(problem_c * puz, int par) :
action(ACT_PREPARATION),
puzzle(puz),
parameters(par),
sortMethod(SRT_COMPLETE_MOVES),
solutionLimit(10),
solutionDrop(1),
ae(0),
disassm(0),
assm(0)
{

  if (par & PAR_DISASSM)
    disassm = new disassembler_0_c(puz);
}

solveThread_c::~solveThread_c(void) {

  kill();

  if (disassm) {
    delete disassm;
    disassm = 0;
  }
}

bool solveThread_c::assembly(assembly_c * a) {

  enum {
    SOL_COUNT_ASM,
    SOL_SAVE_ASM,
    SOL_COUNT_DISASM,
    SOL_DISASM,
  };

  int _solutionAction = 0;
  if (!(parameters & PAR_JUST_COUNT)) _solutionAction += 1;
  if (parameters & PAR_DISASSM) _solutionAction += 2;

  switch(_solutionAction) {
  case SOL_COUNT_ASM:
    delete a;
    break;
  case SOL_SAVE_ASM:

    if (puzzle->getNumAssemblies() % (solutionDrop*dropMultiplicator) == 0)
      puzzle->addSolution(a);
    else
      delete a;

    break;

  case SOL_DISASM:
  case SOL_COUNT_DISASM:
    {

      // when the assembly has only 1 piece, we don't need
      // to disassemble, the disassembler will return 0 anyway
      if (a->placementCount() <= 1) {

        // only one piece, that is always a solution, so increment number
        // of solutions but save only the assembly
        puzzle->addSolution(a);
        puzzle->incNumSolutions();

        break;
      }

      // try to disassemble
      action = ACT_DISASSEMBLING;
      separation_c * s = disassm->disassemble(a);
      action = ACT_ASSEMBLING;

      // check, if we found a disassembly sequence
      if (!s) {
        // no disassembly sequence found, delete assembly
        delete a;

        break;
      }

      // if the user wants to save the solution, do it
      if (_solutionAction != SOL_DISASM) {

        // if not, delete disassembly AND assembly
        delete s;
        delete a;

        // yes, the puzzle is disassembable count solutions
        puzzle->incNumSolutions();

        break;
      }

      // find the place to insert and insert the new solution so that
      // they are sorted by the complexity of the disassembly

      bool ins = false;

      switch(sortMethod) {
        case SRT_COMPLETE_MOVES:
          {
            unsigned int lev = s->sumMoves();

            for (unsigned int i = 0; i < puzzle->solutionNumber(); i++) {

              const disassembly_c * s2 = puzzle->getSolution(i)->getDisassembly();

              if (s2 && s2->sumMoves() > lev) {
                if (parameters & PAR_DROP_DISASSEMBLIES) {
                  puzzle->addSolution(a, new separationInfo_c(s), i);
                  delete s;
                } else
                  puzzle->addSolution(a, s, i);
                ins = true;
                break;
              }
            }
          }

          if (!ins) {
            if (parameters & PAR_DROP_DISASSEMBLIES) {
              puzzle->addSolution(a, new separationInfo_c(s));
              delete s;
            } else
              puzzle->addSolution(a, s);
          }

          // remove the front most solution, if we only want to save
          // a limited number of solutions, as the front most
          // solutions are the more unimportant ones
          if (solutionLimit && (puzzle->solutionNumber() > solutionLimit))
            puzzle->removeSolution(0);

          break;
        case SRT_LEVEL:
          {
            for (unsigned int i = 0; i < puzzle->solutionNumber(); i++) {

              const disassembly_c * s2 = puzzle->getSolution(i)->getDisassemblyInfo();

              if (s2 && (s2->compare(s) > 0)) {
                if (parameters & PAR_DROP_DISASSEMBLIES) {
                  puzzle->addSolution(a, new separationInfo_c(s), i);
                  delete s;
                } else
                  puzzle->addSolution(a, s, i);
                ins = true;
                break;
              }
            }

            if (!ins)  {
              if (parameters & PAR_DROP_DISASSEMBLIES) {
                puzzle->addSolution(a, new separationInfo_c(s));
                delete s;
              } else
                puzzle->addSolution(a, s);
            }
          }

          // remove the front most solution, if we only want to save
          // a limited number of solutions, as the front most
          // solutions are the more unimportant ones
          if (solutionLimit && (puzzle->solutionNumber() > solutionLimit))
            puzzle->removeSolution(0);

          break;
        case SRT_UNSORT:
          /* only save every solutionDrop-th solution */
          if (puzzle->getNumSolutions() % (solutionDrop * dropMultiplicator) == 0) {
            if (parameters & PAR_DROP_DISASSEMBLIES) {
              puzzle->addSolution(a, new separationInfo_c(s));
              delete s;
            } else
              puzzle->addSolution(a, s);
          } else {
            delete a;
            delete s;
          }

          break;
      }

      // yes, the puzzle is disassembably, count solutions
      puzzle->incNumSolutions();
    }
    break;
  }

  puzzle->incNumAssemblies();

  // this is the case for assembly only or unsorted disassembly solutions
  // we need to thin out the list
  if (solutionLimit && (puzzle->solutionNumber() > solutionLimit)) {
    unsigned int idx = (_solutionAction == SOL_SAVE_ASM) ? puzzle->getNumAssemblies()-1
                                                         : puzzle->getNumSolutions()-1;

    idx = (idx % (solutionLimit * solutionDrop * dropMultiplicator)) / (solutionDrop * dropMultiplicator);

    if (idx == solutionLimit-1)
      dropMultiplicator *= 2;

    puzzle->removeSolution(idx+1);
  }

  return true;
}

void solveThread_c::stop(void) {

  if ((action != ACT_ASSEMBLING) &&
      (action != ACT_REDUCE) &&
      (action != ACT_DISASSEMBLING) &&
      (action != ACT_PREPARATION)
     )
    return;

  action = ACT_WAIT_TO_STOP;

  if (puzzle->getAssembler())
    puzzle->getAssembler()->stop();

  stopPressed = true;
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

    if (!puzzle->numAssembliesKnown())
      a = 0;
    else
      a = puzzle->getNumAssemblies();
  } else {
    if (!puzzle->numSolutionsKnown())
      a = 0;
    else
      a = puzzle->getNumSolutions();
  }

  while (a+solutionDrop > 2 * solutionLimit * solutionDrop) {
    dropMultiplicator *= 2;
    a = (a+1) / 2;
  }

  return thread_c::start();
}

unsigned int solveThread_c::currentActionParameter(void) {

  switch(action) {
  case ACT_REDUCE:
  case ACT_PREPARATION:
    if (assm)
      return assm->getReducePiece();
    else
      return 0;

  default:
    return 0;
  }
}
