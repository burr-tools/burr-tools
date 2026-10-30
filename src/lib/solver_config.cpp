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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "solver_config.h"

#include <cerrno>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

/* the only place the solver's environment variables are read */
std::optional<std::string> readEnv(const char * name) {
  const char * v = std::getenv(name);
  if (!v || !*v)
    return std::nullopt;
  return std::string(v);
}

/* invalid_argument, not bt_te: the message is built here, and
 * assert_exception only keeps a pointer to it */
[[noreturn]] void bad(const char * name, const std::string & expected) {
  throw std::invalid_argument(std::string(name) + " must be " + expected);
}

/* each one: the explicit value, else the environment, else `fallback` */

bool flag(const char * name, const std::optional<bool> & chosen, bool fallback) {
  if (chosen)
    return *chosen;
  if (auto v = readEnv(name)) {
    if (*v == "0")
      return false;
    if (*v == "1")
      return true;
    bad(name, "0 or 1");
  }
  return fallback;
}

unsigned long long number(const char * name, const std::optional<unsigned long long> & chosen,
                          unsigned long long fallback, unsigned long long lo, unsigned long long hi) {
  if (chosen) {
    if (*chosen < lo || *chosen > hi)
      throw std::invalid_argument(std::string("the setting of ") + name + " must be a number from " +
                                  std::to_string(lo) + " to " + std::to_string(hi));
    return *chosen;
  }
  if (auto v = readEnv(name)) {
    char * end = nullptr;
    errno = 0;
    const unsigned long long n = std::strtoull(v->c_str(), &end, 10);
    if (v->front() == '-' || *end != '\0' || errno || n < lo || n > hi)
      bad(name, "a number from " + std::to_string(lo) + " to " + std::to_string(hi));
    return n;
  }
  return fallback;
}

template <class T>
std::optional<unsigned long long> widen(const std::optional<T> & v) {
  if (!v)
    return std::nullopt;
  return (unsigned long long)*v;
}

/* x86 feature detection, where it exists; everything else answers false */
bool cpuHas(const char * feature) {
#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (std::string(feature) == "avx2")
    return __builtin_cpu_supports("avx2");
  if (std::string(feature) == "avx512f")
    return __builtin_cpu_supports("avx512f");
#endif
  (void)feature;
  return false;
}

}

bool solverSettings_t::avx2(void) const {
  return (vector == VECTOR_AUTO || vector == VECTOR_AVX2 || vector == VECTOR_AVX512) && cpuHas("avx2");
}

bool solverSettings_t::avx512(void) const {
  return (vector == VECTOR_AUTO || vector == VECTOR_AVX512) && cpuHas("avx512f");
}

bool solverSettings_t::neon(void) const {
#if defined(__aarch64__) || defined(__ARM_NEON)
  return vector == VECTOR_AUTO || vector == VECTOR_NEON;
#else
  return false;
#endif
}

rotationSettings_t rotationConfig_t::resolve(void) const {

  rotationFamily_e fam = ROTATION_FAMILY_WIDE;
  if (family)
    fam = *family;
  else if (auto v = readEnv("BURRTOOLS_ROT_FAMILY")) {
    if (*v == "compass") fam = ROTATION_FAMILY_COMPASS;
    else if (*v == "wide") fam = ROTATION_FAMILY_WIDE;
    else bad("BURRTOOLS_ROT_FAMILY", "compass or wide");
  }

  /* the defaults are the initializers; choices and the environment on top */
  rotationSettings_t s;
  if (fam == ROTATION_FAMILY_COMPASS)
    s.wideAmounts.clear();
  s.planner = flag("BURRTOOLS_ROT_PLANNER", planner, s.planner);
  if (slack)
    s.slack = *slack;
  return s;
}

/* Names the toggles had before 2026-10-28: setting one is an error rather
 * than silently measuring the default. */
static void rejectRenamed(void) {
  static const char * const renamed[][2] = {
    { "BURRTOOLS_NO_SIMD", "BURRTOOLS_BITPARALLEL=0" },
    { "BURRTOOLS_NO_VECTOR", "BURRTOOLS_VECTOR=scalar" },
    { "BURRTOOLS_NO_AVX2", "BURRTOOLS_VECTOR=scalar" },
    { "BURRTOOLS_NO_NEON", "BURRTOOLS_VECTOR=scalar" },
    { "BURRTOOLS_NO_AVX512", "BURRTOOLS_VECTOR=avx2" },
    { "BURRTOOLS_NO_DISASM_SIMD", "BURRTOOLS_DISASM_VECTOR=0" },
    { "BURRTOOLS_NO_DISASM_POOL", "BURRTOOLS_DISASM_POOL=0" },
    { "BURRTOOLS_NO_DONATE", "BURRTOOLS_DONATE=0" },
    { "BURRTOOLS_SPLIT_BREADTH_FIRST", "BURRTOOLS_SPLIT_ORDER=breadth" },
    { "BURRTOOLS_NO_BUDGET", "BURRTOOLS_THREAD_BUDGET=0" },
    { "BURRTOOLS_NO_ROT_PLANNER", "BURRTOOLS_ROT_PLANNER=0" },
  };
  for (const auto & r : renamed)
    if (readEnv(r[0]))
      throw std::invalid_argument(std::string(r[0]) + " was renamed: use " + r[1]);
}

solverSettings_t solverConfig_t::resolve(void) const {

  rejectRenamed();
  solverSettings_t s;

  s.threads = (unsigned int)number("BURRTOOLS_THREADS", widen(threads), s.threads, 0, 4096);
  s.bitParallel = flag("BURRTOOLS_BITPARALLEL", bitParallel, s.bitParallel);

  if (vector)
    s.vector = *vector;
  else if (auto v = readEnv("BURRTOOLS_VECTOR")) {
    if (*v == "auto") s.vector = VECTOR_AUTO;
    else if (*v == "scalar") s.vector = VECTOR_SCALAR;
    else if (*v == "avx2") s.vector = VECTOR_AVX2;
    else if (*v == "avx512") s.vector = VECTOR_AVX512;
    else if (*v == "neon") s.vector = VECTOR_NEON;
    else bad("BURRTOOLS_VECTOR", "auto, scalar, avx2, avx512 or neon");
  }

  s.disassemblerVector = flag("BURRTOOLS_DISASM_VECTOR", disassemblerVector, s.disassemblerVector);
  s.disassemblerPool = flag("BURRTOOLS_DISASM_POOL", disassemblerPool, s.disassemblerPool);
  s.huangMemoryBytes = number("BURRTOOLS_HUANG_MEM_MB", widen(huangMemoryMB), s.huangMemoryBytes >> 20, 0, uint64_t(1) << 40) << 20;
  s.splitFanout = (unsigned int)number("BURRTOOLS_SPLIT_FANOUT", widen(splitFanout), s.splitFanout, 1, 4096);

  if (splitBreadthFirst)
    s.splitBreadthFirst = *splitBreadthFirst;
  else if (auto v = readEnv("BURRTOOLS_SPLIT_ORDER")) {
    if (*v != "depth" && *v != "breadth")
      bad("BURRTOOLS_SPLIT_ORDER", "depth or breadth");
    s.splitBreadthFirst = (*v == "breadth");
  }

  s.donate = flag("BURRTOOLS_DONATE", donate, s.donate);
  s.donateEvery = (unsigned long)number("BURRTOOLS_DONATE_EVERY", widen(donateEvery), s.donateEvery, 0, ~0ul);
  s.threadBudget = flag("BURRTOOLS_THREAD_BUDGET", threadBudget, s.threadBudget);
  s.rotation = rotation.resolve();
  return s;
}
