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
#include "bt_assert.h"

#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <thread>
#include <mutex>

template <typename BitsetType>
SimdHuangCover<BitsetType>::SimdHuangCover(unsigned int num_cols, unsigned int num_s)
  : num_columns(num_cols), num_shapes(num_s) {
  bt_assert(num_cols <= BitsetType::NUM_WORDS * 64);
  columns.resize(num_columns + 1);

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  /* BURRTOOLS_NO_SIMD and BURRTOOLS_NO_AVX2 clear *both* flags, as in
   * SimdExactCover. filterRows() tries the AVX-512 kernel first, so gating
   * only use_avx2 would make BURRTOOLS_NO_AVX2=1 run wider SIMD instead of
   * narrower, and leave the scalar fallback unreachable from tier 512 up.
   */
  if (!std::getenv("BURRTOOLS_NO_SIMD") && !std::getenv("BURRTOOLS_NO_AVX2")) {
    use_avx2 = __builtin_cpu_supports("avx2");
    use_avx512 = __builtin_cpu_supports("avx512f");
  }
  if (std::getenv("BURRTOOLS_NO_AVX512")) {
    use_avx512 = false;
  }
#elif defined(__aarch64__) || defined(__ARM_NEON)
  use_neon = !(std::getenv("BURRTOOLS_NO_SIMD") || std::getenv("BURRTOOLS_NO_NEON"));
#endif
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

template <typename BitsetType>
void SimdHuangCover<BitsetType>::solveSubtree(
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

template <typename BitsetType>
void SimdHuangCover<BitsetType>::generateTasks(
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

  /* A node that expand() cannot refine any further still has to be handed to a
   * worker: it may itself be a complete cover. Dropping it here (as the plain
   * `return`s used to) loses that solution, because in this round the node is
   * replaced by its set of children rather than being a task in its own right.
   */
  auto emitAsTask = [&](unsigned int depth, SearchContext &ctx) {
    SubtreeTask t;
    t.depth = depth;
    t.ctx = ctx;
    tasks.push_back(std::move(t));
  };

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
     * walks it. The guard it replaces was dead anyway: every placed row
     * consumes at least one voxel column, so depth < num_columns always, and
     * the vector is created with num_columns + 16 entries.
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

template <typename BitsetType>
void SimdHuangCover<BitsetType>::parallelSolve(
  unsigned int num_workers,
  SolutionCallback callback,
  const std::atomic<bool> &abort_flag,
  std::atomic<unsigned long> &iterations,
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
  std::exception_ptr worker_exception = nullptr;
  std::mutex exception_mutex;

  auto worker_fn = [&]() {
    try {
      while (!abort_flag.load(std::memory_order_relaxed)) {
        size_t idx = next_task_idx.fetch_add(1, std::memory_order_relaxed);
        if (idx >= tasks.size())
          break;

        auto &t = tasks[idx];
        std::atomic<uint64_t> task_iter{0};
        search(t.depth, t.ctx, callback, abort_flag, task_iter);

        uint64_t rem = t.ctx.local_iterations & 255;
        if (rem > 0) {
          task_iter.fetch_add(rem, std::memory_order_relaxed);
        }
        iterations.fetch_add(task_iter.load(std::memory_order_relaxed), std::memory_order_relaxed);
        completed_tasks.fetch_add(1, std::memory_order_relaxed);
      }
    } catch (...) {
      std::lock_guard<std::mutex> lock(exception_mutex);
      if (!worker_exception)
        worker_exception = std::current_exception();
      const_cast<std::atomic<bool>&>(abort_flag).store(true, std::memory_order_relaxed);
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

  if (worker_exception) {
    std::rethrow_exception(worker_exception);
  }
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::search(
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
      if (all_fulfilled) {
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

  // 2. Check range column if present
  if (has_range && ctx.col_weights[range_column] < columns[range_column].min_weight) {
    unsigned int count = ctx.col_counts[range_column];
    if (ctx.col_weights[range_column] + count < columns[range_column].min_weight)
      return; // Dead end: cannot satisfy range minimum
    unsigned int remaining_need = columns[range_column].min_weight - ctx.col_weights[range_column];
    unsigned int metric = count * remaining_need;
    if (metric < min_metric) {
      min_metric = metric;
      best_col = range_column;
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
   * and leave it dangling before the loop below walks it. The guard this
   * replaces was dead anyway -- every placed row consumes at least one voxel
   * column, so depth < num_columns always, and the vector is created with
   * num_columns + 16 entries.
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

template class SimdHuangCover<SimdBitset256>;
template class SimdHuangCover<SimdBitset512>;
template class SimdHuangCover<SimdBitset1024>;
template class SimdHuangCover<SimdBitset2048>;
template class SimdHuangCover<SimdBitset4096>;
template class SimdHuangCover<SimdBitset8192>;
template class SimdHuangCover<SimdBitset16384>;
template class SimdHuangCover<SimdBitset32768>;
