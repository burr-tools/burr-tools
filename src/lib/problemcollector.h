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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02110-1307, USA.
 */
#ifndef __PROBLEM_COLLECTOR_H__
#define __PROBLEM_COLLECTOR_H__

/** @file
 * Host-side consumer that drains solverResult_t items from a solverSession_c
 * into a problem_c, applying user-facing storage policies (solution count
 * limits, keep-every-Nth thinning, dropping move lists, live sorting, and
 * keeping the most complex solutions).
 *
 * Lives beside problem_c (outside src/lib/solver/) so the solver core never
 * touches problem_c or puzzle_c.
 */

#include "solver/solversession.h"

#include <atomic>
#include <cstdint>

class problem_c;

struct problemCollectorOptions_t {
  enum {
    SRT_UNSORT = 0,
    SRT_COMPLETE_MOVES = 1,
    SRT_LEVEL = 2
  };

  bool disassemble = false;
  /** count without storing. Without disassemble the session runs countOnly
   *  too (syncCountOnly()); with it the session enqueues every result and
   *  the collector counts them, storing only single-piece assemblies (the
   *  old PAR_JUST_COUNT | PAR_DISASSM behaviour) */
  bool countOnly = false;
  bool dropDisassemblies = false;
  int sortMethod = SRT_COMPLETE_MOVES;
  unsigned int solutionLimit = 10;
  unsigned int solutionDrop = 1;
};

class problemCollector_c {
public:
  problemCollector_c(problem_c & problem, problemCollectorOptions_t options = {});

  void setSortMethod(int sort) { options_.sortMethod = sort; }
  void setSolutionLimits(unsigned int limit, unsigned int drop = 1) {
    options_.solutionLimit = limit;
    options_.solutionDrop = drop;
  }

  /** If >= 0, keep the saved solution list sorted by this problem_c::sortSolutions
   *  method after every solution added. -1 = off. Safe to call from another thread. */
  void setLiveSort(int method) { liveSort_.store(method, std::memory_order_relaxed); }

  /** Compute initial thinning factor from problem's existing counts before a run. */
  void resetDropMultiplicator(void);

  /** Consume one result from solverSession_c on the host thread into problem_c. */
  void consume(solverResult_t && r);

  /** Sync counters in countOnly mode (where the session enqueues no results). */
  void syncCountOnly(uint64_t sessionAssemblies, uint64_t sessionSolutions);

  /** Drain all currently queued items from session without blocking. */
  size_t drainFrom(solverSession_c & session, size_t maxItems = SIZE_MAX);

  /** Pull and consume every result from session until pop() returns std::nullopt. */
  void consumeAll(solverSession_c & session);

private:
  void consumeAssemblyOnly(solverResult_t && r);
  void consumeDisassembled(solverResult_t && r);

  problem_c & problem_;
  problemCollectorOptions_t options_;
  unsigned int dropMultiplicator_ = 1;
  std::atomic<int> liveSort_{-1};
  uint64_t syncedAssemblies_ = 0;
  uint64_t syncedSolutions_ = 0;
};

#endif // __PROBLEM_COLLECTOR_H__
