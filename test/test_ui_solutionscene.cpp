/* Tests for btui's Solver scene: an assembly's pieces at a step of their
 * disassembly, solid, wireframe or hidden (C21).
 */
#include <catch2/catch_test_macros.hpp>

#include "../src/uicore/solutionscene.h"

#include "../src/lib/disasmtomoves.h"
#include "../src/lib/disassembly.h"
#include "../src/lib/problem.h"
#include "../src/lib/puzzle.h"
#include "../src/lib/solution.h"
#include "../src/lib/solvethread.h"
#include "../src/lib/voxel.h"

#include <chrono>
#include <memory>
#include <thread>

using namespace btui;

namespace {

  ShapeMesh bar(unsigned length) {
    gridType_c gt(gridType_c::GT_BRICKS);
    std::unique_ptr<voxel_c> v(gt.getVoxel(length, 1, 1, voxel_c::VX_FILLED));
    MeshOptions o;
    return buildShapeMesh(*v, o);
  }

  /* Pelikan Burr, solved with its disassembly */
  std::unique_ptr<puzzle_c> solvedPelikan(void) {
    auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
    REQUIRE(p);
    problem_c & prob = *p->getProblem(0);
    prob.removeAllSolutions();
    solveThread_c t(prob, solveThread_c::PAR_REDUCE | solveThread_c::PAR_DISASSM, 1);
    REQUIRE(t.start());
    while (t.currentAction() != solveThread_c::ACT_FINISHED && t.currentAction() != solveThread_c::ACT_ERROR)
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    REQUIRE(prob.getNumberOfSavedSolutions() == 1);
    return p;
  }

  size_t vertices(const ShapeMesh & m) { return m.opaque.size() + m.translucent.size(); }
}

TEST_CASE("a cube's hull has its twelve edges", "[ui][solutionscene]") {
  const auto edges = hullEdges(bar(1));
  CHECK(edges.size() == 12);
  for (const HullEdge & e : edges)
    CHECK(dot(e.n1, e.n2) < 0.5f);      // two faces at right angles
}

TEST_CASE("a bar's hull leaves out the seams between its voxels", "[ui][solutionscene]") {
  // four long edges in two segments each, the two ends' four edges each; the
  // ring where the two voxels meet lies in flat faces and is no edge
  CHECK(hullEdges(bar(2)).size() == 16);
  CHECK(hullEdges(ShapeMesh{}).empty());
}

TEST_CASE("an assembly scene holds each placed piece, framed on the whole", "[ui][solutionscene]") {
  auto p = solvedPelikan();
  const problem_c & prob = *p->getProblem(0);
  const assembly_c * a = prob.getSavedSolution(0)->getAssembly();
  const SolutionScene scene = SolutionScene::build(prob, a, MeshOptions{});
  REQUIRE_FALSE(scene.empty());
  CHECK(scene.pieceCount() == 7);
  CHECK(scene.radius > 1.0f);

  ShapeMesh m;
  std::vector<HullEdge> wire;
  std::vector<Rgba8> colours;
  scene.compose(m, wire, colours, nullptr, {});
  const size_t all = vertices(m);
  CHECK(all > 0);
  CHECK(wire.empty());
  CHECK(m.translucent.empty());

  SECTION("a hidden piece is not drawn") {
    std::vector<PieceState> states(7, PieceState::Solid);
    states[0] = PieceState::Hidden;
    scene.compose(m, wire, colours, nullptr, states);
    CHECK(vertices(m) < all);
    CHECK(wire.empty());
  }

  SECTION("a wireframe piece is its edges and a faint tint") {
    std::vector<PieceState> states(7, PieceState::Solid);
    states[3] = PieceState::Wireframe;
    scene.compose(m, wire, colours, nullptr, states);
    CHECK(vertices(m) == all);
    CHECK_FALSE(m.translucent.empty());
    CHECK(wire.size() == colours.size());
    CHECK_FALSE(wire.empty());
    for (const MeshVertex & v : m.translucent)
      CHECK(v.color.a <= 16);            // ≈ 6 %
  }

  SECTION("at the end of the disassembly the pieces have moved apart or left") {
    const separation_c * d = prob.getSavedSolution(0)->getDisassembly();
    REQUIRE(d);
    disasmToMoves_c moves(d, 2 * getResultShape(prob)->getBiggestDimension(), prob.getNumberOfPieces());
    moves.setStep(0, false, true);
    ShapeMesh at0;
    scene.compose(at0, wire, colours, &moves, {});
    CHECK(vertices(at0) == all);
    CHECK(at0.boundsMin.x == m.boundsMin.x);   // step 0 is the assembly

    moves.setStep(float(d->sumMoves()) / 2, false, true);
    ShapeMesh half;
    scene.compose(half, wire, colours, &moves, {});
    const bool moved = half.boundsMin.x != m.boundsMin.x || half.boundsMax.x != m.boundsMax.x
                    || half.boundsMin.y != m.boundsMin.y || half.boundsMax.y != m.boundsMax.y
                    || half.boundsMin.z != m.boundsMin.z || half.boundsMax.z != m.boundsMax.z
                    || vertices(half) != all;
    CHECK(moved);
  }

  CHECK(SolutionScene::build(prob, nullptr, MeshOptions{}).empty());
}
