/* Tests for btui::buildShapeMesh, picking and the overlay geometry, on every
 * grid type. Example puzzles are read from the project root.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/uicore/camera.h"
#include "../src/uicore/scenemesh.h"
#include "../src/uicore/voxeledit.h"

#include "test_helpers.h"

#include "../src/halfedge/polyhedron.h"
#include "../src/lib/stl.h"
#include "../src/tools/gzstream.h"
#include "../src/tools/xml.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <string>

using namespace btui;
using Catch::Matchers::WithinAbs;

namespace {

  MeshOptions blue(void) {
    MeshOptions o;
    o.piece = { 0, 0, 1 };
    return o;
  }

  std::unique_ptr<puzzle_c> loadPuzzle(const char * file) {
    auto str = openGzFile(file);
    if (!str) return nullptr;
    xmlParser_c pars(*str);
    return std::make_unique<puzzle_c>(pars);
  }

  bool inside(const ShapeMesh & m, const MeshVertex & v) {
    const float e = 0.51f;   // the voxel style's bevels stay inside, the variable marker lifts by 0.005
    return v.pos[0] >= m.boundsMin.x - e && v.pos[0] <= m.boundsMax.x + e &&
           v.pos[1] >= m.boundsMin.y - e && v.pos[1] <= m.boundsMax.y + e &&
           v.pos[2] >= m.boundsMin.z - e && v.pos[2] <= m.boundsMax.z + e;
  }
}

TEST_CASE("an empty shape gives an empty mesh", "[ui][scenemesh]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 3, 3, 3);
  ShapeMesh m = buildShapeMesh(*v, blue());
  CHECK(m.empty());
  CHECK(buildPickMesh(*v).empty());
}

TEST_CASE("every grid type builds triangles inside its bounds", "[ui][scenemesh]") {
  for (auto t : bttest::ALL_GRIDS) {
    gridType_c gt(t);
    auto v = bttest::makeVoxel(gt, 4, 4, 4);
    // a few filled cells, wherever the grid has valid ones
    int placed = 0;
    for (unsigned x = 0; x < 4 && placed < 4; x++)
      for (unsigned y = 0; y < 4 && placed < 4; y++)
        for (unsigned z = 0; z < 4 && placed < 4; z++)
          if (v->validCoordinate(x, y, z)) { v->setState(x, y, z, voxel_c::VX_FILLED); placed++; }
    REQUIRE(placed > 0);

    ShapeMesh m = buildShapeMesh(*v, blue());
    INFO(bttest::gridName(t));
    CHECK_FALSE(m.opaque.empty());
    CHECK(m.translucent.empty());
    CHECK(m.opaque.size() % 3 == 0);
    for (const auto & vert : m.opaque) {
      CHECK(inside(m, vert));
      CHECK_THAT(std::hypot(vert.normal[0], vert.normal[1], vert.normal[2]), WithinAbs(1.0, 1e-3));
    }
    CHECK_FALSE(buildPickMesh(*v).empty());
  }
}

TEST_CASE("an STL exporter mesh is drawn grey, or see-through with its insides", "[ui][scenemesh]") {
  for (auto t : bttest::ALL_GRIDS) {
    gridType_c gt(t);
    if (!(gt.getCapabilities() & gridType_c::CAP_STLEXPORT))
      continue;
    INFO(bttest::gridName(t));
    auto v = bttest::makeVoxel(gt, 3, 3, 3);
    int placed = 0;
    for (unsigned x = 0; x < 3 && placed < 2; x++)
      for (unsigned y = 0; y < 3 && placed < 2; y++)
        for (unsigned z = 0; z < 3 && placed < 2; z++)
          if (v->validCoordinate(x, y, z)) { v->setState(x, y, z, voxel_c::VX_FILLED); placed++; }
    std::unique_ptr<stlExporter_c> stl(gt.getStlExporter());
    REQUIRE(stl);
    std::unique_ptr<Polyhedron> poly(stl->getMesh(*v));
    REQUIRE(poly);

    const ShapeMesh solid = buildPolyhedronMesh(*poly, false);
    CHECK_FALSE(solid.opaque.empty());
    CHECK(solid.translucent.empty());
    CHECK(solid.opaque.size() % 3 == 0);
    CHECK(solid.boundsMax.x > solid.boundsMin.x);
    CHECK(solid.opaque.front().color.r == solid.opaque.front().color.g);
    CHECK(solid.opaque.front().color.a == 255);
    for (const auto & vert : solid.opaque) {
      CHECK(vert.pos[0] >= solid.boundsMin.x - 1e-4f);
      CHECK(vert.pos[0] <= solid.boundsMax.x + 1e-4f);
    }

    const ShapeMesh xray = buildPolyhedronMesh(*poly, true);
    CHECK(xray.opaque.empty());
    CHECK(xray.translucent.size() == solid.opaque.size());
    CHECK(xray.translucent.front().color.a == 26);    // 10 %
  }
}

TEST_CASE("classic style: variable voxels are opaque and carry the black marker", "[ui][scenemesh]") {
  // as legacy voxelframe draws them: solid, told apart by the marker
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 2, 1, 1);
  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  v->setState(1, 0, 0, voxel_c::VX_VARIABLE);
  MeshOptions o = blue();
  o.style = VoxelStyle::Legacy;
  ShapeMesh m = buildShapeMesh(*v, o);
  CHECK(m.translucent.empty());
  REQUIRE_FALSE(m.opaque.empty());
  bool marker = false, variableFace = false;
  for (const auto & vert : m.opaque) {
    CHECK(vert.color.a == 255);
    if (vert.cell[0] == 1.0f) {
      variableFace = true;
      if (vert.color.r == 0 && vert.color.g == 0 && vert.color.b == 0)
        marker = true;
    }
  }
  CHECK(variableFace);
  CHECK(marker);
}

TEST_CASE("classic style: piece colour alternates the legacy light and dark shades", "[ui][scenemesh]") {
  // legacy's voxel style checker: neighbouring voxels take opposite shades
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 2, 1, 1);
  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  v->setState(1, 0, 0, voxel_c::VX_FILLED);
  MeshOptions o;
  o.style = VoxelStyle::Legacy;
  o.piece = { 0.0f, 1.0f, 0.0f };
  ShapeMesh m = buildShapeMesh(*v, o);
  std::set<int> greens0, greens1;
  for (const auto & vert : m.opaque) {
    CHECK(vert.color.r <= 26);   // light shade of 0 is 0.1
    (vert.cell[0] == 0 ? greens0 : greens1).insert(vert.color.g);
  }
  // dark shade 0.9 * 1 -> 230, light shade 1.0 -> 255, one per voxel
  REQUIRE(greens0.size() == 1);
  REQUIRE(greens1.size() == 1);
  CHECK(*greens0.begin() != *greens1.begin());
  for (int g : { *greens0.begin(), *greens1.begin() })
    CHECK((g == 230 || g == 255));
}

namespace {

  /* the sides of triangle i of a vertex list that carry an outline: a side's
   * coordinate stays below kNoOutline at all three corners */
  int outlinedSides(const std::vector<MeshVertex> & v, size_t tri) {
    int n = 0;
    for (int side = 0; side < 3; side++) {
      bool outlined = true;
      for (size_t k = 0; k < 3; k++)
        if (v[3 * tri + k].edge[side] >= 1.5f)
          outlined = false;
      n += outlined ? 1 : 0;
    }
    return n;
  }
}

TEST_CASE("flat style: each exposed face in the shape's own colour, no checker or bevels", "[ui][scenemesh]") {
  // C06 / the mock's render3d: plain faces, the colour as is
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 2, 1, 1);
  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  v->setState(1, 0, 0, voxel_c::VX_FILLED);
  MeshOptions o;
  o.piece = { 0.0f, 1.0f, 0.0f };
  REQUIRE(o.style == VoxelStyle::Flat);             // the default
  const ShapeMesh m = buildShapeMesh(*v, o);
  CHECK(m.translucent.empty());
  // two cubes side by side: 10 outer faces, two triangles each, nothing else
  CHECK(m.opaque.size() == 10 * 2 * 3);
  for (const auto & vert : m.opaque) {
    CHECK(vert.color.r == 0);
    CHECK(vert.color.g == 255);
    CHECK(vert.color.b == 0);
    CHECK(vert.edge[3] == 0.0f);                    // solid outlines
  }
}

TEST_CASE("flat style: the outline follows each face's sides, not the fan's diagonals", "[ui][scenemesh]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 1, 1, 1);
  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  const ShapeMesh m = buildShapeMesh(*v, blue());
  REQUIRE(m.opaque.size() == 6 * 2 * 3);
  // every square face is two triangles; each has two of the square's sides
  // outlined and the shared diagonal not
  for (size_t tri = 0; tri < m.opaque.size() / 3; tri++)
    CHECK(outlinedSides(m.opaque, tri) == 2);
  // the barycentric part: each corner is 1 for its own side's coordinate
  const MeshVertex & a = m.opaque[0];
  CHECK((a.edge[0] == 1.0f || a.edge[0] == 1.0f + kNoOutline));

  // a triangle grid's faces are outlined all round
  gridType_c tri(gridType_c::GT_TRIANGULAR_PRISM);
  auto p = bttest::makeVoxel(tri, 2, 2, 1);
  for (unsigned x = 0; x < 2; x++)
    for (unsigned y = 0; y < 2; y++)
      if (p->validCoordinate(x, y, 0)) p->setState(x, y, 0, voxel_c::VX_FILLED);
  const ShapeMesh pm = buildShapeMesh(*p, blue());
  REQUIRE_FALSE(pm.opaque.empty());
  int full = 0;
  for (size_t t = 0; t < pm.opaque.size() / 3; t++)
    full += outlinedSides(pm.opaque, t) == 3 ? 1 : 0;
  CHECK(full > 0);                                   // the triangular end faces
}

TEST_CASE("flat style: variable voxels are slightly see-through with a dashed outline, no marker", "[ui][scenemesh]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 2, 1, 1);
  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  v->setState(1, 0, 0, voxel_c::VX_VARIABLE);
  const ShapeMesh m = buildShapeMesh(*v, blue());
  REQUIRE_FALSE(m.translucent.empty());
  CHECK(m.edges.size() == 12);                       // the dashed outline: lines, one per edge
  for (const auto & vert : m.translucent) {
    CHECK(vert.cell[0] == 1.0f);
    CHECK(vert.edge[3] == 0.0f);                     // no dashes drawn by the face itself
    CHECK(vert.color.a == 153);                      // 60 %
    CHECK(vert.color.b == 255);                      // its colour, not legacy's black marker
  }
  for (const auto & vert : m.opaque)
    CHECK(vert.edge[3] == 0.0f);
  // the fixed voxel stays a closed cube, seen through its variable
  // neighbour; the variable one has no face against it
  CHECK(m.opaque.size() == 6 * 2 * 3);
  CHECK(m.translucent.size() == 5 * 2 * 3);
}

TEST_CASE("flat style: variable neighbours keep the face between them", "[ui][scenemesh]") {
  // so the inner variable voxels show through the outer ones; each voxel
  // has its own face there, facing away from the other
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 2, 1, 1);
  v->setState(0, 0, 0, voxel_c::VX_VARIABLE);
  v->setState(1, 0, 0, voxel_c::VX_VARIABLE);
  const ShapeMesh m = buildShapeMesh(*v, blue());
  CHECK(m.opaque.empty());
  CHECK(m.translucent.size() == 2 * 6 * 2 * 3);      // every face of both voxels

  // the classic style keeps legacy's surface only
  MeshOptions classic = blue();
  classic.style = VoxelStyle::Legacy;
  const ShapeMesh c = buildShapeMesh(*v, classic);
  CHECK(c.translucent.empty());
}

TEST_CASE("flat style: spheres stay round and draw no facet lines", "[ui][scenemesh]") {
  gridType_c gt(gridType_c::GT_SPHERES);
  auto v = bttest::makeVoxel(gt, 2, 2, 2);
  int placed = 0;
  for (unsigned x = 0; x < 2; x++)
    for (unsigned y = 0; y < 2; y++)
      for (unsigned z = 0; z < 2; z++)
        if (v->validCoordinate(x, y, z)) { v->setState(x, y, z, voxel_c::VX_FILLED); placed++; }
  REQUIRE(placed > 0);
  const ShapeMesh flat = buildShapeMesh(*v, blue());
  MeshOptions classic = blue();
  classic.style = VoxelStyle::Legacy;
  const ShapeMesh old = buildShapeMesh(*v, classic);
  CHECK(flat.opaque.size() == old.opaque.size());    // the same round mesh
  for (const auto & vert : flat.opaque) {
    CHECK(vert.edge[0] >= 1.5f);
    CHECK(vert.edge[1] >= 1.5f);
    CHECK(vert.edge[2] >= 1.5f);
    CHECK(vert.color.b == 255);                      // one colour, no checker
  }
}

TEST_CASE("STL style: the fixed voxels chamfered in the shape's colour, the variable ones not drawn", "[ui][scenemesh]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 2, 1, 1);
  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  v->setState(1, 0, 0, voxel_c::VX_VARIABLE);
  MeshOptions o = blue();
  o.style = VoxelStyle::Stl;
  const ShapeMesh m = buildShapeMesh(*v, o);
  CHECK(m.translucent.empty());
  REQUIRE_FALSE(m.opaque.empty());
  CHECK(m.opaque.size() > 6 * 2 * 3);              // the chamfers: more than a plain cube's faces
  float lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f };
  for (const auto & vert : m.opaque) {
    CHECK(vert.color.b == 255);
    CHECK(vert.color.a == 255);
    CHECK(vert.edge[0] == kNoOutline);              // no outlines
    CHECK(vert.cell[0] == 0.0f);                    // the fixed voxel's, for Dim other layers
    CHECK(vert.cell[1] == 0.0f);
    CHECK(vert.cell[2] == 0.0f);
    CHECK_THAT(std::hypot(vert.normal[0], vert.normal[1], vert.normal[2]), WithinAbs(1.0, 1e-3));
    for (int k = 0; k < 3; k++) {
      lo[k] = std::min(lo[k], vert.pos[k]);
      hi[k] = std::max(hi[k], vert.pos[k]);
    }
  }
  // on the voxel grid, in the shape's own units: the fixed cube only
  // (each side inset by the view's gap, 0.02)
  for (int k = 0; k < 3; k++) {
    CHECK_THAT(lo[k], WithinAbs(0.0, 0.03));
    CHECK_THAT(hi[k], WithinAbs(1.0, 0.03));
  }
  // the view's bounds stay the whole shape's, so switching styles keeps the camera
  CHECK_THAT(m.boundsMax.x, WithinAbs(2.0, 1e-6));

  // only variable voxels: nothing to draw
  auto w = bttest::makeVoxel(gt, 1, 1, 1);
  w->setState(0, 0, 0, voxel_c::VX_VARIABLE);
  CHECK(buildShapeMesh(*w, o).empty());
}

TEST_CASE("STL style: a face across several layers is cut at them, so Dim other layers dims only theirs", "[ui][scenemesh]") {
  // the exporter merges a column's sides into one face; each layer gets its own pieces
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 1, 1, 3);
  v->setAll(voxel_c::VX_FILLED);
  MeshOptions o = blue();
  o.style = VoxelStyle::Stl;
  const ShapeMesh m = buildShapeMesh(*v, o);
  REQUIRE(m.opaque.size() % 3 == 0);
  std::set<int> layers;
  for (size_t i = 0; i < m.opaque.size(); i += 3) {
    const float cz = m.opaque[i].cell[2];
    layers.insert(int(cz));
    for (size_t k = 0; k < 3; k++) {
      const MeshVertex & mv = m.opaque[i + k];
      CHECK(mv.cell[2] == cz);                              // one cell a triangle
      CHECK(mv.pos[2] >= cz - 1e-3f);                       // and within its layer
      CHECK(mv.pos[2] <= cz + 1.0f + 1e-3f);
    }
  }
  CHECK(layers == std::set<int>{ 0, 1, 2 });
}

TEST_CASE("STL style: every grid builds its mesh inside the bounds, each face on its voxel", "[ui][scenemesh]") {
  MeshOptions o = blue();
  o.style = VoxelStyle::Stl;
  for (auto t : bttest::ALL_GRIDS) {
    gridType_c gt(t);
    auto v = bttest::makeVoxel(gt, 4, 4, 4);
    int placed = 0;
    for (unsigned x = 0; x < 4 && placed < 4; x++)
      for (unsigned y = 0; y < 4 && placed < 4; y++)
        for (unsigned z = 0; z < 4 && placed < 4; z++)
          if (v->validCoordinate(x, y, z)) { v->setState(x, y, z, voxel_c::VX_FILLED); placed++; }
    REQUIRE(placed > 0);
    INFO(bttest::gridName(t));
    const ShapeMesh m = buildShapeMesh(*v, o);
    CHECK_FALSE(m.opaque.empty());
    CHECK(m.translucent.empty());
    for (const auto & vert : m.opaque)
      CHECK(inside(m, vert));
    // each face knows its voxel, a filled one (Dim other layers)
    for (const auto & vert : m.opaque) {
      REQUIRE(vert.cell[0] >= 0.0f);
      CHECK(v->getState(unsigned(vert.cell[0]), unsigned(vert.cell[1]), unsigned(vert.cell[2])) == voxel_c::VX_FILLED);
    }
  }
}

TEST_CASE("voxel colour uses the puzzle palette for coloured voxels", "[ui][scenemesh]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 2, 1, 1);
  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  v->setState(1, 0, 0, voxel_c::VX_FILLED);
  v->setColor(1, 0, 0, 1);
  MeshOptions o = blue();
  o.colors = ColorMode::Voxel;
  o.palette = { { 1, 0, 0 } };
  ShapeMesh m = buildShapeMesh(*v, o);
  bool red = false;
  for (const auto & vert : m.opaque)
    if (vert.color.r == 255 && vert.color.b == 0)
      red = true;
  CHECK(red);
}

TEST_CASE("picking a cube from outside finds the voxel and face under the ray", "[ui][scenemesh]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 3, 1, 1);
  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  v->setState(2, 0, 0, voxel_c::VX_FILLED);
  auto pm = buildPickMesh(*v);

  // straight down the -Z axis onto the voxel at x = 2
  PickHit h = pick(pm, { 2.5f, 0.5f, 10 }, { 0, 0, -1 });
  REQUIRE(h.hit);
  unsigned x, y, z;
  REQUIRE(v->indexToXYZ(h.voxel, &x, &y, &z));
  CHECK(x == 2);
  CHECK_THAT(h.point.z, WithinAbs(1.0, 1e-4));

  // the face's neighbour index leads to the cell in front of it
  int nx, ny, nz;
  REQUIRE(v->getNeighbor(unsigned(h.face), 0, int(x), int(y), int(z), &nx, &ny, &nz));
  CHECK(nx == 2);
  CHECK(nz == 1);

  // along the X axis the near voxel shadows the far one
  PickHit near = pick(pm, { -5, 0.5f, 0.5f }, { 1, 0, 0 });
  REQUIRE(near.hit);
  REQUIRE(v->indexToXYZ(near.voxel, &x, &y, &z));
  CHECK(x == 0);

  // and the gap between them is empty
  CHECK_FALSE(pick(pm, { 1.5f, 0.5f, 10 }, { 0, 0, -1 }).hit);
}

TEST_CASE("the fast pick mesh picks what the library's flat mesh does, for every voxel type", "[ui][scenemesh]") {
  // rays from every side through a scattered shape (variable voxels among
  // the fixed ones) meet the same voxel and face in both
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    auto v = bttest::makeVoxel(gt, 6, 6, 6);
    for (int x = 0; x < 6; x++)
      for (int y = 0; y < 6; y++)
        for (int z = 0; z < 6; z++)
          if (v->validCoordinate(x, y, z) && (x * 7 + y * 3 + z * 5) % 4 != 0)
            v->setState(unsigned(x), unsigned(y), unsigned(z),
                        (x + y + z) % 5 == 0 ? voxel_c::VX_VARIABLE : voxel_c::VX_FILLED);
    const auto fast = buildPickMesh(*v);
    const auto slow = buildPickMeshFromFlatMesh(*v);
    REQUIRE(!fast.empty());
    float cx, cy, cz;
    v->calculateSize(&cx, &cy, &cz);
    int compared = 0, hits = 0;
    const Vec3 dirs[6] = { { 0, 0, -1 }, { 0, 0, 1 }, { 0, -1, 0 }, { 0, 1, 0 }, { -1, 0, 0 }, { 1, 0, 0 } };
    for (const Vec3 & d : dirs)
      for (int i = 0; i < 12; i++)
        for (int j = 0; j < 12; j++) {
          // a grid of rays across the box, off the cells' edges
          const float u = (i + 0.37f) / 12.0f, w = (j + 0.61f) / 12.0f;
          Vec3 o{ u * cx, w * cy, w * cz };
          if (d.z != 0) o = { u * cx, w * cy, d.z < 0 ? cz + 5 : -5 };
          if (d.y != 0) o = { u * cx, d.y < 0 ? cy + 5 : -5, w * cz };
          if (d.x != 0) o = { d.x < 0 ? cx + 5 : -5, u * cy, w * cz };
          const PickHit a = pick(fast, o, d), b = pick(slow, o, d);
          compared++;
          CHECK(a.hit == b.hit);
          if (a.hit && b.hit) {
            hits++;
            CHECK(std::abs(a.t - b.t) < 1e-3f);
            CHECK(a.voxel == b.voxel);
            CHECK(a.face == b.face);
          }
        }
    CHECK(hits > compared / 4);
  }
}

TEST_CASE("picking works through the camera on an example of every grid", "[ui][scenemesh]") {
  // one example per grid type from examples/; a ray through the centre of the
  // view of a framed, non-empty shape always meets it
  const char * files[] = {
    "examples/PelikanBurr.xmpuzzle",            // bricks
    "examples/Bermuda.xmpuzzle",                // triangular prisms
    "examples/BallRoom.xmpuzzle",               // spheres
    "examples/12PieceSeparation.xmpuzzle",      // rhombic tetrahedra
    "examples/FourPieceTetrahedron.xmpuzzle",   // tetrahedra-octahedra
  };
  for (const char * f : files) {
    auto puz = loadPuzzle(f);
    INFO(f);
    REQUIRE(puz);
    const puzzle_c & p = *puz;
    for (unsigned s = 0; s < p.getNumberOfShapes(); s++) {
      const voxel_c * v = p.getShape(s);
      if (v->countState(voxel_c::VX_FILLED) + v->countState(voxel_c::VX_VARIABLE) == 0)
        continue;
      auto pm = buildPickMesh(*v);
      ShapeMesh m = buildShapeMesh(*v, blue());
      REQUIRE_FALSE(m.empty());
      // aim at a triangle's centroid from outside, along its normal
      const auto & t = pm.front();
      Vec3 c = (t.a + t.b + t.c) * (1.0f / 3.0f);
      Vec3 n = normalize(cross(t.b - t.a, t.c - t.a));
      PickHit h = pick(pm, c + n * 50.0f, -n);
      CHECK(h.hit);
      break;
    }
  }
}

TEST_CASE("the layer slab is one layer thick and spans the grid", "[ui][scenemesh]") {
  // T-C06-9 (geometry): a 4x3x5 shape, XY layer 2 spans 4x3 and z in [1, 2]
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 4, 3, 5);
  Box s = layerSlab(*v, Plane::XY, 1);
  CHECK_THAT(s.min.x, WithinAbs(0, 1e-6));
  CHECK_THAT(s.max.x, WithinAbs(4, 1e-6));
  CHECK_THAT(s.max.y, WithinAbs(3, 1e-6));
  CHECK_THAT(s.min.z, WithinAbs(1, 1e-6));
  CHECK_THAT(s.max.z, WithinAbs(2, 1e-6));

  Box x = layerSlab(*v, Plane::YZ, 3);
  CHECK_THAT(x.min.x, WithinAbs(3, 1e-6));
  CHECK_THAT(x.max.x, WithinAbs(4, 1e-6));
  CHECK_THAT(x.max.z, WithinAbs(5, 1e-6));

  // C08 / T-C08-1: layer counts per plane
  CHECK(layerCount(*v, Plane::XY) == 5);
  CHECK(layerCount(*v, Plane::XZ) == 3);
  CHECK(layerCount(*v, Plane::YZ) == 4);
}

TEST_CASE("the layer slab holds every voxel of its layer as drawn, of every voxel type", "[ui][scenemesh]") {
  // the spheres of one layer reach into the next (centres sqrt(0.5) apart,
  // radius 0.5): the slab is as thick as they are, not as the layer step
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    gridType_c gt(t);
    auto v = bttest::makeVoxel(gt, 6, 6, 6);
    for (int layer = 0; layer < 6; layer++) {
      INFO(bttest::gridName(t) << ", layer " << layer);
      auto only = bttest::makeVoxel(gt, 6, 6, 6);
      for (int x = 0; x < 6; x++)
        for (int y = 0; y < 6; y++)
          if (only->validCoordinate(x, y, layer))
            only->setState(unsigned(x), unsigned(y), unsigned(layer), voxel_c::VX_FILLED);
      MeshOptions o;
      o.style = VoxelStyle::Flat;
      const ShapeMesh m = buildShapeMesh(*only, o);
      if (m.opaque.empty())
        continue;
      float lo = 1e30f, hi = -1e30f;
      for (const MeshVertex & mv : m.opaque) {
        lo = std::min(lo, mv.pos[2]);
        hi = std::max(hi, mv.pos[2]);
      }
      const Box s = layerSlab(*v, Plane::XY, layer);
      CHECK(s.min.z <= lo + 1e-3f);
      CHECK(s.max.z >= hi - 1e-3f);
      // and no thicker than they are
      CHECK(s.min.z >= lo - 0.02f);
      CHECK(s.max.z <= hi + 0.02f);
    }
  }
}

TEST_CASE("box overlays have 12 edges and 36 face vertices; axes reach past the grid", "[ui][scenemesh]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 4, 3, 5);
  std::vector<LineSeg> lines;
  addBoxEdges(lines, gridBounds(*v), {}, 1, 4, 4);
  CHECK(lines.size() == 12);
  CHECK(lines.front().dashDp == 4);

  std::vector<MeshVertex> faces;
  addBoxFaces(faces, gridBounds(*v), {});
  CHECK(faces.size() == 36);

  std::vector<LineSeg> axes;
  addAxes(axes, *v, {}, {}, {}, 2.5f);
  REQUIRE(axes.size() == 3);
  CHECK_THAT(axes[0].b.x, WithinAbs(4.7, 1e-5));
  CHECK_THAT(axes[2].b.z, WithinAbs(5.7, 1e-5));
}

TEST_CASE("a ghost is its cells' every face and edge, in its style, never dimmed", "[ui][scenemesh]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 3, 3, 3);
  v->setAll(voxel_c::VX_FILLED);              // the shape's voxels hide no face of a ghost
  std::vector<MeshVertex> faces;
  std::vector<LineSeg> lines;
  const GhostStyle s{ Rgba8{ 10, 20, 30, 140 }, Rgba8{ 1, 2, 3, 255 }, 2.0f, 5.0f, 4.0f };
  addGhost(faces, lines, *v, { { 1, 1, 1 } }, s);
  CHECK(faces.size() == 36);                  // 6 faces, 2 triangles each
  REQUIRE(lines.size() == 12);
  for (const MeshVertex & m : faces) {
    CHECK(m.color == s.fill);
    CHECK(m.cell[0] < -1000);
    CHECK(m.pos[0] >= 1.0f - 1e-5f);
    CHECK(m.pos[0] <= 2.0f + 1e-5f);
  }
  for (const LineSeg & l : lines) {
    CHECK(l.color == s.outline);
    CHECK(l.widthDp == 2.0f);
    CHECK(l.dashDp == 5.0f);
  }
  // two neighbours: both cubes whole, their shared edges once
  faces.clear();
  lines.clear();
  addGhost(faces, lines, *v, { { 0, 0, 0 }, { 1, 0, 0 } }, s);
  CHECK(faces.size() == 72);
  CHECK(lines.size() == 20);
  // every voxel type has a ghost; none for no cells, nor for a coordinate
  // that is no cell of the type
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    INFO(bttest::gridName(t));
    gridType_c g(t);
    auto w = bttest::makeVoxel(g, 3, 3, 3);
    std::array<int, 3> cell{ -1, -1, -1 }, notCell{ -1, -1, -1 };
    for (int i = 0; i < 27; i++)
      (w->validCoordinate(i % 3, i / 3 % 3, i / 9) ? cell : notCell) = { i % 3, i / 3 % 3, i / 9 };
    faces.clear();
    lines.clear();
    addGhost(faces, lines, *w, { cell }, s);
    CHECK(faces.size() >= 12);
    const size_t n = faces.size();
    addGhost(faces, lines, *w, {}, s);
    addGhost(faces, lines, *w, { notCell, { 7, 0, 0 } }, s);
    CHECK(faces.size() == n);
  }
}

TEST_CASE("a transform's preview: the result centred on the shape, and legacy's hint", "[ui][scenemesh]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 4, 2, 2);
  auto r = bttest::makeVoxel(gt, 2, 4, 2);    // as a turn about Z leaves it
  r->setState(0, 0, 0, voxel_c::VX_FILLED);
  r->setState(1, 3, 1, voxel_c::VX_VARIABLE);
  std::vector<MeshVertex> faces;
  std::vector<LineSeg> lines;
  const GhostStyle s{ Rgba8{ 10, 20, 30, 128 }, Rgba8{ 1, 2, 3, 255 } };
  addResultGhost(faces, lines, *v, r.get(), s);
  CHECK(faces.size() == 72);                  // both voxels, fixed and variable
  // the result's box (2 x 4 x 2) centred on the shape's (4 x 2 x 2): x from 1, y from -1
  float lo[3] = { 1e9f, 1e9f, 1e9f };
  for (const MeshVertex & m : faces)
    for (int k = 0; k < 3; k++)
      lo[k] = std::min(lo[k], m.pos[k]);
  CHECK_THAT(lo[0], WithinAbs(1.0, 1e-4));
  CHECK_THAT(lo[1], WithinAbs(-1.0, 1e-4));
  CHECK_THAT(lo[2], WithinAbs(0.0, 1e-4));
  faces.clear();
  addResultGhost(faces, lines, *v, nullptr, s);   // blocked: no ghost
  CHECK(faces.empty());

  // legacy's radius: min(0.4 * the box's half diagonal + 0.5, 2)
  CHECK_THAT(transformHintRadius(*v), WithinAbs(0.4 * std::sqrt(24.0) / 2 + 0.5, 1e-4));
  CHECK(transformHintRadius(*bttest::makeVoxel(gt, 20, 20, 20)) == 2.0f);

  const Vec3 c{ 2, 1, 1 };
  const float rad = 1.5f;
  const Rgba8 col{ 255, 0, 0, 255 };
  auto within = [&](const std::vector<LineSeg> & ls) {
    for (const LineSeg & l : ls)
      for (Vec3 p : { l.a, l.b })
        if (length(p - c) > rad + 1e-4f)
          return false;
    return true;
  };
  // a rotation: an arc of 32 strokes about the axis, in its plane, and a head of 2
  std::vector<LineSeg> h;
  addTransformHint(h, c, rad, TransformHint::Rotate, { 0, 0, 1 }, 90, col, 3);
  REQUIRE(h.size() == 34);
  for (int i = 0; i < 32; i++) {
    CHECK_THAT(h[size_t(i)].a.z, WithinAbs(1.0, 1e-5));
    CHECK_THAT(length(h[size_t(i)].a - c), WithinAbs(rad, 1e-4));
  }
  CHECK(h[0].color == col);
  CHECK(h[0].widthDp == 3.0f);
  // a mirror: the shaft along the normal and a head at each end; a nudge one head
  h.clear();
  addTransformHint(h, c, rad, TransformHint::Mirror, { 2, 0, 0 }, 0, col, 3);
  REQUIRE(h.size() == 5);
  CHECK_THAT(h[0].a.x, WithinAbs(c.x - rad, 1e-5));
  CHECK_THAT(h[0].b.x, WithinAbs(c.x + rad, 1e-5));
  CHECK(within(h));
  h.clear();
  addTransformHint(h, c, rad, TransformHint::Nudge, { 0, 1, 0 }, 0, col, 3);
  CHECK(h.size() == 3);
  h.clear();
  addTransformHint(h, c, rad, TransformHint::Nudge, { 0, 0, 0 }, 0, col, 3);   // no direction: nothing
  CHECK(h.empty());
}

TEST_CASE("the slab holds its layer in every plane the voxel type has", "[ui][scenemesh]") {
  // C06 / C08: XZ and YZ for all types but the prism (2026-10-28) -- the
  // slab along Y or X holds that layer's cells as drawn, as the XY one does
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    gridType_c gt(t);
    const unsigned n = t == gridType_c::GT_RHOMBIC ? 15 : t == gridType_c::GT_TETRA_OCTA ? 12 : 5;
    auto v = bttest::makeVoxel(gt, n, n, n);
    for (const Plane plane : { Plane::XY, Plane::XZ, Plane::YZ }) {
      if (!planeAvailable(t, plane))
        continue;
      const int axis = layerAxis(plane);
      int seen = 0;
      for (int layer = 0; layer < layerCount(*v, plane); layer++) {
        INFO(bttest::gridName(t) << ", plane " << int(plane) << ", layer " << layer);
        const auto only = layerCells(*v, plane, layer);
        MeshOptions o;
        o.style = VoxelStyle::Flat;
        const ShapeMesh m = buildShapeMesh(*only, o);
        if (m.opaque.empty())
          continue;
        seen++;
        float lo = 1e30f, hi = -1e30f;
        for (const MeshVertex & mv : m.opaque) {
          lo = std::min(lo, mv.pos[axis]);
          hi = std::max(hi, mv.pos[axis]);
        }
        const Box s = layerSlab(*v, plane, layer);
        CHECK(s.min[axis] <= lo + 1e-3f);
        CHECK(s.max[axis] >= hi - 1e-3f);
        // across the layer it spans the grid
        const Box g = gridBounds(*v);
        for (int k = 0; k < 3; k++)
          if (k != axis) {
            CHECK(s.min[k] <= g.min[k] + 1e-3f);
            CHECK(s.max[k] >= g.max[k] - 1e-3f);
          }
      }
      INFO(bttest::gridName(t) << ", plane " << int(plane));
      CHECK(seen >= 2);
    }
  }
}

TEST_CASE("flat style: a variable voxel's edges are dashed lines, each edge once", "[ui][scenemesh]") {
  // the outline dashes run along an edge as one line, the same seen from
  // both faces that meet there, not restarting in every triangle
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 3, 1, 1);
  v->setState(0, 0, 0, voxel_c::VX_VARIABLE);
  v->setState(1, 0, 0, voxel_c::VX_VARIABLE);
  v->setState(2, 0, 0, voxel_c::VX_FILLED);
  MeshOptions o = blue();
  o.style = VoxelStyle::Flat;
  ShapeMesh m = buildShapeMesh(*v, o);
  // two unit cubes side by side: 8 edges along X, 4 across at each of x = 0, 1, 2
  REQUIRE(m.edges.size() == 20);
  std::set<std::array<float, 6>> seen;
  for (const LineSeg & e : m.edges) {
    CHECK(e.dashDp > 0);
    CHECK(e.gapDp > 0);
    CHECK(e.color.a < 255);
    CHECK(std::abs(length(e.b - e.a) - 1.0f) < 1e-4f);
    std::array<float, 6> k{ e.a.x, e.a.y, e.a.z, e.b.x, e.b.y, e.b.z };
    std::array<float, 6> r{ e.b.x, e.b.y, e.b.z, e.a.x, e.a.y, e.a.z };
    CHECK(seen.count(r) == 0);
    CHECK(seen.insert(k).second);
  }
  // their faces carry no outline of their own; the fixed voxel's still do
  for (const auto & vert : m.translucent)
    for (int i = 0; i < 3; i++)
      CHECK(vert.edge[i] > 0.5f);
  bool fixedOutlined = false;
  for (const auto & vert : m.opaque)
    fixedOutlined |= vert.edge[0] < 1.5f;     // the side opposite corner 0 is a polygon side
  CHECK(fixedOutlined);

  // no variable voxel, no lines; the other styles draw none either
  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  v->setState(1, 0, 0, voxel_c::VX_FILLED);
  CHECK(buildShapeMesh(*v, o).edges.empty());
  v->setState(0, 0, 0, voxel_c::VX_VARIABLE);
  o.style = VoxelStyle::Legacy;
  CHECK(buildShapeMesh(*v, o).edges.empty());
}
