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


#ifndef __ASSEMBLYCALLBACK_H__
#define __ASSEMBLYCALLBACK_H__

#include "puzzle.h"
#include "assembler.h"
#include "disassembler_0.h"
#include "bt_assert.h"

/* this class will handle the solving of one problem of the puzzle, it can also
 * be used to continue an already started solution, so that you can save you results
 * and continue later on
 */
class assemblerThread : public assembler_cb {

  unsigned int action;
  unsigned int _solutionAction;

  puzzle_c * puzzle;
  unsigned int prob;

  assembler_c::errState errState;
  int errParam;

  bool stopPressed;

  bool _reduce;

  time_t startTime;

  disassembler_0_c disassm;
  assembler_c * assm;

  assert_exception *ae;

public:

  enum {
    SOL_COUNT_ASM,
    SOL_COUNT_DISASM,
    SOL_SAVE_ASM,
    SOL_DISASM
  };

  // create all the necessary data structures to start the thread later on
  assemblerThread(puzzle_c * puz, unsigned int problemNum, unsigned int solAction, bool reduce = false);

  // stop and exit
  virtual ~assemblerThread(void);

  // the callbacl
  bool assembly(assembly_c* a);

  enum {
    ACT_PREPARATION,
    ACT_REDUCE,
    ACT_ASSEMBLING,
    ACT_DISASSEMBLING,
    ACT_PAUSING,
    ACT_FINISHED,
    ACT_ERROR,
    ACT_WAIT_TO_STOP
  };

  unsigned int currentAction(void) { return action; }

  unsigned int currentActionParameter(void);

  // let the thread start
  // returns true, if everything went well, false otherwise
  bool start(void);

  // try to stop the thread at the next possible position
  void stop(void);

  bool stopped(void) const {
    return ((action == ACT_PAUSING) ||
            (action == ACT_FINISHED) ||
            (action == ACT_ERROR)
           );
  }

  unsigned int getProblem(void) { return prob; }
  assembler_c::errState getErrorState(void) { return errState; }
  int getErrorParam(void) { return errParam; }

  unsigned long getTime(void) { return time(0) - startTime; }

  assert_exception * getAssertException(void) { return ae; }


#ifdef WIN32
  friend unsigned long __stdcall start_th(void * c);
#else
  friend void* start_th(void * c);
#endif

};

#endif
