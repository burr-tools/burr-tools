#include <catch2/catch_test_macros.hpp>

#include "lib/minkmesh.h"
#include "lib/gridtype.h"
#include "lib/voxel.h"
#include "lib/stl.h"
#include "lib/converter.h"
#include "lib/puzzle.h"
#include "halfedge/polyhedron.h"
#include "halfedge/volume.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <utility>
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

struct grid_test_def_s {
  gridType_c::gridType g = gridType_c::GT_BRICKS;
  unsigned sx = 0;
  unsigned sy = 0;
  unsigned sz = 0;
  const char * name = nullptr;
};

} // namespace

TEST_CASE("Minkowski mesher: prism, rhombic and tetra-octa shapes, both variants", "[minkmesh]") {
  const grid_test_def_s grids[] = {
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
  /* The view mesh has to be the CHAMFERED one, not the flat fallback.

     getSTLMesh() is defensive: when the Minkowski mesher fails it logs to
     stderr and returns getFlatMesh() so the draw path cannot crash. That
     makes "numFaces() > 0" useless as a check that the chamfer mesher ran
     -- the flat mesh is non-empty too, and its faces are the plain cell
     faces, so the face-tagging, normal and containment assertions
     elsewhere in this file are satisfied by it as well. With only those,
     the mesher could be removed from the 3D view entirely and nothing
     here would go red; the prism, rhombic and tetra-octa render styles
     would quietly become unchamfered cubes-of-cells.

     A chamfer adds geometry: cutting every edge and corner off a cell
     produces strictly more faces than the flat mesh's one polygon per
     exposed cell face. */
  std::unique_ptr<Polyhedron> view(v->getSTLMesh());
  std::unique_ptr<Polyhedron> flat(v->getFlatMesh());
  CHECK(view->numFaces() > 0);
  CHECK(flat->numFaces() > 0);
  CHECK(view->numFaces() > flat->numFaces());
}

/* STL files of the test shapes for a look, when MINK_DUMP names a directory.

   Catch2 counts a case that runs no assertion as passed unless --warn
   NoAssertions is given, which this suite does not pass, so with MINK_DUMP
   unset this used to report green having checked nothing at all. The
   SUCCEED() below makes the skip explicit rather than silent, and the
   write is checked when the variable IS set. */
TEST_CASE("Minkowski mesher: dump the test shapes as STL", "[minkmesh][dump]") {
  const char * dir = getenv("MINK_DUMP");
  if (!dir) {
    SUCCEED("MINK_DUMP is not set, so there is nothing to dump");
    return;
  }
  const grid_test_def_s grids[] = {
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
      REQUIRE_NOTHROW(ex->write(name.c_str(), *v));
    }
  }
}

/* random shapes on the three grids, both variants, a few gap / bevel
 * settings including the b = 2g tie: every result a valid polyhedron.
 * MINK_STRESS scales the count. */
TEST_CASE("Minkowski mesher: random shapes", "[minkmesh][stress]") {
  int count = getenv("MINK_STRESS") ? atoi(getenv("MINK_STRESS")) : 3;
  std::mt19937 rng(12345);
  const grid_test_def_s grids[] = {
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
  const grid_test_def_s grids[] = {
    { gridType_c::GT_BRICKS, 3, 3, 2, "bricks" },
    { gridType_c::GT_RHOMBIC, 5, 5, 5, "rhombic" },
    { gridType_c::GT_TETRA_OCTA, 5, 5, 5, "tetra-octa" },
  };
  for (const auto & gr : grids) {
    gridType_c gt(gr.g);
    std::unique_ptr<voxel_c> v = shape(gt, gr.sx, gr.sy, gr.sz);
    std::unique_ptr<Polyhedron> view(v->getSTLMesh());
    int tagged = 0, faces = 0;
    for (Polyhedron::const_face_iterator it = view->fBegin(); it != view->fEnd(); ++it) {
      const Face * f = *it;
      faces++;
      unsigned int x, y, z;
      INFO(gr.name << " face " << faces << " voxel " << f->_fb_index << " side " << f->_fb_face);
      REQUIRE(v->indexToXYZ(f->_fb_index, &x, &y, &z));
      REQUIRE(v->getState(x, y, z) == voxel_c::VX_FILLED);
      /* the cell's faces as outward planes; the centroid inside all of them */
      Vector3Df cen(0, 0, 0);
      int nv = 0;
      for (Face::const_edge_circulator e = f->begin(), s = e; ; ) { cen = cen + (*e)->dst()->position(); ++nv; ++e; if (e == s) break; }
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


/* ---- the described grids (NG's grids as data) ---- */

namespace {

const gridType_c::gridType DESCRIBED_GRIDS[] = {
  gridType_c::GT_HEXAGONAL_PRISM,
  gridType_c::GT_CAIRO_PRISM,
  gridType_c::GT_RHOMBIC_TETRAHEDRA,
  gridType_c::GT_OCTAHEDRAL_TETRAHEDRA,
  gridType_c::GT_DESCRIBED_BRICKS,
  gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM,
};

/* a closed, consistently oriented manifold: every directed edge (by vertex
 * index) is used by exactly one face and its reverse by exactly one other,
 * no face is degenerate, and the enclosed volume is positive (outward
 * winding); the holes finalize() would have closed show up as edges
 * without their reverse */
void checkClosedManifold(const Polyhedron & p) {
  std::map<std::pair<int, int>, int> uses;
  for (Polyhedron::const_face_iterator it = p.fBegin(); it != p.fEnd(); ++it) {
    std::vector<int> ring;
    for (Face::const_edge_circulator e = (*it)->begin(), s = e; ; ) { ring.push_back((*e)->dst()->index()); ++e; if (e == s) break; }
    REQUIRE(ring.size() >= 3);
    CHECK((*it)->normal().squaredModule() > 0);
    for (size_t k = 0; k < ring.size(); k++) uses[std::make_pair(ring[k], ring[(k + 1) % ring.size()])]++;
  }
  size_t bad = 0;
  for (const auto & u : uses) {
    auto rev = uses.find(std::make_pair(u.first.second, u.first.first));
    if (u.second != 1 || rev == uses.end() || rev->second != 1) bad++;
  }
  CHECK(bad == 0);
  CHECK(volume(p) > 0);
}

struct box_s { double lo[3] = { 1e30, 1e30, 1e30 }, hi[3] = { -1e30, -1e30, -1e30 }; };

/* the exporter's mesh, its exception turned into a failure with its message */
std::unique_ptr<Polyhedron> exported(const stlExporter_c & ex, const voxel_c & v, const char * what = "") {
  try {
    return std::unique_ptr<Polyhedron>(ex.getMesh(v));
  } catch (const stlException_c & e) {
    FAIL(what << " exporter threw: " << e.comment);
  }
  return nullptr;
}

box_s boundingBox(const Polyhedron & p) {
  box_s b;
  for (const Vertex * v : p.vertices()) {
    const Vector3Df & q = v->position();
    for (int k = 0; k < 3; k++) { b.lo[k] = std::min(b.lo[k], (double)q[k]); b.hi[k] = std::max(b.hi[k], (double)q[k]); }
  }
  return b;
}

} // namespace

TEST_CASE("Minkowski mesher: every described grid exports a closed, outward manifold", "[minkmesh][stl][lattice]") {
  for (gridType_c::gridType t : DESCRIBED_GRIDS) {
    gridType_c gt(t);
    INFO("grid " << t);
    CHECK((gt.getCapabilities() & gridType_c::CAP_STLEXPORT) != 0);
    /* boxes in NG's voxel coordinates holding a few lattice cells' worth */
    const unsigned size = (t == gridType_c::GT_RHOMBIC_TETRAHEDRA || t == gridType_c::GT_OCTAHEDRAL_TETRAHEDRA) ? 6 : 3;
    std::unique_ptr<voxel_c> v = shape(gt, size, size, 2);
    REQUIRE(v->countState(voxel_c::VX_FILLED) > 2);
    std::unique_ptr<stlExporter_c> ex(gt.getStlExporter());
    REQUIRE(ex);
    ex->setParameter(0, 10);
    const double settings[][2] = { { 0, 0 }, { 0.2, 0.5 }, { 0.3, 0 }, { 0, 0.4 } };   /* offset, bevel in output units */
    for (const auto & s : settings)
      for (int fills = 1; fills >= 0; fills--) {
        INFO("offset " << s[0] << " bevel " << s[1] << " fills " << fills);
        ex->setParameter(4, s[0]); ex->setParameter(3, s[1]); ex->setParameter(5, fills);
        std::unique_ptr<Polyhedron> p = exported(*ex, *v);
        REQUIRE(p);
        checkClosedManifold(*p);
      }
    /* the 3D view's STL style is chamfered, not the flat fallback */
    std::unique_ptr<Polyhedron> view(v->getSTLMesh());
    std::unique_ptr<Polyhedron> flat(v->getFlatMesh());
    CHECK(view->numFaces() > flat->numFaces());
  }
}

/* the same pieces exported on the native rhombic and tetra-octa grids and
 * on NG's grids (convertToDescribed) are the same solids: equal volume and
 * equal bounding box size, flat and chamfered */
TEST_CASE("Minkowski mesher: rhombic and tetra-octa pieces export alike natively and on NG's grids", "[minkmesh][stl][lattice][cutcube]") {
  const char * const files[] = {
    "examples/HexSticks.xmpuzzle",                    /* rhombic */
    "examples/AugmentedSecondStellation.xmpuzzle",
    "examples/FourPieceTetrahedron.xmpuzzle",         /* tetra-octa */
    "examples/DiagonalCube.xmpuzzle",
  };
  bool seen[2] = { false, false };
  for (const char * file : files) {
    auto nat = puzzle_c::load(file, cutCubeGrids_e::native);
    REQUIRE(nat);
    const gridType_c::gridType native = nat->getGridType()->getType();
    REQUIRE((native == gridType_c::GT_RHOMBIC || native == gridType_c::GT_TETRA_OCTA));
    seen[native == gridType_c::GT_RHOMBIC ? 0 : 1] = true;
    std::unique_ptr<puzzle_c> ng(convertToDescribed(nat.get()));
    REQUIRE(ng);
    REQUIRE(ng->getNumberOfShapes() == nat->getNumberOfShapes());
    std::unique_ptr<stlExporter_c> exN(nat->getGridType()->getStlExporter()), exD(ng->getGridType()->getStlExporter());
    REQUIRE(exN); REQUIRE(exD);
    unsigned compared = 0;
    for (unsigned s = 0; s < nat->getNumberOfShapes() && compared < 2; s++) {
      const voxel_c * vn = nat->getShape(s), * vd = ng->getShape(s);
      if (vn->countState(voxel_c::VX_VARIABLE) || vn->countState(voxel_c::VX_FILLED) == 0) continue;
      compared++;
      const double settings[][3] = { { 0, 0, 1 }, { 0.2, 0.5, 1 }, { 0.2, 0.5, 0 } };   /* offset, bevel, fills */
      for (const auto & st : settings) {
        INFO(file << " shape " << s << " offset " << st[0] << " bevel " << st[1] << " fills " << st[2]);
        for (stlExporter_c * ex : { exN.get(), exD.get() }) {
          ex->setParameter(0, 10); ex->setParameter(4, st[0]); ex->setParameter(3, st[1]); ex->setParameter(5, st[2]);
        }
        std::unique_ptr<Polyhedron> pn = exported(*exN, *vn, "native"), pd = exported(*exD, *vd, "NG grid");
        REQUIRE(pn); REQUIRE(pd);
        checkClosedManifold(*pd);
        const double voln = volume(*pn), vold = volume(*pd);
        INFO("volume native " << voln << " NG " << vold);
        CHECK(std::fabs(voln - vold) < 1e-4 * voln);
        const box_s bn = boundingBox(*pn), bd = boundingBox(*pd);
        for (int k = 0; k < 3; k++) {
          INFO("axis " << k << " native " << bn.lo[k] << ".." << bn.hi[k] << " NG " << bd.lo[k] << ".." << bd.hi[k]);
          CHECK(std::fabs((bn.hi[k] - bn.lo[k]) - (bd.hi[k] - bd.lo[k])) < 1e-3);
        }
      }
    }
    CHECK(compared > 0);
  }
  CHECK(seen[0]);
  CHECK(seen[1]);
}

/* every facet of a described cube lies on a coordinate axis: the bevel must
 * be the same on all three axes (prism bits would make x the prism axis and
 * leave the edges along x sharp), so a bar has the same volume whichever
 * axis it lies along */
TEST_CASE("Minkowski mesher: described cubes bevel every axis alike", "[minkmesh][stl][lattice]") {
  gridType_c gt(gridType_c::GT_DESCRIBED_BRICKS);
  std::unique_ptr<stlExporter_c> ex(gt.getStlExporter());
  REQUIRE(ex);
  ex->setParameter(0, 10); ex->setParameter(4, 0); ex->setParameter(3, 0.5); ex->setParameter(5, 1);
  const unsigned dims[3][3] = { { 3, 1, 1 }, { 1, 3, 1 }, { 1, 1, 3 } };
  double vol[3];
  for (int a = 0; a < 3; a++) {
    std::unique_ptr<voxel_c> v(gt.getVoxel(dims[a][0], dims[a][1], dims[a][2], voxel_c::VX_FILLED));
    std::unique_ptr<Polyhedron> p = exported(*ex, *v);
    REQUIRE(p);
    checkClosedManifold(*p);
    vol[a] = volume(*p);
  }
  INFO("volumes along x, y, z: " << vol[0] << " " << vol[1] << " " << vol[2]);
  CHECK(std::fabs(vol[0] - vol[1]) < 1e-4 * vol[0]);
  CHECK(std::fabs(vol[0] - vol[2]) < 1e-4 * vol[0]);
}
