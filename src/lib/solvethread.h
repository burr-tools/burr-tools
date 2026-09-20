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
#include "disassemblerpool.h"
#include "thread_budget.h"
#include "bt_assert.h"

#include <time.h>
#include <atomic>
#include <memory>
#include <thread>

class problem_c;

/* this class will handle the solving of one problem of the puzzle, it can also
 * be used to continue an already started solution, so that you can save you results
 * and continue later on
 */
class solveThread_c : public assembler_cb {

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

    assembler_c::errState errState = assembler_c::ERR_NONE;
    int errParam = 0;

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

    time_t startTime = 0;

  public:

    /* how much time has passed since calling start */
    unsigned long getTime(void) { return time(0) - startTime; }

  private:

    problem_c & puzzle;
    int parameters;
    unsigned int numThreads;

  public:

    static const int PAR_REDUCE =             0x01;  // do a reduction after preparation
    static const int PAR_KEEP_MIRROR =        0x02;  // keep mirror solutions
    static const int PAR_KEEP_ROTATIONS =     0x04;  // keep rotated solutions
    static const int PAR_DROP_DISASSEMBLIES = 0x08;  // remove disassembly instructions after analysis
    static const int PAR_DISASSM =            0x10;  // do the disassembly analysis
    static const int PAR_JUST_COUNT =         0x20;  // just count the solutions, don't save them
    static const int PAR_COMPLETE_ROTATIONS = 0x40;  // do a thorough rotation check

    // create all the necessary data structures to start the thread later on;
    // threads is the worker count shared by the assembler and the
    // disassembler pool, 0 = auto (BURRTOOLS_THREADS, else hardware_concurrency)
    solveThread_c(problem_c & puz, int par, unsigned int threads = 0);
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
    unsigned int dropMultiplicator = 1;

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


  std::atomic<bool> stopPressed{false};  // set by the GUI thread, read by the worker
  bool return_after_prep = false;  // sometimes it is useful to only prepare and return,
                           // if this flag is set, the program will return



  /* Shared cap on working threads, declared BEFORE disasm_pool so it
   * outlives the pool (members destroy in reverse order; the pool dtor
   * touches the budget). Created iff a real (non-inline) pool exists and
   * budgeting is enabled; otherwise null and every budget call site
   * behaves exactly as without any cap.
   */
  std::unique_ptr<ThreadBudget> threadBudget_;

  std::unique_ptr<disassemblerPool_c> disasm_pool;

  /* the worker publishes the assembler here once it is fully constructed so
   * that currentActionParameter(), called from the GUI thread, can query its
   * progress. Atomic with release/acquire so the GUI never sees a
   * half-constructed object (which would be a vptr race on the virtual call).
   */
  std::atomic<assembler_c *> assm;

  std::jthread worker_thread;
  std::atomic<bool> running{false};

  /* Progress state, all written by the worker and read by the GUI thread.
   *
   * The assembly phase has no cost meter of its own, so its cost is derived:
   * worker-seconds = wall time the phase ran, times the number of threads that
   * ran it. assemblyEndNs freezes that wall time when assemble() returns --
   * without it the assembly cost would keep growing throughout the disassembly
   * tail, when no assembly work is happening at all, and would drag the blend
   * towards 1 for a reason unrelated to the work left.
   *
   * assemblyStartNs doubles as the release/acquire edge that lets the GUI
   * touch the assembler at all: `assm` is published before createMatrix() and
   * reduce() build the matrix getFinished() reads, so polling across that is a
   * genuine data race.
   *
   * reportedProgress is the monotone guard. It is never reset: the GUI builds
   * one solveThread_c per solve and destroys it when the solve ends, so the
   * object's lifetime is exactly the span the bar must not move backwards over.
   */
  std::atomic<long long> assemblyStartNs{0};   // 0 = assembly has not started
  std::atomic<long long> assemblyEndNs{0};     // 0 = assembly still running
  std::atomic<unsigned int> assemblyThreads{1};
  mutable std::atomic<float> reportedProgress{0.0f};

public:

  // stop and exit
  virtual ~solveThread_c(void);

  /** return true, if the thread is running */
  bool isRunning(void) const { return running.load(std::memory_order_relaxed); }

  void joinThread(void) {
    if (worker_thread.joinable())
      worker_thread.join();
  }

private:

  // helper to stop without virtual dispatch in destructor
  void stopInternal(void);

  // the call-back
  bool assembly(std::unique_ptr<assembly_c> a) override;

  // Shared thread budget for the search (null when uncapped).
  ThreadBudget *threadBudget() override { return threadBudget_.get(); }

  void onDisassemblyResult(uint64_t seqNo, std::unique_ptr<assembly_c> a, std::unique_ptr<separation_c> s);

public:

  // let the thread start
  // returns true, if everything went well, false otherwise
  bool start(bool stop_after_prep = false);

  /* Whole-solve progress in [0,1] for the GUI's bar and its time estimate.
   *
   * This is the only place that can see both phases, so this is where the
   * assembler's own fraction and the disassembly pool's are blended, weighted
   * by each phase's measured cost (see progressModel_c). Cost-weighted rather
   * than count-weighted so that elapsed/progress - elapsed is a usable estimate
   * of the time remaining.
   *
   * Monotone -- projections revise as evidence accumulates, and a bar that
   * moves backwards reads as a bug -- and strictly below 1.0 until the solve
   * actually reaches ACT_FINISHED. Safe to call from the GUI thread at any
   * time, including before the thread is started and after it has ended.
   */
  float getProgress(void) const;

  // try to stop the thread at the next possible position
  void stop(void);

  /* true once the worker has left run() for good. ACT_ASSERT belongs here:
   * an assert in the worker ends the thread just as surely as the other three,
   * and a caller polling for the thread to finish would otherwise wait forever.
   */
  bool stopped(void) const {
    return ((action == ACT_PAUSING) ||
            (action == ACT_FINISHED) ||
            (action == ACT_ERROR) ||
            (action == ACT_ASSERT)
           );
  }

  void run(void);

private:

  // no copying and assigning
  solveThread_c(const solveThread_c&);
  void operator=(const solveThread_c&);
};

#endif
