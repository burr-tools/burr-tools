/* Tests for btui's multi-piece scenes and the image export's page layout.
 * Example puzzles are read from the project root.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/uicore/assemblyscene.h"
#include "../src/uicore/imagepages.h"

#include "../src/lib/disasmtomoves.h"
#include "../src/lib/disassembly.h"
#include "../src/lib/problem.h"
#include "../src/lib/puzzle.h"
#include "../src/lib/solution.h"
#include "../src/tools/gzstream.h"
#include "../src/tools/xml.h"

#include <memory>

using namespace btui;

namespace {

  std::unique_ptr<puzzle_c> loadPuzzle(const char * file) {
    auto str = openGzFile(file);
    if (!str) return nullptr;
    xmlParser_c pars(*str);
    return std::make_unique<puzzle_c>(pars);
  }

  /* the first problem with a saved solution that has a disassembly */
  bool findDisassembly(const puzzle_c & p, unsigned & prob, unsigned & sol) {
    for (prob = 0; prob < p.getNumberOfProblems(); prob++) {
      const problem_c * pr = p.getProblem(prob);
      for (sol = 0; sol < pr->getNumberOfSavedSolutions(); sol++)
        if (pr->getSavedSolution(sol)->getDisassembly() && pr->resultValid())
          return true;
    }
    return false;
  }

  size_t vertices(const ShapeMesh & m) { return m.opaque.size() + m.translucent.size(); }
}

TEST_CASE("a shape scene frames the shape on its grid box", "[ui][assemblyscene]") {
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p);
  const SceneContent s = buildShapeScene(*p, 2, ColorMode::Piece);
  CHECK_FALSE(s.mesh.empty());
  CHECK(s.radius > 0.5f);
  const Vec3 mid = (s.mesh.boundsMin + s.mesh.boundsMax) * 0.5f;
  CHECK_THAT(s.centre.x, Catch::Matchers::WithinAbs(mid.x, 1e-5));
  CHECK(buildShapeScene(*p, 999, ColorMode::Piece).mesh.empty());
}

TEST_CASE("an assembly puts every placed piece in, and a step moves them apart", "[ui][assemblyscene]") {
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p);
  unsigned prob = 0, sol = 0;
  REQUIRE(findDisassembly(*p, prob, sol));
  const problem_c & pr = *p->getProblem(prob);

  const SceneContent assembled = buildAssemblyScene(pr, sol, ColorMode::Piece);
  REQUIRE_FALSE(assembled.mesh.empty());
  CHECK(assembled.mesh.translucent.empty());

  // the assembly has the pieces' meshes, nothing more or less
  size_t expected = 0;
  for (unsigned part = 0; part < pr.getNumberOfParts(); part++) {
    MeshOptions o;
    const size_t one = vertices(buildShapeMesh(*pr.getPartShape(part), o));
    expected += one * pr.getPartMaximum(part);
  }
  CHECK(vertices(assembled.mesh) <= expected);
  CHECK(vertices(assembled.mesh) * 2 > expected);

  // the assembled puzzle fits the result shape's box (plus bevel slack)
  const SceneContent result = buildShapeScene(*p, pr.getResultId(), ColorMode::Piece);
  CHECK(assembled.radius <= result.radius * 1.2f);

  // half way through the disassembly the pieces spread out
  const separation_c * t = pr.getSavedSolution(sol)->getDisassembly();
  disasmToMoves_c dtm(t, 20, pr.getNumberOfPieces());
  dtm.setStep(float(t->sumMoves()) * 0.5f, false, true);
  const SceneContent apart = buildAssemblyScene(pr, sol, ColorMode::Piece, &dtm, true);
  CHECK(apart.radius > assembled.radius);

  // dimming lightens the static pieces
  disasmToMoves_c first(t, 20, pr.getNumberOfPieces());
  first.setStep(1, false, true);
  const SceneContent plain = buildAssemblyScene(pr, sol, ColorMode::Piece, &first, false);
  const SceneContent dim = buildAssemblyScene(pr, sol, ColorMode::Piece, &first, true);
  REQUIRE(vertices(plain.mesh) == vertices(dim.mesh));
  long sumPlain = 0, sumDim = 0;
  for (const auto & v : plain.mesh.opaque) sumPlain += v.color.r + v.color.g + v.color.b;
  for (const auto & v : dim.mesh.opaque) sumDim += v.color.r + v.color.g + v.color.b;
  CHECK(sumDim > sumPlain);
}

TEST_CASE("no assembly without a solution", "[ui][assemblyscene]") {
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p);
  CHECK(buildAssemblyScene(*p->getProblem(0), 9999, ColorMode::Piece).mesh.empty());
}

TEST_CASE("paper sizes become pixels the legacy way", "[ui][imagepages]") {
  CHECK(pixelsFor(210, 300) == 2480);
  CHECK(pixelsFor(297, 300) == 3508);
  CHECK(pixelsFor(216, 72) == 612);
}

TEST_CASE("the line height is the largest that fits the pages", "[ui][imagepages]") {
  // one square picture on a square page: a full-height line
  CHECK(planLineHeight({ 1.0 }, 300, 300, 1) == 300);
  // four squares on one page need two lines of two
  const unsigned h = planLineHeight({ 1.0, 1.0, 1.0, 1.0 }, 300, 300, 1);
  CHECK(h <= 150);
  CHECK(h > 100);
  // more pages allow bigger pictures; more pages than pictures change nothing
  CHECK(planLineHeight({ 1.0, 1.0, 1.0, 1.0 }, 300, 300, 4) > h);
  CHECK(planLineHeight({ 1.0, 1.0 }, 300, 300, 9) == planLineHeight({ 1.0, 1.0 }, 300, 300, 2));
  // a very wide picture still gets a line
  CHECK(planLineHeight({ 1000.0 }, 300, 300, 1) >= 1);
  CHECK(planLineHeight({}, 300, 200, 1) == 200);
}

TEST_CASE("pictures fill lines left to right, then pages", "[ui][imagepages]") {
  const auto pl = placePictures({ 100, 100, 100, 100 }, 300, 200, 100);
  REQUIRE(pl.size() == 4);
  CHECK(pl[0].page == 0); CHECK(pl[0].x == 0);   CHECK(pl[0].y == 0);
  CHECK(pl[1].page == 0); CHECK(pl[1].x == 105); CHECK(pl[1].y == 0);   // gap: 300/60
  CHECK(pl[2].page == 0); CHECK(pl[2].x == 0);   CHECK(pl[2].y == 100);
  CHECK(pl[3].page == 0); CHECK(pl[3].x == 105); CHECK(pl[3].y == 100);
  const auto more = placePictures({ 100, 100, 100, 100, 100 }, 300, 200, 100);
  CHECK(more[4].page == 1);
  CHECK(more[4].y == 0);
}
