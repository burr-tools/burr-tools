/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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
#include "assm_0_frontend_0.h"
#include "puzzle.h"
#include "assembly.h"

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#ifdef WIN32
unsigned long __stdcall start_th(void * c)
#else
void* start_th(void * c)
#endif
{
  assemblerThread_c * p = (assemblerThread_c*)c;

  try {

    /* first check, if there is an assembler available with the
     * problem, if there is one take that
     */
    if (p->puzzle->probGetAssembler(p->prob))
      p->assm = p->puzzle->probGetAssembler(p->prob);

    else {

      /* otherwise we have to chreate a new one
       */
      p->action = assemblerThread_c::ACT_PREPARATION;
      p->assm = p->puzzle->getGridType()->getAssembler();

      p->errState = p->assm->createMatrix(p->puzzle, p->prob);
      if (p->errState != assm_0_frontend_0_c::ERR_NONE) {

        p->errParam = p->assm->getErrorsParam();

        p->action = assemblerThread_c::ACT_ERROR;

        delete p->assm;
        return 0;
      }

      if (p->_reduce) {

        if (!p->stopPressed)
          p->action = assemblerThread_c::ACT_REDUCE;

        p->assm->reduce();
      }

      /* set the assembler to the problem as soon as it is finished
       * with initialisation, NOT EARLIER as the function
       * also restores the assembler state to a state that might
       * be saved within the problem
       */
      p->errState = p->puzzle->probSetAssembler(p->prob, p->assm);
      if (p->errState != assm_0_frontend_0_c::ERR_NONE) {
        p->action = assemblerThread_c::ACT_ERROR;
        return 0;
      }
    }

    if (!p->stopPressed) {

      p->action = assemblerThread_c::ACT_ASSEMBLING;
      p->assm->assemble(p);

      if (p->assm->getFinished() >= 1) {
        p->action = assemblerThread_c::ACT_FINISHED;
        p->puzzle->probFinishedSolving(p->prob);
      } else
        p->action = assemblerThread_c::ACT_PAUSING;

    } else
      p->action = assemblerThread_c::ACT_PAUSING;

    p->puzzle->probAddTime(p->prob, time(0)-p->startTime);
  }

  catch (assert_exception *a) {

    p->ae = a;
    p->action = assemblerThread_c::ACT_ERROR;
    if (p->puzzle->probGetAssembler(p->prob))
      p->puzzle->probRemoveAllSolutions(p->prob);
  }

  return 0;
}

assemblerThread_c::assemblerThread_c(puzzle_c * puz, unsigned int problemNum, unsigned int solAction, bool red) :
action(ACT_PREPARATION),
_solutionAction(solAction),
puzzle(puz),
prob(problemNum),
_reduce(red),
disassm(puz->getGridType()->getDisassembler(puz, problemNum)),
ae(0),
sortMethod(SRT_COMPLETE_MOVES),
solutionLimit(10),
solutionDrop(1)
{
}

assemblerThread_c::~assemblerThread_c(void) {

  if (puzzle->probGetAssembler(prob)) {

    puzzle->probGetAssembler(prob)->stop();

    while (!puzzle->probGetAssembler(prob)->stopped())
#ifdef WIN32
      Sleep(1);
#else
      usleep(10000);
#endif
  }

  delete disassm;
}

bool assemblerThread_c::assembly(assembly_c * a) {

  switch(_solutionAction) {
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
      // to disassemble, the disassembler will return 0 anyways
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
        // no diassembly sequence found, delete assembly
        delete a;

        break;
      }

      // if the user wants to save the solution, do it
      if (_solutionAction != SOL_DISASM) {

        // if not, delete disassembly AND assembly
        delete s;
        delete a;

        // yes, the puzzle is disassembably, count solutions
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

              const separation_c * s2 = puzzle->probGetDisassembly(prob, i);

              if (s2 && s2->sumMoves() > lev) {
                puzzle->probAddSolution(prob, a, s, i);
                ins = true;
                break;
              }
            }
          }

          if (!ins) puzzle->probAddSolution(prob, a, s);

          // remove the front most solution, if we only want to save
          // a limited number of solutions, as the front most
          // solutions are the more unimportant ones
          if (solutionLimit && (puzzle->probSolutionNumber(prob) > solutionLimit))
            puzzle->probRemoveSolution(prob, 0);

          break;
        case SRT_LEVEL:
          {
            for (unsigned int i = 0; i < puzzle->probSolutionNumber(prob); i++) {

              const separation_c * s2 = puzzle->probGetDisassembly(prob, i);

              if (s2 && (s2->compare(s) > 0)) {
                puzzle->probAddSolution(prob, a, s, i);
                ins = true;
                break;
              }
            }
          }

          if (!ins) puzzle->probAddSolution(prob, a, s);

          // remove the front most solution, if we only want to save
          // a limited number of solutions, as the front most
          // solutions are the more unimportant ones
          if (solutionLimit && (puzzle->probSolutionNumber(prob) > solutionLimit))
            puzzle->probRemoveSolution(prob, 0);

          break;
        case SRT_UNSORT:
          /* only save every solutionDrop-th solution */
          if (puzzle->probGetNumSolutions(prob) % (solutionDrop * dropMultiplicator) == 0)
            puzzle->probAddSolution(prob, a, s);
          else {
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

bool assemblerThread_c::start(void) {

  stopPressed = false;
  startTime = time(0);

  // calculate dropMultiplicator

  dropMultiplicator = 1;

  unsigned int a;

  if (_solutionAction == SOL_SAVE_ASM) {
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

#ifdef WIN32
  DWORD threadID;
  return CreateThread(NULL, 0, start_th, this, 0, &threadID) != NULL;
#else
  pthread_t th;
  return pthread_create(&th, 0, start_th, this) == 0;
#endif
}

unsigned int assemblerThread_c::currentActionParameter(void) {

  switch(action) {
  case ACT_REDUCE:
    return assm->getReducePiece();

  default:
    return 0;
  }

}
