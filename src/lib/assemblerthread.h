/* Burr Solver
 * Copyright (C) 2003-2007  Andreas R�ver
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
#ifndef __ASSEMBLERTHREAD_H__
#define __ASSEMBLERTHREAD_H__

#include "assembler.h"
#include "disassembler.h"
#include "bt_assert.h"

class puzzle_c;
class gridType_c;

/* this class will handle the solving of one problem of the puzzle, it can also
 * be used to continue an already started solution, so that you can save you results
 * and continue later on
 */
class assemblerThread_c : public assembler_cb {

  unsigned int action;
  unsigned int _solutionAction;

  const gridType_c * gt;
  puzzle_c * puzzle;
  unsigned int prob;

  assembler_c::errState errState;
  int errParam;

  bool stopPressed;
  bool return_after_prep;  // sometimes it is useful to only prepare and return,
                           // if this flag is set, the program will return

  bool _reduce;
  bool _dropDisassemblies;
  bool _keepMirrors;
  bool _keepRotations;

  time_t startTime;

  disassembler_c * disassm;
  assembler_c * assm;

  assert_exception *ae;

  int sortMethod;

  /* don't save more than this number of solutions 0 means no limit */
  unsigned int solutionLimit;

  /* save only every x-th solution, the others are dropped */
  unsigned int solutionDrop;

  /* this is used to increase the drop with time, when the limit is reached
   * and only every 2nd valid solution is taken
   */
  unsigned int dropMultiplicator;

public:

  enum {
    SOL_COUNT_ASM,
    SOL_COUNT_DISASM,
    SOL_SAVE_ASM,
    SOL_DISASM
  };

  static const int PAR_REDUCE = 1;
  static const int PAR_KEEP_MIRROR = 2;
  static const int PAR_KEEP_ROTATIONS = 4;

  // create all the necessary data structures to start the thread later on
  assemblerThread_c(puzzle_c * puz, unsigned int problemNum, unsigned int solAction, int par);

  // stop and exit
  virtual ~assemblerThread_c(void);

  // the callback
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
  bool start(bool stop_after_prep = false);

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

  enum {
    SRT_UNSORT,
    SRT_COMPLETE_MOVES,
    SRT_LEVEL
  };

  void setSortMethod(int sort) { sortMethod = sort; }

  void setSolutionLimits(unsigned int limit, unsigned int drop = 1) {
    solutionLimit = limit;
    solutionDrop = drop;
  }

  void setDropDisassemblies(bool drop) { _dropDisassemblies = drop; }

#ifdef WIN32
  friend unsigned long __stdcall start_th(void * c);
#else
  friend void* start_th(void * c);
#endif

};

#endif
