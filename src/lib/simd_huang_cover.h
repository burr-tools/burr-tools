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
#include <bit>

/**
 * Interface for hardware-vectorized exact cover solvers for Huang's algorithm
 * (assembler 1), handling duplicate piece shapes, variable voxels (holes),
 * and piece range constraints.
 */
class ISimdHuangCover {
public:
  virtual ~ISimdHuangCover() = default;

  using SolutionCallback = std::function<bool(const std::vector<unsigned int> &solution_nodes)>;

  virtual void setColumnBounds(
    unsigned int col,
    unsigned int min_w,
    unsigned int max_w,
    bool is_voxel,
    bool is_shape,
    bool is_range,
    bool is_hole
  ) = 0;

  virtual void setHoles(unsigned int h) = 0;

  virtual uint32_t addRow(
    unsigned int node_id,
    unsigned int shape_id,
    unsigned int shape_col,
    unsigned int shape_row_idx,
    unsigned int range_weight,
    const std::vector<unsigned int> &cols,
    const std::vector<unsigned int> &weights
  ) = 0;

  virtual void registerNodeAlias(unsigned int node_id, uint32_t row_idx) = 0;

  virtual void solve(
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const = 0;

  virtual void solveSubtree(
    const std::vector<unsigned int> &prefix_node_ids,
    const std::vector<unsigned int> &hidden_node_ids,
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const = 0;

  virtual void parallelSolve(
    unsigned int num_workers,
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<unsigned long> &iterations,
    std::atomic<size_t> &total_tasks,
    std::atomic<size_t> &completed_tasks
  ) const = 0;

  virtual unsigned int getNumRows() const = 0;
  virtual unsigned int getNumColumns() const = 0;
  virtual unsigned int getNumShapes() const = 0;

  /** Which filterRows() kernel this instance will dispatch to: "avx512",
   *  "avx2", "neon" or "scalar". Exposed so the BURRTOOLS_NO_* kill switches
   *  are testable -- every kernel computes the same answer, so a result
   *  comparison cannot tell which one actually ran.
   */
  virtual const char * activeKernel() const = 0;
};

/**
 * Templated hardware-vectorized exact cover solver for Huang's algorithm.
 */
template <typename BitsetType>
class SimdHuangCover : public ISimdHuangCover {
public:
  struct Row {
    BitsetType voxel_mask;            // bitmask of voxels covered
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

  SimdHuangCover(unsigned int num_columns, unsigned int num_shapes);

  void setColumnBounds(
    unsigned int col,
    unsigned int min_w,
    unsigned int max_w,
    bool is_voxel,
    bool is_shape,
    bool is_range,
    bool is_hole
  ) override;

  void setHoles(unsigned int h) override { holes = h; }

  uint32_t addRow(
    unsigned int node_id,
    unsigned int shape_id,
    unsigned int shape_col,
    unsigned int shape_row_idx,
    unsigned int range_weight,
    const std::vector<unsigned int> &cols,
    const std::vector<unsigned int> &weights
  ) override;

  void registerNodeAlias(unsigned int node_id, uint32_t row_idx) override;

  void solve(
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const override;

  void solveSubtree(
    const std::vector<unsigned int> &prefix_node_ids,
    const std::vector<unsigned int> &hidden_node_ids,
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const override;

  void parallelSolve(
    unsigned int num_workers,
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<unsigned long> &iterations,
    std::atomic<size_t> &total_tasks,
    std::atomic<size_t> &completed_tasks
  ) const override;

  unsigned int getNumRows() const override { return rows.size(); }
  unsigned int getNumColumns() const override { return num_columns; }
  unsigned int getNumShapes() const override { return num_shapes; }

  /* must mirror the dispatch ladder at the top of filterRows() */
  const char * activeKernel() const override {
    if (use_avx512 && BitsetType::NUM_WORDS >= 8) return "avx512";
    if (use_avx2) return "avx2";
    if (use_neon) return "neon";
    return "scalar";
  }

private:
  unsigned int num_columns;
  unsigned int num_shapes;
  unsigned int total_min_pieces = 0;
  unsigned int holes = 0;
  unsigned int range_column = 0;
  bool has_range = false;

  BitsetType required_voxels;
  std::vector<Column> columns;
  std::vector<Row> rows;
  std::vector<unsigned int> active_column_list;
  std::vector<unsigned int> hole_columns;
  std::unordered_map<unsigned int, uint32_t> node_to_row_idx;
  [[maybe_unused]] bool use_avx2 = false;
  [[maybe_unused]] bool use_avx512 = false;

  /* mirrors SimdExactCover: without this the NEON kernel is unconditional on
   * ARM, so BURRTOOLS_NO_AVX2/NO_SIMD silently A/B the same code against
   * itself on the project's primary development platform and the scalar loop
   * below the dispatch is dead
   */
  [[maybe_unused]] bool use_neon = false;

  struct SearchContext {
    BitsetType placed_voxels;
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
    const BitsetType &chosen_voxel_mask,
    unsigned int chosen_shape,
    bool shape_is_full,
    bool filter_monotonic,
    unsigned int chosen_shape_row_idx,
    bool check_range,
    unsigned int max_allowed_range_weight,
    std::vector<uint32_t> &dst
  ) const;

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  void filterRowsAvx512(
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
  ) const;

  void filterRowsAvx2(
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
  ) const;
#elif defined(__aarch64__) || defined(__ARM_NEON)
  void filterRowsNeon(
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
  ) const;
#endif
};

using SimdHuangCover256 = SimdHuangCover<SimdBitset256>;
using SimdHuangCover512 = SimdHuangCover<SimdBitset512>;
using SimdHuangCover1024 = SimdHuangCover<SimdBitset1024>;
using SimdHuangCover2048 = SimdHuangCover<SimdBitset2048>;
using SimdHuangCover4096 = SimdHuangCover<SimdBitset4096>;
using SimdHuangCover8192 = SimdHuangCover<SimdBitset8192>;
using SimdHuangCover16384 = SimdHuangCover<SimdBitset16384>;
using SimdHuangCover32768 = SimdHuangCover<SimdBitset32768>;

#endif // __SIMD_HUANG_COVER_H__
