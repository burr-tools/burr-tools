/* Tests for btui's Puzzle scene (C18): which objects it holds, and where. */
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "../src/uicore/problemlist.h"
#include "../src/uicore/puzzlescene.h"

#include "../src/lib/problem.h"
#include "../src/lib/puzzle.h"
#include "../src/lib/voxel.h"

#include <algorithm>
#include <memory>

using namespace btui;

namespace {

  /* shapes(n) of bars, a problem whose result is shape 0 */
  std::unique_ptr<puzzle_c> puzzle(unsigned n, gridType_c::gridType type = gridType_c::GT_BRICKS) {
    auto p = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(type));
    for (unsigned k = 0; k < n; k++) {
      const unsigned i = p->addShape(k + 6, 6, 6);
      voxel_c * s = p->getShape(i);
      for (unsigned z = 0; z < s->getZ(); z++)
        for (unsigned y = 0; y < s->getY(); y++)
          for (unsigned x = 0; x < s->getX(); x++)
            if (s->validCoordinate(int(x), int(y), int(z)))
              s->setState(x, y, z, voxel_c::VX_FILLED);
    }
    newProblem(*p);
    setResult(*p, 0, 0);
    return p;
  }

  long count(const PuzzleScene & s, SceneRole role) {
    return std::count_if(s.objects.begin(), s.objects.end(), [&](const SceneObject & o) { return o.role == role; });
  }
}

TEST_CASE("the scene holds the result and every copy, a shape's copies in one run", "[ui][puzzlescene]") {
  auto p = puzzle(3);
  setPieceCount(*p, 0, 1, 3, 3);
  setPieceCount(*p, 0, 2, 1, 1);

  // AC-C18-01; the selected piece has no slot of its own (2026-11-17)
  const PuzzleScene s = buildPuzzleScene(*p, 0, 2, MeshOptions{});
  CHECK(count(s, SceneRole::Goal) == 1);
  CHECK(count(s, SceneRole::Piece) == 4);
  REQUIRE(s.objects.size() == 5);
  CHECK(s.objects[0].shape == 0);
  CHECK(s.objects[0].run == -1);
  CHECK(s.objects[1].shape == 1);
  CHECK(s.objects[1].copy == 0);
  CHECK(s.objects[3].copy == 2);
  CHECK(s.objects[3].copies == 3);
  CHECK(s.objects[4].shape == 2);
  CHECK(s.shapes == 2);
  CHECK(s.copies == 4);
  CHECK(s.optional == 0);

  REQUIRE(s.runs.size() == 2);
  CHECK(s.runs[0].shape == 1);
  CHECK(s.runs[0].first == 1);
  CHECK(s.runs[0].count == 3);
  CHECK_FALSE(s.runs[0].continued);
  CHECK(s.runs[1].shape == 2);
  CHECK(s.runs[1].first == 4);
  CHECK(s.runs[1].count == 1);
  for (int i = 1; i < 4; i++)
    CHECK(s.objects[size_t(i)].run == 0);
}

TEST_CASE("a selected piece's copies are lifted and the other pieces step back", "[ui][puzzlescene]") {
  auto p = puzzle(3);
  setPieceCount(*p, 0, 1, 2, 2);
  setPieceCount(*p, 0, 2, 1, 1);
  const PuzzleScene plain = buildPuzzleScene(*p, 0, -1, MeshOptions{});
  CHECK_FALSE(plain.emphasis);
  CHECK(plain.mesh.translucent.empty());

  const PuzzleScene s = buildPuzzleScene(*p, 0, 2, MeshOptions{});
  CHECK(s.emphasis);
  REQUIRE(s.objects.size() == 4);
  CHECK(s.objects[3].radius == Catch::Approx(plain.objects[3].radius * selectedLift));
  CHECK(s.objects[1].radius == plain.objects[1].radius);
  // the result and the selected piece stay opaque; S2's two copies fade
  CHECK_FALSE(s.mesh.translucent.empty());
  const auto at = [](const MeshVertex & v, const SceneObject & o) { return v.cell[0] == o.anchor.x && v.cell[1] == o.anchor.y; };
  for (const MeshVertex & v : s.mesh.translucent) {
    CHECK((at(v, s.objects[1]) || at(v, s.objects[2])));
    CHECK(int(v.color.a) == int(255 * othersAlpha + 0.5f));
  }
  CHECK(s.mesh.opaque.size() + s.mesh.translucent.size() == plain.mesh.opaque.size());

  // the result, a piece not in the problem, none: nothing steps back
  CHECK_FALSE(buildPuzzleScene(*p, 0, 0, MeshOptions{}).emphasis);
  CHECK_FALSE(buildPuzzleScene(*p, 0, 9, MeshOptions{}).emphasis);
  setPieceCount(*p, 0, 2, 0, 0);
  CHECK(buildPuzzleScene(*p, 0, 2, MeshOptions{}).mesh.translucent.empty());
}

TEST_CASE("a group's members stand together, before the ungrouped pieces", "[ui][puzzlescene]") {
  auto p = puzzle(4);
  for (int s = 1; s <= 3; s++)
    setPieceCount(*p, 0, s, 1, 1);
  REQUIRE(setPieceGroupCount(*p, 0, 3, 1, 1));
  const PuzzleScene s = buildPuzzleScene(*p, 0, -1, MeshOptions{});
  REQUIRE(s.runs.size() == 3);
  CHECK(s.runs[0].shape == 3);
  CHECK(s.runs[0].group == 1);
  CHECK(s.runs[1].shape == 1);
  CHECK(s.runs[1].group == 0);
  CHECK(s.runs[2].shape == 2);
}

TEST_CASE("a run goes to the next row whole, and splits only when no row holds it", "[ui][puzzlescene]") {
  auto p = puzzle(3);
  setPieceCount(*p, 0, 1, 3, 3);
  setPieceCount(*p, 0, 2, 3, 3);
  const PuzzleScene s = buildPuzzleScene(*p, 0, -1, MeshOptions{});
  REQUIRE(s.runs.size() == 2);
  CHECK(s.runs[0].row == 0);
  CHECK(s.runs[1].row == 1);
  CHECK_FALSE(s.runs[1].continued);

  setPieceCount(*p, 0, 2, 0, 0);
  setPieceCount(*p, 0, 1, 7, 7);
  const PuzzleScene long_ = buildPuzzleScene(*p, 0, -1, MeshOptions{});
  REQUIRE(long_.runs.size() == 2);
  CHECK(long_.runs[0].count == 5);
  CHECK(long_.runs[1].count == 2);
  CHECK(long_.runs[1].continued);
  CHECK(long_.runs[1].row == 1);
}

TEST_CASE("small pieces stand closer together than large ones", "[ui][puzzlescene]") {
  auto p = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  for (const unsigned n : { 4u, 1u, 4u }) {
    const unsigned i = p->addShape(n, n, n);
    for (unsigned z = 0; z < n; z++)
      for (unsigned y = 0; y < n; y++)
        for (unsigned x = 0; x < n; x++)
          p->getShape(i)->setState(x, y, z, voxel_c::VX_FILLED);
  }
  newProblem(*p);
  setResult(*p, 0, 0);
  setPieceCount(*p, 0, 1, 3, 3);
  setPieceCount(*p, 0, 2, 2, 2);
  const PuzzleScene s = buildPuzzleScene(*p, 0, -1, MeshOptions{});
  REQUIRE(s.objects.size() == 6);
  const float small = s.objects[2].anchor.x - s.objects[1].anchor.x;
  const float large = s.objects[5].anchor.x - s.objects[4].anchor.x;
  CHECK(small > 0);
  CHECK(small < large / 2);
}

TEST_CASE("a range's copies beyond its minimum are optional and drawn as the others", "[ui][puzzlescene]") {
  // product decision 2026-11-14: not C18's translucent dashed copies; its run's name says so
  auto p = puzzle(2);
  setPieceCount(*p, 0, 1, 1, 3);

  const PuzzleScene s = buildPuzzleScene(*p, 0, -1, MeshOptions{});
  REQUIRE(s.objects.size() == 4);
  CHECK_FALSE(s.objects[1].optional);
  CHECK(s.objects[2].optional);
  CHECK(s.objects[3].optional);
  CHECK(s.mesh.translucent.empty());
  for (const MeshVertex & v : s.mesh.opaque) {
    CHECK(int(v.color.a) == 255);
    CHECK(v.edge[3] == 0.0f);        // no dashed outline
  }

  setPieceCount(*p, 0, 1, 3, 3);    // the same copies, all fixed: the same mesh
  const PuzzleScene fixed = buildPuzzleScene(*p, 0, -1, MeshOptions{});
  CHECK(fixed.mesh.opaque.size() == s.mesh.opaque.size());
}

TEST_CASE("pieces lie five to a row under the goal, none on another", "[ui][puzzlescene]") {
  auto p = puzzle(2);
  setPieceCount(*p, 0, 1, 7, 7);

  const PuzzleScene s = buildPuzzleScene(*p, 0, -1, MeshOptions{});
  REQUIRE(s.objects.size() == 8);
  const float row1 = s.objects[1].anchor.y;
  CHECK(s.objects[0].anchor.y > row1);
  CHECK(s.objects[5].anchor.y == row1);
  CHECK(s.objects[6].anchor.y < row1);
  for (size_t a = 0; a < s.objects.size(); a++)
    for (size_t b = a + 1; b < s.objects.size(); b++)
      CHECK(length(s.objects[a].anchor - s.objects[b].anchor) >= s.objects[a].radius + s.objects[b].radius - 1e-3f);
}

TEST_CASE("the whole arrangement fits the scene's radius", "[ui][puzzlescene]") {
  auto p = puzzle(3);
  setPieceCount(*p, 0, 1, 9, 9);
  setPieceCount(*p, 0, 2, 4, 4);
  const PuzzleScene s = buildPuzzleScene(*p, 0, 1, MeshOptions{});
  for (const SceneObject & o : s.objects) {
    CHECK(std::abs(o.anchor.x) + o.radius <= s.radius * 1.01f);
    CHECK(std::abs(o.anchor.y) + o.radius <= s.radius * 1.01f);
  }
}

TEST_CASE("each object's vertices lie around its centre and carry its anchor", "[ui][puzzlescene]") {
  auto p = puzzle(2);
  setPieceCount(*p, 0, 1, 2, 2);
  const PuzzleScene s = buildPuzzleScene(*p, 0, -1, MeshOptions{});
  for (const MeshVertex & v : s.mesh.opaque) {
    const SceneObject * own = nullptr;
    for (const SceneObject & o : s.objects)
      if (o.anchor.x == v.cell[0] && o.anchor.y == v.cell[1])
        own = &o;
    REQUIRE(own);
    CHECK(length(Vec3{ v.pos[0], v.pos[1], v.pos[2] }) <= own->radius + 1e-3f);
  }
}

TEST_CASE("more copies than can be shown are counted, not drawn", "[ui][puzzlescene]") {
  auto p = puzzle(2);
  setPieceCount(*p, 0, 1, maxSceneObjects + 25, maxSceneObjects + 25);
  const PuzzleScene s = buildPuzzleScene(*p, 0, -1, MeshOptions{});
  CHECK(count(s, SceneRole::Piece) == maxSceneObjects);
  CHECK(s.hidden == 25);
}

TEST_CASE("no problem or no result still gives a scene", "[ui][puzzlescene]") {
  auto p = puzzle(2);
  CHECK(buildPuzzleScene(*p, -1, -1, MeshOptions{}).objects.empty());
  CHECK(buildPuzzleScene(*p, 5, -1, MeshOptions{}).objects.empty());
  p->addProblem();
  stepPiece(*p, 1, 1, 1);
  CHECK(count(buildPuzzleScene(*p, 1, -1, MeshOptions{}), SceneRole::Goal) == 0);
  CHECK(count(buildPuzzleScene(*p, 1, -1, MeshOptions{}), SceneRole::Piece) == 1);
}

TEST_CASE("every voxel type and style makes its scene", "[ui][puzzlescene]") {
  // AC-C18-05
  for (const gridType_c::gridType type : bttest::ALL_GRIDS)
    for (const VoxelStyle style : { VoxelStyle::Flat, VoxelStyle::Legacy }) {
      auto p = puzzle(2, type);
      setPieceCount(*p, 0, 1, 1, 2);
      MeshOptions o;
      o.style = style;
      const PuzzleScene s = buildPuzzleScene(*p, 0, -1, o);
      CHECK(s.objects.size() == 3);
      CHECK_FALSE(s.mesh.empty());
    }
}

TEST_CASE("copies past what a click can wait for are counted, not drawn", "[ui][puzzlescene]") {
  auto p = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  p->addShape(1, 1, 1);
  const unsigned s = p->addShape(24, 24, 24);
  voxel_c * v = p->getShape(s);
  for (unsigned z = 0; z < 24; z++)
    for (unsigned y = 0; y < 24; y++)
      for (unsigned x = 0; x < 24; x++)
        if ((x + y + z) % 2 == 0)
          v->setState(x, y, z, voxel_c::VX_FILLED);
  newProblem(*p);
  setResult(*p, 0, 0);
  setPieceCount(*p, 0, int(s), 100, 100);

  const PuzzleScene scene = buildPuzzleScene(*p, 0, -1, MeshOptions{});
  CHECK(scene.mesh.opaque.size() + scene.mesh.translucent.size() <= maxSceneVertices);
  CHECK(scene.hidden > 0);
  CHECK(int(scene.objects.size()) - 1 + scene.hidden == 100);
}
