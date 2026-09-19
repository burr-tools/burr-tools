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
#ifndef __SIMD_HUANG_COVER_H__
#define __SIMD_HUANG_COVER_H__

#include "simd_exact_cover.h"

#include <vector>
#include <functional>
#include <atomic>
#include <cstdint>
#include <unordered_map>

/**
 * High-performance hardware-vectorized exact cover solver for Huang's algorithm
 * (assembler 1), handling duplicate piece shapes, variable voxels (holes),
 * and piece range constraints for matrices with <= 256 columns.
 */
class SimdHuangCover256 {
public:
  struct Row {
    SimdBitset256 voxel_mask;         // 256-bit bitmask of voxels covered
    unsigned int node_id = 0;         // original DLX node ID
    unsigned int shape_id = 0;        // piece shape index (0 .. num_shapes-1)
    unsigned int shape_col = 0;       // column index for piece shape
    unsigned int shape_row_idx = 0;   // row index within this shape's placements
    unsigned int range_weight = 0;    // weight in range column (if hasRange)
    std::vector<unsigned int> columns;// all columns touched
    std::vector<unsigned int> weights;// weight in each column touched
  };

  struct Column {
    unsigned int min_weight = 1;
    unsigned int max_weight = 1;
    bool is_voxel = false;
    bool is_shape = false;
    bool is_range = false;
    bool is_hole = false;
  };

  SimdHuangCover256(unsigned int num_columns, unsigned int num_shapes);

  void setColumnBounds(
    unsigned int col,
    unsigned int min_w,
    unsigned int max_w,
    bool is_voxel,
    bool is_shape,
    bool is_range,
    bool is_hole
  );

  void setHoles(unsigned int h) { holes = h; }

  uint32_t addRow(
    unsigned int node_id,
    unsigned int shape_id,
    unsigned int shape_col,
    unsigned int shape_row_idx,
    unsigned int range_weight,
    const std::vector<unsigned int> &cols,
    const std::vector<unsigned int> &weights
  );

  void registerNodeAlias(unsigned int node_id, uint32_t row_idx);

  using SolutionCallback = std::function<bool(const std::vector<unsigned int> &solution_nodes)>;

  void solve(
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const;

  void solveSubtree(
    const std::vector<unsigned int> &prefix_node_ids,
    const std::vector<unsigned int> &hidden_node_ids,
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const;

  struct SearchContext {
    SimdBitset256 placed_voxels;
    std::vector<uint32_t> col_weights;
    std::vector<std::vector<uint32_t>> scratch_active_rows;
    std::vector<unsigned int> current_solution;
    std::vector<uint32_t> col_counts;
    uint64_t local_iterations = 0;
  };

  struct SubtreeTask {
    unsigned int depth = 0;
    SearchContext ctx;
  };

  void generateTasks(unsigned int target_tasks, std::vector<SubtreeTask> &tasks) const;

  void parallelSolve(
    unsigned int num_workers,
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations,
    std::atomic<size_t> &total_tasks,
    std::atomic<size_t> &completed_tasks
  ) const;

  unsigned int getNumRows() const { return rows.size(); }
  unsigned int getNumColumns() const { return num_columns; }
  unsigned int getNumShapes() const { return num_shapes; }

private:
  unsigned int num_columns;
  unsigned int num_shapes;
  unsigned int holes = 0;
  unsigned int range_column = 0;
  bool has_range = false;

  SimdBitset256 required_voxels;
  std::vector<Column> columns;
  std::vector<Row> rows;
  std::vector<unsigned int> active_column_list;
  std::vector<unsigned int> hole_columns;
  std::unordered_map<unsigned int, uint32_t> node_to_row_idx;
  bool use_avx2 = false;

  void search(
    unsigned int depth,
    SearchContext &ctx,
    SolutionCallback &callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const;

  void filterRows(
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
  ) const;

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  void filterRowsAvx2(
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
  ) const;
#elif defined(__aarch64__) || defined(__ARM_NEON)
  void filterRowsNeon(
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
  ) const;
#endif
};

#endif // __SIMD_HUANG_COVER_H__
