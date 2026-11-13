#include <catch2/catch_test_macros.hpp>

#include "lib/solver_config.h"
#include "test_helpers.h"

#include <stdexcept>
#include <string>

using bttest::ScopedEnv;

namespace {

/* every variable resolve() reads, unset for the scope */
struct CleanEnv {
  ScopedEnv vars[13] = {
    { "BURRTOOLS_THREADS", nullptr }, { "BURRTOOLS_BITPARALLEL", nullptr },
    { "BURRTOOLS_VECTOR", nullptr }, { "BURRTOOLS_DISASM_VECTOR", nullptr },
    { "BURRTOOLS_DISASM_POOL", nullptr }, { "BURRTOOLS_HUANG_MEM_MB", nullptr },
    { "BURRTOOLS_SPLIT_FANOUT", nullptr }, { "BURRTOOLS_SPLIT_ORDER", nullptr },
    { "BURRTOOLS_DONATE", nullptr }, { "BURRTOOLS_DONATE_EVERY", nullptr },
    { "BURRTOOLS_THREAD_BUDGET", nullptr }, { "BURRTOOLS_ROT_MODE", nullptr },
    { "BURRTOOLS_ROT_STATS", nullptr },
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
  CHECK(s.rotation.mode == ROTATION_BALANCED);
}

TEST_CASE("solver settings: the environment fills what the caller leaves unset", "[config]") {
  CleanEnv clean;
  ScopedEnv threads("BURRTOOLS_THREADS", "3"), bits("BURRTOOLS_BITPARALLEL", "0"),
            vec("BURRTOOLS_VECTOR", "scalar"), dvec("BURRTOOLS_DISASM_VECTOR", "0"),
            pool("BURRTOOLS_DISASM_POOL", "0"), mem("BURRTOOLS_HUANG_MEM_MB", "7"),
            fan("BURRTOOLS_SPLIT_FANOUT", "2"), order("BURRTOOLS_SPLIT_ORDER", "breadth"),
            donate("BURRTOOLS_DONATE", "0"), every("BURRTOOLS_DONATE_EVERY", "5"),
            budget("BURRTOOLS_THREAD_BUDGET", "0"), mode("BURRTOOLS_ROT_MODE", "slow");

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
  CHECK(s.splitFanout == 2);
  CHECK(s.splitBreadthFirst);
  CHECK_FALSE(s.donate);
  CHECK(s.donateEvery == 5);
  CHECK_FALSE(s.threadBudget);
  CHECK(s.rotation.mode == ROTATION_SLOW);

  /* and the caller's choice wins over it */
  solverConfig_t chosen;
  chosen.threads = 1;
  chosen.bitParallel = true;
  chosen.vector = VECTOR_AUTO;
  chosen.disassemblerPool = true;
  chosen.huangMemoryMB = 0;
  chosen.splitFanout = 16;
  chosen.donate = true;
  chosen.rotation.mode = ROTATION_FAST;
  const solverSettings_t c = chosen.resolve();
  CHECK(c.threads == 1);
  CHECK(c.bitParallel);
  CHECK(c.vector == VECTOR_AUTO);
  CHECK(c.disassemblerPool);
  CHECK(c.huangMemoryBytes == 0);
  CHECK(c.splitFanout == 16);
  CHECK(c.donate);
  CHECK(c.rotation.mode == ROTATION_FAST);
  /* unchosen fields still come from the environment */
  CHECK_FALSE(c.disassemblerVector);
  CHECK(c.donateEvery == 5);
}

TEST_CASE("solver settings: a malformed or renamed variable is an error", "[config]") {
  CleanEnv clean;
  for (auto [name, value] : { std::pair{ "BURRTOOLS_THREADS", "four" }, { "BURRTOOLS_THREADS", "-1" },
                              { "BURRTOOLS_BITPARALLEL", "yes" }, { "BURRTOOLS_VECTOR", "sse" },
                              { "BURRTOOLS_SPLIT_FANOUT", "0" }, { "BURRTOOLS_SPLIT_ORDER", "random" },
                              { "BURRTOOLS_DONATE", "2" }, { "BURRTOOLS_HUANG_MEM_MB", "1e3" },
                              { "BURRTOOLS_THREADS", " 5" }, { "BURRTOOLS_THREADS", "+5" },
                              { "BURRTOOLS_DONATE_EVERY", " -1" } }) {
    ScopedEnv bad(name, value);
    INFO(name << "=" << value);
    CHECK(errorOf(solverConfig_t{}).find(name) != std::string::npos);
  }

  /* a chosen value gets the same range check */
  {
    solverConfig_t c;
    c.splitFanout = 0;
    CHECK(errorOf(c).find("BURRTOOLS_SPLIT_FANOUT") != std::string::npos);
  }

  /* an old name says what replaced it, instead of measuring the default */
  ScopedEnv old("BURRTOOLS_NO_SIMD", "1");
  CHECK(errorOf(solverConfig_t{}).find("BURRTOOLS_BITPARALLEL=0") != std::string::npos);
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

TEST_CASE("rotation settings: caller, then environment, then defaults", "[config]") {
  CleanEnv clean;

  /* the defaults are rotationSettings_t's initializers: the wide family,
   * balanced */
  rotationSettings_t s = rotationConfig_t{}.resolve();
  const rotationSettings_t d;
  CHECK(s.model.play == d.model.play);
  CHECK(s.amounts == d.amounts);
  CHECK(s.wideAmounts == WIDE_AMOUNTS);
  CHECK(s.mode == ROTATION_BALANCED);
  CHECK(rotationFamily(s).size() == 881);

  /* the environment where the caller leaves the mode unset */
  for (rotationMode_e m : { ROTATION_OFF, ROTATION_FAST, ROTATION_BALANCED, ROTATION_SLOW }) {
    ScopedEnv mode("BURRTOOLS_ROT_MODE", rotationModeName(m));
    INFO(rotationModeName(m));
    CHECK(rotationConfig_t{}.resolve().mode == m);

    /* the caller wins */
    rotationConfig_t chosen;
    chosen.mode = (m == ROTATION_FAST) ? ROTATION_SLOW : ROTATION_FAST;
    CHECK(chosen.resolve().mode == *chosen.mode);
  }
}

TEST_CASE("rotation settings: the names of the modes", "[config]") {
  for (rotationMode_e m : { ROTATION_OFF, ROTATION_FAST, ROTATION_BALANCED, ROTATION_SLOW })
    CHECK(rotationModeFromName(rotationModeName(m)) == m);
  for (const char * name : { "", "Fast", "on", "1", "balanced " })
    CHECK_FALSE(rotationModeFromName(name));
}

TEST_CASE("rotation settings: balanced gates at play + max(play, 0.04)", "[config]") {
  using rotationGeometry::model_t;
  CHECK(rotationGatePlay(model_t{ 0.002, 0.03 }) == 0.002 + 0.04);
  CHECK(rotationGatePlay(model_t{ 0.04, 0 }) == 0.08);
  CHECK(rotationGatePlay(model_t{ 0.081, 0.032 }) == 0.081 + 0.081);
  /* never so much play that no cube is left inside the bevel */
  CHECK(rotationGatePlay(model_t{ 0.3, 0.3 }) < 1 - 2 * 0.3);
  CHECK(rotationGatePlay(model_t{ 0.3, 0.3 }) >= 0.3);
}

TEST_CASE("rotation settings: a malformed mode is an error", "[config]") {
  CleanEnv clean;
  for (const char * value : { "Balanced", "on", "1", "" }) {
    if (!*value)
      continue;   // empty is unset
    ScopedEnv bad("BURRTOOLS_ROT_MODE", value);
    INFO(value);
    CHECK(errorOf(solverConfig_t{}).find("BURRTOOLS_ROT_MODE") != std::string::npos);
  }
}
