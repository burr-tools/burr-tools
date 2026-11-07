/* Tests for btui's Puzzle scene (C18): which objects it holds, and where. */
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
    return p;
  }

  long count(const PuzzleScene & s, SceneRole role) {
    return std::count_if(s.objects.begin(), s.objects.end(), [&](const SceneObject & o) { return o.role == role; });
  }
}

TEST_CASE("the scene holds the result, the selected piece and every copy", "[ui][puzzlescene]") {
  auto p = puzzle(3);
  setPieceCount(*p, 0, 1, 3, 3);
  setPieceCount(*p, 0, 2, 1, 1);

  // AC-C18-01
  const PuzzleScene s = buildPuzzleScene(*p, 0, 2, MeshOptions{});
  CHECK(count(s, SceneRole::Goal) == 1);
  CHECK(count(s, SceneRole::Selected) == 1);
  CHECK(count(s, SceneRole::Piece) == 4);
  REQUIRE(s.objects.size() == 6);
  CHECK(s.objects[0].shape == 0);
  CHECK(s.objects[1].shape == 2);
  CHECK(s.objects[2].shape == 1);
  CHECK(s.objects[2].copy == 0);
  CHECK(s.objects[4].copy == 2);
  CHECK(s.objects[4].copies == 3);
  CHECK(s.objects[5].shape == 2);
}

TEST_CASE("the Selected slot is left out for none and for the result", "[ui][puzzlescene]") {
  auto p = puzzle(2);
  CHECK(count(buildPuzzleScene(*p, 0, -1, MeshOptions{}), SceneRole::Selected) == 0);
  CHECK(count(buildPuzzleScene(*p, 0, 0, MeshOptions{}), SceneRole::Selected) == 0);
  CHECK(count(buildPuzzleScene(*p, 0, 9, MeshOptions{}), SceneRole::Selected) == 0);
}

TEST_CASE("a range's copies beyond its minimum are optional and see-through", "[ui][puzzlescene]") {
  auto p = puzzle(2);
  setPieceCount(*p, 0, 1, 1, 3);

  const PuzzleScene s = buildPuzzleScene(*p, 0, -1, MeshOptions{});
  REQUIRE(s.objects.size() == 4);
  CHECK_FALSE(s.objects[1].optional);
  CHECK(s.objects[2].optional);
  CHECK(s.objects[3].optional);
  CHECK_FALSE(s.mesh.translucent.empty());
  for (const MeshVertex & v : s.mesh.translucent)
    CHECK(int(v.color.a) < 255);
}

TEST_CASE("pieces lie five to a row under the top row, none on another", "[ui][puzzlescene]") {
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
  setPieceCount(*p, 0, int(s), 100, 100);

  const PuzzleScene scene = buildPuzzleScene(*p, 0, -1, MeshOptions{});
  CHECK(scene.mesh.opaque.size() + scene.mesh.translucent.size() <= maxSceneVertices);
  CHECK(scene.hidden > 0);
  CHECK(int(scene.objects.size()) - 1 + scene.hidden == 100);
}
