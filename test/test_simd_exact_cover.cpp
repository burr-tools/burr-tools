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
#include "src/lib/simd_exact_cover.h"

#include <vector>
#include <algorithm>
#include <set>
#include <thread>
#include <mutex>

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

TEST_CASE("SimdExactCover1024 and 2048 solve", "[simd][exact_cover]") {
  for (unsigned int offset : {600u, 1200u}) {
    std::unique_ptr<ISimdExactCover> solver;
    if (offset < 1000) {
      solver = std::make_unique<SimdExactCover1024>(offset + 7, 3);
    } else {
      solver = std::make_unique<SimdExactCover2048>(offset + 7, 3);
    }

    for (unsigned int i = 0; i < 7; i++) {
      solver->setRequiredColumn(offset + i);
    }

    solver->addRow(1, 0, {offset + 2, offset + 4});
    solver->addRow(2, 1, {offset + 0, offset + 3, offset + 6});
    solver->addRow(3, 2, {offset + 1, offset + 2, offset + 5});
    solver->addRow(4, 1, {offset + 0, offset + 3, offset + 5});
    solver->addRow(5, 2, {offset + 1, offset + 6});
    solver->addRow(6, 1, {offset + 3, offset + 4, offset + 6});

    std::vector<std::vector<unsigned int>> solutions;
    std::atomic<bool> abort_flag{false};
    std::atomic<uint64_t> iterations{0};

    solver->solve([&](const std::vector<unsigned int> &sol) {
      std::vector<unsigned int> sorted = sol;
      std::sort(sorted.begin(), sorted.end());
      solutions.push_back(sorted);
      return true;
    }, abort_flag, iterations);

    REQUIRE(solutions.size() == 1);
    REQUIRE(solutions[0] == std::vector<unsigned int>{1, 4, 5});
    REQUIRE(iterations.load() > 0);
  }
}

TEST_CASE("SimdExactCover256 Knuth textbook example", "[simd][exact_cover]") {
  // Knuth's exact cover problem from TAOCP:
  // Items 0..6: {A, B, C, D, E, F, G}
  // Row 1: {C, E}       -> {2, 4}
  // Row 2: {A, D, G}    -> {0, 3, 6}
  // Row 3: {B, C, F}    -> {1, 2, 5}
  // Row 4: {A, D}       -> {0, 3}
  // Row 5: {B, G}       -> {1, 6}
  // Row 6: {D, E, G}    -> {3, 4, 6}
  //
  // Unique solution: Row 1, 4, 5 ({C, E}, {A, D}, {B, G})

  SimdExactCover256 solver(7, 3);

  SimdBitset256 req;
  for (unsigned int i = 0; i < 7; i++) req.set(i);
  solver.setRequiredColumns(req);

  solver.addRow(1, 0, {2, 4});
  solver.addRow(2, 1, {0, 3, 6});
  solver.addRow(3, 2, {1, 2, 5});
  solver.addRow(4, 1, {0, 3, 5});
  solver.addRow(5, 2, {1, 6});
  solver.addRow(6, 1, {3, 4, 6});

  std::vector<std::vector<unsigned int>> solutions;
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};

  solver.solve([&](const std::vector<unsigned int> &sol) {
    std::vector<unsigned int> sorted = sol;
    std::sort(sorted.begin(), sorted.end());
    solutions.push_back(sorted);
    return true;
  }, abort_flag, iterations);

  REQUIRE(solutions.size() == 1);
  REQUIRE(solutions[0] == std::vector<unsigned int>{1, 4, 5});
  REQUIRE(iterations.load() > 0);
}

TEST_CASE("SimdExactCover256 unsolvable problem", "[simd][exact_cover]") {
  SimdExactCover256 solver(4, 2);

  SimdBitset256 req;
  for (unsigned int i = 0; i < 4; i++) req.set(i);
  solver.setRequiredColumns(req);

  // Column 3 is never covered
  solver.addRow(1, 0, {0, 1});
  solver.addRow(2, 1, {1, 2});

  std::vector<std::vector<unsigned int>> solutions;
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};

  solver.solve([&](const std::vector<unsigned int> &sol) {
    solutions.push_back(sol);
    return true;
  }, abort_flag, iterations);

  REQUIRE(solutions.empty());
}

TEST_CASE("SimdExactCover256 abort flag", "[simd][exact_cover]") {
  SimdExactCover256 solver(3, 3);

  SimdBitset256 req;
  for (unsigned int i = 0; i < 3; i++) req.set(i);
  solver.setRequiredColumns(req);

  solver.addRow(1, 0, {0});
  solver.addRow(2, 1, {1});
  solver.addRow(3, 2, {2});

  std::atomic<bool> abort_flag{true};
  std::atomic<uint64_t> iterations{0};
  std::vector<std::vector<unsigned int>> solutions;

  solver.solve([&](const std::vector<unsigned int> &sol) {
    solutions.push_back(sol);
    return true;
  }, abort_flag, iterations);

  REQUIRE(solutions.empty());
}

TEST_CASE("SimdExactCover512 large column cover", "[simd][exact_cover]") {
  // Problem with columns up to 400 (spanning across >256 bit boundary):
  // 3 pieces, required columns: {50, 150, 270, 350, 400}
  // Row 1: piece 0 -> {50, 270}
  // Row 2: piece 1 -> {150}
  // Row 3: piece 2 -> {350, 400}
  // Row 4: piece 0 -> {50, 150} (conflict with row 2)

  SimdExactCover512 solver(401, 3);
  SimdBitset512 req;
  req.set(50);
  req.set(150);
  req.set(270);
  req.set(350);
  req.set(400);
  solver.setRequiredColumns(req);

  solver.addRow(1, 0, {50, 270});
  solver.addRow(2, 1, {150});
  solver.addRow(3, 2, {350, 400});
  solver.addRow(4, 0, {50, 150});

  std::vector<std::vector<unsigned int>> solutions;
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};

  solver.solve([&](const std::vector<unsigned int> &sol) {
    std::vector<unsigned int> sorted = sol;
    std::sort(sorted.begin(), sorted.end());
    solutions.push_back(sorted);
    return true;
  }, abort_flag, iterations);

  REQUIRE(solutions.size() == 1);
  REQUIRE(solutions[0] == std::vector<unsigned int>{1, 2, 3});
  REQUIRE(iterations.load() > 0);
}

TEST_CASE("SimdExactCover256 solveSubtree", "[simd][exact_cover]") {
  SimdExactCover256 solver(7, 3);

  SimdBitset256 req;
  for (unsigned int i = 0; i < 7; i++) req.set(i);
  solver.setRequiredColumns(req);

  solver.addRow(1, 0, {2, 4});
  solver.addRow(2, 1, {0, 3, 6});
  solver.addRow(3, 2, {1, 2, 5});
  solver.addRow(4, 1, {0, 3, 5});
  solver.addRow(5, 2, {1, 6});
  solver.addRow(6, 1, {3, 4, 6});

  std::vector<std::vector<unsigned int>> solutions;
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};

  // Subtree with correct prefix {1} should find the unique solution
  solver.solveSubtree({1}, [&](const std::vector<unsigned int> &sol) {
    std::vector<unsigned int> sorted = sol;
    std::sort(sorted.begin(), sorted.end());
    solutions.push_back(sorted);
    return true;
  }, abort_flag, iterations);

  REQUIRE(solutions.size() == 1);
  REQUIRE(solutions[0] == std::vector<unsigned int>{1, 4, 5});

  // Subtree with wrong prefix {2} should find no solutions
  solutions.clear();
  solver.solveSubtree({2}, [&](const std::vector<unsigned int> &sol) {
    solutions.push_back(sol);
    return true;
  }, abort_flag, iterations);

  REQUIRE(solutions.empty());
}

TEST_CASE("SimdExactCover256 concurrent solveSubtree", "[simd][exact_cover][threads]") {
  SimdExactCover256 solver(7, 3);

  SimdBitset256 req;
  for (unsigned int i = 0; i < 7; i++) req.set(i);
  solver.setRequiredColumns(req);

  solver.addRow(1, 0, {2, 4});
  solver.addRow(2, 1, {0, 3, 6});
  solver.addRow(3, 2, {1, 2, 5});
  solver.addRow(4, 1, {0, 3, 5});
  solver.addRow(5, 2, {1, 6});
  solver.addRow(6, 1, {3, 4, 6});

  std::vector<std::vector<unsigned int>> all_solutions;
  std::mutex sol_mutex;
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};

  // Branch on candidate rows covering column 0: row 2 and row 4
  std::vector<unsigned int> prefixes = {2, 4};
  std::vector<std::thread> threads;

  for (unsigned int p : prefixes) {
    threads.emplace_back([&, p]() {
      std::atomic<uint64_t> thread_iter{0};
      solver.solveSubtree({p}, [&](const std::vector<unsigned int> &sol) {
        std::vector<unsigned int> sorted = sol;
        std::sort(sorted.begin(), sorted.end());
        std::lock_guard<std::mutex> lock(sol_mutex);
        all_solutions.push_back(sorted);
        return true;
      }, abort_flag, thread_iter);
      iterations.fetch_add(thread_iter.load());
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  REQUIRE(all_solutions.size() == 1);
  REQUIRE(all_solutions[0] == std::vector<unsigned int>{1, 4, 5});
  REQUIRE(iterations.load() > 0);
}

TEST_CASE("SimdExactCover256 bounds check assertions", "[simd][exact_cover]") {
  CHECK_THROWS_AS(SimdExactCover256(257, 1), assert_exception);

  SimdExactCover256 solver(10, 2);
  CHECK_THROWS_AS(solver.addRow(1, 0, {10}), assert_exception);
  CHECK_THROWS_AS(solver.addRow(2, 0, {256}), assert_exception);

  SimdBitset256 bitset;
  CHECK_THROWS_AS(bitset.set(256), assert_exception);
  CHECK_THROWS_AS(bitset.reset(256), assert_exception);
  CHECK_THROWS_AS(bitset.test(256), assert_exception);
}

#include "src/lib/simd_huang_cover.h"

TEST_CASE("SimdHuangCover256 duplicate pieces exact cover", "[simd][huang]") {
  // Shape 1 (col 1): 2 pieces required
  // Shape 2 (col 2): 1 piece required
  // Voxels (cols 3..7): 5 voxels, 1 required each
  SimdHuangCover256 solver(7, 2);

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
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};

  solver.solve([&](const std::vector<unsigned int> &sol) {
    std::vector<unsigned int> sorted = sol;
    std::sort(sorted.begin(), sorted.end());
    solutions.push_back(sorted);
    return true;
  }, abort_flag, iterations);

  // Two distinct valid solutions: {1, 2, 4} and {1, 3, 5}
  REQUIRE(solutions.size() == 2);
  std::set<std::vector<unsigned int>> sol_set(solutions.begin(), solutions.end());
  REQUIRE(sol_set.count(std::vector<unsigned int>{1, 2, 4}) == 1);
  REQUIRE(sol_set.count(std::vector<unsigned int>{1, 3, 5}) == 1);
  REQUIRE(iterations.load() > 0);
}
