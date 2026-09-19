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

#include "bt_assert.h"
#include <cstdint>
#include <vector>
#include <functional>
#include <atomic>
#include <cstring>
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
    bt_assert(bit < 256);
    words[bit >> 6] |= (1ULL << (bit & 63));
  }

  void reset(unsigned int bit) {
    bt_assert(bit < 256);
    words[bit >> 6] &= ~(1ULL << (bit & 63));
  }

  bool test(unsigned int bit) const {
    bt_assert(bit < 256);
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
 * 512-bit bitset aligned to 64 bytes for vector operations.
 */
struct alignas(64) SimdBitset512 {
  uint64_t words[8];

  constexpr SimdBitset512() : words{0, 0, 0, 0, 0, 0, 0, 0} {}
  constexpr SimdBitset512(uint64_t w0, uint64_t w1, uint64_t w2, uint64_t w3,
                          uint64_t w4, uint64_t w5, uint64_t w6, uint64_t w7)
    : words{w0, w1, w2, w3, w4, w5, w6, w7} {}

  void set(unsigned int bit) {
    bt_assert(bit < 512);
    words[bit >> 6] |= (1ULL << (bit & 63));
  }

  void reset(unsigned int bit) {
    bt_assert(bit < 512);
    words[bit >> 6] &= ~(1ULL << (bit & 63));
  }

  bool test(unsigned int bit) const {
    bt_assert(bit < 512);
    return (words[bit >> 6] & (1ULL << (bit & 63))) != 0;
  }

  void clear() {
    for (int i = 0; i < 8; i++) words[i] = 0;
  }

  bool empty() const {
    return (words[0] | words[1] | words[2] | words[3] |
            words[4] | words[5] | words[6] | words[7]) == 0;
  }

  bool containsAll(const SimdBitset512 &target) const {
    return ((target.words[0] & ~words[0]) |
            (target.words[1] & ~words[1]) |
            (target.words[2] & ~words[2]) |
            (target.words[3] & ~words[3]) |
            (target.words[4] & ~words[4]) |
            (target.words[5] & ~words[5]) |
            (target.words[6] & ~words[6]) |
            (target.words[7] & ~words[7])) == 0;
  }

  SimdBitset512 operator|(const SimdBitset512 &other) const {
    return SimdBitset512(words[0] | other.words[0],
                         words[1] | other.words[1],
                         words[2] | other.words[2],
                         words[3] | other.words[3],
                         words[4] | other.words[4],
                         words[5] | other.words[5],
                         words[6] | other.words[6],
                         words[7] | other.words[7]);
  }

  SimdBitset512 operator&(const SimdBitset512 &other) const {
    return SimdBitset512(words[0] & other.words[0],
                         words[1] & other.words[1],
                         words[2] & other.words[2],
                         words[3] & other.words[3],
                         words[4] & other.words[4],
                         words[5] & other.words[5],
                         words[6] & other.words[6],
                         words[7] & other.words[7]);
  }

  SimdBitset512 operator^(const SimdBitset512 &other) const {
    return SimdBitset512(words[0] ^ other.words[0],
                         words[1] ^ other.words[1],
                         words[2] ^ other.words[2],
                         words[3] ^ other.words[3],
                         words[4] ^ other.words[4],
                         words[5] ^ other.words[5],
                         words[6] ^ other.words[6],
                         words[7] ^ other.words[7]);
  }

  bool operator==(const SimdBitset512 &other) const {
    for (int i = 0; i < 8; i++) {
      if (words[i] != other.words[i]) return false;
    }
    return true;
  }

  bool operator!=(const SimdBitset512 &other) const {
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

inline bool is_disjoint_scalar(const SimdBitset512 &a, const SimdBitset512 &b) {
  return ((a.words[0] & b.words[0]) |
          (a.words[1] & b.words[1]) |
          (a.words[2] & b.words[2]) |
          (a.words[3] & b.words[3]) |
          (a.words[4] & b.words[4]) |
          (a.words[5] & b.words[5]) |
          (a.words[6] & b.words[6]) |
          (a.words[7] & b.words[7])) == 0;
}

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
__attribute__((target("avx2")))
inline bool is_disjoint_avx2(const SimdBitset256 &a, const SimdBitset256 &b) {
  __m256i va = _mm256_load_si256(reinterpret_cast<const __m256i*>(a.words));
  __m256i vb = _mm256_load_si256(reinterpret_cast<const __m256i*>(b.words));
  return _mm256_testz_si256(va, vb) != 0;
}

__attribute__((target("avx2")))
inline bool is_disjoint_avx2(const SimdBitset512 &a, const SimdBitset512 &b) {
  __m256i va0 = _mm256_load_si256(reinterpret_cast<const __m256i*>(&a.words[0]));
  __m256i vb0 = _mm256_load_si256(reinterpret_cast<const __m256i*>(&b.words[0]));
  if (_mm256_testz_si256(va0, vb0) == 0) return false;
  __m256i va1 = _mm256_load_si256(reinterpret_cast<const __m256i*>(&a.words[4]));
  __m256i vb1 = _mm256_load_si256(reinterpret_cast<const __m256i*>(&b.words[4]));
  return _mm256_testz_si256(va1, vb1) != 0;
}
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

inline bool is_disjoint_neon(const SimdBitset512 &a, const SimdBitset512 &b) {
  uint64x2_t va0 = vld1q_u64(&a.words[0]);
  uint64x2_t vb0 = vld1q_u64(&b.words[0]);
  uint64x2_t va1 = vld1q_u64(&a.words[2]);
  uint64x2_t vb1 = vld1q_u64(&b.words[2]);
  uint64x2_t va2 = vld1q_u64(&a.words[4]);
  uint64x2_t vb2 = vld1q_u64(&b.words[4]);
  uint64x2_t va3 = vld1q_u64(&a.words[6]);
  uint64x2_t vb3 = vld1q_u64(&b.words[6]);
  uint64x2_t c0 = vandq_u64(va0, vb0);
  uint64x2_t c1 = vandq_u64(va1, vb1);
  uint64x2_t c2 = vandq_u64(va2, vb2);
  uint64x2_t c3 = vandq_u64(va3, vb3);
  uint64x2_t c = vorrq_u64(vorrq_u64(c0, c1), vorrq_u64(c2, c3));
  return (vgetq_lane_u64(c, 0) | vgetq_lane_u64(c, 1)) == 0;
}
#endif

/**
 * Interface for hardware-vectorized exact cover solvers.
 */
class ISimdExactCover {
public:
  virtual ~ISimdExactCover() = default;

  using SolutionCallback = std::function<bool(const std::vector<unsigned int> &solution_nodes)>;

  virtual void setRequiredColumn(unsigned int col) = 0;
  virtual uint32_t addRow(unsigned int node_id, unsigned int piece_id, const std::vector<unsigned int> &cols) = 0;
  virtual void registerNodeAlias(unsigned int node_id, uint32_t row_idx) = 0;

  virtual void solve(
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const = 0;

  virtual void solveSubtree(
    const std::vector<unsigned int> &prefix_node_ids,
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const = 0;

  virtual unsigned int getNumRows() const = 0;
  virtual unsigned int getNumColumns() const = 0;
  virtual unsigned int getNumPieces() const = 0;
};

/**
 * High-performance hardware-vectorized exact cover solver templated on BitsetType (256 or 512 bits).
 */
template <typename BitsetType>
class SimdExactCover : public ISimdExactCover {
public:
  struct Row {
    BitsetType mask;
    unsigned int node_id = 0;
    unsigned int piece_id = 0;
    std::vector<unsigned int> columns;
  };

  SimdExactCover(unsigned int num_columns, unsigned int num_pieces);

  void setRequiredColumn(unsigned int col) override;
  void setRequiredColumns(const BitsetType &required);
  uint32_t addRow(unsigned int node_id, unsigned int piece_id, const std::vector<unsigned int> &cols) override;
  void registerNodeAlias(unsigned int node_id, uint32_t row_idx) override;

  void solve(
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const override;

  void solveSubtree(
    const std::vector<unsigned int> &prefix_node_ids,
    SolutionCallback callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const override;

  unsigned int getNumRows() const override { return rows.size(); }
  unsigned int getNumColumns() const override { return num_columns; }
  unsigned int getNumPieces() const override { return num_pieces; }

private:
  unsigned int num_columns;
  unsigned int num_pieces;
  BitsetType required_columns;
  std::vector<Row> rows;
  std::vector<unsigned int> active_column_list;
  std::unordered_map<unsigned int, uint32_t> node_to_row_idx;
  bool use_avx2 = false;
#if defined(__aarch64__) || defined(__ARM_NEON)
  bool use_neon = true;
#endif

  struct SearchContext {
    std::vector<std::vector<uint32_t>> scratch_active_rows;
    std::vector<unsigned int> current_solution;
    std::vector<uint32_t> col_counts;
    uint64_t local_iterations = 0;
  };

  void search(
    unsigned int depth,
    const BitsetType &occupied,
    SearchContext &ctx,
    SolutionCallback &callback,
    const std::atomic<bool> &abort_flag,
    std::atomic<uint64_t> &iterations
  ) const;

  void filterRows(
    const std::vector<uint32_t> &src,
    const BitsetType &chosen_mask,
    std::vector<uint32_t> &dst
  ) const;

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  void filterRowsAvx2(
    const std::vector<uint32_t> &src,
    const BitsetType &chosen_mask,
    std::vector<uint32_t> &dst
  ) const;
#elif defined(__aarch64__) || defined(__ARM_NEON)
  void filterRowsNeon(
    const std::vector<uint32_t> &src,
    const BitsetType &chosen_mask,
    std::vector<uint32_t> &dst
  ) const;
#endif
};

using SimdExactCover256 = SimdExactCover<SimdBitset256>;
using SimdExactCover512 = SimdExactCover<SimdBitset512>;

#endif // __SIMD_EXACT_COVER_H__
