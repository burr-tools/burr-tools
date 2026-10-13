#include <catch2/catch_test_macros.hpp>

#include "lib/minkmesh.h"
#include "lib/gridtype.h"
#include "lib/voxel.h"
#include "lib/stl.h"
#include "halfedge/polyhedron.h"
#include "halfedge/face.h"

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

/* the distance of p, on the plane of the convex polygon pts with unit
 * normal nrm, from the polygon: 0 inside */
double polygonDistance(const Vector3Df & p, const std::vector<Vector3Df> & pts, const Vector3Df & nrm) {
  const Vector3Df turn = (pts[1] - pts[0]) ^ (pts[2] - pts[0]);
  bool inside = true;
  double best = 1e300;
  for (size_t i = 0; i < pts.size(); i++) {
    const Vector3Df a = pts[i], e = pts[(i + 1) % pts.size()] - a;
    if (((e ^ (p - a)) * nrm) * (turn * nrm) < 0) inside = false;
    double s = (e * (p - a)) / (e * e);
    s = s < 0 ? 0 : s > 1 ? 1 : s;
    best = std::min(best, (double)(p - a - e * (float)s).module());
  }
  return inside ? 0 : best;
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
    { gridType_c::GT_TRIANGULAR_PRISM, 4, 3, 2, "prism" },
    { gridType_c::GT_RHOMBIC, 5, 5, 5, "rhombic" },
    { gridType_c::GT_TETRA_OCTA, 5, 5, 5, "tetra-octa" },
  };
  for (const auto & gr : grids) {
    gridType_c gt(gr.g);
    std::unique_ptr<voxel_c> v = shape(gt, gr.sx, gr.sy, gr.sz);
    for (bool fills : { true, false })
      for (bool split : { false, true }) {
        std::string err;
        std::unique_ptr<Polyhedron> p(minkMesh::polyhedron(*v, 0.02, 0.05, fills, split, err));
        INFO(gr.name << " fills " << fills << " split " << split << ": " << err);
        REQUIRE(p);
        CHECK(p->numFaces() > 0);
        CHECK(p->check(false));
      }
  }
}

/* shape 2 of a user's clasp puzzle: its chamfer once splintered in the
 * view, because a Manifold Simplify between the Minkowski steps deleted
 * real material and left corners standing unchamfered. With interior
 * chamfers every edge is bevelled, so no edge may turn by more than about
 * 63 degrees; the splintered mesh had edges turning by 90. */
TEST_CASE("Minkowski mesher: a rhombic shape whose chamfer splintered", "[minkmesh]") {
  gridType_c gt(gridType_c::GT_RHOMBIC);
  std::unique_ptr<voxel_c> v(gt.getVoxel(10, 10, 10, voxel_c::VX_EMPTY));
  const int cells[][3] = {
    { 2, 1, 0 }, { 3, 2, 0 }, { 4, 2, 1 }, { 5, 2, 1 }, { 4, 3, 2 }, { 5, 3, 2 },
    { 3, 4, 2 }, { 6, 4, 2 }, { 6, 5, 2 }, { 4, 2, 3 }, { 7, 4, 3 }, { 3, 2, 4 },
    { 7, 3, 4 }, { 3, 2, 5 }, { 7, 3, 5 }, { 4, 2, 6 }, { 5, 2, 6 },
  };
  for (const auto & c : cells) {
    REQUIRE(v->validCoordinate(c[0], c[1], c[2]));
    v->setState(c[0], c[1], c[2], voxel_c::VX_FILLED);
  }
  std::string err;
  std::unique_ptr<Polyhedron> p(minkMesh::polyhedron(*v, 0.02, 0.05, true, false, err));
  INFO(err);
  REQUIRE(p);
  REQUIRE(p->check(false));
  int sharp = 0;
  for (Polyhedron::const_edge_iterator it = p->eBegin(); it != p->eEnd(); ++it) {
    Vector3Df a = (*it)->face()->normal(), b = (*it)->twin()->face()->normal();
    a.normalize();
    b.normalize();
    if (a * b < 0.342f) sharp++;  /* cos 70 degrees */
  }
  CHECK(sharp == 0);
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
      REQUIRE_NOTHROW(ex->write(name.c_str(), *v));
    }
  }
}

/* random shapes on the three grids, both variants, a few gap / bevel
 * settings including the b = 2g tie: every result a valid polyhedron.
 * The view's mesh, split along the cell faces: the export's is the same
 * construction without the split. MINK_STRESS scales the count. */
TEST_CASE("Minkowski mesher: random shapes", "[minkmesh][stress]") {
  int count = getenv("MINK_STRESS") ? atoi(getenv("MINK_STRESS")) : 3;
  std::mt19937 rng(12345);
  const grid_test_def_s grids[] = {
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
          std::unique_ptr<Polyhedron> p(minkMesh::polyhedron(*v, s[0], s[1], fills, true, err));
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
/* Shapes with pockets, where two cells touch only along an edge but are
 * joined around its ends through other cells: four faces meet along the
 * edge. The body's corners used to be shared cell by cell, which made that
 * edge one edge of four faces; Manifold repaired it with triangles without
 * area, and the exact rebuild then traced an outline through them that
 * isn't a face ("triangle turned over", "fewer than three corners"). */
TEST_CASE("Minkowski mesher: shapes with pockets", "[minkmesh]") {
  struct case_s { gridType_c::gridType g; unsigned sx, sy, sz; const char * name; };
  const case_s cases[] = {
    { gridType_c::GT_TRIANGULAR_PRISM, 8, 4, 3, "prism, every seventh cell missing" },
    { gridType_c::GT_RHOMBIC, 6, 6, 6, "rhombic, the middle cell missing" },
  };
  for (const case_s & cs : cases) {
    gridType_c gt(cs.g);
    std::unique_ptr<voxel_c> v(gt.getVoxel(cs.sx, cs.sy, cs.sz, voxel_c::VX_EMPTY));
    int mx = -1, my = -1, mz = -1;
    double md = 1e9;
    for (unsigned z = 0; z < cs.sz; z++)
      for (unsigned y = 0; y < cs.sy; y++)
        for (unsigned x = 0; x < cs.sx; x++) {
          if (!v->validCoordinate(x, y, z)) continue;
          if (cs.g == gridType_c::GT_TRIANGULAR_PRISM && (x + 2 * y + 3 * z) % 7 == 0) continue;
          v->setState(x, y, z, voxel_c::VX_FILLED);
          const double d = (x - cs.sx / 2.0) * (x - cs.sx / 2.0) + (y - cs.sy / 2.0) * (y - cs.sy / 2.0) + (z - cs.sz / 2.0) * (z - cs.sz / 2.0);
          if (d < md) { md = d; mx = x; my = y; mz = z; }
        }
    if (cs.g == gridType_c::GT_RHOMBIC) v->setState(mx, my, mz, voxel_c::VX_EMPTY);
    for (double g : { 0.0, 0.02 })
      for (bool fills : { true, false }) {
        std::string err;
        std::unique_ptr<Polyhedron> p(minkMesh::polyhedron(*v, g, 0.05, fills, false, err));
        INFO(cs.name << " g " << g << " fills " << fills << ": " << err);
        REQUIRE(p);
        CHECK(p->check(false));
      }
  }
}

/* On the prism grid the bits' hexagon reaches 209 (sqrt3/2) / 181 of their
 * height sideways, so side faces land a hair less than the gap inside
 * their cells (3.7e-6 of it). At a gap near the cells' inradius that is
 * more than the face tagging's 1e-6, and the side faces used to lose the
 * cell face the 3D view adds the neighbour across. */
TEST_CASE("Minkowski mesher: prism side faces keep their cell face at a large gap", "[minkmesh][view]") {
  gridType_c gt(gridType_c::GT_TRIANGULAR_PRISM);
  std::unique_ptr<voxel_c> v = shape(gt, 4, 3, 2);
  std::string err;
  std::unique_ptr<Polyhedron> p(minkMesh::polyhedron(*v, 0.28, 0, false, false, err));
  INFO(err);
  REQUIRE(p);
  /* every facet face whose voxel's cell has a cell face pointing the same
   * way lies on it (a face that no cell face was inset from, the floor a
   * concave vertex or edge leaves, belongs to the cell holding it, which
   * may have none, and is left out) */
  int checked = 0, untagged = 0;
  for (Polyhedron::const_face_iterator it = p->fBegin(); it != p->fEnd(); ++it) {
    const Face * f = *it;
    if (f->_flags & FF_BEVEL_FACE) continue;
    Vector3Df nrm = f->normal();
    nrm.normalize();
    unsigned int x, y, z;
    REQUIRE(v->indexToXYZ(f->_fb_index, &x, &y, &z));
    std::vector<float> corners;
    int nx, ny, nz;
    bool sameWay = false;
    for (unsigned q = 0; v->getNeighbor(q, 0, x, y, z, &nx, &ny, &nz); q++) {
      corners.clear();
      v->getConnectionFace(x, y, z, (int)q, 0, 0, corners);
      if (corners.size() < 9) continue;
      const Vector3Df a(corners[0], corners[1], corners[2]), b(corners[3], corners[4], corners[5]), c(corners[6], corners[7], corners[8]);
      Vector3Df cn = (b - a) ^ (c - a);
      cn.normalize();
      if (std::fabs(cn * nrm) > 0.9999f) {
        /* outward: away from the cell's own centre */
        Vector3Df centre(0, 0, 0);
        int nc = 0;
        for (unsigned m = 0; v->getNeighbor(m, 0, x, y, z, &nx, &ny, &nz); m++) {
          std::vector<float> k2;
          v->getConnectionFace(x, y, z, (int)m, 0, 0, k2);
          for (unsigned i = 0; i + 2 < k2.size(); i += 3) { centre = centre + Vector3Df(k2[i], k2[i + 1], k2[i + 2]); nc++; }
        }
        centre = centre / (float)nc;
        if (cn * (a - centre) < 0) cn = -cn;
        if (cn * nrm > 0.9999f) sameWay = true;
      }
    }
    if (!sameWay) continue;
    checked++;
    if (f->_fb_face < 0) untagged++;
  }
  CHECK(checked > 0);
  CHECK(untagged == 0);
}

/* The rebuild merges coplanar faces across cells, so one triangle could
 * span several cell faces, and its centroid lie in a neighbouring cell
 * with no face on that plane: on the prism grid, the downward triangles
 * whose tips reach into the face g above a row's bottom. Such faces were
 * tagged with that neighbour and no cell face. The view's mesh is cut
 * along the cell faces' edges: every facet face lying over an exposed cell
 * face (its centroid moved back by g onto that face's plane falls in its
 * polygon) is tagged with exactly that cell and face. */
TEST_CASE("Minkowski mesher: the view's facet faces belong to the cell face they lie over", "[minkmesh][view]") {
  const grid_test_def_s grids[] = {
    { gridType_c::GT_TRIANGULAR_PRISM, 4, 3, 2, "prism" },
    { gridType_c::GT_RHOMBIC, 5, 5, 5, "rhombic" },
    { gridType_c::GT_TETRA_OCTA, 5, 5, 5, "tetra-octa" },
  };
  for (const auto & gr : grids) {
    gridType_c gt(gr.g);
    std::unique_ptr<voxel_c> v = shape(gt, gr.sx, gr.sy, gr.sz);
    /* the exposed cell faces as (voxel, face, outward normal, corners) */
    struct exposed_s { unsigned idx; int n; Vector3Df nrm; std::vector<Vector3Df> pts; };
    std::vector<exposed_s> exposed;
    for (unsigned z = 0; z < v->getZ(); z++)
      for (unsigned y = 0; y < v->getY(); y++)
        for (unsigned x = 0; x < v->getX(); x++) {
          if (!v->validCoordinate(x, y, z) || v->getState(x, y, z) != voxel_c::VX_FILLED) continue;
          std::vector<std::vector<Vector3Df> > polys;
          std::vector<bool> open;
          Vector3Df centre(0, 0, 0);
          int nc = 0, nx, ny, nz;
          for (unsigned n = 0; v->getNeighbor(n, 0, x, y, z, &nx, &ny, &nz); n++) {
            std::vector<float> corners;
            v->getConnectionFace(x, y, z, (int)n, 0, 0, corners);
            std::vector<Vector3Df> pts;
            for (unsigned k = 0; k + 2 < corners.size(); k += 3) { pts.push_back(Vector3Df(corners[k], corners[k + 1], corners[k + 2])); centre = centre + pts.back(); nc++; }
            polys.push_back(pts);
            open.push_back(nx < 0 || ny < 0 || nz < 0 || nx >= (int)v->getX() || ny >= (int)v->getY() || nz >= (int)v->getZ() ||
                           !v->validCoordinate(nx, ny, nz) || v->getState(nx, ny, nz) != voxel_c::VX_FILLED);
          }
          centre = centre / (float)nc;
          for (unsigned n = 0; n < polys.size(); n++) {
            if (polys[n].size() < 3 || !open[n]) continue;
            Vector3Df nrm = (polys[n][1] - polys[n][0]) ^ (polys[n][2] - polys[n][0]);
            if (nrm * (polys[n][0] - centre) < 0) nrm = -nrm;
            nrm.normalize();
            exposed.push_back({ (unsigned)v->getIndex(x, y, z), (int)n, nrm, polys[n] });
          }
        }
    for (double r : { 0.0, 0.05 })
      for (bool fills : { false, true }) {
        const double g = 0.1;
        std::string err;
        std::unique_ptr<Polyhedron> p(minkMesh::polyhedron(*v, g, r, fills, true, err));
        INFO(gr.name << " r " << r << " fills " << fills << ": " << err);
        REQUIRE(p);
        REQUIRE(p->check(false));
        int over = 0;
        for (Polyhedron::const_face_iterator it = p->fBegin(); it != p->fEnd(); ++it) {
          const Face * f = *it;
          if (f->_flags & FF_BEVEL_FACE) continue;
          Vector3Df cen(0, 0, 0);
          int nv = 0;
          for (Face::const_edge_circulator e = f->begin(), s0 = e; ; ) { cen = cen + (*e)->dst()->position(); ++nv; ++e; if (e == s0) break; }
          cen = cen / (float)nv;
          Vector3Df fn = f->normal();
          fn.normalize();
          for (const exposed_s & ex : exposed) {
            if (fn * ex.nrm < 0.9999f) continue;
            /* on the prism grid side faces land 3.7e-6 g short of the gap */
            if (std::fabs(ex.nrm * (cen - ex.pts[0]) + g) > 1e-5) continue;
            const Vector3Df back = cen - ex.nrm * (ex.nrm * (cen - ex.pts[0]));
            if (polygonDistance(back, ex.pts, ex.nrm) > 0) continue;
            over++;
            INFO("face at " << cen.x() << " " << cen.y() << " " << cen.z() << " normal " << fn.x() << " " << fn.y() << " " << fn.z());
            CHECK(f->_fb_index == ex.idx);
            CHECK(f->_fb_face == ex.n);
          }
        }
        CHECK(over > 0);
      }
  }
}

TEST_CASE("the view's STL mesh tags every face with its voxel on every grid", "[minkmesh][view]") {
  const grid_test_def_s grids[] = {
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
        /* an untagged face lies in its cell (a fill at a concave edge
         * bulges into the empty cell by the gap) */
        if (f->_fb_face < 0) CHECK(nrm * (cen - pts[0]) < 0.05);
        if (f->_fb_face == (int)n) {
          tagged++;
          CHECK(f->normal() * nrm > 0.99);
          /* a tagged face lies the gap inside its cell face and belongs to
           * it: moved back onto it, its centroid is in its polygon, even
           * where the face passes through a neighbouring cell. Or near
           * it: at a concave edge of angle a the face reaches g cot(a / 2)
           * past its cell face, sqrt3 g at the prism block's 60 degree
           * notches */
          CHECK(std::fabs(nrm * (cen - pts[0]) + 0.02) < 1e-4);
          const double away = polygonDistance(cen - nrm * (nrm * (cen - pts[0])), pts, nrm);
          INFO("moved back " << away << " from its cell face");
          CHECK(away < 0.02 * 1.7320508 + 1e-4);
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

