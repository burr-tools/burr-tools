/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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
#include "assemblerthread.h"

#include "disassembly.h"
#include "puzzle.h"
#include "assembly.h"

void assemblerThread_c::run(void){

  try {

    /* first check, if there is an assembler available with the
     * problem, if there is one take that
     */
    if (puzzle->probGetAssembler(prob))
      assm = puzzle->probGetAssembler(prob);

    else {

      /* otherwise we have to create a new one
       */
      action = assemblerThread_c::ACT_PREPARATION;
      assm = puzzle->getGridType()->findAssembler(puzzle, prob);

      errState = assm->createMatrix(puzzle, prob, parameters & PAR_KEEP_MIRROR, parameters & PAR_KEEP_ROTATIONS);
      if (errState != assembler_c::ERR_NONE) {

        errParam = assm->getErrorsParam();

        action = assemblerThread_c::ACT_ERROR;

        delete assm;
        return;
      }

      if (parameters & PAR_REDUCE) {

        if (!stopPressed)
          action = assemblerThread_c::ACT_REDUCE;

        assm->reduce();
      }

      /* set the assembler to the problem as soon as it is finished
       * with initialisation, NOT EARLIER as the function
       * also restores the assembler state to a state that might
       * be saved within the problem
       */
      errState = puzzle->probSetAssembler(prob, assm);
      if (errState != assembler_c::ERR_NONE) {
        action = assemblerThread_c::ACT_ERROR;
        return;
      }
    }

    if (return_after_prep) {
      action = assemblerThread_c::ACT_PAUSING;
      return;
    }

    if (!stopPressed) {

      action = assemblerThread_c::ACT_ASSEMBLING;
      assm->assemble(this);

      if (assm->getFinished() >= 1) {
        action = assemblerThread_c::ACT_FINISHED;
        puzzle->probFinishedSolving(prob);
      } else
        action = assemblerThread_c::ACT_PAUSING;

    } else
      action = assemblerThread_c::ACT_PAUSING;

    puzzle->probAddTime(prob, time(0)-startTime);
  }

  catch (assert_exception *a) {

    ae = a;
    action = assemblerThread_c::ACT_ERROR;
    if (puzzle->probGetAssembler(prob))
      puzzle->probRemoveAllSolutions(prob);
  }
}

assemblerThread_c::assemblerThread_c(puzzle_c * puz, unsigned int problemNum, int par) :
action(ACT_PREPARATION),
puzzle(puz),
prob(problemNum),
parameters(par),
disassm(0),
assm(0),
ae(0),
sortMethod(SRT_COMPLETE_MOVES),
solutionLimit(10),
solutionDrop(1)
{

  if (par & PAR_DISASSM)
    disassm = puz->getGridType()->getDisassembler(puz, problemNum);
}

assemblerThread_c::~assemblerThread_c(void) {

  kill();

  if (disassm) {
    delete disassm;
    disassm = 0;
  }
}

bool assemblerThread_c::assembly(assembly_c * a) {

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

    if (puzzle->probGetNumAssemblies(prob) % (solutionDrop*dropMultiplicator) == 0)
      puzzle->probAddSolution(prob, a);
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
        puzzle->probAddSolution(prob, a);
        puzzle->probIncNumSolutions(prob);

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
        puzzle->probIncNumSolutions(prob);

        break;
      }

      // find the place to insert and insert the new solution so that
      // they are sorted by the complexity of the disassembly

      bool ins = false;

      switch(sortMethod) {
        case SRT_COMPLETE_MOVES:
          {
            unsigned int lev = s->sumMoves();

            for (unsigned int i = 0; i < puzzle->probSolutionNumber(prob); i++) {

              const separationInfo_c * s2 = puzzle->probGetDisassemblyInfo(prob, i);

              if (s2 && s2->sumMoves() > lev) {
                if (parameters & PAR_DROP_DISASSEMBLIES) {
                  separationInfo_c * si = new separationInfo_c(s);
                  puzzle->probAddSolution(prob, a, si, i);
                  delete s;
                } else
                  puzzle->probAddSolution(prob, a, s, i);
                ins = true;
                break;
              }
            }
          }

          if (!ins) {
            if (parameters & PAR_DROP_DISASSEMBLIES) {
              separationInfo_c * si = new separationInfo_c(s);
              puzzle->probAddSolution(prob, a, si);
              delete s;
            } else
              puzzle->probAddSolution(prob, a, s);
          }

          // remove the front most solution, if we only want to save
          // a limited number of solutions, as the front most
          // solutions are the more unimportant ones
          if (solutionLimit && (puzzle->probSolutionNumber(prob) > solutionLimit))
            puzzle->probRemoveSolution(prob, 0);

          break;
        case SRT_LEVEL:
          {
            separationInfo_c * si = new separationInfo_c(s);

            for (unsigned int i = 0; i < puzzle->probSolutionNumber(prob); i++) {

              const separationInfo_c * s2 = puzzle->probGetDisassemblyInfo(prob, i);

              if (s2 && (s2->compare(si) > 0)) {
                if (parameters & PAR_DROP_DISASSEMBLIES) {
                  puzzle->probAddSolution(prob, a, si, i);
                  delete s;
                  si = 0;
                } else
                  puzzle->probAddSolution(prob, a, s, i);
                ins = true;
                break;
              }
            }

            if (!ins)  {
              if (parameters & PAR_DROP_DISASSEMBLIES) {
                puzzle->probAddSolution(prob, a, si);
                si = 0;
                delete s;
              } else
                puzzle->probAddSolution(prob, a, s);
            }

            if (si) delete si;
          }

          // remove the front most solution, if we only want to save
          // a limited number of solutions, as the front most
          // solutions are the more unimportant ones
          if (solutionLimit && (puzzle->probSolutionNumber(prob) > solutionLimit))
            puzzle->probRemoveSolution(prob, 0);

          break;
        case SRT_UNSORT:
          /* only save every solutionDrop-th solution */
          if (puzzle->probGetNumSolutions(prob) % (solutionDrop * dropMultiplicator) == 0) {
            if (parameters & PAR_DROP_DISASSEMBLIES) {
              separationInfo_c * si = new separationInfo_c(s);
              puzzle->probAddSolution(prob, a, si);
              delete s;
            } else
              puzzle->probAddSolution(prob, a, s);
          } else {
            delete a;
            delete s;
          }

          break;
      }

      // yes, the puzzle is disassembably, count solutions
      puzzle->probIncNumSolutions(prob);
    }
    break;
  }

  puzzle->probIncNumAssemblies(prob);

  // this is the case for assembly only or unsorted disassembly solutions
  // we need to thin out the list
  if (solutionLimit && (puzzle->probSolutionNumber(prob) > solutionLimit)) {
    unsigned int idx = (_solutionAction == SOL_SAVE_ASM) ? puzzle->probGetNumAssemblies(prob)-1
                                                         : puzzle->probGetNumSolutions(prob)-1;

    idx = (idx % (solutionLimit * solutionDrop * dropMultiplicator)) / (solutionDrop * dropMultiplicator);

    if (idx == solutionLimit-1)
      dropMultiplicator *= 2;

    puzzle->probRemoveSolution(prob, idx+1);
  }

  return true;
}

void assemblerThread_c::stop(void) {

  if ((action != ACT_ASSEMBLING) &&
      (action != ACT_REDUCE) &&
      (action != ACT_DISASSEMBLING) &&
      (action != ACT_PREPARATION)
     )
    return;

  action = ACT_WAIT_TO_STOP;

  if (puzzle->probGetAssembler(prob))
    puzzle->probGetAssembler(prob)->stop();

  stopPressed = true;
}

bool assemblerThread_c::start(bool stop_after_prep) {

  stopPressed = false;
  return_after_prep = stop_after_prep;
  startTime = time(0);

  // calculate dropMultiplicator

  dropMultiplicator = 1;

  unsigned int a;

  // only when we save count the possible assemblies we use the assembly counter, in all
  // other cases we use the solution counter
  if ((parameters & (PAR_JUST_COUNT | PAR_DISASSM)) == 0) {
    a = puzzle->probGetNumAssemblies(prob);
    if (!puzzle->probNumAssembliesKnown(prob))
      a = 0;
  } else {
    a = puzzle->probGetNumSolutions(prob);
    if (!puzzle->probNumSolutionsKnown(prob))
      a = 0;
  }

  while (a+solutionDrop > 2 * solutionLimit * solutionDrop) {
    dropMultiplicator *= 2;
    a = (a+1) / 2;
  }

  return thread_c::start();
}

unsigned int assemblerThread_c::currentActionParameter(void) {

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
