#include <catch2/catch_test_macros.hpp>

#include "lib/solver_config.h"
#include "test_helpers.h"

#include <stdexcept>
#include <string>

using bttest::ScopedEnv;

namespace {

/* every variable resolve() reads and the names 0.8 shipped, unset for the
 * scope, so that none from the shell reaches these tests */
struct CleanEnv {
  ScopedEnv vars[19] = {
    { "BURRTOOLS_THREADS", nullptr }, { "BURRTOOLS_BITPARALLEL", nullptr },
    { "BURRTOOLS_VECTOR", nullptr }, { "BURRTOOLS_DISASM_VECTOR", nullptr },
    { "BURRTOOLS_DISASM_POOL", nullptr }, { "BURRTOOLS_HUANG_MEM_MB", nullptr },
    { "BURRTOOLS_SPLIT_ORDER", nullptr }, { "BURRTOOLS_DONATE", nullptr },
    { "BURRTOOLS_THREAD_BUDGET", nullptr },
    { "BURRTOOLS_NO_SIMD", nullptr }, { "BURRTOOLS_NO_VECTOR", nullptr },
    { "BURRTOOLS_NO_AVX2", nullptr }, { "BURRTOOLS_NO_NEON", nullptr },
    { "BURRTOOLS_NO_AVX512", nullptr }, { "BURRTOOLS_NO_DISASM_SIMD", nullptr },
    { "BURRTOOLS_NO_DISASM_POOL", nullptr }, { "BURRTOOLS_NO_DONATE", nullptr },
    { "BURRTOOLS_SPLIT_BREADTH_FIRST", nullptr }, { "BURRTOOLS_NO_BUDGET", nullptr },
  };
};

std::string errorOf(const solverConfig_t & config) {
  try {
    config.resolve();
  } catch (const std::invalid_argument & e) {
    return e.what();
  }
  return "";
}

}

TEST_CASE("solver settings: defaults without a choice or an environment", "[config]") {
  CleanEnv clean;
  const solverSettings_t s = solverConfig_t{}.resolve();
  const solverSettings_t d;
  CHECK(s.threads == 0);
  CHECK(s.bitParallel);
  CHECK(s.vector == VECTOR_AUTO);
  CHECK(s.disassemblerVector);
  CHECK(s.disassemblerPool);
  CHECK(s.huangMemoryBytes == d.huangMemoryBytes);
  CHECK(s.splitFanout == SplitFanout::kDefault);
  CHECK_FALSE(s.splitBreadthFirst);
  CHECK(s.donate);
  CHECK(s.donateEvery == 0);
  CHECK(s.threadBudget);
  CHECK(s.rotation.amounts.size() == 9);
}

TEST_CASE("solver settings: the environment fills what the caller leaves unset", "[config]") {
  CleanEnv clean;
  ScopedEnv threads("BURRTOOLS_THREADS", "3"), bits("BURRTOOLS_BITPARALLEL", "0"),
            vec("BURRTOOLS_VECTOR", "scalar"), dvec("BURRTOOLS_DISASM_VECTOR", "0"),
            pool("BURRTOOLS_DISASM_POOL", "0"), mem("BURRTOOLS_HUANG_MEM_MB", "7"),
            fan("BURRTOOLS_SPLIT_FANOUT", "2"), order("BURRTOOLS_SPLIT_ORDER", "breadth"),
            donate("BURRTOOLS_DONATE", "0"), every("BURRTOOLS_DONATE_EVERY", "5"),
            budget("BURRTOOLS_THREAD_BUDGET", "0");

  const solverSettings_t s = solverConfig_t{}.resolve();
  CHECK(s.threads == 3);
  CHECK_FALSE(s.bitParallel);
  CHECK(s.vector == VECTOR_SCALAR);
  CHECK_FALSE(s.avx2());
  CHECK_FALSE(s.avx512());
  CHECK_FALSE(s.neon());
  CHECK_FALSE(s.disassemblerVector);
  CHECK_FALSE(s.disassemblerPool);
  CHECK(s.huangMemoryBytes == (uint64_t(7) << 20));
  /* the fan-out and the donation interval have no variable */
  CHECK(s.splitFanout == SplitFanout::kDefault);
  CHECK(s.splitBreadthFirst);
  CHECK_FALSE(s.donate);
  CHECK(s.donateEvery == 0);
  CHECK_FALSE(s.threadBudget);

  /* and the caller's choice wins over it */
  solverConfig_t chosen;
  chosen.threads = 1;
  chosen.bitParallel = true;
  chosen.vector = VECTOR_AUTO;
  chosen.disassemblerPool = true;
  chosen.huangMemoryMB = 0;
  chosen.splitFanout = 2;
  chosen.donate = true;
  chosen.donateEvery = 5;
  const solverSettings_t c = chosen.resolve();
  CHECK(c.threads == 1);
  CHECK(c.bitParallel);
  CHECK(c.vector == VECTOR_AUTO);
  CHECK(c.disassemblerPool);
  CHECK(c.huangMemoryBytes == 0);
  CHECK(c.splitFanout == 2);
  CHECK(c.donate);
  /* unchosen fields still come from the environment */
  CHECK_FALSE(c.disassemblerVector);
  CHECK(c.donateEvery == 5);
  CHECK(c.splitBreadthFirst);
}

TEST_CASE("solver settings: a malformed or renamed variable is an error", "[config]") {
  CleanEnv clean;
  for (auto [name, value] : { std::pair{ "BURRTOOLS_THREADS", "four" }, { "BURRTOOLS_THREADS", "-1" },
                              { "BURRTOOLS_BITPARALLEL", "yes" }, { "BURRTOOLS_VECTOR", "sse" },
                              { "BURRTOOLS_SPLIT_ORDER", "random" },
                              { "BURRTOOLS_DONATE", "2" }, { "BURRTOOLS_HUANG_MEM_MB", "1e3" },
                              { "BURRTOOLS_THREADS", " 5" }, { "BURRTOOLS_THREADS", "+5" },
                              { "BURRTOOLS_HUANG_MEM_MB", " -1" } }) {
    ScopedEnv bad(name, value);
    INFO(name << "=" << value);
    CHECK(errorOf(solverConfig_t{}).find(name) != std::string::npos);
  }

  /* a chosen value gets the same range check, named as the setting: the
   * variable need not be set */
  {
    solverConfig_t c;
    c.splitFanout = 0;
    CHECK(errorOf(c).find("split fan-out") != std::string::npos);
    solverConfig_t t;
    t.threads = solverConfig_t::maxThreads + 1;
    CHECK(errorOf(t).find("thread count") != std::string::npos);
    CHECK(errorOf(t).find("BURRTOOLS_THREADS") == std::string::npos);
  }

  /* an old name says what replaced it, instead of measuring the default */
  {
    ScopedEnv old("BURRTOOLS_NO_SIMD", "1");
    CHECK(errorOf(solverConfig_t{}).find("BURRTOOLS_BITPARALLEL=0") != std::string::npos);
  }
  /* even empty: 0.8 only asked whether it was there (Windows has no empty
   * variables: setting one to "" removes it) */
#ifndef _WIN32
  {
    ScopedEnv old("BURRTOOLS_NO_DISASM_POOL", "");
    CHECK(errorOf(solverConfig_t{}).find("BURRTOOLS_DISASM_POOL=0") != std::string::npos);
  }
#endif
}

TEST_CASE("solver settings: a vector cap this CPU doesn't have is an error", "[config]") {
  CleanEnv clean;
  const solverSettings_t a;
  /* NEON exists only on ARM, AVX2 and AVX-512 only on x86 */
  const vectorIsa_e missing = a.neon() ? VECTOR_AVX2 : VECTOR_NEON;
  solverConfig_t c;
  c.vector = missing;
  CHECK(errorOf(c).find("BURRTOOLS_VECTOR") != std::string::npos);
  ScopedEnv v("BURRTOOLS_VECTOR", missing == VECTOR_NEON ? "neon" : "avx2");
  CHECK(errorOf(solverConfig_t{}).find("BURRTOOLS_VECTOR") != std::string::npos);
  /* scalar and auto exist everywhere */
  ScopedEnv s("BURRTOOLS_VECTOR", "scalar");
  CHECK(errorOf(solverConfig_t{}).empty());
}

TEST_CASE("solver settings: the vector setting caps the kernels", "[config]") {
  solverSettings_t s;
  s.vector = VECTOR_SCALAR;
  CHECK_FALSE(s.avx2());
  CHECK_FALSE(s.avx512());
  CHECK_FALSE(s.neon());

  s.vector = VECTOR_AVX2;
  CHECK_FALSE(s.avx512());

  /* auto is what the hardware has: at least what any cap allows */
  solverSettings_t a;
  for (vectorIsa_e cap : { VECTOR_SCALAR, VECTOR_AVX2, VECTOR_AVX512, VECTOR_NEON }) {
    s.vector = cap;
    CHECK((!s.avx2() || a.avx2()));
    CHECK((!s.avx512() || a.avx512()));
    CHECK((!s.neon() || a.neon()));
  }
}
