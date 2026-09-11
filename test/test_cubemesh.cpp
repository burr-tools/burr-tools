#include <catch2/catch_test_macros.hpp>

#include "lib/cubemesh.h"
#include "lib/gridtype.h"
#include "lib/voxel.h"
#include "lib/stl.h"
#include "lib/cubepoly.h"
#include "halfedge/polyhedron.h"
#include "halfedge/modifiers.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace {

/* the 12 face-connected vertex bodies (bit 4x+2y+z of the 2x2x2 block) */
const int CANONICAL[] = { 1, 3, 7, 15, 23, 27, 31, 61, 63, 111, 126, 127 };

std::vector<char> block(int mask) {
  std::vector<char> filled(8, 0);
  for (int cx = 0; cx < 2; cx++)
    for (int cy = 0; cy < 2; cy++)
      for (int cz = 0; cz < 2; cz++)
        if (mask & (1 << (4 * cx + 2 * cy + cz))) filled[cx + 2 * (cy + 2 * cz)] = 1;
  return filled;
}

/* every directed edge exactly once, and its reverse once */
bool watertight(const cubeMesh::mesh_s & m) {
  std::map<std::pair<int,int>, int> cnt;
  for (unsigned t = 0; t + 2 < m.tris.size(); t += 3)
    for (int c = 0; c < 3; c++)
      cnt[std::make_pair(m.tris[t + c], m.tris[t + (c + 1) % 3])]++;
  for (const auto & e : cnt) {
    if (e.second != 1) return false;
    if (!cnt.count(std::make_pair(e.first.second, e.first.first))) return false;
  }
  return true;
}

double volume(const cubeMesh::mesh_s & m) {
  double vol = 0;
  for (unsigned t = 0; t + 2 < m.tris.size(); t += 3) {
    const cubeMesh::vec3 & a = m.verts[m.tris[t]], & b = m.verts[m.tris[t + 1]], & c = m.verts[m.tris[t + 2]];
    vol += a.x * (b.y * c.z - b.z * c.y) - a.y * (b.x * c.z - b.z * c.x) + a.z * (b.x * c.y - b.y * c.x);
  }
  return vol / 6;
}

} // namespace

TEST_CASE("cube lookup mesher: every vertex body is watertight in both variants", "[cubemesh]") {
  const double g = 0.05;
  for (int mask : CANONICAL)
    for (double ratio : { 0.0, 0.7, 2.686, 4.5 })
      for (bool fills : { true, false }) {
        cubeMesh::mesh_s m;
        std::string err;
        INFO("mask " << mask << " ratio " << ratio << " fills " << fills << ": " << err);
        REQUIRE(cubeMesh::generate(2, 2, 2, block(mask), g, ratio * g, fills, m, err));
        CHECK(watertight(m));
        CHECK(volume(m) > 0);
      }
}

TEST_CASE("cube lookup mesher: chamfers remove material, fills add it back", "[cubemesh]") {
  const double g = 0.05, r = 0.12;
  std::string err;
  cubeMesh::mesh_s gap, fill, nofill;
  REQUIRE(cubeMesh::generate(2, 2, 2, block(23), g, 0, true, gap, err));
  REQUIRE(cubeMesh::generate(2, 2, 2, block(23), g, r, true, fill, err));
  REQUIRE(cubeMesh::generate(2, 2, 2, block(23), g, r, false, nofill, err));
  CHECK(volume(gap) < 4.0);                 /* four cells, gapped */
  CHECK(volume(fill) < volume(gap));         /* convex edges bevelled */
  CHECK(volume(nofill) < volume(fill));      /* and without the concave fills, less again */
}

TEST_CASE("cube lookup mesher: the zero cases and interior vertices", "[cubemesh]") {
  std::string err;
  cubeMesh::mesh_s m;
  /* a solid 3x3x3 block has an interior vertex, no surface there */
  std::vector<char> solid(27, 1);
  REQUIRE(cubeMesh::generate(3, 3, 3, solid, 0.05, 0.1, true, m, err));
  CHECK(watertight(m));
  /* gap and bevel both zero: the plain cells */
  REQUIRE(cubeMesh::generate(3, 3, 3, solid, 0, 0, true, m, err));
  CHECK(watertight(m));
  CHECK(volume(m) == 27.0);
  /* gap zero with a bevel: the r/g -> infinity limit */
  REQUIRE(cubeMesh::generate(2, 2, 2, block(27), 0, 0.1, true, m, err));
  CHECK(watertight(m));
  /* out of range is refused with a message */
  CHECK_FALSE(cubeMesh::generate(2, 2, 2, block(27), 0.2, 0.2, true, m, err));
  CHECK(!err.empty());
}

TEST_CASE("cube STL exporter uses the lookup mesher", "[cubemesh][stl]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v(gt.getVoxel(3, 3, 2, voxel_c::VX_EMPTY));
  const int cells[][3] = { {0,0,0}, {0,1,0}, {0,1,1}, {1,0,0}, {1,0,1}, {2,0,0}, {2,1,0}, {2,2,0}, {1,2,0}, {0,2,0} };
  for (const auto & c : cells) v->setState(c[0], c[1], c[2], voxel_c::VX_FILLED);
  std::unique_ptr<stlExporter_c> ex(gt.getStlExporter());
  REQUIRE(ex->numParameters() == 11);
  ex->setParameter(0, 10);      /* unit size */
  ex->setParameter(3, 1.0);     /* bevel */
  ex->setParameter(4, 0.5);     /* offset */
  faceList_c holes;
  std::unique_ptr<Polyhedron> withFills(ex->getMesh(*v, holes));
  CHECK(withFills->numFaces() > 0);
  ex->setParameter(10, 0);      /* interior chamfers off */
  std::unique_ptr<Polyhedron> noFills(ex->getMesh(*v, holes));
  CHECK(noFills->numFaces() > 0);
  CHECK(noFills->numFaces() != withFills->numFaces());
  /* the zero cases export too */
  ex->setParameter(3, 0); ex->setParameter(4, 0);
  std::unique_ptr<Polyhedron> plain(ex->getMesh(*v, holes));
  CHECK(plain->numFaces() == 80);      /* 40 exposed cell faces, two triangles each */
  /* tubes are refused */
  ex->setParameter(3, 1.0); ex->setParameter(4, 0.5); ex->setParameter(5, 1.5);
  holes.addFace(0, 0);
  CHECK_THROWS_AS(ex->getMesh(*v, holes), stlException_c);
}

TEST_CASE("the 3D view's STL style uses the lookup mesher on the cube grid", "[cubemesh][view]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v(gt.getVoxel(2, 2, 2, voxel_c::VX_EMPTY));
  for (int i = 0; i < 8; i++)
    if (23 & (1 << i)) v->setState((i >> 2) & 1, (i >> 1) & 1, i & 1, voxel_c::VX_FILLED);
  std::unique_ptr<Polyhedron> view(v->getSTLMesh());
  std::string err;
  std::unique_ptr<Polyhedron> same(cubePolyhedron(*v, 0.02, 0.05, true, 0, err));
  REQUIRE(same);
  CHECK(view->numFaces() == same->numFaces());
  CHECK(view->numFaces() > 0);
}
