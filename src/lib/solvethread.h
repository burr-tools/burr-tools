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
#ifndef __SOLVETHREAD_H__
#define __SOLVETHREAD_H__

#include "assembler.h"
#include "disassembler.h"
#include "bt_assert.h"
#include "thread.h"

#include <time.h>
#include <atomic>

class problem_c;

/* this class will handle the solving of one problem of the puzzle, it can also
 * be used to continue an already started solution, so that you can save you results
 * and continue later on
 */
class solveThread_c : public assembler_cb, public thread_c {

  public:

    enum {
      ACT_PREPARATION,
      ACT_REDUCE,
      ACT_ASSEMBLING,
      ACT_DISASSEMBLING,
      ACT_PAUSING,
      ACT_FINISHED,
      ACT_ERROR,
      ACT_ASSERT,
      ACT_WAIT_TO_STOP
    };

  private:
    /* what is currently happening the the assembler thread. This is written by
     * the worker thread and read (and written, via stop()) by the GUI thread,
     * so it must be atomic. It also gates access to errState/errParam/ae which
     * are published before action is set to ACT_ERROR / ACT_ASSERT.
     */
    std::atomic<unsigned int> action;

  public:
    /* return the current activity */
    unsigned int currentAction(void) { return action; }

    /* some activities might have a parameter, return that */
    unsigned int currentActionParameter(void);

  private:

    assembler_c::errState errState;
    int errParam;

  public:

    assembler_c::errState getErrorState(void) {
      bt_assert(action == ACT_ERROR);
      return errState;
    }
    int getErrorParam(void) {
      bt_assert(action == ACT_ERROR);
      return errParam;
    }

  private:

    time_t startTime;

  public:

    /* how much time has passed since calling start */
    unsigned long getTime(void) { return time(0) - startTime; }

  private:

    problem_c & puzzle;
    int parameters;

  public:

    static const int PAR_REDUCE =             0x01;  // do a reduction after preparation
    static const int PAR_KEEP_MIRROR =        0x02;  // keep mirror solutions
    static const int PAR_KEEP_ROTATIONS =     0x04;  // keep rotated solutions
    static const int PAR_DROP_DISASSEMBLIES = 0x08;  // remove disassembly instructions after analysis
    static const int PAR_DISASSM =            0x10;  // do the disassembly analysis
    static const int PAR_JUST_COUNT =         0x20;  // just count the solutions, don't save them
    static const int PAR_COMPLETE_ROTATIONS = 0x40;  // do a thorough rotation check

    // create all the necessary data structures to start the thread later on
    solveThread_c(problem_c & puz, int par);
    const problem_c & getProblem(void) const { return puzzle; }

  private:

    int sortMethod;

  public:

    enum {
      SRT_UNSORT,
      SRT_COMPLETE_MOVES,
      SRT_LEVEL
    };

    void setSortMethod(int sort) { sortMethod = sort; }

    /* If >= 0, the worker keeps the (limit-bounded) solution list sorted by
     * this problem_c::sortSolutions method after every solution it adds, so a
     * sort chosen in the GUI stays applied as new solutions arrive. -1 = off.
     * Atomic: set from the GUI thread, read by the worker.
     */
    void setLiveSort(int method) { liveSort.store(method, std::memory_order_relaxed); }

  private:

    std::atomic<int> liveSort;

    /* don't save more than this number of solutions 0 means no limit */
    unsigned int solutionLimit;

    /* save only every x-th solution, the others are dropped */
    unsigned int solutionDrop;

    /* this is used to increase the drop with time, when the limit is reached
     * and only every 2nd valid solution is taken
     */
    unsigned int dropMultiplicator;

  public:

    void setSolutionLimits(unsigned int limit, unsigned int drop = 1) {
      solutionLimit = limit;
      solutionDrop = drop;
    }

  private:

    assert_exception ae;

  public:

    const assert_exception & getAssertException(void) {
      return ae;
    }

  private:


  std::atomic<bool> stopPressed;  // set by the GUI thread, read by the worker
  bool return_after_prep;  // sometimes it is useful to only prepare and return,
                           // if this flag is set, the program will return



  disassembler_c * disassm;

  /* the worker publishes the assembler here once it is fully constructed so
   * that currentActionParameter(), called from the GUI thread, can query its
   * progress. Atomic with release/acquire so the GUI never sees a
   * half-constructed object (which would be a vptr race on the virtual call).
   */
  std::atomic<assembler_c *> assm;





public:


  // stop and exit
  virtual ~solveThread_c(void);

private:

  // the call-back
  bool assembly(assembly_c* a);

public:

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

  void run(void);

private:

  // no copying and assigning
  solveThread_c(const solveThread_c&);
  void operator=(const solveThread_c&);
};

#endif
