/* BurrTools: an open source tool for solving and designing burr puzzles
 *
 * Copyright (C) 2003-2026 Andreas Röver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __SIMD_CONFIG_H__
#define __SIMD_CONFIG_H__

#include <cstdlib>

/**
 * Centralized configuration for SIMD / vector acceleration and solver toggles.
 *
 * Semantic levels:
 * 1. Algorithmic Level (Bit-Parallel vs. Knuth DLX):
 *    - BURRTOOLS_NO_SIMD=1: Disables the bit-parallel solver in both assemblers,
 *      forcing fallback to classical Knuth Dancing Links (DLX).
 *
 * 2. Hardware Vector Level (Architecture-Agnostic):
 *    - BURRTOOLS_NO_VECTOR=1: Disables all vector instructions (AVX2, AVX-512, NEON) across
 *      the entire codebase, falling back to the portable 64-bit scalar word loop.
 *    - Backwards-compatible legacy switches:
 *      - BURRTOOLS_NO_AVX2=1 (on x86-64)
 *      - BURRTOOLS_NO_NEON=1 (on ARM)
 *      - BURRTOOLS_NO_SIMD=1 (also implies no vector acceleration in SIMD engines)
 *
 * 3. Granular Hardware Sub-ISA Overrides:
 *    - BURRTOOLS_NO_AVX512=1: Disables AVX-512 only, keeping AVX2 / NEON.
 *
  * 4. Disassembler Movement Analysis:
  *    - BURRTOOLS_NO_DISASM_SIMD=1: Disables vector Roy-Floyd-Warshall closure in disassembler.
  *      The planar closure and bitboard movement checks are always enabled;
  *      only the vector kernels stay toggleable for A/B benchmarking.
  */
namespace SimdConfig {

inline bool isBitParallelSolverEnabled() {
  return std::getenv("BURRTOOLS_NO_SIMD") == nullptr;
}

inline bool isVectorAccelerationEnabled() {
  if (std::getenv("BURRTOOLS_NO_VECTOR") != nullptr)
    return false;
  if (std::getenv("BURRTOOLS_NO_SIMD") != nullptr)
    return false;
#if defined(__x86_64__) || defined(_M_X64)
  if (std::getenv("BURRTOOLS_NO_AVX2") != nullptr)
    return false;
#elif defined(__aarch64__) || defined(__ARM_NEON)
  if (std::getenv("BURRTOOLS_NO_NEON") != nullptr)
    return false;
#endif
  return true;
}

inline bool isAvx512Allowed() {
  if (!isVectorAccelerationEnabled())
    return false;
  return std::getenv("BURRTOOLS_NO_AVX512") == nullptr;
}

inline bool isNeonAllowed() {
#if defined(__aarch64__) || defined(__ARM_NEON)
  return isVectorAccelerationEnabled();
#else
  return false;
#endif
}

inline bool isDisassemblerSimdEnabled() {
  if (std::getenv("BURRTOOLS_NO_DISASM_SIMD") != nullptr)
    return false;
  return isVectorAccelerationEnabled();
}

} // namespace SimdConfig

#endif // __SIMD_CONFIG_H__
