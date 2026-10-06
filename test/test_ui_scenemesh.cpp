/* Tests for btui::buildShapeMesh, picking and the overlay geometry, on every
 * grid type. Example puzzles are read from the project root.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/uicore/camera.h"
#include "../src/uicore/scenemesh.h"

#include "test_helpers.h"

#include "../src/halfedge/polyhedron.h"
#include "../src/lib/stl.h"
#include "../src/tools/gzstream.h"
#include "../src/tools/xml.h"

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
  for (const auto & vert : m.translucent) {
    CHECK(vert.cell[0] == 1.0f);
    CHECK(vert.edge[3] == 1.0f);                     // dashed
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
