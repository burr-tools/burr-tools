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
#ifndef __SIMD_EXACT_COVER_H__
#define __SIMD_EXACT_COVER_H__

#include <cstdint>
#include <vector>
#include <functional>
#include <atomic>
#include <cstring>
#include <cassert>
#include <algorithm>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

/**
 * 256-bit bitset aligned to 32 bytes for AVX2 vector operations.
 */
struct alignas(32) SimdBitset256 {
  uint64_t words[4];

  constexpr SimdBitset256() : words{0, 0, 0, 0} {}
  constexpr SimdBitset256(uint64_t w0, uint64_t w1, uint64_t w2, uint64_t w3)
    : words{w0, w1, w2, w3} {}

  void set(unsigned int bit) {
    assert(bit < 256);
    words[bit >> 6] |= (1ULL << (bit & 63));
  }

  void reset(unsigned int bit) {
    assert(bit < 256);
    words[bit >> 6] &= ~(1ULL << (bit & 63));
  }

  bool test(unsigned int bit) const {
    assert(bit < 256);
    return (words[bit >> 6] & (1ULL << (bit & 63))) != 0;
  }

  void clear() {
    words[0] = 0; words[1] = 0; words[2] = 0; words[3] = 0;
  }

  bool empty() const {
    return (words[0] | words[1] | words[2] | words[3]) == 0;
  }

  bool containsAll(const SimdBitset256 &target) const {
    return ((target.words[0] & ~words[0]) |
            (target.words[1] & ~words[1]) |
            (target.words[2] & ~words[2]) |
            (target.words[3] & ~words[3])) == 0;
  }

  SimdBitset256 operator|(const SimdBitset256 &other) const {
    return SimdBitset256(words[0] | other.words[0],
                         words[1] | other.words[1],
                         words[2] | other.words[2],
                         words[3] | other.words[3]);
  }

  SimdBitset256 operator&(const SimdBitset256 &other) const {
    return SimdBitset256(words[0] & other.words[0],
                         words[1] & other.words[1],
                         words[2] & other.words[2],
                         words[3] & other.words[3]);
  }

  SimdBitset256 operator^(const SimdBitset256 &other) const {
    return SimdBitset256(words[0] ^ other.words[0],
                         words[1] ^ other.words[1],
                         words[2] ^ other.words[2],
                         words[3] ^ other.words[3]);
  }

  bool operator==(const SimdBitset256 &other) const {
    return words[0] == other.words[0] &&
           words[1] == other.words[1] &&
           words[2] == other.words[2] &&
           words[3] == other.words[3];
  }

  bool operator!=(const SimdBitset256 &other) const {
    return !(*this == other);
  }
};

/**
 * Returns true if bitsets a and b have NO overlapping 1-bits ((a & b) == 0).
 */
inline bool is_disjoint_scalar(const SimdBitset256 &a, const SimdBitset256 &b) {
  return ((a.words[0] & b.words[0]) |
          (a.words[1] & b.words[1]) |
          (a.words[2] & b.words[2]) |
          (a.words[3] & b.words[3])) == 0;
}

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
#pragma GCC push_options
#pragma GCC target("avx2")
inline bool is_disjoint_avx2(const SimdBitset256 &a, const SimdBitset256 &b) {
  __m256i va = _mm256_load_si256(reinterpret_cast<const __m256i*>(a.words));
  __m256i vb = _mm256_load_si256(reinterpret_cast<const __m256i*>(b.words));
  return _mm256_testz_si256(va, vb) != 0;
}
#pragma GCC pop_options
#endif

#if defined(__aarch64__) || defined(__ARM_NEON)
inline bool is_disjoint_neon(const SimdBitset256 &a, const SimdBitset256 &b) {
  uint64x2_t va0 = vld1q_u64(&a.words[0]);
  uint64x2_t vb0 = vld1q_u64(&b.words[0]);
  uint64x2_t va1 = vld1q_u64(&a.words[2]);
  uint64x2_t vb1 = vld1q_u64(&b.words[2]);
  uint64x2_t c0 = vandq_u64(va0, vb0);
  uint64x2_t c1 = vandq_u64(va1, vb1);
  uint64x2_t c = vorrq_u64(c0, c1);
  return (vgetq_lane_u64(c, 0) | vgetq_lane_u64(c, 1)) == 0;
}
#endif

/**
 * High-performance hardware-vectorized exact cover solver for matrices with <= 256 columns.
 */
class SimdExactCover256 {
public:
  struct Row {
    SimdBitset256 mask;
    unsigned int node_id = 0;
    unsigned int piece_id = 0;
    std::vector<unsigned int> columns;
  };

  SimdExactCover256(unsigned int num_columns, unsigned int num_pieces);

  void setRequiredColumns(const SimdBitset256 &required);
  uint32_t addRow(unsigned int node_id, unsigned int piece_id, const std::vector<unsigned int> &cols);
  void registerNodeAlias(unsigned int node_id, uint32_t row_idx);

  using SolutionCallback = std::function<bool(const std::vector<unsigned int> &solution_nodes)>;

  void solve(
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const;

  void solveSubtree(
    const std::vector<unsigned int> &prefix_node_ids,
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const;

  unsigned int getNumRows() const { return rows.size(); }
  unsigned int getNumColumns() const { return num_columns; }
  unsigned int getNumPieces() const { return num_pieces; }

private:
  unsigned int num_columns;
  unsigned int num_pieces;
  SimdBitset256 required_columns;
  std::vector<Row> rows;
  std::vector<unsigned int> active_column_list;
  std::unordered_map<unsigned int, uint32_t> node_to_row_idx;
  bool use_avx2 = false;

  struct SearchContext {
    std::vector<std::vector<uint32_t>> scratch_active_rows;
    std::vector<unsigned int> current_solution;
    std::vector<uint32_t> col_counts;
    uint64_t local_iterations = 0;
  };

  void search(
    unsigned int depth,
    const SimdBitset256 &occupied,
    SearchContext &ctx,
    SolutionCallback &callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const;

  void filterRows(
    const std::vector<uint32_t> &src,
    const SimdBitset256 &chosen_mask,
    std::vector<uint32_t> &dst
  ) const;

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  void filterRowsAvx2(
    const std::vector<uint32_t> &src,
    const SimdBitset256 &chosen_mask,
    std::vector<uint32_t> &dst
  ) const;
#elif defined(__aarch64__) || defined(__ARM_NEON)
  void filterRowsNeon(
    const std::vector<uint32_t> &src,
    const SimdBitset256 &chosen_mask,
    std::vector<uint32_t> &dst
  ) const;
#endif
};

#endif // __SIMD_EXACT_COVER_H__
