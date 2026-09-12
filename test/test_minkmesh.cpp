#include <catch2/catch_test_macros.hpp>

#include "lib/minkmesh.h"
#include "lib/gridtype.h"
#include "lib/voxel.h"
#include "lib/stl.h"
#include "halfedge/polyhedron.h"

#include <cstdlib>
#include <memory>
#include <random>
#include <array>
#include <vector>

namespace {

/* a small shape on a grid: every valid cell of a block but the first
 * valid cell of the top layer, so that there are concave edges (the grid
 * type must outlive the voxel, which keeps a pointer to it) */
std::unique_ptr<voxel_c> shape(const gridType_c & gt, unsigned sx, unsigned sy, unsigned sz) {
  std::unique_ptr<voxel_c> v(gt.getVoxel(sx, sy, sz, voxel_c::VX_EMPTY));
  bool removed = false;
  for (unsigned z = 0; z < sz; z++)
    for (unsigned y = 0; y < sy; y++)
      for (unsigned x = 0; x < sx; x++) {
        if (!v->validCoordinate(x, y, z)) continue;
        if (z == sz - 1 && !removed) { removed = true; continue; }
        v->setState(x, y, z, voxel_c::VX_FILLED);
      }
  return v;
}

} // namespace

TEST_CASE("Minkowski mesher: prism, rhombic and tetra-octa shapes, both variants", "[minkmesh]") {
  struct { gridType_c::gridType g; unsigned sx, sy, sz; const char * name; } grids[] = {
    { gridType_c::GT_TRIANGULAR_PRISM, 4, 3, 2, "prism" },
    { gridType_c::GT_RHOMBIC, 5, 5, 5, "rhombic" },
    { gridType_c::GT_TETRA_OCTA, 5, 5, 5, "tetra-octa" },
  };
  for (const auto & gr : grids) {
    gridType_c gt(gr.g);
    std::unique_ptr<voxel_c> v = shape(gt, gr.sx, gr.sy, gr.sz);
    for (bool fills : { true, false }) {
      std::string err;
      std::unique_ptr<Polyhedron> p(minkMesh::polyhedron(*v, 0.02, 0.05, fills, err));
      INFO(gr.name << " fills " << fills << ": " << err);
      REQUIRE(p);
      CHECK(p->numFaces() > 0);
      CHECK(p->check(false));
    }
  }
}

TEST_CASE("Minkowski mesher: the exporter and the view use it on the rhombic grid", "[minkmesh][stl]") {
  gridType_c gt(gridType_c::GT_RHOMBIC);
  std::unique_ptr<voxel_c> v = shape(gt, 5, 5, 5);
  std::unique_ptr<stlExporter_c> ex(gt.getStlExporter());
  ex->setParameter(0, 10); ex->setParameter(3, 0.5); ex->setParameter(4, 0.2);
  std::unique_ptr<Polyhedron> withFills(ex->getMesh(*v));
  ex->setParameter(5, 0);
  std::unique_ptr<Polyhedron> noFills(ex->getMesh(*v));
  CHECK(withFills->numFaces() > 0);
  CHECK(noFills->numFaces() > 0);
  CHECK(withFills->numFaces() != noFills->numFaces());
  std::unique_ptr<Polyhedron> view(v->getSTLMesh());
  CHECK(view->numFaces() > 0);
}

/* STL files of the test shapes for a look, when MINK_DUMP names a directory */
TEST_CASE("Minkowski mesher: dump the test shapes as STL", "[minkmesh][dump]") {
  const char * dir = getenv("MINK_DUMP");
  if (!dir) return;
  struct { gridType_c::gridType g; unsigned sx, sy, sz; const char * name; } grids[] = {
    { gridType_c::GT_TRIANGULAR_PRISM, 4, 3, 2, "prism" },
    { gridType_c::GT_RHOMBIC, 5, 5, 5, "rhombic" },
    { gridType_c::GT_TETRA_OCTA, 5, 5, 5, "tetra-octa" },
  };
  for (const auto & gr : grids) {
    gridType_c gt(gr.g);
    std::unique_ptr<voxel_c> v = shape(gt, gr.sx, gr.sy, gr.sz);
    std::unique_ptr<stlExporter_c> ex(gt.getStlExporter());
    ex->setParameter(0, 10); ex->setParameter(3, 0.5); ex->setParameter(4, 0.2);
    for (int fills = 1; fills >= 0; fills--) {
      ex->setParameter(5, fills);
      std::string name = std::string(dir) + "/" + gr.name + (fills ? "-fill" : "-nofill") + ".stl";
      ex->write(name.c_str(), *v);
    }
  }
}

/* random shapes on the three grids, both variants, a few gap / bevel
 * settings including the b = 2g tie: every result a valid polyhedron.
 * MINK_STRESS scales the count. */
TEST_CASE("Minkowski mesher: random shapes", "[minkmesh][stress]") {
  int count = getenv("MINK_STRESS") ? atoi(getenv("MINK_STRESS")) : 3;
  std::mt19937 rng(12345);
  struct { gridType_c::gridType g; unsigned sx, sy, sz; const char * name; } grids[] = {
    { gridType_c::GT_TRIANGULAR_PRISM, 6, 4, 3, "prism" },
    { gridType_c::GT_RHOMBIC, 7, 7, 7, "rhombic" },
    { gridType_c::GT_TETRA_OCTA, 7, 7, 7, "tetra-octa" },
  };
  /* gap + 2 x 1.707 x bevel must stay below the cells' inradius (about
   * 0.29 for the tetrahedra); 0.02 / 0.04 is the b = 2g tie */
  const double settings[][2] = { { 0.02, 0.05 }, { 0.02, 0.04 }, { 0.03, 0.06 }, { 0.03, 0.0 }, { 0.0, 0.04 }, { 0.0, 0.0 } };
  for (const auto & gr : grids) {
    gridType_c gt(gr.g);
    for (int n = 0; n < count; n++) {
      /* a random connected blob: grow from a seed cell through face neighbours */
      std::unique_ptr<voxel_c> v(gt.getVoxel(gr.sx, gr.sy, gr.sz, voxel_c::VX_EMPTY));
      std::vector<std::array<int, 3> > cells;
      for (int tries = 0; cells.empty() && tries < 1000; tries++) {
        int x = rng() % gr.sx, y = rng() % gr.sy, z = rng() % gr.sz;
        if (v->validCoordinate(x, y, z)) { v->setState(x, y, z, voxel_c::VX_FILLED); cells.push_back({x, y, z}); }
      }
      unsigned target = 6 + rng() % 20;
      for (int tries = 0; cells.size() < target && tries < 2000; tries++) {
        const std::array<int, 3> & c = cells[rng() % cells.size()];
        int nx, ny, nz;
        unsigned face = rng() % 12;
        if (!v->getNeighbor(face, 0, c[0], c[1], c[2], &nx, &ny, &nz)) continue;
        if (nx < 0 || ny < 0 || nz < 0 || nx >= (int)gr.sx || ny >= (int)gr.sy || nz >= (int)gr.sz) continue;
        if (!v->validCoordinate(nx, ny, nz) || v->getState(nx, ny, nz) == voxel_c::VX_FILLED) continue;
        v->setState(nx, ny, nz, voxel_c::VX_FILLED); cells.push_back({nx, ny, nz});
      }
      for (const auto & s : settings)
        for (bool fills : { true, false }) {
          std::string err;
          std::unique_ptr<Polyhedron> p(minkMesh::polyhedron(*v, s[0], s[1], fills, err));
          INFO(gr.name << " shape " << n << " (" << cells.size() << " cells) g " << s[0] << " r " << s[1] << " fills " << fills << ": " << err);
          REQUIRE(p);
          CHECK(p->numFaces() > 0);
          CHECK(p->check(false));
        }
    }
  }
}

/* the 3D view removes the voxel a clicked face is tagged with and adds a
 * voxel across the tagged cell face: every face of the view's mesh must
 * lie in the cell of its voxel, and a tagged cell face must face the
 * neighbour it names */
TEST_CASE("the view's STL mesh tags every face with its voxel on every grid", "[minkmesh][view]") {
  struct { gridType_c::gridType g; unsigned sx, sy, sz; const char * name; } grids[] = {
    { gridType_c::GT_BRICKS, 3, 3, 2, "bricks" },
    { gridType_c::GT_TRIANGULAR_PRISM, 4, 3, 2, "prism" },
    { gridType_c::GT_RHOMBIC, 5, 5, 5, "rhombic" },
    { gridType_c::GT_TETRA_OCTA, 5, 5, 5, "tetra-octa" },
  };
  for (const auto & gr : grids) {
    gridType_c gt(gr.g);
    std::unique_ptr<voxel_c> v = shape(gt, gr.sx, gr.sy, gr.sz);
    std::unique_ptr<Polyhedron> view(v->getSTLMesh());
    int tagged = 0, faces = 0;
    for (Polyhedron::const_face_iterator it = view->fBegin(); it != view->fEnd(); it++) {
      const Face * f = *it;
      faces++;
      unsigned int x, y, z;
      INFO(gr.name << " face " << faces << " voxel " << f->_fb_index << " side " << f->_fb_face);
      REQUIRE(v->indexToXYZ(f->_fb_index, &x, &y, &z));
      REQUIRE(v->getState(x, y, z) == voxel_c::VX_FILLED);
      /* the cell's faces as outward planes; the centroid inside all of them */
      Vector3Df cen(0, 0, 0);
      int nv = 0;
      for (Face::const_edge_circulator e = f->begin(), s = e; ; ) { cen = cen + (*e)->dst()->position(); nv++; e++; if (e == s) break; }
      cen = cen / (float)nv;
      std::vector<float> corners;
      int nx, ny, nz;
      Vector3Df centre(0, 0, 0); int nc = 0;
      std::vector<std::vector<Vector3Df> > polys;
      for (unsigned n = 0; v->getNeighbor(n, 0, x, y, z, &nx, &ny, &nz); n++) {
        corners.clear();
        v->getConnectionFace(x, y, z, (int)n, 0, 0, corners);
        std::vector<Vector3Df> pts;
        for (unsigned k = 0; k + 2 < corners.size(); k += 3) { pts.push_back(Vector3Df(corners[k], corners[k + 1], corners[k + 2])); centre = centre + pts.back(); nc++; }
        polys.push_back(pts);
      }
      centre = centre / (float)nc;
      for (unsigned n = 0; n < polys.size(); n++) {
        const std::vector<Vector3Df> & pts = polys[n];
        if (pts.size() < 3) continue;
        Vector3Df nrm = (pts[1] - pts[0]) ^ (pts[2] - pts[0]);
        if (nrm * (pts[0] - centre) < 0) nrm = -nrm;
        nrm.normalize();
        /* a fill at a concave edge bulges into the empty cell by the gap */
        CHECK(nrm * (cen - pts[0]) < (f->_fb_face >= 0 ? 1e-4 : 0.05));
        if (f->_fb_face == (int)n) {
          tagged++;
          CHECK(f->normal() * nrm > 0.99);
          /* the neighbour the view would add lies across this face: its
           * centre (from its own cell geometry) is on the outside of the
           * face plane, and the face plane cuts the segment between the
           * two centres */
          REQUIRE(v->getNeighbor(n, 0, x, y, z, &nx, &ny, &nz));
          Vector3Df nbCentre(0, 0, 0); int nn = 0;
          int qx, qy, qz;
          for (unsigned m = 0; v->getNeighbor(m, 0, nx, ny, nz, &qx, &qy, &qz); m++) {
            corners.clear();
            v->getConnectionFace(nx, ny, nz, (int)m, 0, 0, corners);
            for (unsigned k = 0; k + 2 < corners.size(); k += 3) { nbCentre = nbCentre + Vector3Df(corners[k], corners[k + 1], corners[k + 2]); nn++; }
          }
          nbCentre = nbCentre / (float)nn;
          double outside = nrm * (nbCentre - pts[0]), inside = nrm * (centre - pts[0]);
          INFO("neighbour " << nx << "," << ny << "," << nz << " outside " << outside << " own centre inside " << inside);
          CHECK(outside > 0);
          CHECK(inside < 0);
          CHECK((nbCentre - centre).module() < 2.0);
        }
      }
    }
    CHECK(tagged > 0);
  }
}

