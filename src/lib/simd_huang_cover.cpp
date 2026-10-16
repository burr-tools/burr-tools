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
#include "simd_huang_cover.h"
#include "assembler_pool.h"
#include "simd_config.h"
#include "bt_assert.h"

#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <thread>
#include <mutex>
#include <stop_token>

template <typename BitsetType>
SimdHuangCover<BitsetType>::SimdHuangCover(unsigned int num_cols, unsigned int num_s)
  : num_columns(num_cols), num_shapes(num_s) {
  bt_assert(num_cols <= BitsetType::NUM_WORDS * 64);
  columns.resize(num_columns + 1);

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  use_avx2 = __builtin_cpu_supports("avx2") && SimdConfig::isVectorAccelerationEnabled();
  use_avx512 = __builtin_cpu_supports("avx512f") && SimdConfig::isAvx512Allowed();
#endif

  use_neon = SimdConfig::isNeonAllowed();
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::setColumnBounds(
  unsigned int col,
  unsigned int min_w,
  unsigned int max_w,
  bool is_voxel,
  bool is_shape,
  bool is_range,
  bool is_hole
) {
  bt_assert(col <= num_columns);
  bt_assert(col <= BitsetType::NUM_WORDS * 64);

  /* search() checks voxel columns with placed_voxels.containsAll(required_voxels),
   * which is a presence test: it cannot distinguish weight 1 from weight n. A
   * voxel column demanding more than one unit would be reported satisfied at
   * one, so the contract for this class is min_weight <= 1 on voxel columns.
   * assembler_1_c always sets 1 for real voxels and 0 for hole columns.
   */
  bt_assert(!is_voxel || min_w <= 1);

  if (col >= columns.size()) {
    columns.resize(col + 1);
  }

  /* Contribution this column already made to total_min_pieces, so that calling
   * setColumnBounds() twice for the same shape column replaces it rather than
   * double-counting. A double count would push the goal-check gate in search()
   * out of reach and silently suppress every solution.
   */
  const unsigned int prev_min_pieces = columns[col].is_shape ? columns[col].min_weight : 0u;

  columns[col].min_weight = min_w;
  columns[col].max_weight = max_w;
  columns[col].is_voxel = is_voxel;
  columns[col].is_shape = is_shape;
  columns[col].is_range = is_range;
  columns[col].is_hole = is_hole;

  if (is_shape) {
    total_min_pieces += min_w;
  }
  total_min_pieces -= prev_min_pieces;

  if (is_range) {
    has_range = true;
    range_column = col;
  }
  if (is_hole && std::find(hole_columns.begin(), hole_columns.end(), col) == hole_columns.end()) {
    /* guarded for the same reason as total_min_pieces above: a repeated call
     * must not list the same hole column twice and inflate the empty-hole
     * count that search() prunes on
     */
    hole_columns.push_back(col);
  }
  if (is_voxel && min_w > 0 && col > 0 && (col - 1) < BitsetType::NUM_WORDS * 64) {
    required_voxels.set(col - 1);
  }
  active_column_list.push_back(col);
}

template <typename BitsetType>
uint32_t SimdHuangCover<BitsetType>::addRow(
  unsigned int node_id,
  unsigned int shape_id,
  unsigned int shape_col,
  unsigned int shape_row_idx,
  unsigned int range_weight,
  const std::vector<unsigned int> &cols,
  const std::vector<unsigned int> &weights
) {
  uint32_t idx = static_cast<uint32_t>(rows.size());
  Row r;
  r.node_id = node_id;
  r.shape_id = shape_id;
  r.shape_col = shape_col;
  r.shape_row_idx = shape_row_idx;
  r.range_weight = range_weight;
  r.columns = cols;
  r.weights = weights;

  for (size_t i = 0; i < cols.size(); i++) {
    unsigned int c = cols[i];
    bt_assert(c <= num_columns);
    bt_assert(c <= BitsetType::NUM_WORDS * 64);
    if (c > 0 && c <= num_columns && (c - 1) < BitsetType::NUM_WORDS * 64 && columns[c].is_voxel) {
      r.voxel_mask.set(c - 1);
    }
  }

  node_to_row_idx[node_id] = idx;
  rows.push_back(std::move(r));
  return idx;
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::registerNodeAlias(unsigned int node_id, uint32_t row_idx) {
  node_to_row_idx[node_id] = row_idx;
}

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
template <typename BitsetType>
__attribute__((target("avx512f")))
void SimdHuangCover<BitsetType>::filterRowsAvx512(
  const std::vector<uint32_t> &src,
  uint32_t chosen_idx,
  const BitsetType &chosen_voxel_mask,
  unsigned int chosen_shape,
  bool shape_is_full,
  bool filter_monotonic,
  unsigned int chosen_shape_row_idx,
  bool check_range,
  unsigned int max_allowed_range_weight,
  std::vector<uint32_t> &dst
) const {
  if constexpr (BitsetType::NUM_WORDS >= 8) {
    constexpr size_t N_VEC = BitsetType::NUM_WORDS / 8;
    __m512i va[N_VEC];
    for (size_t i = 0; i < N_VEC; ++i) {
      va[i] = _mm512_load_si512(reinterpret_cast<const void*>(&chosen_voxel_mask.words[i * 8]));
    }

    for (uint32_t idx : src) {
      if (idx == chosen_idx)
        continue;
      const auto &cand = rows[idx];
      if (cand.shape_id == chosen_shape) {
        if (shape_is_full || (filter_monotonic && cand.shape_row_idx <= chosen_shape_row_idx))
          continue;
      }
      if (check_range && cand.range_weight > max_allowed_range_weight)
        continue;

      const uint64_t *rw = cand.voxel_mask.words;
      bool disjoint = true;
      for (size_t i = 0; i < N_VEC; ++i) {
        __m512i vb = _mm512_load_si512(reinterpret_cast<const void*>(&rw[i * 8]));
        if (_mm512_test_epi64_mask(va[i], vb) != 0) {
          disjoint = false;
          break;
        }
      }
      if (disjoint) {
        dst.push_back(idx);
      }
    }
  } else {
    filterRowsAvx2(src, chosen_idx, chosen_voxel_mask, chosen_shape, shape_is_full,
                   filter_monotonic, chosen_shape_row_idx, check_range, max_allowed_range_weight, dst);
  }
}
template <typename BitsetType>
__attribute__((target("avx2")))
void SimdHuangCover<BitsetType>::filterRowsAvx2(
  const std::vector<uint32_t> &src,
  uint32_t chosen_idx,
  const BitsetType &chosen_voxel_mask,
  unsigned int chosen_shape,
  bool shape_is_full,
  bool filter_monotonic,
  unsigned int chosen_shape_row_idx,
  bool check_range,
  unsigned int max_allowed_range_weight,
  std::vector<uint32_t> &dst
) const {
  constexpr size_t N_VEC = BitsetType::NUM_WORDS / 4;
  __m256i va[N_VEC];
  for (size_t i = 0; i < N_VEC; ++i) {
    va[i] = _mm256_load_si256(reinterpret_cast<const __m256i*>(&chosen_voxel_mask.words[i * 4]));
  }

  for (uint32_t idx : src) {
    if (idx == chosen_idx)
      continue;
    const auto &cand = rows[idx];
    if (cand.shape_id == chosen_shape) {
      if (shape_is_full || (filter_monotonic && cand.shape_row_idx <= chosen_shape_row_idx))
        continue;
    }
    if (check_range && cand.range_weight > max_allowed_range_weight)
      continue;

    const uint64_t *rw = cand.voxel_mask.words;
    bool disjoint = true;
    for (size_t i = 0; i < N_VEC; ++i) {
      __m256i vb = _mm256_load_si256(reinterpret_cast<const __m256i*>(&rw[i * 4]));
      if (!_mm256_testz_si256(va[i], vb)) {
        disjoint = false;
        break;
      }
    }
    if (disjoint) {
      dst.push_back(idx);
    }
  }
}
#elif defined(__aarch64__) || defined(__ARM_NEON)
template <typename BitsetType>
void SimdHuangCover<BitsetType>::filterRowsNeon(
  const std::vector<uint32_t> &src,
  uint32_t chosen_idx,
  const BitsetType &chosen_voxel_mask,
  unsigned int chosen_shape,
  bool shape_is_full,
  bool filter_monotonic,
  unsigned int chosen_shape_row_idx,
  bool check_range,
  unsigned int max_allowed_range_weight,
  std::vector<uint32_t> &dst
) const {
  constexpr size_t N_VEC = BitsetType::NUM_WORDS / 2;
  uint64x2_t va[N_VEC];
  for (size_t i = 0; i < N_VEC; ++i) {
    va[i] = vld1q_u64(&chosen_voxel_mask.words[i * 2]);
  }

  for (uint32_t idx : src) {
    if (idx == chosen_idx)
      continue;
    const auto &cand = rows[idx];
    if (cand.shape_id == chosen_shape) {
      if (shape_is_full || (filter_monotonic && cand.shape_row_idx <= chosen_shape_row_idx))
        continue;
    }
    if (check_range && cand.range_weight > max_allowed_range_weight)
      continue;

    const uint64_t *rw = cand.voxel_mask.words;
    bool disjoint = true;
    for (size_t i = 0; i < N_VEC; ++i) {
      uint64x2_t vb = vld1q_u64(&rw[i * 2]);
      uint64x2_t c = vandq_u64(va[i], vb);
      if ((vgetq_lane_u64(c, 0) | vgetq_lane_u64(c, 1)) != 0) {
        disjoint = false;
        break;
      }
    }
    if (disjoint) {
      dst.push_back(idx);
    }
  }
}
#endif

template <typename BitsetType>
void SimdHuangCover<BitsetType>::filterRows(
  const std::vector<uint32_t> &src,
  uint32_t chosen_idx,
  const BitsetType &chosen_voxel_mask,
  unsigned int chosen_shape,
  bool shape_is_full,
  bool filter_monotonic,
  unsigned int chosen_shape_row_idx,
  bool check_range,
  unsigned int max_allowed_range_weight,
  std::vector<uint32_t> &dst
) const {
  if (dst.capacity() < src.size()) {
    dst.reserve(src.size());
  }

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (use_avx512 && BitsetType::NUM_WORDS >= 8) {
    filterRowsAvx512(src, chosen_idx, chosen_voxel_mask, chosen_shape, shape_is_full,
                     filter_monotonic, chosen_shape_row_idx, check_range, max_allowed_range_weight, dst);
    return;
  }
  if (use_avx2) {
    filterRowsAvx2(src, chosen_idx, chosen_voxel_mask, chosen_shape, shape_is_full,
                   filter_monotonic, chosen_shape_row_idx, check_range, max_allowed_range_weight, dst);
    return;
  }
#elif defined(__aarch64__) || defined(__ARM_NEON)
  if (use_neon) {
    filterRowsNeon(src, chosen_idx, chosen_voxel_mask, chosen_shape, shape_is_full,
                   filter_monotonic, chosen_shape_row_idx, check_range, max_allowed_range_weight, dst);
    return;
  }
#endif

  for (uint32_t idx : src) {
    if (idx == chosen_idx)
      continue;
    const auto &cand = rows[idx];
    if (cand.shape_id == chosen_shape) {
      if (shape_is_full || (filter_monotonic && cand.shape_row_idx <= chosen_shape_row_idx))
        continue;
    }
    if (check_range && cand.range_weight > max_allowed_range_weight)
      continue;

    if (is_disjoint_scalar(chosen_voxel_mask, cand.voxel_mask)) {
      dst.push_back(idx);
    }
  }
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::solve(
  SolutionCallback callback,
  std::stop_token stop,
  std::atomic<uint64_t> &iterations
) const {
  if (rows.empty() || active_column_list.empty())
    return;

  SearchContext ctx;
  initContext(ctx);

  ctx.scratch_active_rows[0].resize(rows.size());
  for (size_t i = 0; i < rows.size(); i++) {
    ctx.scratch_active_rows[0][i] = static_cast<uint32_t>(i);
  }

  search(0, ctx, callback, stop, iterations);

  flushIterations(ctx, iterations);
}

template <typename BitsetType>
bool SimdHuangCover<BitsetType>::taskFromSeed(
  const HuangSeed &seed,
  SubtreeTask &task
) const {
  if (rows.empty() || active_column_list.empty())
    return false;

  SearchContext ctx;
  initContext(ctx);

  ctx.scratch_active_rows[0].reserve(rows.size());
  for (size_t i = 0; i < rows.size(); i++) {
    ctx.scratch_active_rows[0].push_back(static_cast<uint32_t>(i));
  }

  if (!replayPrefix(seed.prefix, seed.pivotShape, ctx))
    return false;

  task.depth = static_cast<unsigned int>(seed.prefix.size());
  task.ctx = std::move(ctx);
  task.pivot_shape = seed.pivotShape;
  task.pivot_shape.resize(seed.prefix.size(), 0);
  task.rest_skip = seed.restSkip;
  return true;
}

/**
 * Shared prefix-application used by solveSubtree() (with caller-provided
 * hidden filtering already applied to scratch_active_rows[0]) and by
 * resume-seed construction (full initial set): maps each placed node id to
 * its row, checks voxel/shape/range conflicts, places it and filters the
 * next level. Returns false when the prefix no longer resolves.
 */
template <typename BitsetType>
bool SimdHuangCover<BitsetType>::replayPrefix(
  const std::vector<unsigned int> &prefix_node_ids,
  const std::vector<uint8_t> &pivot_shape,
  SearchContext &ctx
) const {
  for (unsigned int d = 0; d < prefix_node_ids.size(); d++) {
    auto it = node_to_row_idx.find(prefix_node_ids[d]);
    if (it == node_to_row_idx.end()) {
      return false;
    }
    uint32_t r_idx = it->second;
    const auto &cand = rows[r_idx];

    // Check voxel conflict
    if (!is_disjoint_scalar(ctx.placed_voxels, cand.voxel_mask)) {
      return false;
    }
    // Check shape bound
    if (ctx.col_weights[cand.shape_col] + 1 > columns[cand.shape_col].max_weight) {
      return false;
    }
    // Check range bound
    if (has_range && ctx.col_weights[range_column] + cand.range_weight > columns[range_column].max_weight) {
      return false;
    }

    // Place candidate
    ctx.placed_voxels = ctx.placed_voxels | cand.voxel_mask;
    for (size_t i = 0; i < cand.columns.size(); i++) {
      ctx.col_weights[cand.columns[i]] += cand.weights[i];
    }
    ctx.current_solution.push_back(cand.node_id);

    bool shape_full = (ctx.col_weights[cand.shape_col] >= columns[cand.shape_col].max_weight);
    // Same rule as search(): the monotonic filter applies below a step
    // chosen on a shape pivot while the shape still needs copies.
    bool pivot_was_shape = d < pivot_shape.size() && pivot_shape[d] != 0;
    bool filter_monotonic = (!shape_full && pivot_was_shape);
    unsigned int max_allowed_range = has_range ? (columns[range_column].max_weight - ctx.col_weights[range_column]) : 0;

    auto &next_active = ctx.scratch_active_rows[d + 1];
    next_active.clear();
    filterRows(ctx.scratch_active_rows[d], r_idx, cand.voxel_mask, cand.shape_id, shape_full,
               filter_monotonic, cand.shape_row_idx, has_range, max_allowed_range, next_active);
  }
  return true;
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::solveSubtree(
  const std::vector<unsigned int> &prefix_node_ids,
  const std::vector<unsigned int> &hidden_node_ids,
  SolutionCallback callback,
  std::stop_token stop,
  std::atomic<uint64_t> &iterations
) const {
  if (rows.empty() || active_column_list.empty())
    return;

  SearchContext ctx;
  initContext(ctx);

  std::vector<bool> is_hidden(rows.size(), false);
  for (unsigned int h : hidden_node_ids) {
    if (h == 0) continue;
    auto it = node_to_row_idx.find(h);
    if (it != node_to_row_idx.end()) {
      is_hidden[it->second] = true;
    }
  }

  ctx.scratch_active_rows[0].reserve(rows.size());
  for (size_t i = 0; i < rows.size(); i++) {
    if (!is_hidden[i]) {
      ctx.scratch_active_rows[0].push_back(static_cast<uint32_t>(i));
    }
  }

  bool conflict = !replayPrefix(prefix_node_ids, {}, ctx);

  if (!conflict) {
    search(prefix_node_ids.size(), ctx, callback, stop, iterations);
  }

  flushIterations(ctx, iterations);
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::generateTasks(
  unsigned int target_tasks,
  std::vector<SubtreeTask> &tasks
) const {
  tasks.clear();
  if (rows.empty() || active_column_list.empty())
    return;

  SearchContext root_ctx;
  initContext(root_ctx);

  root_ctx.scratch_active_rows[0].resize(rows.size());
  for (size_t i = 0; i < rows.size(); i++) {
    root_ctx.scratch_active_rows[0][i] = static_cast<uint32_t>(i);
  }

  /* A node that expand() cannot refine any further still has to be handed to a
   * worker: it may itself be a complete cover. Dropping it here loses that
   * solution, because in this round the node is replaced by its set of
   * children rather than being a task in its own right.
   */
  // Pivot kind per placed step along the current expansion path; tasks
  // carry it so their seeds replay the same monotonic filtering.
  std::vector<uint8_t> pivot_path;
  auto emitAsTask = [&](unsigned int depth, SearchContext &ctx) {
    SubtreeTask t;
    t.depth = depth;
    t.ctx = ctx;
    t.pivot_shape = pivot_path;
    tasks.push_back(std::move(t));
  };

  std::function<void(unsigned int, SearchContext&, unsigned int)> expand;
  expand = [&](unsigned int depth, SearchContext &ctx, unsigned int max_depth) {
    if (depth == max_depth) {
      emitAsTask(depth, ctx);
      return;
    }

    const auto &curr_active = ctx.scratch_active_rows[depth];
    if (curr_active.empty()) {
      emitAsTask(depth, ctx);
      return;
    }

    std::fill(ctx.col_counts.begin(), ctx.col_counts.end(), 0);
    for (uint32_t r_idx : curr_active) {
      const auto &r = rows[r_idx];
      for (size_t i = 0; i < r.columns.size(); i++) {
        ctx.col_counts[r.columns[i]] += r.weights[i];
      }
    }

    for (unsigned int c : active_column_list) {
      if (columns[c].min_weight > ctx.col_weights[c]) {
        if (ctx.col_weights[c] + ctx.col_counts[c] < columns[c].min_weight)
          return;
      }
      if (columns[c].is_voxel && columns[c].min_weight > 0 && !ctx.placed_voxels.test(c - 1)) {
        if (ctx.col_counts[c] == 0)
          return;
      }
    }

    unsigned int min_metric = UINT32_MAX;
    unsigned int best_col = UINT32_MAX;

    for (unsigned int c : active_column_list) {
      if (columns[c].is_hole)
        continue;
      if (ctx.col_weights[c] >= columns[c].min_weight)
        continue;
      if (columns[c].is_voxel && ctx.placed_voxels.test(c - 1))
        continue;
      // Never pivot on the range column without variable voxels (same
      // argument as in search(): exact voxel cover forces its weight, so it
      // only validates; pivoting would emit duplicate task partitions).
      // Its dead-end contribution above is unaffected.
      if (columns[c].is_range && hole_columns.empty())
        continue;

      unsigned int remaining_need = columns[c].min_weight - ctx.col_weights[c];
      unsigned int count = ctx.col_counts[c];

      if (count == 0)
        return;

      unsigned int metric = count * remaining_need;
      if (metric < min_metric) {
        min_metric = metric;
        best_col = c;
        if (metric <= 1)
          break;
      }
    }

    if (best_col == UINT32_MAX) {
      /* no pivot left to branch on: this node is already as deep as it goes,
       * and may be a complete cover
       */
      emitAsTask(depth, ctx);
      return;
    }

    /* DELIBERATELY no resize of ctx.scratch_active_rows here.
     *
     * curr_active is a reference into that vector, and a resize would
     * reallocate the outer buffer and leave it dangling before the loop below
     * walks it. Depth stays in bounds regardless: every placed row consumes
     * at least one shape-column unit, so depth never exceeds searchDepthBound
     * (sum of shape max_weights), and every placed row also sets a unique
     * voxel bit, so depth never exceeds num_columns either; the vector is
     * created with min(bound, num_columns) + 16 entries (see initContext).
     */
    bt_assert(depth + 1 < ctx.scratch_active_rows.size());

    for (uint32_t r_idx : curr_active) {
      const auto &cand = rows[r_idx];

      bool covers_best = columns[best_col].is_shape ? (cand.shape_col == best_col)
                       : columns[best_col].is_voxel ? cand.voxel_mask.test(best_col - 1)
                       : (cand.range_weight > 0);
      if (!covers_best)
        continue;

      if (ctx.col_weights[cand.shape_col] + 1 > columns[cand.shape_col].max_weight)
        continue;
      if (has_range && ctx.col_weights[range_column] + cand.range_weight > columns[range_column].max_weight)
        continue;

      ctx.placed_voxels = ctx.placed_voxels | cand.voxel_mask;
      for (size_t i = 0; i < cand.columns.size(); i++) {
        ctx.col_weights[cand.columns[i]] += cand.weights[i];
      }
      ctx.current_solution.push_back(cand.node_id);

      bool shape_full = (ctx.col_weights[cand.shape_col] >= columns[cand.shape_col].max_weight);
      bool filter_monotonic = (!shape_full && columns[best_col].is_shape);
      unsigned int max_allowed_range = has_range ? (columns[range_column].max_weight - ctx.col_weights[range_column]) : 0;

      auto &next_active = ctx.scratch_active_rows[depth + 1];
      next_active.clear();

      filterRows(curr_active, r_idx, cand.voxel_mask, cand.shape_id, shape_full,
                 filter_monotonic, cand.shape_row_idx, has_range, max_allowed_range, next_active);

      pivot_path.push_back(columns[best_col].is_shape ? 1 : 0);
      expand(depth + 1, ctx, max_depth);
      pivot_path.pop_back();

      ctx.current_solution.pop_back();
      for (size_t i = 0; i < cand.columns.size(); i++) {
        ctx.col_weights[cand.columns[i]] -= cand.weights[i];
      }
      ctx.placed_voxels = ctx.placed_voxels ^ cand.voxel_mask;
    }
  };

  expand(0, root_ctx, 1);
  if (tasks.size() < target_tasks && !tasks.empty()) {
    std::vector<SubtreeTask> d1_tasks = std::move(tasks);
    tasks.clear();
    for (auto &t : d1_tasks) {
      pivot_path = t.pivot_shape;
      expand(t.depth, t.ctx, 2);
    }
  }
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::parallelSolve(
  unsigned int num_workers,
  ConsumeCallback callback,
  std::stop_source &runStop,
  std::atomic<unsigned long> &iterations,
  std::atomic<size_t> &total_tasks,
  std::atomic<size_t> &completed_tasks,
  ThreadBudget *budget,
  const std::vector<HuangSeed> *resume_seeds,
  std::vector<HuangSeed> *salvaged_seeds
) const {
  // One token for the whole run: workers share it for cancellation checks,
  // and a worker that throws fires the source so siblings stop promptly.
  std::stop_token stop = runStop.get_token();
  // Stop-time fan-out (see StopTrace), fixed for the whole run.
  const unsigned int fanout = SplitFanout::fromEnv();
  std::vector<SubtreeTask> tasks;
  if (resume_seeds != nullptr && !resume_seeds->empty()) {
    // Incremental continue: rebuild pool tasks by replaying the salvaged
    // seeds instead of regenerating (and re-searching) everything. Seeds
    // that no longer resolve are skipped defensively; that cannot happen
    // for salvage from an identical matrix.
    for (const auto &seed : *resume_seeds) {
      SubtreeTask t;
      if (taskFromSeed(seed, t))
        tasks.push_back(std::move(t));
    }
  } else {
    unsigned int target_tasks = std::max(16u, num_workers * 4);
    generateTasks(target_tasks, tasks);
  }

  // A resume keeps the paused run's counters (as the DLX path does): the
  // seeds are exactly the tasks it left uncompleted, already counted in
  // total_tasks (in-flight ones via the salvage bump, the pool remainder
  // from the start), so resetting here would drop the bar back to 0.
  if (resume_seeds == nullptr || resume_seeds->empty()) {
    total_tasks.store(tasks.size(), std::memory_order_relaxed);
    completed_tasks.store(0, std::memory_order_relaxed);
  }

  if (tasks.empty() || stop.stop_requested()) {
    // Stopped before (or without) any work: salvage the built tasks as
    // seeds so a continue resumes them instead of regenerating.
    if (stop.stop_requested() && salvaged_seeds != nullptr) {
      // An empty prefix is a valid seed too (the whole search).
      for (auto &t : tasks)
        salvaged_seeds->push_back(t.seed());
    }
    return;
  }

  // Non-terminating pool (see assembler_pool.h): a worker that finds the
  // queue empty waits for quiescence, so siblings blocked in
  // disassemblerPool_c::submit() still have a live crew searching when they
  // wake. No dynamic splitting here -- Huang subtrees run as seeded, skew
  // absorbed at task granularity.
  AssemblyTaskPool<SubtreeTask> pool;
  pool.seed(std::move(tasks));
  pool.setBudget(budget);

  // Deliver stop promptly to parked workers (see assembler_0_c): waking only,
  // no state change, so in-flight tasks are unaffected.
  std::stop_callback wakeParkedOnStop(stop, [&] {
    pool.notify();
    if (budget != nullptr)
      budget->notify();
  });

  std::exception_ptr worker_exception = nullptr;
  std::mutex exception_mutex;
  // Salvaged seeds from interrupted in-flight tasks. Shared across workers.
  std::mutex salvage_mutex;

  auto worker_fn = [&](std::stop_token st = {}) {
    try {
      SubtreeTask t;
      while (pool.pop_task(t, stop, st)) {
        try {
          std::atomic<uint64_t> task_iter{0};
          // Remainder of this task if the stop interrupts it (see
          // StopTrace): filled only on the stop path while search()
          // unwinds, empty when the task runs to completion.
          StopTrace trace;
          trace.rootDepth = t.depth;
          trace.fanout = fanout;
          trace.rootRestSkip = t.rest_skip;
          t.ctx.trace = &trace;

          // Drain per-solution remainders to the shared counter as they
          // arise, so live readers observe advancing iterations (see the
          // serial flush above); exact totals preserved (exchange drains).
          ISimdHuangCover::SolutionCallback flushingCb = [&](const std::vector<unsigned int> &s) -> bool {
            iterations.fetch_add(task_iter.exchange(0, std::memory_order_relaxed),
                                 std::memory_order_relaxed);
            bool didConsume = false;
            bool cont = callback(s, didConsume);
            // Found but declined by the stop (never delivered): the frames
            // above drop this path as searched, so keep it as a piece.
            if (!didConsume && stop.stop_requested())
              trace.addPiece(s);
            return cont && !stop.stop_requested() && !st.stop_requested();
          };
          search(t.depth, t.ctx, flushingCb, stop, task_iter, t.rest_skip);
          t.ctx.trace = nullptr;

          flushIterations(t.ctx, task_iter);
          iterations.fetch_add(task_iter.load(std::memory_order_relaxed), std::memory_order_relaxed);
          if (trace.pieces.empty()) {
            // Fully searched, every solution delivered -- even if a stop
            // landed after the last branch (the trace is exact, unlike
            // re-reading the token).
            completed_tasks.fetch_add(1, std::memory_order_relaxed);
          } else if (salvaged_seeds != nullptr) {
            // Interrupted: the pieces partition the task's unexplored
            // territory, so the task's unit transfers to them.
            trace.finalize(t.pivot_shape);
            const size_t n = trace.pieces.size();
            {
              std::lock_guard<std::mutex> lock(salvage_mutex);
              for (auto &piece : trace.pieces)
                salvaged_seeds->push_back(std::move(piece));
            }
            total_tasks.fetch_add(n - 1, std::memory_order_relaxed);
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
  threads.reserve(num_workers - 1);
  for (unsigned int i = 1; i < num_workers; i++) {
    threads.emplace_back(worker_fn);
  }

  worker_fn();

  for (auto &th : threads) {
    if (th.joinable())
      th.join();
  }

  if (worker_exception) {
    std::rethrow_exception(worker_exception);
  }

  // Salvage the pool remainder for incremental in-session resume:
  // queued tasks never ran, so their placed prefixes are pristine by construction.
  if (salvaged_seeds != nullptr) {
    for (auto &t : pool.drain())
      salvaged_seeds->push_back(t.seed());
  }
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::search(
  unsigned int depth,
  SearchContext &ctx,
  SolutionCallback &callback,
  const std::stop_token &stop,
  std::atomic<uint64_t> &iterations,
  size_t start
) const {
  if (stop.stop_requested()) {
    // Never explored: the whole node is remainder (see StopTrace). At the
    // task root this is the task itself, rest position included.
    if (ctx.trace != nullptr)
      ctx.trace->addPiece(ctx.current_solution,
                          depth == ctx.trace->rootDepth ? ctx.trace->rootRestSkip : 0);
    return;
  }

  ctx.local_iterations++;
  // Batched publish (DLX-worker idiom): at most one relaxed add per 256
  // nodes; remainder flushes at solutions keep live readers advancing.
  if (ctx.local_iterations - ctx.flushed_iterations >= 256) {
    iterations.fetch_add(256, std::memory_order_relaxed);
    ctx.flushed_iterations += 256;
  }

  // Goal check: are all conditions fulfilled?
  if (ctx.current_solution.size() >= total_min_pieces) {
    if (ctx.placed_voxels.containsAll(required_voxels)) {
      bool all_fulfilled = true;
      for (unsigned int c = 1; c <= num_shapes; c++) {
        if (ctx.col_weights[c] < columns[c].min_weight || ctx.col_weights[c] > columns[c].max_weight) {
          all_fulfilled = false;
          break;
        }
      }
      if (all_fulfilled && has_range) {
        if (ctx.col_weights[range_column] < columns[range_column].min_weight ||
            ctx.col_weights[range_column] > columns[range_column].max_weight) {
          all_fulfilled = false;
        }
      }
      /* the hole budget applies to the assembly we are about to report, not
       * just to the search below us: every hole column still unfilled here is
       * a hole in this assembly. assembler_1_c::rec() gets this from its check
       * running before it reaches a solution; our goal test runs first, so it
       * has to count them itself or it hands back assemblies with more holes
       * than the puzzle allows.
       */
      if (all_fulfilled && holes < hole_columns.size()) {
        unsigned int empty_holes = 0;
        for (unsigned int hc : hole_columns) {
          if (ctx.col_weights[hc] == 0) {
            empty_holes++;
            if (empty_holes > holes) {
              all_fulfilled = false;
              break;
            }
          }
        }
      }

      if (all_fulfilled) {
        flushIterations(ctx, iterations);
        if (!callback(ctx.current_solution))
          return;
        return;
      }
    }
  }

  const auto &curr_active = ctx.scratch_active_rows[depth];
  if (curr_active.empty())
    return;

  // Calculate col_counts for active rows
  std::fill(ctx.col_counts.begin(), ctx.col_counts.end(), 0);
  for (uint32_t r_idx : curr_active) {
    const auto &r = rows[r_idx];
    const unsigned int *cols = r.columns.data();
    const unsigned int *wgts = r.weights.data();
    size_t sz = r.columns.size();
    for (size_t i = 0; i < sz; i++) {
      ctx.col_counts[cols[i]] += wgts[i];
    }
  }

  // Hole pruning
  if (holes < hole_columns.size()) {
    unsigned int empty_holes = 0;
    for (unsigned int hc : hole_columns) {
      if (ctx.col_counts[hc] == 0 && ctx.col_weights[hc] == 0) {
        empty_holes++;
        if (empty_holes > holes)
          return;
      }
    }
  }

  // Combined dead-end pruning and MRV pivot column selection
  unsigned int min_metric = UINT32_MAX;
  unsigned int best_col = UINT32_MAX;

  // 1. Check shape columns (1..num_shapes)
  for (unsigned int c = 1; c <= num_shapes; c++) {
    if (ctx.col_weights[c] >= columns[c].min_weight)
      continue;
    unsigned int count = ctx.col_counts[c];
    if (ctx.col_weights[c] + count < columns[c].min_weight)
      return; // Dead end: cannot satisfy shape piece requirement
    unsigned int remaining_need = columns[c].min_weight - ctx.col_weights[c];
    unsigned int metric = count * remaining_need;
    if (metric < min_metric) {
      min_metric = metric;
      best_col = c;
    }
  }

  // 2. Check range column if present, for pruning only -- never as a pivot
  // when there are no variable voxels. With exact voxel cover the range
  // weight of every complete solution is forced (each placed voxel is
  // covered exactly once), so the range column only validates: over-max
  // rows are filtered, under-min prunes here, min/max is checked at the
  // goal. Pivoting on it would report compatible range-row pairs once per
  // order ({r1,r2} and {r2,r1}): unlike shape pivots no monotonic filter
  // applies to it. With variable voxels the pivot stays, since filling
  // holes may be the only way to reach the minimum.
  if (has_range && ctx.col_weights[range_column] < columns[range_column].min_weight) {
    unsigned int count = ctx.col_counts[range_column];
    if (ctx.col_weights[range_column] + count < columns[range_column].min_weight)
      return; // Dead end: cannot satisfy range minimum
    if (!hole_columns.empty()) {
      unsigned int remaining_need = columns[range_column].min_weight - ctx.col_weights[range_column];
      unsigned int metric = count * remaining_need;
      if (metric < min_metric) {
        min_metric = metric;
        best_col = range_column;
      }
    }
  }

  // 3. Check unplaced voxels using bitset scanning (skips placed voxels entirely)
  for (size_t w = 0; w < BitsetType::NUM_WORDS; ++w) {
    uint64_t unplaced = required_voxels.words[w] & ~ctx.placed_voxels.words[w];
    while (unplaced != 0) {
      int bit = std::countr_zero(unplaced);
      unsigned int c = static_cast<unsigned int>(w * 64 + bit + 1);
      unplaced &= (unplaced - 1);

      unsigned int count = ctx.col_counts[c];
      if (count == 0)
        return; // Dead end: required voxel has 0 remaining placements!

      if (count < min_metric) {
        min_metric = count;
        best_col = c;
      }
    }
  }

  if (best_col == UINT32_MAX)
    return;

  /* Branch on candidate rows covering best_col.
   *
   * DELIBERATELY no resize of ctx.scratch_active_rows here: curr_active is a
   * reference into that vector, and a resize would reallocate the outer buffer
   * and leave it dangling before the loop below walks it (depth stays in
   * bounds by the searchDepthBound() argument -- every placement consumes a
   * shape-column unit -- and independently by num_columns via unique voxel
   * bits; see initContext).
   */
  bt_assert(depth + 1 < ctx.scratch_active_rows.size());

  for (size_t i = start; i < curr_active.size(); i++) {
    const uint32_t r_idx = curr_active[i];
    const auto &cand = rows[r_idx];

    if (!coversPivot(cand, best_col) || !fitsBounds(cand, ctx))
      continue;

    // Place candidate row
    ctx.placed_voxels = ctx.placed_voxels | cand.voxel_mask;
    for (size_t i = 0; i < cand.columns.size(); i++) {
      ctx.col_weights[cand.columns[i]] += cand.weights[i];
    }
    ctx.current_solution.push_back(cand.node_id);

    bool shape_full = (ctx.col_weights[cand.shape_col] >= columns[cand.shape_col].max_weight);
    bool filter_monotonic = (!shape_full && columns[best_col].is_shape);
    unsigned int max_allowed_range = has_range ? (columns[range_column].max_weight - ctx.col_weights[range_column]) : 0;

    auto &next_active = ctx.scratch_active_rows[depth + 1];
    next_active.clear();

    filterRows(curr_active, r_idx, cand.voxel_mask, cand.shape_id, shape_full,
               filter_monotonic, cand.shape_row_idx, has_range, max_allowed_range, next_active);

    search(depth + 1, ctx, callback, stop, iterations);

    // Backtrack (zero heap allocation, zero pointer re-linking)
    ctx.current_solution.pop_back();
    for (size_t i = 0; i < cand.columns.size(); i++) {
      ctx.col_weights[cand.columns[i]] -= cand.weights[i];
    }
    ctx.placed_voxels = ctx.placed_voxels ^ cand.voxel_mask;

    if (stop.stop_requested()) {
      // This branch is done (the child returned); the ones after it are
      // untouched. Stop path only: nothing here runs per node.
      if (ctx.trace != nullptr)
        recordFrame(depth, best_col, i + 1, ctx);
      return;
    }
  }
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::recordFrame(
  unsigned int depth,
  unsigned int best_col,
  size_t next,
  SearchContext &ctx
) const {
  StopTrace &trace = *ctx.trace;
  trace.setPivot(depth, columns[best_col].is_shape);

  // The frame state is back at its entry (the branch was just undone), so
  // the same filters as the branch loop select exactly the branches it
  // would still have taken. scratch_active_rows[depth] is intact: deeper
  // frames only write deeper levels.
  const auto &curr_active = ctx.scratch_active_rows[depth];
  unsigned int emitted = 0;
  for (size_t j = next; j < curr_active.size(); j++) {
    const auto &cand = rows[curr_active[j]];
    if (!coversPivot(cand, best_col) || !fitsBounds(cand, ctx))
      continue;
    if (emitted == trace.fanout) {
      // Everything from here on as one rest: the frame resumes its loop at
      // index j on the identically replayed state.
      trace.addPiece(ctx.current_solution, static_cast<uint32_t>(j));
      return;
    }
    std::vector<unsigned int> prefix = ctx.current_solution;
    prefix.push_back(cand.node_id);
    trace.addPiece(std::move(prefix));
    emitted++;
  }
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::StopTrace::finalize(
  const std::vector<uint8_t> & taskPivots
) {
  for (auto &piece : pieces) {
    piece.pivotShape.assign(piece.prefix.size(), 0);
    for (size_t k = 0; k < piece.prefix.size(); k++) {
      if (k < rootDepth) {
        piece.pivotShape[k] = k < taskPivots.size() ? taskPivots[k] : 0;
      } else {
        // Every step below the root was chosen by a frame that unwound
        // through its stop check and recorded its pivot.
        bt_assert(k < pivotKnown.size() && pivotKnown[k]);
        piece.pivotShape[k] = k < pivots.size() ? pivots[k] : 0;
      }
    }
  }
}

template class SimdHuangCover<SimdBitset256>;
template class SimdHuangCover<SimdBitset512>;
template class SimdHuangCover<SimdBitset1024>;
template class SimdHuangCover<SimdBitset2048>;
template class SimdHuangCover<SimdBitset4096>;
template class SimdHuangCover<SimdBitset8192>;
template class SimdHuangCover<SimdBitset16384>;
template class SimdHuangCover<SimdBitset32768>;
