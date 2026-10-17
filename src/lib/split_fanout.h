/* BurrTools: an open source tool for solving and designing burr puzzles
 *
 * Copyright (C) 2003-2026 Andreas Röver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __SPLIT_FANOUT_H__
#define __SPLIT_FANOUT_H__

#include <cstddef>
#include <cstdlib>

/**
 * Fan-out cap K for dividing one search level into tasks: a level with
 * more remaining branches than K yields K singleton tasks plus one rest
 * task for the others. Used by assembler_0's runtime splitter and by the
 * stop-time remainders of every engine; it bounds the queue and the saved
 * remainder at O(depth x K) per task. Any K >= 1 is exact -- K trades
 * remainder size against how finely a resume can balance its work.
 *
 * BURRTOOLS_SPLIT_FANOUT=N overrides the default. Read once per run (never
 * per node). Tests set it low so small puzzles exercise every rest path
 * that only wide levels reach at the default.
 */
namespace SplitFanout {

inline constexpr unsigned int kDefault = 16;

/* BURRTOOLS_SPLIT_BREADTH_FIRST=1 queues assembler_0's split children at
 * the back (breadth-first order) instead of the front, for A/B
 * benchmarking the depth-first order (see AssemblyTaskPool::
 * push_tasks_front). Read once per run. */
inline bool breadthFirstFromEnv() {
  return std::getenv("BURRTOOLS_SPLIT_BREADTH_FIRST") != nullptr;
}

/* Donation (split on demand): a running task (assembler_0 DLX and SIMD,
 * Huang) hands the unexplored part of its shallowest open level to the
 * pool whenever a sibling is waiting for work, and stops before that part
 * itself. Tasks are thereby split where the work actually is, instead of
 * eagerly at pop time by depth guesses that cannot split a task found
 * large after it started. Busy workers poll the pool's hunger flag every
 * few hundred nodes; one donor claims it per waiter.
 * BURRTOOLS_NO_DONATE=1 turns it off (A/B toggle): assembler_0 then splits
 * eagerly at pop time again, Huang runs its tasks as seeded. Read once per
 * run. */
inline bool noDonateFromEnv() {
  return std::getenv("BURRTOOLS_NO_DONATE") != nullptr;
}

/* BURRTOOLS_DONATE_EVERY=N (tests only): additionally donate every N search
 * nodes whether or not anyone is waiting, so donation paths run
 * deterministically even single-threaded. 0 / unset = only on demand. */
inline unsigned long donateEveryFromEnv() {
  const char * env = std::getenv("BURRTOOLS_DONATE_EVERY");
  if (env != nullptr && *env != '\0') {
    const long val = std::strtol(env, nullptr, 10);
    if (val >= 1)
      return static_cast<unsigned long>(val);
  }
  return 0;
}

} // namespace SplitFanout

/* Progress units of one generated or restored task in the engines whose
 * progress counts tasks (assembler_1, Huang): a task that donates part of
 * its territory, or is divided at a stop, splits its units among the
 * pieces instead of adding tasks, so the total does not move and the bar
 * does not step back.
 *
 * Invariant: every outstanding task holds at least one unit. Completed
 * units reach the total only when no work is left, and solvethread treats
 * a full bar as a finished search -- a 0-unit piece would let it finish
 * with that piece unsearched. So a donor gives away units only while it
 * holds two or more and keeps at least one (give() below), and a stop that
 * divides a task into more pieces than it has units adds the missing
 * units to the total (stop time only, where a small dip is harmless). */
namespace TaskUnits {
inline constexpr size_t kWhole = size_t(1) << 20;

/* Units for a donated piece out of `held`, given its estimated share of
 * `root` units: at least 1, and at least 1 left to the donor. 0 means the
 * donor cannot spare any (held < 2) and must not donate. */
inline size_t give(size_t held, size_t root, double share) {
  if (held < 2)
    return 0;
  double est = share * static_cast<double>(root);
  size_t u = est < 1.0 ? 1 : static_cast<size_t>(est);
  return u > held - 1 ? held - 1 : u;
}
} // namespace TaskUnits

namespace SplitFanout {

inline unsigned int fromEnv() {
  const char * env = std::getenv("BURRTOOLS_SPLIT_FANOUT");
  if (env != nullptr && *env != '\0') {
    const long val = std::strtol(env, nullptr, 10);
    if (val >= 1)
      return val > 4096 ? 4096u : static_cast<unsigned int>(val);
  }
  return kDefault;
}


/* Divide one frame's untried branches into stop-time remainder pieces:
 * of the indices next..end-1 of the frame's active-row list that `takes`
 * accepts, the first `fanout` become singletons, emit(j, false), and the
 * next one starts a single rest, emit(j, true), that stands for every
 * branch from index j to the END OF THE LIST (a rest resumes the frame's
 * loop at j on the identically replayed state). So `end` may only be below
 * the list size when nothing past it is this frame's to record. Disjoint
 * and complete by construction; shared by the SIMD engines so the rest
 * indexing cannot drift between them.
 */
template <class Rows, class Takes, class Emit>
void emitFrame(const Rows & active, size_t next, size_t end, unsigned int fanout,
               Takes takes, Emit emit) {
  unsigned int emitted = 0;
  for (size_t j = next; j < end; j++) {
    if (!takes(active[j]))
      continue;
    if (emitted == fanout) {
      emit(j, true);
      return;
    }
    emit(j, false);
    emitted++;
  }
}
} // namespace SplitFanout

#endif // __SPLIT_FANOUT_H__
