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
#include "cells_cover.h"

#include "assembler_pool.h"
#include "bt_assert.h"
#include "solver_config.h"
#include "thread_budget.h"

#include <exception>
#include <thread>

/* what a running task needs to hand part of itself to a waiting worker */
struct CellsCover::Donor {
  AssemblyTaskPool<HuangSeed> * pool;
  size_t * units;             // progress units the task still holds
  unsigned long interval;     // search nodes between two looks at the pool
  bool forced;                // tests: donate at every look
  std::atomic<unsigned long> * count;
  uint64_t nextPoll;
  std::atomic<size_t> * total; // the run's progress units
};

CellsCover::CellsCover(unsigned int num_columns_, unsigned int num_shapes_,
                       const solverSettings_t & settings)
  : num_columns(num_columns_), num_shapes(num_shapes_), donate(settings.donate),
    donateEvery(settings.donateEvery), fanout(settings.splitFanout), items(num_columns_ + 1) {
  optStart.push_back(0);
}

void CellsCover::setColumnBounds(unsigned int col, unsigned int min_w, unsigned int max_w,
                                 bool, bool, bool is_range, bool is_hole) {
  if (col >= items.size())
    bt_te("CellsCover::setColumnBounds: column out of range");
  items[col].min = min_w;
  items[col].max = max_w;
  items[col].declared = true;
  items[col].hole = is_hole;
  if (is_range)
    rangeItem = col;
}

uint32_t CellsCover::addRow(unsigned int node_id, unsigned int, unsigned int, unsigned int,
                            unsigned int range_weight, const std::vector<unsigned int> & cols,
                            const std::vector<unsigned int> & weights) {
  const uint32_t o = static_cast<uint32_t>(optNode.size());
  for (size_t i = 0; i < cols.size(); i++) {
    if (cols[i] >= items.size())
      bt_te("CellsCover::addRow: column out of range");
    if (cols[i] == rangeItem)
      continue;
    // only the range column counts in units other than rows
    if (weights[i] != 1)
      bt_te("CellsCover::addRow: weight other than 1 outside the range column");
    cellItem.push_back(cols[i]);
    cellOpt.push_back(o);
  }
  if (cellItem.size() == optStart.back())
    bt_te("CellsCover::addRow: row without columns");
  optStart.push_back(static_cast<uint32_t>(cellItem.size()));
  optRange.push_back(rangeItem == NONE ? 0 : range_weight);
  optNode.push_back(node_id);
  nodeToOpt[node_id] = o;
  return o;
}

void CellsCover::registerNodeAlias(unsigned int node_id, uint32_t row_idx) {
  nodeToOpt[node_id] = row_idx;
}

void CellsCover::build(void) const {
  std::call_once(builtOnce, [this] {
    const size_t n = items.size();
    std::vector<uint32_t> count(n, 0);
    for (uint32_t j : cellItem) {
      if (!items[j].declared)
        bt_te("CellsCover: row in a column without bounds");
      count[j]++;
    }
    itemBase.assign(n, 0);
    uint32_t at = 0;
    for (size_t j = 0; j < n; j++) {
      itemBase[j] = at;
      at += count[j];
    }
    base.set.assign(cellItem.size(), 0);
    base.pos.assign(cellItem.size(), 0);
    base.size.assign(n, 0);
    base.cnt.assign(n, 0);
    base.act.clear();
    base.actPos.assign(n, NONE);
    itemHole.assign(n, 0);
    itemMin.assign(n, 0);
    itemMax.assign(n, 0);
    base.need.assign(n, 0);
    for (size_t j = 0; j < n; j++) {
      itemMin[j] = items[j].min;
      itemMax[j] = items[j].max;
      base.need[j] = static_cast<int32_t>(items[j].min);
    }
    for (uint32_t c = 0; c < cellItem.size(); c++) {
      const uint32_t j = cellItem[c];
      base.pos[c] = itemBase[j] + base.size[j];
      base.set[base.pos[c]] = c;
      base.size[j]++;
    }
    /* A column that takes nothing (max 0) can have none of its rows: they
     * go before the search starts. It stays open only to fail the search
     * when it also wants something (min > 0), and otherwise is not a
     * column at all, nor a hole. assembler_1 never makes one (problem_c
     * drops parts with a maximum of 0); a direct caller can.
     */
    for (uint32_t j = 0; j < n; j++) {
      if (!items[j].declared || j == rangeItem || itemMax[j] > 0)
        continue;
      while (base.size[j] > 0) {
        const uint32_t o = cellOpt[base.set[itemBase[j] + base.size[j] - 1]];
        hide(base, o); // a column left short is found by choose() at the root
      }
    }
    base.nAct = 0;
    for (uint32_t j = 0; j < n; j++) {
      if (!items[j].declared || j == rangeItem)
        continue;
      if (itemMax[j] == 0 && itemMin[j] == 0)
        continue;
      base.actPos[j] = static_cast<uint32_t>(base.act.size());
      base.act.push_back(j);
      if (items[j].hole) {
        itemHole[j] = 1;
        holeCount++;
      }
    }
    base.nAct = static_cast<uint32_t>(base.act.size());
  });
}

/* Take row o out of the list of each of its columns. false: one of them
 * can then no longer reach its minimum. The row is taken out completely
 * either way, so the lists stay consistent.
 */
inline bool CellsCover::hide(State & st, uint32_t o) const {
  bool ok = true;
  for (uint32_t c = optStart[o]; c < optStart[o + 1]; c++) {
    const uint32_t j = cellItem[c];
    const uint32_t p = st.pos[c];
    const uint32_t last = itemBase[j] + --st.size[j];
    const uint32_t other = st.set[last];
    st.set[p] = other;
    st.pos[other] = p;
    st.set[last] = c;
    st.pos[c] = last;
    if (static_cast<int32_t>(st.size[j]) < st.need[j])
      ok = false;
  }
  return ok;
}

/* Row o joins the solution: it and every row of a column it fills go out,
 * and a column that is full leaves the open columns. false: that leaves
 * some column short of its minimum, found as soon as it happens and not a
 * node later. The caller takes the selection back the same way in both
 * cases (restore() and unselect()).
 */
bool CellsCover::select(State & st, uint32_t o) const {
  st.sol.push_back(o);
  if (rangeItem != NONE)
    st.cnt[rangeItem] += optRange[o];
  // counted before the row is hidden, or its own columns would look short
  for (uint32_t c = optStart[o]; c < optStart[o + 1]; c++) {
    st.cnt[cellItem[c]]++;
    st.need[cellItem[c]]--;
  }
  if (!hide(st, o))
    return false;
  for (uint32_t c = optStart[o]; c < optStart[o + 1]; c++) {
    const uint32_t j = cellItem[c];
    if (st.cnt[j] < itemMax[j])
      continue;
    // out of the open columns: swapped behind their end, like a row
    const uint32_t p = st.actPos[j], last = --st.nAct, other = st.act[last];
    st.act[p] = other;
    st.actPos[other] = p;
    st.act[last] = j;
    st.actPos[j] = last;
    while (st.size[j] > 0)
      if (!hide(st, cellOpt[st.set[itemBase[j] + st.size[j] - 1]]))
        return false;
  }
  return true;
}

/* the counters of select(); the lists come back with restore() */
void CellsCover::unselect(State & st, uint32_t o) const {
  for (uint32_t c = optStart[o]; c < optStart[o + 1]; c++) {
    st.cnt[cellItem[c]]--;
    st.need[cellItem[c]]++;
  }
  if (rangeItem != NONE)
    st.cnt[rangeItem] -= optRange[o];
  st.sol.pop_back();
}

/* The frame at top has done (or cannot do) the search with its row: go on
 * without the row, unless that part is not this stack's, or the column the
 * row was picked for can no longer reach its minimum without it.
 */
void CellsCover::discard(State & st, size_t top) const {
  Frame & f = st.frames[top];
  if (f.closed) {
    st.frames.pop_back();
    return;
  }
  if (f.item != NONE) {
    const uint32_t have = st.cnt[f.item], min = items[f.item].min;
    if (have < min && st.size[f.item] < min - have + 1) {
      st.frames.pop_back();
      return;
    }
  }
  // a column short of its minimum without the row: nothing to search
  // (the row stays out until a frame below restores its lists)
  if (!hide(st, f.x)) {
    st.frames.pop_back();
    return;
  }
  f.state = 2;
  st.frames.emplace_back();
}

/* Rebuild the stack a seed describes on a state with nothing selected.
 * REPLAY_DEAD: a step leaves a column short of its minimum, so the seed
 * holds no solution (a donated "without this row" can turn out that way).
 * REPLAY_INVALID: the seed is not one of this matrix: a row it names is
 * unknown, or not available when its turn comes.
 */
CellsCover::Replay CellsCover::replay(const HuangSeed & seed, State & st) const {
  if (seed.pivotShape.size() != seed.prefix.size())
    return REPLAY_INVALID;
  st.frames.clear();
  st.snap.clear();
  for (size_t i = 0; i < seed.prefix.size(); i++) {
    auto it = nodeToOpt.find(seed.prefix[i]);
    if (it == nodeToOpt.end() || !present(st, it->second))
      return REPLAY_INVALID;
    Frame f;
    f.x = it->second;
    switch (seed.pivotShape[i]) {
      case STEP_DISCARD:
        f.state = 2;
        st.frames.push_back(f);
        if (!hide(st, f.x))
          return REPLAY_DEAD;
        break;
      case STEP_SELECT:
      case STEP_SELECT_OPEN:
        if (!fitsRange(st, f.x))
          return REPLAY_INVALID;
        f.state = 1;
        f.closed = seed.pivotShape[i] == STEP_SELECT;
        f.snap = snapshot(st);
        st.frames.push_back(f);
        if (!select(st, f.x))
          return REPLAY_DEAD;
        break;
      default:
        return REPLAY_INVALID;
    }
  }
  st.frames.emplace_back();
  return REPLAY_OK;
}

/* the stack as a seed; only valid while its top frame is still unlooked at */
HuangSeed CellsCover::freeze(const State & st) const {
  HuangSeed seed;
  for (const Frame & f : st.frames) {
    if (f.state == 0)
      break;
    seed.prefix.push_back(optNode[f.x]);
    seed.pivotShape.push_back(f.state == 2 ? STEP_DISCARD
                              : f.closed ? STEP_SELECT : STEP_SELECT_OPEN);
  }
  return seed;
}

/* Split on demand: hand the search without the row of the shallowest frame
 * that still owes it to the pool. Shallowest, because that is the largest
 * piece this task has left.
 */
void CellsCover::donateFrame(State & st, Donor & donor) const {
  for (size_t k = 0; k < st.frames.size(); k++) {
    Frame & f = st.frames[k];
    if (f.state != 1 || f.closed)
      continue;
    // one donor per waiter (see SimdHuangCover::donateFrame); claimed
    // first, so a lost race costs nothing
    if (!donor.forced && !donor.pool->claim_work())
      return;
    /* The piece without the row is usually the larger one, and it is the
     * one handed on again, so a fixed share of the units runs out after a
     * few hand-overs. Half, and a task down to its last unit gets one more
     * (the total grows by one in a million, as at a stop: see TaskUnits).
     */
    if (*donor.units < 2) {
      donor.total->fetch_add(1, std::memory_order_relaxed);
      ++*donor.units;
    }
    const size_t units = *donor.units / 2;
    HuangSeed seed;
    for (size_t i = 0; i < k; i++) {
      seed.prefix.push_back(optNode[st.frames[i].x]);
      seed.pivotShape.push_back(st.frames[i].state == 2 ? STEP_DISCARD : STEP_SELECT);
    }
    seed.prefix.push_back(optNode[f.x]);
    seed.pivotShape.push_back(STEP_DISCARD);
    seed.units = units;
    f.closed = true;
    *donor.units -= units;
    if (donor.count != nullptr)
      donor.count->fetch_add(1, std::memory_order_relaxed);
    std::vector<HuangSeed> one;
    one.push_back(std::move(seed));
    donor.pool->push_tasks(std::move(one));
    return;
  }
}

/* What the node the state is at branches on: the column with the fewest
 * ways left to reach its minimum, or, with every minimum met, any column
 * that still has rows (more pieces of a shape below its maximum, into
 * variable voxels: with and without each of them is the same branching,
 * and what is reported is the leaf where none is left, every set of rows
 * once). NONE: a leaf. dead: some bound can no longer be met.
 */
uint32_t CellsCover::choose(const State & st, bool & dead) const {
  uint32_t best = NONE, bestWays = UINT32_MAX, spare = NONE;
  unsigned int empty = 0;
  const bool holeLimit = holes < holeCount;
  dead = false;
  // the open columns only: a full one has no rows and wants none
  for (uint32_t k = 0; k < st.nAct; k++) {
    const uint32_t j = st.act[k];
    const uint32_t have = st.cnt[j], min = itemMin[j], size = st.size[j];
    if (have >= min) {
      // variable voxels nothing can fill any more
      if (size == 0) {
        if (holeLimit && have == 0 && itemHole[j] && ++empty > holes) {
          dead = true;
          return NONE;
        }
      } else if (j < spare) {
        spare = j;
      }
      continue;
    }
    const uint32_t need = min - have;
    if (size < need) {
      dead = true;
      return NONE;
    }
    // the order of the open columns changes with the search: ties go to
    // the lower column, so the choice does not
    const uint32_t ways = size - need + 1;
    if (ways < bestWays || (ways == bestWays && j < best)) {
      bestWays = ways;
      best = j;
    }
  }
  return best != NONE ? best : spare;
}

/* The search divided into pieces before any worker starts, so that they
 * all have work at once: the node of a piece becomes one piece per row of
 * the column it branches on (with that row and without those before it),
 * level by level until there are enough. At most `fanout` rows of a node
 * are taken out singly; one more piece stands for the rest.
 */
std::vector<HuangSeed> CellsCover::generateTasks(size_t target) const {
  std::vector<HuangSeed> tasks(1);
  State st;
  for (int level = 0; level < 3 && tasks.size() < target; level++) {
    std::vector<HuangSeed> next;
    for (const HuangSeed & seed : tasks) {
      st = base;
      const Replay r = replay(seed, st);
      if (r == REPLAY_INVALID)
        bt_te("CellsCover::generateTasks: seed does not replay");
      if (r == REPLAY_DEAD)
        continue;
      bool dead = false;
      const uint32_t j = choose(st, dead);
      if (dead)
        continue;
      if (j == NONE) {
        next.push_back(seed); // a leaf: the task reports it
        continue;
      }
      HuangSeed without = seed;
      unsigned int singles = 0;
      const uint32_t rows = st.size[j];
      for (uint32_t i = 0; i < rows && singles < fanout; i++) {
        const uint32_t o = cellOpt[st.set[itemBase[j] + i]];
        if (fitsRange(st, o)) {
          HuangSeed with = without;
          with.prefix.push_back(optNode[o]);
          with.pivotShape.push_back(STEP_SELECT);
          next.push_back(std::move(with));
        }
        without.prefix.push_back(optNode[o]);
        without.pivotShape.push_back(STEP_DISCARD);
        singles++;
      }
      // what is left without those rows, if the column can do without them
      if (singles < rows || st.cnt[j] >= itemMin[j])
        next.push_back(std::move(without));
    }
    if (next.size() <= tasks.size() && level > 0) {
      tasks = std::move(next);
      break;
    }
    tasks = std::move(next);
  }
  return tasks;
}

/* Search everything the stack in st stands for. true: done, every solution
 * delivered. false: stopped; the stack then describes exactly what is left
 * (see freeze()), a solution the callback did not take included.
 */
template <class Counter>
bool CellsCover::run(State & st, ConsumeCallback & callback, const std::stop_token & stop,
                     Counter & iterations, Donor * donor) const {
  uint64_t local = 0, flushed = 0;
  auto flush = [&] {
    iterations.fetch_add(local - flushed, std::memory_order_relaxed);
    flushed = local;
  };
  std::vector<unsigned int> nodes;
  bool halt = false;
  const bool holeLimit = holes < holeCount;

  while (!st.frames.empty()) {
    const size_t top = st.frames.size() - 1;
    const uint8_t state = st.frames[top].state;

    if (state == 2) {
      // its row stays out until a frame below restores its lists
      st.frames.pop_back();
      continue;
    }
    if (state == 1) {
      restore(st, st.frames[top].snap);
      unselect(st, st.frames[top].x);
      discard(st, top);
      continue;
    }

    if (halt || stop.stop_requested()) {
      flush();
      return false;
    }
    local++;
    // at most one shared add per 256 nodes (see SimdHuangCover::search)
    if (local - flushed >= 256)
      flush();
    if (donor != nullptr && local >= donor->nextPoll) {
      donor->nextPoll = local + donor->interval;
      if (donor->forced || donor->pool->wants_work())
        donateFrame(st, *donor);
    }

    bool dead = false;
    uint32_t best = choose(st, dead);
    if (dead) {
      st.frames.pop_back();
      continue;
    }

    if (best == NONE) {
      bool ok = rangeItem == NONE || st.cnt[rangeItem] >= items[rangeItem].min;
      if (ok && holeLimit) {
        // unfilled variable voxels: open columns by now, with no rows left
        unsigned int empty = 0;
        for (uint32_t k = 0; k < st.nAct; k++)
          if (itemHole[st.act[k]] && st.cnt[st.act[k]] == 0 && ++empty > holes) {
            ok = false;
            break;
          }
      }
      if (ok) {
        nodes.clear();
        for (uint32_t o : st.sol)
          nodes.push_back(optNode[o]);
        flush();
        bool consumed = false;
        const bool cont = callback(nodes, consumed);
        // not taken: the leaf stays on the stack and is found again
        if (!consumed)
          return false;
        if (!cont)
          halt = true;
      }
      st.frames.pop_back();
      continue;
    }

    Frame & f = st.frames[top];
    f.item = best;
    f.x = cellOpt[st.set[itemBase[best]]];
    f.closed = false;
    const uint32_t x = f.x;
    if (fitsRange(st, x)) {
      const uint32_t snap = snapshot(st);
      st.frames[top].snap = snap;
      if (select(st, x)) {
        st.frames[top].state = 1;
        st.frames.emplace_back();
        continue;
      }
      restore(st, snap);
      unselect(st, x);
    }
    discard(st, top);
  }
  flush();
  return true;
}

void CellsCover::solve(SolutionCallback callback, std::stop_token stop,
                       std::atomic<uint64_t> & iterations) const {
  solveSubtree({}, {}, std::move(callback), stop, iterations);
}

void CellsCover::solveSubtree(const std::vector<unsigned int> & prefix_node_ids,
                              const std::vector<unsigned int> & hidden_node_ids,
                              SolutionCallback callback, std::stop_token stop,
                              std::atomic<uint64_t> & iterations) const {
  build();
  HuangSeed seed;
  for (unsigned int n : hidden_node_ids) {
    seed.prefix.push_back(n);
    seed.pivotShape.push_back(STEP_DISCARD);
  }
  for (unsigned int n : prefix_node_ids) {
    seed.prefix.push_back(n);
    seed.pivotShape.push_back(STEP_SELECT);
  }
  State st = base;
  const Replay r = replay(seed, st);
  if (r == REPLAY_INVALID)
    bt_te("CellsCover::solveSubtree: the rows given do not fit together");
  if (r == REPLAY_DEAD)
    return;
  ConsumeCallback cb = [&](const std::vector<unsigned int> & s, bool & consumed) {
    consumed = true;
    return callback(s);
  };
  run(st, cb, stop, iterations, nullptr);
}

bool CellsCover::seedReplays(const HuangSeed & seed) const {
  build();
  State st = base;
  return replay(seed, st) != REPLAY_INVALID;
}

void CellsCover::parallelSolve(unsigned int num_workers, ConsumeCallback callback,
                               std::stop_source & runStop,
                               std::atomic<unsigned long> & iterations,
                               std::atomic<size_t> & total_tasks,
                               std::atomic<size_t> & completed_tasks, ThreadBudget * budget,
                               const std::vector<HuangSeed> * resume_seeds,
                               std::vector<HuangSeed> * salvaged_seeds,
                               std::atomic<unsigned long> * donations) const {
  build();
  std::stop_token stop = runStop.get_token();

  /* The pieces a stopped run left, or the search divided for the workers
   * (see generateTasks). A worker that runs out later asks the running
   * ones for part of theirs (see donateFrame).
   */
  std::vector<HuangSeed> tasks;
  const bool resumed = resume_seeds != nullptr && !resume_seeds->empty();
  if (resumed) {
    tasks = *resume_seeds;
  } else {
    tasks = generateTasks(num_workers > 1 ? std::max<size_t>(16, num_workers * 4) : 1);
    // completed first, then the total with release: getFinished() reads them acquire, in reverse
    completed_tasks.store(0, std::memory_order_relaxed);
    total_tasks.store(tasks.size() * TaskUnits::kWhole, std::memory_order_release);
  }

  if (tasks.empty())
    return; // no piece can hold a solution

  if (stop.stop_requested()) {
    if (salvaged_seeds != nullptr)
      for (auto & t : tasks)
        salvaged_seeds->push_back(std::move(t));
    return;
  }

  // the pool, its budget, the stop wake-up and the exception hand-over are
  // those of SimdHuangCover::parallelSolve
  AssemblyTaskPool<HuangSeed> pool;
  pool.seed(std::move(tasks));
  pool.setBudget(budget);

  std::stop_callback wakeParkedOnStop(stop, [&] {
    pool.notify();
    if (budget != nullptr)
      budget->notify();
  });

  std::exception_ptr worker_exception = nullptr;
  std::mutex exception_mutex;
  std::mutex salvage_mutex;

  auto worker_fn = [&](std::stop_token st = {}) {
    try {
      State state;
      HuangSeed t;
      while (pool.pop_task(t, stop, st)) {
        try {
          state = base;
          const Replay r = replay(t, state);
          if (r == REPLAY_INVALID)
            bt_te("CellsCover::parallelSolve: seed does not replay");
          size_t units = t.units;
          Donor donor{&pool, &units, donateEvery > 0 ? donateEvery : 256, donateEvery > 0,
                      donations, 0, &total_tasks};
          donor.nextPoll = donor.interval;
          const bool done = r == REPLAY_DEAD ||
                            run(state, callback, stop, iterations, donate ? &donor : nullptr);
          if (done) {
            completed_tasks.fetch_add(units, std::memory_order_relaxed);
          } else if (salvaged_seeds != nullptr) {
            HuangSeed rest = freeze(state);
            rest.units = units;
            std::lock_guard<std::mutex> lock(salvage_mutex);
            salvaged_seeds->push_back(std::move(rest));
          }
        } catch (...) {
          pool.finishTask();
          throw;
        }
        pool.finishTask();
      }
    } catch (...) {
      std::lock_guard<std::mutex> lock(exception_mutex);
      if (!worker_exception)
        worker_exception = std::current_exception();
      runStop.request_stop();
    }
  };

  std::vector<std::jthread> threads;
  threads.reserve(num_workers > 0 ? num_workers - 1 : 0);
  for (unsigned int i = 1; i < num_workers; i++)
    threads.emplace_back(worker_fn);

  worker_fn();

  for (auto & th : threads)
    if (th.joinable())
      th.join();

  if (worker_exception)
    std::rethrow_exception(worker_exception);

  if (salvaged_seeds != nullptr)
    for (auto & t : pool.drain())
      salvaged_seeds->push_back(std::move(t));
}
