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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef __SUBTREE_PROGRESS_H__
#define __SUBTREE_PROGRESS_H__

/* Progress within a task, for the exact cover engines' task pools.
 *
 * assembler_1_c::getFinished() reports completed progress units over the
 * total (see TaskUnits). A task's units count only when the whole task is
 * done, so without more a long task holds the bar still. Each worker
 * therefore publishes how far it has got into the task it holds, in units,
 * through a slot; getFinished() adds the slots to the completed units.
 */

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

/* How much of one subtree a depth-first walk has got through: a node with
 * n children gives each child 1/n of its own weight.
 *
 * A stack mirrors the walk. Each level holds the weight of its node not yet
 * attributed anywhere (`remaining`), the weight one child slot gets
 * (`childShare`) and the weight held by the slot being handled now
 * (`curSlot`). Weight leaves the stack only into `done`, or is given away
 * (a donated part of the task).
 *
 *  - Conservation: done + given + (remaining + curSlot over the live stack)
 *    is 1. push() moves curSlot into the new level, beginSlot() never takes
 *    more than remaining, pop() credits what a level still holds. A wrong
 *    child count makes the value lumpy, never larger than 1 - given.
 *  - Monotonicity: done only grows.
 *
 * (From the dancing-links search's tracker, removed with it in "one
 * assembler, two engines".)
 */
class subtreeProgress_c {

  struct level_c {
    double remaining;   // share of this node not yet attributed anywhere
    double childShare;  // share each child slot of this node receives
    double curSlot;     // share held by the child slot being handled now
  };

  std::vector<level_c> levels;
  double done = 0;

public:

  /* start a walk whose root node is the whole task */
  void reset(void) {
    levels.clear();
    levels.push_back({ 1.0, 1.0, 0.0 });
    done = 0;
  }

  double doneShare(void) const { return done; }

  void setChildren(unsigned int n) {
    if (levels.empty()) return;
    levels.back().childShare = (n > 0) ? levels.back().remaining / n : 0.0;
  }

  void beginSlot(void) {
    if (levels.empty()) return;
    level_c & l = levels.back();
    const double s = std::min(l.childShare, l.remaining);
    l.remaining -= s;
    l.curSlot = s;
  }

  /* the current slot is finished without being descended into */
  void endSlot(void) {
    if (levels.empty()) return;
    done += levels.back().curSlot;
    levels.back().curSlot = 0.0;
  }

  /* descend into the current slot; it becomes the new top level */
  void push(void) {
    double s = 0.0;
    if (!levels.empty()) {
      s = levels.back().curSlot;
      levels.back().curSlot = 0.0;
    }
    levels.push_back({ s, s, 0.0 });
  }

  void pop(void) {
    if (levels.empty()) return;
    done += levels.back().remaining + levels.back().curSlot;
    levels.pop_back();
  }

  /* level i's untried child slots leave the walk (donated) */
  double takeRemaining(size_t i) {
    if (i >= levels.size()) return 0.0;
    const double r = levels[i].remaining;
    levels[i].remaining = 0.0;
    levels[i].childShare = 0.0;
    return r;
  }
};

/* One published value per worker: the progress units it has got through in
 * the task it holds. A worker stores 0 before it adds its finished task's
 * units to the completed count, so a reader that sums the slots between two
 * reads of that count (see assembler_1_c::getFinished()) never counts a
 * task twice. The vector is built before the workers start and cleared
 * after they stop, under the mutex, which only readers and those two steps
 * take.
 */
class workerProgressSlots_c {
  mutable std::mutex m;
  std::vector<std::unique_ptr<std::atomic<double>>> slots;

public:

  void reset(size_t n) {
    std::lock_guard<std::mutex> lock(m);
    slots.clear();
    for (size_t i = 0; i < n; i++)
      slots.push_back(std::make_unique<std::atomic<double>>(0.0));
  }

  void clear(void) {
    std::lock_guard<std::mutex> lock(m);
    slots.clear();
  }

  /* worker i's slot, null when there are not that many */
  std::atomic<double> * slot(size_t i) {
    std::lock_guard<std::mutex> lock(m);
    return i < slots.size() ? slots[i].get() : nullptr;
  }

  double sum(void) const {
    std::lock_guard<std::mutex> lock(m);
    double s = 0;
    for (const auto & p : slots)
      s += p->load(std::memory_order_relaxed);
    return s;
  }
};

/* What a worker publishes for the task it holds: `done` (a share of the task
 * as it started, `rootUnits`) in units, never as much as the units it still
 * holds (`units`), so the bar reaches the end only when the task is counted.
 */
inline double taskProgressUnits(double done, size_t rootUnits, size_t units) {
  const double v = done * static_cast<double>(rootUnits);
  const double cap = units > 0 ? static_cast<double>(units - 1) : 0.0;
  return v < cap ? v : cap;
}

#endif
