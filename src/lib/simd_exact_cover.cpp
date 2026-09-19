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
#include "simd_exact_cover.h"

#include <cstdlib>

SimdExactCover256::SimdExactCover256(unsigned int cols, unsigned int pieces)
  : num_columns(cols), num_pieces(pieces)
{
  bt_assert(num_columns <= 256);

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  use_avx2 = __builtin_cpu_supports("avx2") != 0;
#else
  use_avx2 = false;
#endif

  if (std::getenv("BURRTOOLS_NO_SIMD") || std::getenv("BURRTOOLS_NO_AVX2")) {
    use_avx2 = false;
  }

#if defined(__aarch64__) || defined(__ARM_NEON)
  use_neon = !(std::getenv("BURRTOOLS_NO_SIMD") || std::getenv("BURRTOOLS_NO_NEON"));
#endif
}

void SimdExactCover256::setRequiredColumns(const SimdBitset256 &required) {
  required_columns = required;
  active_column_list.clear();
  for (unsigned int c = 0; c < num_columns; c++) {
    if (required_columns.test(c)) {
      active_column_list.push_back(c);
    }
  }
}

uint32_t SimdExactCover256::addRow(unsigned int node_id, unsigned int piece_id, const std::vector<unsigned int> &cols) {
  Row r;
  r.node_id = node_id;
  r.piece_id = piece_id;
  r.columns = cols;
  for (unsigned int c : cols) {
    bt_assert(c < num_columns);
    bt_assert(c < 256);
    r.mask.set(c);
  }
  uint32_t idx = static_cast<uint32_t>(rows.size());
  node_to_row_idx[node_id] = idx;
  rows.push_back(std::move(r));
  return idx;
}

void SimdExactCover256::registerNodeAlias(unsigned int node_id, uint32_t row_idx) {
  node_to_row_idx[node_id] = row_idx;
}

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
__attribute__((target("avx2")))
void SimdExactCover256::filterRowsAvx2(
  const std::vector<uint32_t> &src,
  const SimdBitset256 &chosen_mask,
  std::vector<uint32_t> &dst
) const {
  __m256i va = _mm256_load_si256(reinterpret_cast<const __m256i*>(chosen_mask.words));
  for (uint32_t idx : src) {
    __m256i vb = _mm256_load_si256(reinterpret_cast<const __m256i*>(rows[idx].mask.words));
    if (_mm256_testz_si256(va, vb)) {
      dst.push_back(idx);
    }
  }
}
#elif defined(__aarch64__) || defined(__ARM_NEON)
void SimdExactCover256::filterRowsNeon(
  const std::vector<uint32_t> &src,
  const SimdBitset256 &chosen_mask,
  std::vector<uint32_t> &dst
) const {
  uint64x2_t ca0 = vld1q_u64(&chosen_mask.words[0]);
  uint64x2_t ca1 = vld1q_u64(&chosen_mask.words[2]);
  for (uint32_t idx : src) {
    const uint64_t *rw = rows[idx].mask.words;
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

void SimdExactCover256::filterRows(
  const std::vector<uint32_t> &src,
  const SimdBitset256 &chosen_mask,
  std::vector<uint32_t> &dst
) const {
  if (dst.capacity() < src.size()) {
    dst.reserve(src.size());
  }
#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (use_avx2) {
    filterRowsAvx2(src, chosen_mask, dst);
    return;
  }
#elif defined(__aarch64__) || defined(__ARM_NEON)
  if (use_neon) {
    filterRowsNeon(src, chosen_mask, dst);
    return;
  }
#endif
  for (uint32_t idx : src) {
    if (is_disjoint_scalar(chosen_mask, rows[idx].mask)) {
      dst.push_back(idx);
    }
  }
}

void SimdExactCover256::solve(
  SolutionCallback callback,
  const std::atomic<bool> &abort_flag,
  std::atomic<uint64_t> &iterations
) const {
  if (rows.empty() || active_column_list.empty())
    return;

  SearchContext ctx;
  ctx.scratch_active_rows.resize(num_pieces + 4);
  ctx.current_solution.resize(num_pieces);
  ctx.col_counts.resize(num_columns, 0);

  ctx.scratch_active_rows[0].resize(rows.size());
  for (size_t i = 0; i < rows.size(); i++) {
    ctx.scratch_active_rows[0][i] = static_cast<uint32_t>(i);
  }

  SimdBitset256 occupied;
  search(0, occupied, ctx, callback, abort_flag, iterations);

  uint64_t rem = ctx.local_iterations & 255;
  if (rem > 0) {
    iterations.fetch_add(rem, std::memory_order_relaxed);
  }
}

void SimdExactCover256::solveSubtree(
  const std::vector<unsigned int> &prefix_node_ids,
  SolutionCallback callback,
  const std::atomic<bool> &abort_flag,
  std::atomic<uint64_t> &iterations
) const {
  if (rows.empty() || active_column_list.empty())
    return;

  SearchContext ctx;
  ctx.scratch_active_rows.resize(num_pieces + 4);
  ctx.current_solution.resize(num_pieces);
  ctx.col_counts.resize(num_columns, 0);

  ctx.scratch_active_rows[0].resize(rows.size());
  for (size_t i = 0; i < rows.size(); i++) {
    ctx.scratch_active_rows[0][i] = static_cast<uint32_t>(i);
  }

  SimdBitset256 occupied;
  bool conflict = false;

  for (unsigned int d = 0; d < prefix_node_ids.size(); d++) {
    auto it = node_to_row_idx.find(prefix_node_ids[d]);
    if (it == node_to_row_idx.end()) {
      conflict = true;
      break;
    }
    uint32_t r_idx = it->second;
    const auto &row = rows[r_idx];
    if (!is_disjoint_scalar(occupied, row.mask)) {
      conflict = true;
      break;
    }
    occupied = occupied | row.mask;
    if (d >= ctx.current_solution.size())
      ctx.current_solution.resize(d + 1);
    ctx.current_solution[d] = row.node_id;

    auto &next_active = ctx.scratch_active_rows[d + 1];
    next_active.clear();
    filterRows(ctx.scratch_active_rows[d], row.mask, next_active);
  }

  if (!conflict) {
    search(prefix_node_ids.size(), occupied, ctx, callback, abort_flag, iterations);
  }

  uint64_t rem = ctx.local_iterations & 255;
  if (rem > 0) {
    iterations.fetch_add(rem, std::memory_order_relaxed);
  }
}

void SimdExactCover256::search(
  unsigned int depth,
  const SimdBitset256 &occupied,
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

  // Check goal: are all required columns covered?
  if (occupied.containsAll(required_columns)) {
    ctx.current_solution.resize(depth);
    if (!callback(ctx.current_solution))
      return;
    return;
  }

  const auto &curr_active = ctx.scratch_active_rows[depth];
  if (curr_active.empty())
    return;

  // Count options per uncovered column
  std::fill(ctx.col_counts.begin(), ctx.col_counts.end(), 0);
  for (uint32_t r_idx : curr_active) {
    for (unsigned int c : rows[r_idx].columns) {
      ctx.col_counts[c]++;
    }
  }

  // Find the uncovered required column with the minimum count (MRV)
  unsigned int min_count = UINT32_MAX;
  unsigned int best_col = UINT32_MAX;

  for (unsigned int c : active_column_list) {
    if (occupied.test(c))
      continue;
    uint32_t cnt = ctx.col_counts[c];
    if (cnt == 0) {
      // Immediate dead-end prune: required column cannot be covered
      return;
    }
    if (cnt < min_count) {
      min_count = cnt;
      best_col = c;
      if (min_count == 1)
        break; // Forced item, cannot beat 1
    }
  }

  if (best_col == UINT32_MAX)
    return;

  // Branch on all candidate rows covering best_col
  for (uint32_t r_idx : curr_active) {
    const auto &candidate_row = rows[r_idx];
    if (!candidate_row.mask.test(best_col))
      continue;

    if (depth >= ctx.current_solution.size())
      ctx.current_solution.resize(depth + 1);
    ctx.current_solution[depth] = candidate_row.node_id;

    auto &next_active = ctx.scratch_active_rows[depth + 1];
    next_active.clear();

    filterRows(curr_active, candidate_row.mask, next_active);

    search(depth + 1, occupied | candidate_row.mask, ctx, callback, abort_flag, iterations);

    if (abort_flag.load(std::memory_order_relaxed))
      return;
  }
}
