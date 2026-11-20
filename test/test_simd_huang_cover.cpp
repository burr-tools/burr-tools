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
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include "src/lib/simd_bitset.h"

#include <vector>
#include <algorithm>
#include <set>
#include <thread>
#include <mutex>
#include <stop_token>
#include <string>
#include <cstdlib>

TEST_CASE("SimdBitset256 operations", "[simd][bitset]") {
  SimdBitset256 b;
  REQUIRE(b.empty());

  b.set(0);
  b.set(63);
  b.set(64);
  b.set(127);
  b.set(128);
  b.set(191);
  b.set(192);
  b.set(255);

  REQUIRE(b.test(0));
  REQUIRE(b.test(63));
  REQUIRE(b.test(64));
  REQUIRE(b.test(127));
  REQUIRE(b.test(128));
  REQUIRE(b.test(191));
  REQUIRE(b.test(192));
  REQUIRE(b.test(255));

  REQUIRE_FALSE(b.test(1));
  REQUIRE_FALSE(b.test(62));
  REQUIRE_FALSE(b.test(100));
  REQUIRE_FALSE(b.test(200));

  SimdBitset256 target;
  target.set(0);
  target.set(128);
  REQUIRE(b.containsAll(target));

  target.set(1);
  REQUIRE_FALSE(b.containsAll(target));

  SimdBitset256 disjoint_set;
  disjoint_set.set(1);
  disjoint_set.set(2);
  REQUIRE(is_disjoint_scalar(b, disjoint_set));

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (__builtin_cpu_supports("avx2")) {
    REQUIRE(is_disjoint_avx2(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx2(b, target));
  }
#endif
}

TEST_CASE("SimdBitset512 operations", "[simd][bitset]") {
  SimdBitset512 b;
  REQUIRE(b.empty());

  b.set(0);
  b.set(63);
  b.set(64);
  b.set(127);
  b.set(128);
  b.set(191);
  b.set(192);
  b.set(255);
  b.set(256);
  b.set(383);
  b.set(384);
  b.set(511);

  REQUIRE(b.test(0));
  REQUIRE(b.test(63));
  REQUIRE(b.test(64));
  REQUIRE(b.test(127));
  REQUIRE(b.test(128));
  REQUIRE(b.test(191));
  REQUIRE(b.test(192));
  REQUIRE(b.test(255));
  REQUIRE(b.test(256));
  REQUIRE(b.test(383));
  REQUIRE(b.test(384));
  REQUIRE(b.test(511));

  REQUIRE_FALSE(b.test(1));
  REQUIRE_FALSE(b.test(62));
  REQUIRE_FALSE(b.test(257));
  REQUIRE_FALSE(b.test(500));

  SimdBitset512 target;
  target.set(0);
  target.set(384);
  REQUIRE(b.containsAll(target));

  target.set(1);
  REQUIRE_FALSE(b.containsAll(target));

  SimdBitset512 disjoint_set;
  disjoint_set.set(1);
  disjoint_set.set(300);
  REQUIRE(is_disjoint_scalar(b, disjoint_set));

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (__builtin_cpu_supports("avx2")) {
    REQUIRE(is_disjoint_avx2(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx2(b, target));
  }
#endif
}

TEST_CASE("SimdBitset1024 operations", "[simd][bitset]") {
  SimdBitset1024 b;
  REQUIRE(b.empty());

  b.set(0);
  b.set(511);
  b.set(512);
  b.set(1023);

  REQUIRE(b.test(0));
  REQUIRE(b.test(511));
  REQUIRE(b.test(512));
  REQUIRE(b.test(1023));

  REQUIRE_FALSE(b.test(1));
  REQUIRE_FALSE(b.test(500));
  REQUIRE_FALSE(b.test(1000));

  SimdBitset1024 target;
  target.set(0);
  target.set(1023);
  REQUIRE(b.containsAll(target));

  target.set(1);
  REQUIRE_FALSE(b.containsAll(target));

  SimdBitset1024 disjoint_set;
  disjoint_set.set(1);
  disjoint_set.set(700);
  REQUIRE(is_disjoint_scalar(b, disjoint_set));

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (__builtin_cpu_supports("avx2")) {
    REQUIRE(is_disjoint_avx2(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx2(b, target));
  }
  if (__builtin_cpu_supports("avx512f")) {
    REQUIRE(is_disjoint_avx512(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx512(b, target));
  }
#endif
}

TEST_CASE("SimdBitset2048 operations", "[simd][bitset]") {
  SimdBitset2048 b;
  REQUIRE(b.empty());

  b.set(0);
  b.set(1023);
  b.set(1024);
  b.set(2047);

  REQUIRE(b.test(0));
  REQUIRE(b.test(1023));
  REQUIRE(b.test(1024));
  REQUIRE(b.test(2047));

  REQUIRE_FALSE(b.test(1));
  REQUIRE_FALSE(b.test(1000));
  REQUIRE_FALSE(b.test(2000));

  SimdBitset2048 target;
  target.set(0);
  target.set(2047);
  REQUIRE(b.containsAll(target));

  target.set(1);
  REQUIRE_FALSE(b.containsAll(target));

  SimdBitset2048 disjoint_set;
  disjoint_set.set(1);
  disjoint_set.set(1500);
  REQUIRE(is_disjoint_scalar(b, disjoint_set));

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (__builtin_cpu_supports("avx2")) {
    REQUIRE(is_disjoint_avx2(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx2(b, target));
  }
  if (__builtin_cpu_supports("avx512f")) {
    REQUIRE(is_disjoint_avx512(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx512(b, target));
  }
#endif
}

TEST_CASE("SimdBitset4096 and 8192 operations", "[simd][bitset]") {
  SimdBitset8192 b;
  REQUIRE(b.empty());

  b.set(0);
  b.set(4095);
  b.set(4096);
  b.set(8191);

  REQUIRE(b.test(0));
  REQUIRE(b.test(4095));
  REQUIRE(b.test(4096));
  REQUIRE(b.test(8191));

  REQUIRE_FALSE(b.test(1));
  REQUIRE_FALSE(b.test(2000));
  REQUIRE_FALSE(b.test(5832)); // 18^3 voxel index

  SimdBitset8192 target;
  target.set(0);
  target.set(8191);
  REQUIRE(b.containsAll(target));

  target.set(1);
  REQUIRE_FALSE(b.containsAll(target));

  SimdBitset8192 disjoint_set;
  disjoint_set.set(1);
  disjoint_set.set(5832);
  REQUIRE(is_disjoint_scalar(b, disjoint_set));

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (__builtin_cpu_supports("avx2")) {
    REQUIRE(is_disjoint_avx2(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx2(b, target));
  }
  if (__builtin_cpu_supports("avx512f")) {
    REQUIRE(is_disjoint_avx512(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx512(b, target));
  }
#endif
}

TEST_CASE("SimdBitset16384 and 32768 operations", "[simd][bitset]") {
  SimdBitset32768 b;
  REQUIRE(b.empty());

  b.set(0);
  b.set(16383);
  b.set(16384);
  b.set(32767);

  REQUIRE(b.test(0));
  REQUIRE(b.test(16383));
  REQUIRE(b.test(16384));
  REQUIRE(b.test(32767));

  REQUIRE_FALSE(b.test(1));
  REQUIRE_FALSE(b.test(10000));

  SimdBitset32768 target;
  target.set(0);
  target.set(32767);
  REQUIRE(b.containsAll(target));

  target.set(1);
  REQUIRE_FALSE(b.containsAll(target));

  SimdBitset32768 disjoint_set;
  disjoint_set.set(1);
  disjoint_set.set(10000);
  REQUIRE(is_disjoint_scalar(b, disjoint_set));

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (__builtin_cpu_supports("avx2")) {
    REQUIRE(is_disjoint_avx2(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx2(b, target));
  }
  if (__builtin_cpu_supports("avx512f")) {
    REQUIRE(is_disjoint_avx512(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx512(b, target));
  }
#endif
}

#include "src/lib/simd_huang_cover.h"

TEST_CASE("SimdHuangCover256 duplicate pieces exact cover", "[simd][huang]") {
  // Shape 1 (col 1): 2 pieces required
  // Shape 2 (col 2): 1 piece required
  // Voxels (cols 3..7): 5 voxels, 1 required each
  SimdHuangCover256 solver(7, 2, solverSettings_t{});

  // Column bounds
  solver.setColumnBounds(1, 2, 2, false, true, false, false); // Shape 1
  solver.setColumnBounds(2, 1, 1, false, true, false, false); // Shape 2
  for (unsigned int v = 3; v <= 7; v++) {
    solver.setColumnBounds(v, 1, 1, true, false, false, false); // Voxels 3..7
  }

  // Rows for Shape 1:
  // Row 1: Shape 1, voxels 3, 4
  solver.addRow(1, 0, 1, 0, 0, {1, 3, 4}, {1, 1, 1});
  // Row 2: Shape 1, voxels 5, 6
  solver.addRow(2, 0, 1, 1, 0, {1, 5, 6}, {1, 1, 1});
  // Row 3: Shape 1, voxels 6, 7
  solver.addRow(3, 0, 1, 2, 0, {1, 6, 7}, {1, 1, 1});

  // Rows for Shape 2:
  // Row 4: Shape 2, voxel 7
  solver.addRow(4, 1, 2, 0, 0, {2, 7}, {1, 1});
  // Row 5: Shape 2, voxel 5
  solver.addRow(5, 1, 2, 1, 0, {2, 5}, {1, 1});

  std::vector<std::vector<unsigned int>> solutions;
  std::stop_source stopSrc;
  std::atomic<uint64_t> iterations{0};

  solver.solve([&](const std::vector<unsigned int> &sol) {
    std::vector<unsigned int> sorted = sol;
    std::sort(sorted.begin(), sorted.end());
    solutions.push_back(sorted);
    return true;
  }, stopSrc.get_token(), iterations);

  // Two distinct valid solutions: {1, 2, 4} and {1, 3, 5}
  REQUIRE(solutions.size() == 2);
  std::set<std::vector<unsigned int>> sol_set(solutions.begin(), solutions.end());
  REQUIRE(sol_set.count(std::vector<unsigned int>{1, 2, 4}) == 1);
  REQUIRE(sol_set.count(std::vector<unsigned int>{1, 3, 5}) == 1);
  REQUIRE(iterations.load() > 0);
}

TEMPLATE_TEST_CASE("SimdHuangCover extended sizes duplicate pieces exact cover", "[simd][huang]",
                   SimdHuangCover512, SimdHuangCover1024, SimdHuangCover4096, SimdHuangCover32768) {
  unsigned int base_v = 280;
  TestType solver(base_v + 5, 2, solverSettings_t{});

  solver.setColumnBounds(1, 2, 2, false, true, false, false); // Shape 1
  solver.setColumnBounds(2, 1, 1, false, true, false, false); // Shape 2
  for (unsigned int v = base_v; v < base_v + 5; v++) {
    solver.setColumnBounds(v, 1, 1, true, false, false, false);
  }

  // Rows for Shape 1:
  // Row 1: Shape 1, voxels base_v, base_v + 1
  solver.addRow(1, 0, 1, 0, 0, {1, base_v, base_v + 1}, {1, 1, 1});
  // Row 2: Shape 1, voxels base_v + 2, base_v + 3
  solver.addRow(2, 0, 1, 1, 0, {1, base_v + 2, base_v + 3}, {1, 1, 1});
  // Row 3: Shape 1, voxels base_v + 3, base_v + 4
  solver.addRow(3, 0, 1, 2, 0, {1, base_v + 3, base_v + 4}, {1, 1, 1});

  // Rows for Shape 2:
  // Row 4: Shape 2, voxel base_v + 4
  solver.addRow(4, 1, 2, 0, 0, {2, base_v + 4}, {1, 1});
  // Row 5: Shape 2, voxel base_v + 2
  solver.addRow(5, 1, 2, 1, 0, {2, base_v + 2}, {1, 1});

  std::vector<std::vector<unsigned int>> solutions;
  std::stop_source stopSrc;
  std::atomic<uint64_t> iterations{0};

  solver.solve([&](const std::vector<unsigned int> &sol) {
    std::vector<unsigned int> sorted = sol;
    std::sort(sorted.begin(), sorted.end());
    solutions.push_back(sorted);
    return true;
  }, stopSrc.get_token(), iterations);

  REQUIRE(solutions.size() == 2);
  REQUIRE(solutions[0] == std::vector<unsigned int>{1, 2, 4});
  REQUIRE(solutions[1] == std::vector<unsigned int>{1, 3, 5});
  REQUIRE(iterations.load() > 0);

  // Also test parallelSolve
  std::atomic<unsigned long> p_iterations{0};
  std::atomic<size_t> total_tasks{0};
  std::atomic<size_t> completed_tasks{0};
  std::vector<std::vector<unsigned int>> p_solutions;
  std::mutex sol_mutex;

  solver.parallelSolve(
    4,
    [&](const std::vector<unsigned int> &sol) {
      std::lock_guard<std::mutex> lock(sol_mutex);
      std::vector<unsigned int> sorted = sol;
      std::sort(sorted.begin(), sorted.end());
      p_solutions.push_back(sorted);
      return true;
    },
    stopSrc,
    p_iterations,
    total_tasks,
    completed_tasks
  );
  std::sort(p_solutions.begin(), p_solutions.end());
  REQUIRE(p_solutions.size() == 2);
  REQUIRE(p_solutions[0] == std::vector<unsigned int>{1, 2, 4});
  REQUIRE(p_solutions[1] == std::vector<unsigned int>{1, 3, 5});
}


/* The vector setting must actually reach the kernel that runs.
 *
 * filterRows() tries AVX-512 before AVX2, so gating only use_avx2 on the
 * old BURRTOOLS_NO_AVX2 made that variable select *wider* SIMD on an
 * AVX-512 host instead of falling back, leaving the scalar loop unreachable
 * from tier 512 up. Every kernel returns the same solutions, so this cannot
 * be caught by comparing results -- it needs the dispatch decision itself.
 */
TEMPLATE_TEST_CASE("SimdHuangCover: the vector setting caps every kernel",
                   "[simd][huang][dispatch]",
                   SimdHuangCover256, SimdHuangCover512, SimdHuangCover1024,
                   SimdHuangCover4096, SimdHuangCover32768) {
  solverSettings_t settings;
  SECTION("scalar falls all the way back to scalar") {
    settings.vector = VECTOR_SCALAR;
    TestType solver(7, 2, settings);
    CHECK(std::string(solver.activeKernel()) == "scalar");
  }

  SECTION("avx2 does not leave AVX-512 enabled") {
    settings.vector = VECTOR_AVX2;
    TestType solver(7, 2, settings);
    CHECK(std::string(solver.activeKernel()) != "avx512");
  }

  SECTION("auto uses vectors where the CPU has them") {
    TestType solver(7, 2, settings);
    if (settings.avx2() || settings.neon())
      CHECK(std::string(solver.activeKernel()) != "scalar");
  }
}

/* Range-column pivots must not duplicate assemblies. A range pivot branches
 * over covering rows without the same-shape monotonic filter that shape
 * pivots use, so two compatible range rows were reported once per order
 * ({r1,r2} and {r2,r1}). The solver therefore never pivots on the range
 * column when there are no variable voxels (exact voxel cover already
 * forces the range weight); this fixture is built so the range column wins
 * MRV and would duplicate without that rule.
 */
TEMPLATE_TEST_CASE("SimdHuangCover range pivot reports each assembly once", "[simd][huang][range]",
                   SimdHuangCover256, SimdHuangCover512) {
  // col 1: shape, exactly 2 pieces. cols 2-5: voxels, required. col 6: range.
  TestType solver(6, 1, solverSettings_t{});
  solver.setColumnBounds(1, 2, 2, false, true, false, false);
  for (unsigned int v = 2; v <= 5; v++)
    solver.setColumnBounds(v, 1, 1, true, false, false, false);
  solver.setColumnBounds(6, 1, 100, false, false, true, false);

  // A and B each cover half the voxels and carry range weight; C-F are
  // range-free decoys arranged so every voxel is covered by 3 rows (MRV
  // metric 3) while only A and B cover range (metric 2 x need 1 = 2, wins).
  // Unique solution: {A, B} (rows 1, 2). Every other pair either conflicts
  // or leaves range below its minimum.
  solver.addRow(1, 0, 1, 0, 1, {1, 2, 3, 6}, {1, 1, 1, 1});
  solver.addRow(2, 0, 1, 1, 1, {1, 4, 5, 6}, {1, 1, 1, 1});
  solver.addRow(3, 0, 1, 2, 0, {1, 2, 4}, {1, 1, 1});
  solver.addRow(4, 0, 1, 3, 0, {1, 3, 5}, {1, 1, 1});
  solver.addRow(5, 0, 1, 4, 0, {1, 2, 5}, {1, 1, 1});
  solver.addRow(6, 0, 1, 5, 0, {1, 3, 4}, {1, 1, 1});

  std::vector<std::vector<unsigned int>> sols;
  std::stop_source stopSrc;
  std::atomic<uint64_t> iterations{0};
  solver.solve([&](const std::vector<unsigned int> & s) {
    std::vector<unsigned int> sorted = s;
    std::sort(sorted.begin(), sorted.end());
    sols.push_back(sorted);
    return true;
  }, stopSrc.get_token(), iterations);
  std::sort(sols.begin(), sols.end());

  REQUIRE(sols.size() == 1);
  CHECK(sols[0] == std::vector<unsigned int>{1, 2});
}

/* Hole limits. A hole column is a variable voxel: it may be left empty, but
 * only up to the puzzle's hole budget. Assembler 1 routes puzzles with a
 * restrictive budget through this solver, so it has to apply the budget the
 * way assembler_1_c::rec() does: count the hole columns left unfilled, and
 * reject a solution once that count exceeds the budget.
 */
TEMPLATE_TEST_CASE("SimdHuangCover applies the hole budget", "[simd][huang][holes]",
                   SimdHuangCover256, SimdHuangCover512) {

  // col 1: the shape, exactly one piece. cols 2,3: voxels that must be filled.
  // col 4: a variable voxel - it may stay empty, budget permitting.
  auto build = [](unsigned int budget) {
    auto solver = std::make_unique<TestType>(4, 1, solverSettings_t{});
    solver->setHoles(budget);
    solver->setColumnBounds(1, 1, 1, false, true, false, false);
    solver->setColumnBounds(2, 1, 1, true, false, false, false);
    solver->setColumnBounds(3, 1, 1, true, false, false, false);
    solver->setColumnBounds(4, 0, 1, true, false, false, true);

    // row 1 leaves the variable voxel empty, row 2 fills it
    solver->addRow(1, 0, 1, 0, 0, {1, 2, 3}, {1, 1, 1});
    solver->addRow(2, 0, 1, 1, 0, {1, 2, 3, 4}, {1, 1, 1, 1});
    return solver;
  };

  auto run = [](TestType & solver) {
    std::vector<std::vector<unsigned int>> sols;
    std::stop_source stopSrc;
    std::atomic<uint64_t> iterations{0};
    solver.solve([&](const std::vector<unsigned int> & s) {
      std::vector<unsigned int> sorted = s;
      std::sort(sorted.begin(), sorted.end());
      sols.push_back(sorted);
      return true;
    }, stopSrc.get_token(), iterations);
    std::sort(sols.begin(), sols.end());
    return sols;
  };

  SECTION("a budget of one hole admits both placements") {
    auto solver = build(1);
    auto sols = run(*solver);
    REQUIRE(sols.size() == 2);
    CHECK(sols[0] == std::vector<unsigned int>{1});
    CHECK(sols[1] == std::vector<unsigned int>{2});
  }

  SECTION("a budget of no holes admits only the placement that fills the voxel") {
    auto solver = build(0);
    auto sols = run(*solver);
    REQUIRE(sols.size() == 1);
    CHECK(sols[0] == std::vector<unsigned int>{2});
  }
}

TEST_CASE("SimdHuangCover: memory budget predicate follows the tier ladder", "[simd][huang][memory]") {
  // Ladder mapping: mask bytes per row at each tier.
  CHECK(huang_memory::tierMaskBytes(1) == 32);
  CHECK(huang_memory::tierMaskBytes(256) == 32);
  CHECK(huang_memory::tierMaskBytes(257) == 64);
  CHECK(huang_memory::tierMaskBytes(2048) == 256);
  CHECK(huang_memory::tierMaskBytes(2049) == 512);
  CHECK(huang_memory::tierMaskBytes(32768) == 4096);
  CHECK(huang_memory::tierMaskBytes(100000) == 4096);

  // Row sizes grow with the tier (measured: 192 / 384 / 4224 bytes at the
  // 256 / 2048 / 32768 tiers).
  CHECK(huang_memory::tierRowBytes(256) == sizeof(SimdHuangCover256::Row));
  CHECK(huang_memory::tierRowBytes(300) == sizeof(SimdHuangCover512::Row));
  CHECK(huang_memory::tierRowBytes(32768) == sizeof(SimdHuangCover32768::Row));

  // The motivating case from issue #92: 1e6 rows x 8 nodes at the 2048
  // tier costs ~1e6 x 384 + 8e6 x 48 = ~768 MB of rows plus alias payload,
  // where the mask-only estimate (256 MB exactly) would still fit.
  CHECK_FALSE(huang_memory::fitsMemoryBudget(2048, 1000000, 8000000, solverSettings_t{}.huangMemoryBytes));
  // ~1e5 rows at the top tier must refuse.
  CHECK_FALSE(huang_memory::fitsMemoryBudget(32768, 100000, 800000, solverSettings_t{}.huangMemoryBytes));
  // Small problems always fit, with margin for struct layout differences
  // across compilers (no exact-boundary asserts here).
  CHECK(huang_memory::fitsMemoryBudget(131, 10000, 80000, solverSettings_t{}.huangMemoryBytes));
  CHECK(huang_memory::fitsMemoryBudget(256, 100000, 500000, solverSettings_t{}.huangMemoryBytes));
  CHECK(huang_memory::fitsMemoryBudget(2048, 100000, 500000, solverSettings_t{}.huangMemoryBytes));
}

TEST_CASE("SimdHuangCover: parallelSolve does not count aborted tasks as completed", "[simd][huang][abort]") {
  SimdHuangCover256 solver(4, 2, solverSettings_t{});
  solver.setColumnBounds(1, 1, 1, false, true, false, false);
  solver.setColumnBounds(2, 1, 1, false, true, false, false);
  solver.setColumnBounds(3, 1, 1, true, false, false, false);
  solver.setColumnBounds(4, 1, 1, true, false, false, false);

  solver.addRow(1, 0, 1, 0, 0, {1, 3}, {1, 1});
  solver.addRow(2, 0, 1, 1, 0, {1, 4}, {1, 1});
  solver.addRow(3, 1, 2, 0, 0, {2, 4}, {1, 1});
  solver.addRow(4, 1, 2, 1, 0, {2, 3}, {1, 1});

  std::stop_source stopSrc;
  std::atomic<unsigned long> iterations{0};
  std::atomic<size_t> total_tasks{0};
  std::atomic<size_t> completed_tasks{0};

  // The matrix has two solutions, one per task, and each worker may reach
  // its task's solution before the other's stop lands. A consumer that took
  // both would have searched everything, and both tasks would rightly count
  // as completed. So this one consumes as the assemblers' do: delivery is
  // serialised, the first solution stops the run, and any later one is
  // declined -- its task then holds unexplored territory, whatever the
  // interleaving.
  std::mutex deliver;
  solver.parallelSolve(
    2,
    [&](const std::vector<unsigned int> &, bool & consumed) {
      std::lock_guard<std::mutex> lock(deliver);
      consumed = !stopSrc.stop_requested();
      stopSrc.request_stop();
      return false;
    },
    stopSrc,
    iterations,
    total_tasks,
    completed_tasks
  );

  CHECK(stopSrc.stop_requested());
  // A stopped run never reads as finished. Completion is decided by the
  // stop trace, not the token: the task that delivered the stopping
  // solution may have had nothing else left (its root is that leaf) and
  // then counts as completed -- but never the one the stop cut short.
  CHECK(completed_tasks.load() < total_tasks.load());
  CHECK(completed_tasks.load() <= TaskUnits::kWhole);  // counters are in units
}

/* Stopped Huang parallel runs salvage resumable prefixes (issue #111).
 *
 * Single worker on purpose: with one thread the schedule is fully
 * deterministic -- the reporting task is in flight and unstarted tasks
 * are still queued when stop fires, so salvage cannot come back empty
 * (unlike a wall-clock-dependent multithreaded stop). The two load-bearing
 * assertions: salvage is non-empty (fails if the mechanism is dropped and
 * the run silently degrades to full regenerate), and resume covers every
 * solution (fails if salvaged prefixes lose subtrees).
 */
TEST_CASE("SimdHuangCover: stopped parallelSolve salvages seeds for resume", "[simd][huang][resume]") {
  auto makeSolver = [] {
    // Shapes 1,2 (exactly 1 each), voxels 3,4 (required). Solutions:
    // {1, 3} and {2, 4}; {1, 4} and {2, 3} conflict on voxels.
    auto solver = std::make_unique<SimdHuangCover256>(4, 2, solverSettings_t{});
    solver->setColumnBounds(1, 1, 1, false, true, false, false);
    solver->setColumnBounds(2, 1, 1, false, true, false, false);
    solver->setColumnBounds(3, 1, 1, true, false, false, false);
    solver->setColumnBounds(4, 1, 1, true, false, false, false);
    solver->addRow(1, 0, 1, 0, 0, {1, 3}, {1, 1});
    solver->addRow(2, 0, 1, 1, 0, {1, 4}, {1, 1});
    solver->addRow(3, 1, 2, 0, 0, {2, 4}, {1, 1});
    solver->addRow(4, 1, 2, 1, 0, {2, 3}, {1, 1});
    return solver;
  };

  auto runCollect = [](SimdHuangCover256 & solver, std::stop_source & stopSrc,
                       const std::vector<HuangSeed> * seeds,
                       std::vector<HuangSeed> * salvaged) {
    std::vector<std::vector<unsigned int>> sols;
    std::atomic<unsigned long> iterations{0};
    std::atomic<size_t> total_tasks{0};
    std::atomic<size_t> completed_tasks{0};
    solver.parallelSolve(
      1,
      [&](const std::vector<unsigned int> & s) {
        std::vector<unsigned int> sorted = s;
        std::sort(sorted.begin(), sorted.end());
        sols.push_back(sorted);
        return true;
      },
      stopSrc, iterations, total_tasks, completed_tasks,
      nullptr, seeds, salvaged);
    std::sort(sols.begin(), sols.end());
    return sols;
  };

  // Baseline: uninterrupted run finds both solutions.
  std::vector<std::vector<unsigned int>> full;
  {
    auto solver = makeSolver();
    std::stop_source stopSrc;
    full = runCollect(*solver, stopSrc, nullptr, nullptr);
  }
  REQUIRE(full.size() == 2);

  // Stop after the first solution: salvage must be non-empty...
  auto solver = makeSolver();
  std::vector<std::vector<unsigned int>> first;
  std::vector<HuangSeed> salvaged;
  {
    std::stop_source stopSrc;
    std::atomic<unsigned long> iterations{0};
    std::atomic<size_t> total_tasks{0};
    std::atomic<size_t> completed_tasks{0};
    solver->parallelSolve(
      1,
      [&](const std::vector<unsigned int> & s, bool & consumed) {
        std::vector<unsigned int> sorted = s;
        std::sort(sorted.begin(), sorted.end());
        first.push_back(sorted);
        stopSrc.request_stop();
        consumed = true;
        return false;
      },
      stopSrc, iterations, total_tasks, completed_tasks,
      nullptr, nullptr, &salvaged);
  }
  REQUIRE(first.size() == 1);
  CHECK(!salvaged.empty());

  // Resume directly from all salvaged seeds: they hold only unexplored
  // territory, so first[0] is not reported again (no dedup needed).
  std::vector<std::vector<unsigned int>> rest;
  {
    std::stop_source stopSrc2;
    rest = runCollect(*solver, stopSrc2, &salvaged, nullptr);
  }
  REQUIRE(rest.size() == 1);
  CHECK(rest[0] != first[0]);
  std::vector<std::vector<unsigned int>> combined = {first[0], rest[0]};
  std::sort(combined.begin(), combined.end());
  CHECK(combined == full);
}

/* Stop before the first pop salvages the seeds verbatim (issue #111).
 *
 * Deterministic by construction: the stop source is already stopped when
 * parallelSolve runs, so no worker ever pops. The early return must hand
 * back exactly what the caller swapped out of pendingHuangPrefixes --
 * freshly built tasks replay precisely their seed, so salvaged == seeds.
 * Without the early-stop salvage this comes back empty and the next
 * continue falls back to full regeneration.
 */
TEST_CASE("SimdHuangCover: pre-stopped seeded parallelSolve salvages seeds", "[simd][huang][resume]") {
  auto solver = std::make_unique<SimdHuangCover256>(4, 2, solverSettings_t{});
  solver->setColumnBounds(1, 1, 1, false, true, false, false);
  solver->setColumnBounds(2, 1, 1, false, true, false, false);
  solver->setColumnBounds(3, 1, 1, true, false, false, false);
  solver->setColumnBounds(4, 1, 1, true, false, false, false);
  solver->addRow(1, 0, 1, 0, 0, {1, 3}, {1, 1});
  solver->addRow(2, 0, 1, 1, 0, {1, 4}, {1, 1});
  solver->addRow(3, 1, 2, 0, 0, {2, 4}, {1, 1});
  solver->addRow(4, 1, 2, 1, 0, {2, 3}, {1, 1});

  const std::vector<HuangSeed> seeds = {HuangSeed{.prefix = {1}, .pivotShape = {0}},
                                        HuangSeed{.prefix = {2}, .pivotShape = {0}}};
  std::vector<HuangSeed> salvaged;
  std::stop_source stopSrc;
  stopSrc.request_stop();          // stop before anything is popped
  std::atomic<unsigned long> iterations{0};
  std::atomic<size_t> total_tasks{0};
  std::atomic<size_t> completed_tasks{0};
  bool reported = false;
  solver->parallelSolve(
    1,
    [&](const std::vector<unsigned int> &) { reported = true; return false; },
    stopSrc, iterations, total_tasks, completed_tasks,
    nullptr, &seeds, &salvaged);
  CHECK(!reported);
  CHECK(salvaged == seeds);
}


/* A salvaged seed must replay to exactly the subtree its task covered.
 *
 * search() and generateTasks() apply the monotonic filter when they branch
 * on a shape column: after placing row r of a shape that still needs more
 * copies, later copies are restricted to higher shape_row_idx, so the
 * identical pieces {a, b} are enumerated once, not as (a, b) and (b, a).
 * A seed holds only the placed node ids; replaying it without that filter
 * reopens the lower-indexed rows and enumerates the permutations again in
 * a sibling seed's territory.
 *
 * Setup: one shape needed twice, 20 single-voxel placements into optional
 * voxels -- the shape column is the only pivot, so every branch is a shape
 * branch. 20 depth-1 tasks reach the generation target (16) without a
 * second level, so a pre-stopped run salvages exactly those 20 seeds. A
 * resume from them must report each 2-subset once: C(20, 2) = 190.
 */
TEST_CASE("SimdHuangCover: salvaged seeds replay the monotonic shape filter",
          "[simd][huang][resume]") {
  constexpr unsigned int N = 20;
  SimdHuangCover256 solver(N + 1, 1, solverSettings_t{});
  solver.setColumnBounds(1, 2, 2, false, true, false, false);
  for (unsigned int v = 2; v <= N + 1; v++)
    solver.setColumnBounds(v, 0, 1, true, false, false, false);
  for (unsigned int i = 0; i < N; i++)
    solver.addRow(i + 1, 0, 1, i, 0, {1, i + 2}, {1, 1});

  auto run = [&](std::stop_source & stopSrc, const std::vector<HuangSeed> * seeds,
                 std::vector<HuangSeed> * salvaged) {
    std::vector<std::vector<unsigned int>> sols;
    std::atomic<unsigned long> iterations{0};
    std::atomic<size_t> total_tasks{0};
    std::atomic<size_t> completed_tasks{0};
    solver.parallelSolve(
      1,
      [&](const std::vector<unsigned int> & s) {
        std::vector<unsigned int> sorted = s;
        std::sort(sorted.begin(), sorted.end());
        sols.push_back(sorted);
        return true;
      },
      stopSrc, iterations, total_tasks, completed_tasks,
      nullptr, seeds, salvaged);
    return sols;
  };

  {
    std::stop_source stopSrc;
    auto full = run(stopSrc, nullptr, nullptr);
    REQUIRE(full.size() == N * (N - 1) / 2);
  }

  std::vector<HuangSeed> salvaged;
  {
    std::stop_source stopSrc;
    stopSrc.request_stop();
    auto none = run(stopSrc, nullptr, &salvaged);
    REQUIRE(none.empty());
  }
  REQUIRE(salvaged.size() == N);

  std::stop_source stopSrc;
  auto resumed = run(stopSrc, &salvaged, nullptr);
  std::set<std::vector<unsigned int>> distinct(resumed.begin(), resumed.end());
  CHECK(distinct.size() == N * (N - 1) / 2);
  CHECK(resumed.size() == N * (N - 1) / 2);
}

/* Donated Huang rests replay the monotonic shape filter.
 *
 * The donation counterpart of the salvage test above: one shape needed six
 * times, ten single-voxel placements, so every branch is a shape branch
 * and each 6-subset must be reported once: C(10, 6) = 210. Generation
 * stops at depth 2 (45 tasks), and with a donation forced every node the
 * frames at depths 2 and 3 hand rests to the pool. A rest from depth 3
 * holds a step chosen by a live frame, whose pivot bit the donor has to
 * supply; with a wrong bit the replay drops the filter and the rest
 * re-enumerates permutations, reporting subsets twice.
 */
TEST_CASE("SimdHuangCover: donated rests replay the monotonic shape filter",
          "[simd][huang][parallel]") {
  constexpr unsigned int N = 10, K = 6;
  solverSettings_t settings;
  settings.donateEvery = 1;
  SimdHuangCover256 solver(N + 1, 1, settings);
  solver.setColumnBounds(1, K, K, false, true, false, false);
  for (unsigned int v = 2; v <= N + 1; v++)
    solver.setColumnBounds(v, 0, 1, true, false, false, false);
  for (unsigned int i = 0; i < N; i++)
    solver.addRow(i + 1, 0, 1, i, 0, {1, i + 2}, {1, 1});

  std::vector<std::vector<unsigned int>> sols;
  std::stop_source stopSrc;
  std::atomic<unsigned long> iterations{0}, donations{0};
  std::atomic<size_t> total_tasks{0}, completed_tasks{0};
  solver.parallelSolve(
    1,
    [&](const std::vector<unsigned int> & s) {
      std::vector<unsigned int> sorted = s;
      std::sort(sorted.begin(), sorted.end());
      sols.push_back(sorted);
      return true;
    },
    stopSrc, iterations, total_tasks, completed_tasks,
    nullptr, nullptr, nullptr, &donations);
  CHECK(donations.load() > 0);
  std::set<std::vector<unsigned int>> distinct(sols.begin(), sols.end());
  CHECK(distinct.size() == 210);
  CHECK(sols.size() == 210);
  CHECK(completed_tasks.load() == total_tasks.load());
}
