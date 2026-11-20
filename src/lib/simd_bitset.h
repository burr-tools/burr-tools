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
#ifndef __SIMD_BITSET_H__
#define __SIMD_BITSET_H__

/* The fixed-width bitsets of the bit-parallel exact cover engine
 * (SimdHuangCover): one per matrix row, sized by the tier the matrix needs. */

#include "bt_assert.h"
#include <cstdint>
#include <cstddef>
#include <bit>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

/**
 * Bitset templated on N_WORDS (multiples of 4, i.e. 256-bit chunks),
 * aligned to 64 bytes for AVX2 vector operations.
 */
template <size_t N_WORDS>
struct alignas(64) SimdBitset {
  static_assert(N_WORDS > 0 && (N_WORDS % 4 == 0), "N_WORDS must be a positive multiple of 4");
  static constexpr size_t NUM_WORDS = N_WORDS;
  uint64_t words[N_WORDS];

  constexpr SimdBitset() : words{} {}

  template <typename... Args>
  constexpr SimdBitset(Args... args) : words{static_cast<uint64_t>(args)...} {}

  void set(unsigned int bit) {
    bt_assert(bit < N_WORDS * 64);
    words[bit >> 6] |= (1ULL << (bit & 63));
  }

  void reset(unsigned int bit) {
    bt_assert(bit < N_WORDS * 64);
    words[bit >> 6] &= ~(1ULL << (bit & 63));
  }

  bool test(unsigned int bit) const {
    bt_assert(bit < N_WORDS * 64);
    return (words[bit >> 6] & (1ULL << (bit & 63))) != 0;
  }

  void clear() {
    for (size_t i = 0; i < N_WORDS; ++i) words[i] = 0;
  }

  bool empty() const {
    uint64_t acc = 0;
    for (size_t i = 0; i < N_WORDS; ++i) acc |= words[i];
    return acc == 0;
  }

  bool containsAll(const SimdBitset &target) const {
    uint64_t diff = 0;
    for (size_t i = 0; i < N_WORDS; ++i) {
      diff |= (target.words[i] & ~words[i]);
    }
    return diff == 0;
  }

  /** number of bits set in mask but not here: popcount(mask & ~*this) */
  unsigned int countMissing(const SimdBitset &mask) const {
    unsigned int n = 0;
    for (size_t i = 0; i < N_WORDS; ++i) n += static_cast<unsigned int>(std::popcount(mask.words[i] & ~words[i]));
    return n;
  }

  SimdBitset operator|(const SimdBitset &other) const {
    SimdBitset res;
    for (size_t i = 0; i < N_WORDS; ++i) res.words[i] = words[i] | other.words[i];
    return res;
  }

  SimdBitset operator&(const SimdBitset &other) const {
    SimdBitset res;
    for (size_t i = 0; i < N_WORDS; ++i) res.words[i] = words[i] & other.words[i];
    return res;
  }

  SimdBitset operator^(const SimdBitset &other) const {
    SimdBitset res;
    for (size_t i = 0; i < N_WORDS; ++i) res.words[i] = words[i] ^ other.words[i];
    return res;
  }

  bool operator==(const SimdBitset &other) const {
    for (size_t i = 0; i < N_WORDS; ++i) {
      if (words[i] != other.words[i]) return false;
    }
    return true;
  }

  bool operator!=(const SimdBitset &other) const {
    return !(*this == other);
  }
};

using SimdBitset256 = SimdBitset<4>;
using SimdBitset512 = SimdBitset<8>;
using SimdBitset1024 = SimdBitset<16>;
using SimdBitset2048 = SimdBitset<32>;
using SimdBitset4096 = SimdBitset<64>;
using SimdBitset8192 = SimdBitset<128>;
using SimdBitset16384 = SimdBitset<256>;
using SimdBitset32768 = SimdBitset<512>;

/**
 * Returns true if bitsets a and b have NO overlapping 1-bits ((a & b) == 0).
 */
template <size_t N_WORDS>
inline bool is_disjoint_scalar(const SimdBitset<N_WORDS> &a, const SimdBitset<N_WORDS> &b) {
  for (size_t i = 0; i < N_WORDS; ++i) {
    if ((a.words[i] & b.words[i]) != 0) return false;
  }
  return true;
}

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
template <size_t N_WORDS>
__attribute__((target("avx2")))
inline bool is_disjoint_avx2(const SimdBitset<N_WORDS> &a, const SimdBitset<N_WORDS> &b) {
  constexpr size_t N_VEC = N_WORDS / 4;
  for (size_t i = 0; i < N_VEC; ++i) {
    __m256i va = _mm256_load_si256(reinterpret_cast<const __m256i*>(&a.words[i * 4]));
    __m256i vb = _mm256_load_si256(reinterpret_cast<const __m256i*>(&b.words[i * 4]));
    if (!_mm256_testz_si256(va, vb)) return false;
  }
  return true;
}
template <size_t N_WORDS>
__attribute__((target("avx512f")))
inline bool is_disjoint_avx512(const SimdBitset<N_WORDS> &a, const SimdBitset<N_WORDS> &b) {
  if constexpr (N_WORDS >= 8 && N_WORDS % 8 == 0) {
    constexpr size_t N_VEC = N_WORDS / 8;
    for (size_t i = 0; i < N_VEC; ++i) {
      __m512i va = _mm512_load_si512(reinterpret_cast<const void*>(&a.words[i * 8]));
      __m512i vb = _mm512_load_si512(reinterpret_cast<const void*>(&b.words[i * 8]));
      if (_mm512_test_epi64_mask(va, vb) != 0) return false;
    }
    return true;
  } else {
    return is_disjoint_avx2(a, b);
  }
}
#endif

#if defined(__aarch64__) || defined(__ARM_NEON)
template <size_t N_WORDS>
inline bool is_disjoint_neon(const SimdBitset<N_WORDS> &a, const SimdBitset<N_WORDS> &b) {
  constexpr size_t N_VEC = N_WORDS / 2;
  for (size_t i = 0; i < N_VEC; ++i) {
    uint64x2_t va = vld1q_u64(&a.words[i * 2]);
    uint64x2_t vb = vld1q_u64(&b.words[i * 2]);
    uint64x2_t c = vandq_u64(va, vb);
    if ((vgetq_lane_u64(c, 0) | vgetq_lane_u64(c, 1)) != 0) return false;
  }
  return true;
}
#endif

#endif // __SIMD_BITSET_H__
