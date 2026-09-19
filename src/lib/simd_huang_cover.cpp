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

#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <cassert>
#include <thread>

SimdHuangCover256::SimdHuangCover256(unsigned int num_cols, unsigned int num_s)
  : num_columns(num_cols), num_shapes(num_s) {
  columns.resize(num_columns + 1);

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (!std::getenv("BURRTOOLS_NO_AVX2")) {
    use_avx2 = __builtin_cpu_supports("avx2");
  }
#endif
}

void SimdHuangCover256::setColumnBounds(
  unsigned int col,
  unsigned int min_w,
  unsigned int max_w,
  bool is_voxel,
  bool is_shape,
  bool is_range,
  bool is_hole
) {
  if (col >= columns.size()) {
    columns.resize(col + 1);
  }
  columns[col].min_weight = min_w;
  columns[col].max_weight = max_w;
  columns[col].is_voxel = is_voxel;
  columns[col].is_shape = is_shape;
  columns[col].is_range = is_range;
  columns[col].is_hole = is_hole;

  if (is_range) {
    has_range = true;
    range_column = col;
  }
  if (is_hole) {
    hole_columns.push_back(col);
  }
  if (is_voxel && min_w > 0 && col <= 256) {
    required_voxels.set(col - 1);
  }
  active_column_list.push_back(col);
}

uint32_t SimdHuangCover256::addRow(
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
    if (c > 0 && c <= num_columns && columns[c].is_voxel) {
      r.voxel_mask.set(c - 1);
    }
  }

  node_to_row_idx[node_id] = idx;
  rows.push_back(std::move(r));
  return idx;
}

void SimdHuangCover256::registerNodeAlias(unsigned int node_id, uint32_t row_idx) {
  node_to_row_idx[node_id] = row_idx;
}

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
#pragma GCC push_options
#pragma GCC target("avx2")
void SimdHuangCover256::filterRowsAvx2(
  const std::vector<uint32_t> &src,
  uint32_t chosen_idx,
  const SimdBitset256 &chosen_voxel_mask,
  unsigned int chosen_shape,
  bool shape_is_full,
  bool filter_monotonic,
  unsigned int chosen_shape_row_idx,
  bool check_range,
  unsigned int max_allowed_range_weight,
  std::vector<uint32_t> &dst
) const {
  __m256i va = _mm256_load_si256(reinterpret_cast<const __m256i*>(chosen_voxel_mask.words));
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

    __m256i vb = _mm256_load_si256(reinterpret_cast<const __m256i*>(cand.voxel_mask.words));
    if (_mm256_testz_si256(va, vb)) {
      dst.push_back(idx);
    }
  }
}
#pragma GCC pop_options
#elif defined(__aarch64__) || defined(__ARM_NEON)
void SimdHuangCover256::filterRowsNeon(
  const std::vector<uint32_t> &src,
  uint32_t chosen_idx,
  const SimdBitset256 &chosen_voxel_mask,
  unsigned int chosen_shape,
  bool shape_is_full,
  bool filter_monotonic,
  unsigned int chosen_shape_row_idx,
  bool check_range,
  unsigned int max_allowed_range_weight,
  std::vector<uint32_t> &dst
) const {
  uint64x2_t ca0 = vld1q_u64(&chosen_voxel_mask.words[0]);
  uint64x2_t ca1 = vld1q_u64(&chosen_voxel_mask.words[2]);
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
    uint64x2_t rb0 = vld1q_u64(&rw[0]);
    uint64x2_t rb1 = vld1q_u64(&rw[2]);
    uint64x2_t c0 = vandq_u64(ca0, rb0);
    uint64x2_t c1 = vandq_u64(ca1, rb1);
    uint64x2_t c = vorrq_u64(c0, c1);
    if ((vgetq_lane_u64(c, 0) | vgetq_lane_u64(c, 1)) == 0) {
      dst.push_back(idx);
    }
  }
}
#endif

void SimdHuangCover256::filterRows(
  const std::vector<uint32_t> &src,
  uint32_t chosen_idx,
  const SimdBitset256 &chosen_voxel_mask,
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
  if (use_avx2) {
    filterRowsAvx2(src, chosen_idx, chosen_voxel_mask, chosen_shape, shape_is_full,
                   filter_monotonic, chosen_shape_row_idx, check_range, max_allowed_range_weight, dst);
    return;
  }
#elif defined(__aarch64__) || defined(__ARM_NEON)
  filterRowsNeon(src, chosen_idx, chosen_voxel_mask, chosen_shape, shape_is_full,
                 filter_monotonic, chosen_shape_row_idx, check_range, max_allowed_range_weight, dst);
  return;
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

void SimdHuangCover256::solve(
  SolutionCallback callback,
  const std::atomic<bool> &abort_flag,
  std::atomic<uint64_t> &iterations
) const {
  if (rows.empty() || active_column_list.empty())
    return;

  SearchContext ctx;
  ctx.scratch_active_rows.resize(num_columns + 16);
  ctx.current_solution.reserve(num_columns);
  ctx.col_weights.assign(num_columns + 1, 0);
  ctx.col_counts.assign(num_columns + 1, 0);

  ctx.scratch_active_rows[0].resize(rows.size());
  for (size_t i = 0; i < rows.size(); i++) {
    ctx.scratch_active_rows[0][i] = static_cast<uint32_t>(i);
  }

  search(0, ctx, callback, abort_flag, iterations);

  uint64_t rem = ctx.local_iterations & 255;
  if (rem > 0) {
    iterations.fetch_add(rem, std::memory_order_relaxed);
  }
}

void SimdHuangCover256::solveSubtree(
  const std::vector<unsigned int> &prefix_node_ids,
  const std::vector<unsigned int> &hidden_node_ids,
  SolutionCallback callback,
  const std::atomic<bool> &abort_flag,
  std::atomic<uint64_t> &iterations
) const {
  if (rows.empty() || active_column_list.empty())
    return;

  SearchContext ctx;
  ctx.scratch_active_rows.resize(num_columns + 16);
  ctx.current_solution.reserve(num_columns);
  ctx.col_weights.assign(num_columns + 1, 0);
  ctx.col_counts.assign(num_columns + 1, 0);

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

  bool conflict = false;

  for (unsigned int d = 0; d < prefix_node_ids.size(); d++) {
    auto it = node_to_row_idx.find(prefix_node_ids[d]);
    if (it == node_to_row_idx.end()) {
      conflict = true;
      break;
    }
    uint32_t r_idx = it->second;
    const auto &cand = rows[r_idx];

    // Check voxel conflict
    if (!is_disjoint_scalar(ctx.placed_voxels, cand.voxel_mask)) {
      conflict = true;
      break;
    }
    // Check shape bound
    if (ctx.col_weights[cand.shape_col] + 1 > columns[cand.shape_col].max_weight) {
      conflict = true;
      break;
    }
    // Check range bound
    if (has_range && ctx.col_weights[range_column] + cand.range_weight > columns[range_column].max_weight) {
      conflict = true;
      break;
    }

    // Place candidate
    ctx.placed_voxels = ctx.placed_voxels | cand.voxel_mask;
    for (size_t i = 0; i < cand.columns.size(); i++) {
      ctx.col_weights[cand.columns[i]] += cand.weights[i];
    }
    ctx.current_solution.push_back(cand.node_id);

    bool shape_full = (ctx.col_weights[cand.shape_col] >= columns[cand.shape_col].max_weight);
    unsigned int max_allowed_range = has_range ? (columns[range_column].max_weight - ctx.col_weights[range_column]) : 0;

    auto &next_active = ctx.scratch_active_rows[d + 1];
    next_active.clear();
    filterRows(ctx.scratch_active_rows[d], r_idx, cand.voxel_mask, cand.shape_id, shape_full,
               false, cand.shape_row_idx, has_range, max_allowed_range, next_active);
  }

  if (!conflict) {
    search(prefix_node_ids.size(), ctx, callback, abort_flag, iterations);
  }

  uint64_t rem = ctx.local_iterations & 255;
  if (rem > 0) {
    iterations.fetch_add(rem, std::memory_order_relaxed);
  }
}

void SimdHuangCover256::generateTasks(
  unsigned int target_tasks,
  std::vector<SubtreeTask> &tasks
) const {
  tasks.clear();
  if (rows.empty() || active_column_list.empty())
    return;

  SearchContext root_ctx;
  root_ctx.scratch_active_rows.resize(num_columns + 16);
  root_ctx.current_solution.reserve(num_columns);
  root_ctx.col_weights.assign(num_columns + 1, 0);
  root_ctx.col_counts.assign(num_columns + 1, 0);

  root_ctx.scratch_active_rows[0].resize(rows.size());
  for (size_t i = 0; i < rows.size(); i++) {
    root_ctx.scratch_active_rows[0][i] = static_cast<uint32_t>(i);
  }

  std::function<void(unsigned int, SearchContext&, unsigned int)> expand;
  expand = [&](unsigned int depth, SearchContext &ctx, unsigned int max_depth) {
    if (depth == max_depth) {
      SubtreeTask t;
      t.depth = depth;
      t.ctx = ctx;
      tasks.push_back(std::move(t));
      return;
    }

    const auto &curr_active = ctx.scratch_active_rows[depth];
    if (curr_active.empty())
      return;

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

    if (best_col == UINT32_MAX)
      return;

    if (depth + 1 >= ctx.scratch_active_rows.size()) {
      ctx.scratch_active_rows.resize(depth + 16);
    }

    for (uint32_t r_idx : curr_active) {
      const auto &cand = rows[r_idx];

      bool covers_best = false;
      for (unsigned int c : cand.columns) {
        if (c == best_col) {
          covers_best = true;
          break;
        }
      }
      if (!covers_best)
        continue;

      if (!is_disjoint_scalar(ctx.placed_voxels, cand.voxel_mask))
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

      expand(depth + 1, ctx, max_depth);

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
      expand(t.depth, t.ctx, 2);
    }
  }
}

void SimdHuangCover256::parallelSolve(
  unsigned int num_workers,
  SolutionCallback callback,
  const std::atomic<bool> &abort_flag,
  std::atomic<uint64_t> &iterations,
  std::atomic<size_t> &total_tasks,
  std::atomic<size_t> &completed_tasks
) const {
  unsigned int target_tasks = std::max(16u, num_workers * 4);
  std::vector<SubtreeTask> tasks;
  generateTasks(target_tasks, tasks);

  total_tasks.store(tasks.size(), std::memory_order_relaxed);
  completed_tasks.store(0, std::memory_order_relaxed);

  if (tasks.empty() || abort_flag.load(std::memory_order_relaxed))
    return;

  std::atomic<size_t> next_task_idx{0};

  auto worker_fn = [&]() {
    while (!abort_flag.load(std::memory_order_relaxed)) {
      size_t idx = next_task_idx.fetch_add(1, std::memory_order_relaxed);
      if (idx >= tasks.size())
        break;

      auto &t = tasks[idx];
      search(t.depth, t.ctx, callback, abort_flag, iterations);

      uint64_t rem = t.ctx.local_iterations & 255;
      if (rem > 0) {
        iterations.fetch_add(rem, std::memory_order_relaxed);
      }
      completed_tasks.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(num_workers - 1);
  for (unsigned int i = 1; i < num_workers; i++) {
    threads.emplace_back(worker_fn);
  }

  worker_fn();

  for (auto &th : threads) {
    if (th.joinable())
      th.join();
  }
}

void SimdHuangCover256::search(
  unsigned int depth,
  SearchContext &ctx,
  SolutionCallback &callback,
  const std::atomic<bool> &abort_flag,
  std::atomic<uint64_t> &iterations
) const {
  if (abort_flag.load(std::memory_order_relaxed))
    return;

  ctx.local_iterations++;
  if ((ctx.local_iterations & 255) == 0) {
    iterations.fetch_add(256, std::memory_order_relaxed);
  }

  // Goal check: are all conditions fulfilled?
  bool all_fulfilled = true;

  if (!ctx.placed_voxels.containsAll(required_voxels)) {
    all_fulfilled = false;
  } else {
    for (unsigned int c : active_column_list) {
      if (ctx.col_weights[c] < columns[c].min_weight || ctx.col_weights[c] > columns[c].max_weight) {
        all_fulfilled = false;
        break;
      }
    }
  }

  if (all_fulfilled) {
    if (!callback(ctx.current_solution))
      return;
    return;
  }

  const auto &curr_active = ctx.scratch_active_rows[depth];
  if (curr_active.empty())
    return;

  // Calculate col_counts for active rows
  std::fill(ctx.col_counts.begin(), ctx.col_counts.end(), 0);
  for (uint32_t r_idx : curr_active) {
    const auto &r = rows[r_idx];
    for (size_t i = 0; i < r.columns.size(); i++) {
      ctx.col_counts[r.columns[i]] += r.weights[i];
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

  // Dead-end pruning (feasibility checks)
  for (unsigned int c : active_column_list) {
    if (columns[c].min_weight > ctx.col_weights[c]) {
      if (ctx.col_weights[c] + ctx.col_counts[c] < columns[c].min_weight) {
        return; // Cannot satisfy minimum weight requirement
      }
    }
    if (columns[c].is_voxel && columns[c].min_weight > 0 && !ctx.placed_voxels.test(c - 1)) {
      if (ctx.col_counts[c] == 0) {
        return; // Required voxel has no remaining placements
      }
    }
  }

  // Select pivot column via Minimum Remaining Values (MRV) among unfulfilled columns
  unsigned int min_metric = UINT32_MAX;
  unsigned int best_col = UINT32_MAX;

  for (unsigned int c : active_column_list) {
    if (columns[c].is_hole)
      continue;
    if (ctx.col_weights[c] >= columns[c].min_weight)
      continue; // Already fulfilled!
    if (columns[c].is_voxel && ctx.placed_voxels.test(c - 1))
      continue;

    unsigned int remaining_need = columns[c].min_weight - ctx.col_weights[c];
    unsigned int count = ctx.col_counts[c];

    if (count == 0) {
      return; // Dead end: required column has no remaining rows!
    }

    unsigned int metric = count * remaining_need;
    if (metric < min_metric) {
      min_metric = metric;
      best_col = c;
      if (metric <= 1)
        break;
    }
  }

  if (best_col == UINT32_MAX)
    return;

  // Branch on candidate rows covering best_col
  if (depth + 1 >= ctx.scratch_active_rows.size()) {
    ctx.scratch_active_rows.resize(depth + 16);
  }

  for (uint32_t r_idx : curr_active) {
    const auto &cand = rows[r_idx];

    // Check if candidate covers best_col
    bool covers_best = false;
    for (unsigned int c : cand.columns) {
      if (c == best_col) {
        covers_best = true;
        break;
      }
    }
    if (!covers_best)
      continue;

    // Check compatibility with current state
    if (!is_disjoint_scalar(ctx.placed_voxels, cand.voxel_mask))
      continue;
    if (ctx.col_weights[cand.shape_col] + 1 > columns[cand.shape_col].max_weight)
      continue;
    if (has_range && ctx.col_weights[range_column] + cand.range_weight > columns[range_column].max_weight)
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

    search(depth + 1, ctx, callback, abort_flag, iterations);

    // Backtrack (zero heap allocation, zero pointer re-linking)
    ctx.current_solution.pop_back();
    for (size_t i = 0; i < cand.columns.size(); i++) {
      ctx.col_weights[cand.columns[i]] -= cand.weights[i];
    }
    ctx.placed_voxels = ctx.placed_voxels ^ cand.voxel_mask;

    if (abort_flag.load(std::memory_order_relaxed))
      return;
  }
}
