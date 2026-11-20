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
#ifndef __CELLS_COVER_H__
#define __CELLS_COVER_H__

#include "simd_huang_cover.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

/**
 * Exact cover with multiplicities on sparse sets ("dancing cells", Knuth,
 * TAOCP 4B, 7.2.2.3), for assembler 1: piece shapes used min..max times,
 * result voxels covered exactly once, variable voxels at most once.
 *
 * Every column is an item with a contiguous list of the rows (options)
 * still available for it. Taking a row out swaps it behind the end of each
 * of its items' lists and shortens them, so backtracking only restores the
 * lengths and no links are written.
 *
 * The search branches on one row at a time: with it, then without it. The
 * solutions under a node are therefore exactly the row sets that contain
 * every row selected on the way down and none of the rows discarded, which
 * makes a list of such decisions a complete description of a piece of the
 * search, whatever order the engine then explores it in. Donations to
 * waiting workers, the remainder of a stopped run and the saved state are
 * all such lists (see the step kinds below).
 *
 * It takes the matrix through the interface of the bit-parallel engine and
 * runs in the same task pool, so assembler_1_c drives either the same way.
 * Unlike that engine it has no limit on columns and its memory is linear in
 * the matrix.
 *
 * Dancing Cells came to BurrTools through Andreas Röver's engine in
 * BurrTools NG (https://gitlab.com/roever/burrtools, src/lib/dancingcell.cpp):
 * it showed that the algorithm suits puzzle assembly, and the symmetry
 * breaker with piece ranges that made it fast here is his as well. It is
 * the reference this engine was checked against, count for count. This
 * engine is a separate implementation, written for assembler 1's matrix,
 * task pool and saved state.
 */
class CellsCover final : public ISimdHuangCover {
public:
  /* The kinds of decision in a HuangSeed of this engine, in pivotShape
   * (prefix holds the rows): the row was discarded, selected, or selected
   * with the other branch, the search without it, still part of the seed.
   * A seed that ends in open selections is a search stopped in the middle;
   * restSkip is not used.
   */
  enum : uint8_t { STEP_DISCARD = 0, STEP_SELECT = 1, STEP_SELECT_OPEN = 2 };

  CellsCover(unsigned int num_columns, unsigned int num_shapes, const solverSettings_t & settings);

  void setColumnBounds(unsigned int col, unsigned int min_w, unsigned int max_w,
                       bool is_voxel, bool is_shape, bool is_range, bool is_hole) override;
  void setHoles(unsigned int h) override { holes = h; }
  uint32_t addRow(unsigned int node_id, unsigned int shape_id, unsigned int shape_col,
                  unsigned int shape_row_idx, unsigned int range_weight,
                  const std::vector<unsigned int> & cols,
                  const std::vector<unsigned int> & weights) override;
  void registerNodeAlias(unsigned int node_id, uint32_t row_idx) override;

  void solve(SolutionCallback callback, std::stop_token stop,
             std::atomic<uint64_t> & iterations) const override;
  void solveSubtree(const std::vector<unsigned int> & prefix_node_ids,
                    const std::vector<unsigned int> & hidden_node_ids,
                    SolutionCallback callback, std::stop_token stop,
                    std::atomic<uint64_t> & iterations) const override;

  using ISimdHuangCover::parallelSolve;
  void parallelSolve(unsigned int num_workers, ConsumeCallback callback,
                     std::stop_source & runStop, std::atomic<unsigned long> & iterations,
                     std::atomic<size_t> & total_tasks, std::atomic<size_t> & completed_tasks,
                     ThreadBudget * budget = nullptr,
                     const std::vector<HuangSeed> * resume_seeds = nullptr,
                     std::vector<HuangSeed> * salvaged_seeds = nullptr,
                     std::atomic<unsigned long> * donations = nullptr) const override;

  bool seedReplays(const HuangSeed & seed) const override;
  unsigned int getNumRows() const override { return static_cast<unsigned int>(optNode.size()); }
  unsigned int getNumColumns() const override { return num_columns; }
  unsigned int getNumShapes() const override { return num_shapes; }
  const char * activeKernel() const override { return "cells"; }

private:
  static constexpr uint32_t NONE = UINT32_MAX;

  struct Item {
    uint32_t min = 1;
    uint32_t max = 1;
    bool declared = false;
    bool hole = false;
  };

  /* one decision of the search stack; state 0: not looked at yet, 1: row x
   * is selected, 2: row x is discarded. closed: the search without x is not
   * this stack's to do (donated, or never part of its seed).
   */
  struct Frame {
    uint32_t x = NONE;
    uint32_t item = NONE;
    uint32_t snap = 0;
    uint8_t state = 0;
    bool closed = false;
  };

  /* everything a search changes; one per worker, copied from the base */
  struct State {
    std::vector<uint32_t> set;   // cells, grouped by item
    std::vector<uint32_t> pos;   // per cell: where it is in set
    std::vector<uint32_t> size;  // per item: rows still available
    std::vector<uint32_t> cnt;   // per item: weight of the selected rows
    std::vector<int32_t> need;   // per item: rows it still wants, its minimum less cnt
    std::vector<uint32_t> act;    // the columns, those below their maximum first
    std::vector<uint32_t> actPos; // per item: where it is in act
    uint32_t nAct = 0;            // how many of act are below their maximum
    std::vector<uint32_t> snap;  // stack of saved list lengths, see snapshot()
    std::vector<uint32_t> sol;   // selected rows
    std::vector<Frame> frames;
  };

  struct Donor;

  unsigned int num_columns;
  unsigned int num_shapes;
  unsigned int holes = 0;
  uint32_t rangeItem = NONE;
  bool donate = true;
  unsigned long donateEvery = 0;
  unsigned int fanout = 16;

  std::vector<Item> items;           // by column
  std::vector<uint32_t> optStart;    // by row: first cell; one past the last row at the end
  std::vector<uint32_t> cellItem;    // by cell
  std::vector<uint32_t> cellOpt;     // by cell
  std::vector<uint32_t> optRange;    // by row: weight in the range column
  std::vector<unsigned int> optNode; // by row: the matrix node it is reported as
  std::unordered_map<unsigned int, uint32_t> nodeToOpt;

  /* derived from the rows on the first search (the interface has no "matrix
   * complete" call); read-only afterwards */
  mutable std::once_flag builtOnce;
  mutable std::vector<uint32_t> itemBase;  // by column: start of its part of State::set
  mutable std::vector<uint32_t> itemMin, itemMax;
  mutable std::vector<uint8_t> itemHole;   // by column: a variable voxel
  mutable unsigned int holeCount = 0;
  mutable State base;                      // nothing selected
  void build(void) const;

  bool present(const State & st, uint32_t o) const {
    const uint32_t c = optStart[o];
    return st.pos[c] < itemBase[cellItem[c]] + st.size[cellItem[c]];
  }
  bool fitsRange(const State & st, uint32_t o) const {
    return rangeItem == NONE || st.cnt[rangeItem] + optRange[o] <= items[rangeItem].max;
  }
  bool hide(State & st, uint32_t o) const;
  bool select(State & st, uint32_t o) const;
  void unselect(State & st, uint32_t o) const;
  /* Backtracking: a row taken out of a list sits right behind the list's
   * end, so the lists as they were come back by restoring their lengths
   * alone (in another order, which nothing depends on). The same holds for
   * the list of open columns. Only open columns can change, so those are
   * what a snapshot holds: their number, then column and length of each.
   * Snapshots stack with the frames; restoring one drops it and those above.
   */
  static uint32_t snapshot(State & st) {
    const uint32_t at = static_cast<uint32_t>(st.snap.size());
    st.snap.resize(at + 1 + 2 * st.nAct);
    uint32_t * out = st.snap.data() + at;
    *out++ = st.nAct;
    for (uint32_t k = 0; k < st.nAct; k++) {
      *out++ = st.act[k];
      *out++ = st.size[st.act[k]];
    }
    return at;
  }
  static void restore(State & st, uint32_t at) {
    const uint32_t * in = st.snap.data() + at;
    st.nAct = *in++;
    for (uint32_t k = 0; k < st.nAct; k++, in += 2)
      st.size[in[0]] = in[1];
    st.snap.resize(at);
  }
  void discard(State & st, size_t top) const;

  enum Replay { REPLAY_OK, REPLAY_DEAD, REPLAY_INVALID };
  Replay replay(const HuangSeed & seed, State & st) const;
  HuangSeed freeze(const State & st) const;
  void donateFrame(State & st, Donor & donor) const;
  uint32_t choose(const State & st, bool & dead) const;
  std::vector<HuangSeed> generateTasks(size_t target) const;

  template <class Counter>
  bool run(State & st, ConsumeCallback & callback, const std::stop_token & stop,
           Counter & iterations, Donor * donor) const;
};

#endif // __CELLS_COVER_H__
