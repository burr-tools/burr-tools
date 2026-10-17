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

/* Donation (split on demand): a running assembler_0 DLX task hands the
 * unexplored rows of its shallowest open level to the pool whenever a
 * sibling is waiting for work, instead of tasks being split eagerly at pop
 * time. BURRTOOLS_NO_DONATE=1 restores the eager pop-time splitting (A/B
 * toggle). Read once per run. */
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
