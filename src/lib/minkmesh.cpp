/* see minkmesh.h */
#include "minkmesh.h"
#include "voxel.h"
#include "gridtype.h"

#include "../halfedge/polyhedron.h"

#include <manifold/manifold.h>
#include <manifold/polygon.h>

#include <algorithm>
#include <cstdio>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <tuple>
#include <limits>
#include <map>
#include <set>
#include <vector>

namespace minkMesh {

namespace {

using manifold::Manifold;
using manifold::vec3;

const double SQRT2 = 1.4142135623730951;

/* A solid without its cavities: only the shells enclosing positive
 * volume. A printed piece never wants an enclosed void, so the voids of the
 * voxel shape, and any a step leaves, are filled. Never applied to a sweep
 * tube, whose one cavity is the part of the solid an inward step keeps. */
Manifold outerShells(const Manifold & m) {
  std::vector<Manifold> shells = m.Decompose();
  std::vector<Manifold> outer;
  for (const Manifold & s : shells)
    if (s.Volume() > 0) outer.push_back(s);
  if (outer.size() == shells.size()) return m;
  return Manifold::BatchBoolean(outer, manifold::OpType::Add);
}

/* The shape's surface, exactly: the cell faces whose neighbour is not
 * filled (at bevel 0 and offset 0, the same enumeration
 * voxel_c::getMeshInternal uses). The faces between two filled cells
 * cancel by identity here and never reach a boolean, whose float
 * arithmetic would have to rediscover them and now and then leaves a
 * seam.
 *
 * The vertices come from how the surface wraps around each point. Where
 * two cells touch only along an edge, four faces meet there: each is paired
 * with the face reached by turning about the edge into its own cell's side,
 * so that every pair wraps one solid wedge and the wedges stay apart. The
 * corners around a point that the pairings join are one vertex; where the
 * solid only touches itself (along an edge, at a corner) a point gets one
 * vertex per sheet. The mesh is then a manifold as it stands, and Manifold
 * never has to repair it (which it would do with triangles without area). */
Manifold exactBody(const voxel_c & v, std::string & err) {
  std::map<std::tuple<float, float, float>, uint64_t> pointId;
  std::vector<vec3> points;
  auto point = [&](float x, float y, float z) {
    auto it = pointId.find(std::make_tuple(x, y, z));
    if (it != pointId.end()) return it->second;
    points.push_back(vec3(x, y, z));
    return pointId[std::make_tuple(x, y, z)] = points.size() - 1;
  };
  auto filled = [&](int x, int y, int z) {
    return x >= 0 && y >= 0 && z >= 0 && x < (int)v.getX() && y < (int)v.getY() && z < (int)v.getZ()
      && v.validCoordinate(x, y, z) && v.getState(x, y, z) == voxel_c::VX_FILLED;
  };
  /* the boundary cell faces, fanned into triangles over the points */
  std::vector<std::array<uint64_t, 3>> tris;
  std::vector<float> fc;
  int nx, ny, nz;
  for (unsigned z = 0; z < v.getZ(); z++)
    for (unsigned y = 0; y < v.getY(); y++)
      for (unsigned x = 0; x < v.getX(); x++) {
        if (!filled(x, y, z)) continue;
        for (unsigned n = 0; v.getNeighbor(n, 0, x, y, z, &nx, &ny, &nz); n++) {
          if (filled(nx, ny, nz)) continue;
          fc.clear();
          v.getConnectionFace(x, y, z, (int)n, 0, 0, fc);
          if (fc.size() < 9) continue;
          std::vector<uint64_t> face;
          for (unsigned k = 0; k + 2 < fc.size(); k += 3) face.push_back(point(fc[k], fc[k + 1], fc[k + 2]));
          for (size_t k = 1; k + 1 < face.size(); k++) tris.push_back({ face[0], face[k], face[k + 1] });
        }
      }
  if (tris.empty()) { err = "the shape has no surface"; return Manifold(); }
  /* the faces come all wound one way round: outward, from here on */
  double volume6 = 0;
  for (const auto & t : tris) volume6 += manifold::la::dot(points[t[0]], manifold::la::cross(points[t[1]], points[t[2]]));
  if (volume6 < 0) for (auto & t : tris) std::swap(t[1], t[2]);

  /* half-edge h = 3 t + c runs from corner c of triangle t to the next */
  const size_t nh = 3 * tris.size();
  auto from = [&](size_t h) { return tris[h / 3][h % 3]; };
  auto to = [&](size_t h) { return tris[h / 3][(h % 3 + 1) % 3]; };
  auto next = [](size_t h) { return h - h % 3 + (h % 3 + 1) % 3; };
  std::map<std::pair<uint64_t, uint64_t>, std::vector<size_t>> along;
  for (size_t h = 0; h < nh; h++) along[std::make_pair(from(h), to(h))].push_back(h);
  /* Edges with several sheets, and how each pairs its faces: by default
   * each face with the next one turning into its own cell's side (the
   * wedges stay apart). Where the wedges are joined around both ends of the
   * edge anyway (through other cells), that gives two sheets between the
   * same two vertices, which a manifold mesh can't have; such an edge pairs
   * each face with the next one turning into the empty side instead (the
   * wedges join across the edge), which gives each end one vertex per sheet. */
  std::vector<std::pair<uint64_t, uint64_t>> multi;
  std::map<std::pair<uint64_t, uint64_t>, bool> intoEmpty;
  for (const auto & e : along) {
    const uint64_t a = e.first.first, b = e.first.second;
    if (a > b) continue;
    auto back = along.find(std::make_pair(b, a));
    if (back == along.end() || back->second.size() != e.second.size()) { err = "the shape's cell faces do not close up into a solid"; return Manifold(); }
    if (e.second.size() > 1) { multi.push_back(e.first); intoEmpty[e.first] = false; }
  }
  std::vector<size_t> twin(nh, ~(size_t)0);
  auto pairUp = [&]() {
    for (const auto & e : along) {
      const uint64_t a = e.first.first, b = e.first.second;
      if (a > b) continue;
      const std::vector<size_t> & H = e.second;
      const std::vector<size_t> & K = along.find(std::make_pair(b, a))->second;
      if (H.size() == 1) { twin[H[0]] = K[0]; twin[K[0]] = H[0]; continue; }
      const double side = intoEmpty[e.first] ? -1.0 : 1.0;
      const vec3 d = manifold::la::normalize(points[b] - points[a]);
      auto across = [&](size_t h) {
        /* the direction from the edge into triangle h, perpendicular to it */
        const vec3 c = points[tris[h / 3][(h % 3 + 2) % 3]] - points[a];
        return manifold::la::normalize(c - d * manifold::la::dot(c, d));
      };
      const vec3 ref = across(H[0]);
      auto angle = [&](size_t h) {
        const vec3 w = across(h);
        return std::atan2(manifold::la::dot(manifold::la::cross(ref, w), d), manifold::la::dot(ref, w));
      };
      auto solidTurn = [&](size_t h) {
        /* which way about d turns from the face into the solid behind it */
        const size_t t = h / 3;
        const vec3 n = manifold::la::cross(points[tris[t][1]] - points[tris[t][0]], points[tris[t][2]] - points[tris[t][0]]);
        return manifold::la::dot(manifold::la::cross(across(h), -n), d) > 0 ? 1.0 : -1.0;
      };
      auto pair = [&](const std::vector<size_t> & mine, const std::vector<size_t> & theirs) {
        for (size_t h : mine) {
          const double a0 = angle(h), turn = solidTurn(h) * side;
          size_t best = ~(size_t)0; double bestTurn = 1e300;
          for (size_t k : theirs) {
            double delta = (angle(k) - a0) * turn;
            while (delta <= 0) delta += 2 * M_PI;
            if (delta < bestTurn) { bestTurn = delta; best = k; }
          }
          twin[h] = best;
        }
      };
      pair(H, K);
      pair(K, H);
    }
  };

  /* the vertices: corners around a point joined across the pairings (the
   * corner a half-edge starts from, and across its twin the corner of the
   * face beyond, at the same point) */
  std::vector<size_t> parent(nh);
  std::function<size_t(size_t)> find = [&](size_t i) { while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; } return i; };
  for (;;) {
    pairUp();
    for (size_t h = 0; h < nh; h++)
      if (twin[h] == ~(size_t)0 || twin[twin[h]] != h) { err = "the shape's surface does not pair up into sheets along an edge"; return Manifold(); }
    for (size_t h = 0; h < nh; h++) parent[h] = h;
    for (size_t h = 0; h < nh; h++) parent[find(h)] = find(next(twin[h]));
    /* two sheets between the same two vertices? */
    bool switched = false;
    for (const auto & e : multi) {
      const std::vector<size_t> & H = along[e];
      std::set<std::pair<size_t, size_t>> ends;
      for (size_t h : H) ends.insert(std::make_pair(find(h), find(next(h))));
      if (ends.size() == H.size()) continue;
      if (intoEmpty[e]) { err = "the shape's surface has two sheets along an edge between the same corners"; return Manifold(); }
      intoEmpty[e] = true;
      switched = true;
    }
    if (!switched) break;
  }

  manifold::MeshGL64 mesh;
  mesh.numProp = 3;
  std::vector<uint64_t> vertexOf(nh, ~(uint64_t)0);
  mesh.triVerts.resize(nh);
  for (size_t h = 0; h < nh; h++) {
    const size_t r = find(h);
    if (vertexOf[r] == ~(uint64_t)0) {
      vertexOf[r] = mesh.vertProperties.size() / 3;
      const vec3 & p = points[from(h)];
      mesh.vertProperties.push_back(p.x); mesh.vertProperties.push_back(p.y); mesh.vertProperties.push_back(p.z);
    }
    mesh.triVerts[h] = vertexOf[r];
  }
  Manifold m(mesh);
  if (m.Status() != Manifold::Error::NoError) { err = "the shape's cell faces do not close up into a solid"; return Manifold(); }
  if (m.NumTri() != tris.size() || m.NumVert() != mesh.vertProperties.size() / 3) {
    err = "the shape's surface was changed on its way into Manifold"; return Manifold();
  }
  return outerShells(m);
}

/* the distinct unit face normals of one cell (the grid's facet directions
 * as far as this cell shows them) */
void cellNormals(const voxel_c & v, int x, int y, int z, std::vector<vec3> & out) {
  std::vector<float> corners;
  int nx, ny, nz;
  for (unsigned n = 0; v.getNeighbor(n, 0, x, y, z, &nx, &ny, &nz); n++) {
    corners.clear();
    v.getConnectionFace(x, y, z, (int)n, 0, 0, corners);
    if (corners.size() < 9) continue;
    vec3 a(corners[0], corners[1], corners[2]), b(corners[3], corners[4], corners[5]), c(corners[6], corners[7], corners[8]);
    vec3 nn = manifold::la::cross(b - a, c - a);
    double l = manifold::la::length(nn);
    if (l < 1e-12) continue;
    nn = nn / l;
    bool seen = false;
    for (const vec3 & o : out) if (manifold::la::dot(o, nn) > 1 - 1e-6) { seen = true; break; }
    if (!seen) out.push_back(nn);
  }
}

struct bits_s {
  /* vertex sets at unit size: the offset bit (reach 1 along every facet
   * direction), the chamfer bit of the variant without interior chamfers
   * and the one of the variant with them; k and kFill their multipliers */
  std::vector<vec3> offset, chamfer, chamferFill;
  double k = 0, kFill = 0;
};

/* rhombic and tetra-octa: cube and cuboctahedron */
bits_s cubeBits() {
  bits_s b;
  for (int sx = -1; sx <= 1; sx += 2)
    for (int sy = -1; sy <= 1; sy += 2)
      for (int sz = -1; sz <= 1; sz += 2)
        b.offset.push_back(vec3(sx, sy, sz));
  /* the polar body of (100), (110) at 1 and (111) at (4 - sqrt2)/sqrt6:
   * the 24 vertices (+-1, +-(sqrt2-1), +-(sqrt2-1)) and permutations */
  const double q = SQRT2 - 1;
  for (int ax = 0; ax < 3; ax++)
    for (int s0 = -1; s0 <= 1; s0 += 2)
      for (int s1 = -1; s1 <= 1; s1 += 2)
        for (int s2 = -1; s2 <= 1; s2 += 2) {
          double p[3];
          p[ax] = s0; p[(ax + 1) % 3] = s1 * q; p[(ax + 2) % 3] = s2 * q;
          b.chamfer.push_back(vec3(p[0], p[1], p[2]));
        }
  b.k = 1 + 1 / SQRT2;
  /* with interior chamfers the octahedron, vertices at 1 on the axes:
   * its edges sweep a cube's edges to bevels with legs exactly the size */
  for (int ax = 0; ax < 3; ax++)
    for (int s0 = -1; s0 <= 1; s0 += 2) { double p[3] = {0, 0, 0}; p[ax] = s0; b.chamferFill.push_back(vec3(p[0], p[1], p[2])); }
  b.kFill = 1;
  return b;
}

/* The planar faces of a boolean result, rebuilt exactly (see exactFaces),
 * and the convex pieces they split into: the sweep of the boundary by a
 * convex bit is the union of one hull per piece, of which a face has few
 * (one hull per triangle bogs the union down). */
struct faces_s {
  std::vector<vec3> pos;                       /* welded, snapped onto their planes */
  std::vector<uint64_t> tri;                   /* the rebuilt triangulation, index triples */
  std::vector<std::vector<vec3> > pieces;      /* convex pieces of the faces */
};

Manifold fromTriangles(const std::vector<vec3> & pos, const std::vector<uint64_t> & tri) {
  manifold::MeshGL64 mesh;
  mesh.numProp = 3;
  mesh.vertProperties.reserve(pos.size() * 3);
  for (const vec3 & p : pos) { mesh.vertProperties.push_back(p.x); mesh.vertProperties.push_back(p.y); mesh.vertProperties.push_back(p.z); }
  mesh.triVerts = tri;
  return Manifold(mesh);
}

/* one cell of the shape for tagging the faces: its index and its faces
 * as outward planes, in getNeighbor's numbering, with their corners and
 * whether the neighbour across them is empty */
struct cellFaces_s {
  unsigned index = 0;
  struct face_s { int n = 0; vec3 normal{}; double d = 0; bool exposed = false; std::vector<vec3> corners; };
  std::vector<face_s> faces;
};

/* the view's split: points closer than this are one, a point closer
 * than this to a line lies on it (cell units: the rebuilt mesh's
 * rounding is far below it, its features far above) */
const double SPLIT_EPS = 1e-7;

/* a point of a facet triangle being split: its vertex, its place in the
 * plane's 2D frame, the triangle's edges it lies on (bit k: corner k to
 * corner k + 1) and its parameter along each of them */
struct splitPt_s { size_t id = 0; double x = 0, y = 0; unsigned edges = 0; double t[3] = { 0, 0, 0 }; };

/* the distance of (x, y) from a convex polygon, 0 inside */
double polygonDistance(double x, double y, const std::vector<std::pair<double, double> > & q) {
  const size_t n = q.size();
  double area = 0;
  for (size_t i = 0; i < n; i++) area += q[i].first * q[(i + 1) % n].second - q[(i + 1) % n].first * q[i].second;
  bool inside = true;
  double best = 1e300;
  for (size_t i = 0; i < n; i++) {
    const double ax = q[i].first, ay = q[i].second, dx = q[(i + 1) % n].first - ax, dy = q[(i + 1) % n].second - ay;
    if ((dx * (y - ay) - dy * (x - ax)) * area < 0) inside = false;
    const double l2 = dx * dx + dy * dy;
    const double s = l2 > 0 ? std::max(0.0, std::min(1.0, ((x - ax) * dx + (y - ay) * dy) / l2)) : 0;
    best = std::min(best, std::hypot(x - ax - s * dx, y - ay - s * dy));
  }
  return inside ? 0 : best;
}

/* do two convex polygons overlap by more than SPLIT_EPS (no edge of
 * either has the other all on its outside, up to that) */
bool overlaps(const std::vector<std::pair<double, double> > & a, const std::vector<std::pair<double, double> > & b) {
  for (int pass = 0; pass < 2; pass++) {
    const std::vector<std::pair<double, double> > & p = pass ? b : a, & q = pass ? a : b;
    const size_t n = p.size();
    double area = 0;
    for (size_t i = 0; i < n; i++) area += p[i].first * p[(i + 1) % n].second - p[(i + 1) % n].first * p[i].second;
    for (size_t i = 0; i < n; i++) {
      const double ax = p[i].first, ay = p[i].second, dx = p[(i + 1) % n].first - ax, dy = p[(i + 1) % n].second - ay;
      const double l = std::hypot(dx, dy);
      if (l == 0) continue;
      double inside = -1e300;   /* how far the other reaches inside this edge */
      for (const auto & r : q) inside = std::max(inside, (dx * (r.second - ay) - dy * (r.first - ax)) / l * (area > 0 ? 1 : -1));
      if (inside < SPLIT_EPS) return false;
    }
  }
  return true;
}

/* does the segment a-b pass through the inside of the triangle p, more
 * than SPLIT_EPS away from its edges */
bool crossesInside(double ax, double ay, double bx, double by, const splitPt_s p[3]) {
  const double area = (p[1].x - p[0].x) * (p[2].y - p[0].y) - (p[2].x - p[0].x) * (p[1].y - p[0].y);
  double lo = 0, hi = 1;
  for (int k = 0; k < 3; k++) {
    const splitPt_s & e0 = p[k], & e1 = p[(k + 1) % 3];
    const double ex = e1.x - e0.x, ey = e1.y - e0.y, el = std::hypot(ex, ey);
    auto inward = [&](double x, double y) { return (ex * (y - e0.y) - ey * (x - e0.x)) / el * (area > 0 ? 1 : -1) - SPLIT_EPS; };
    const double fa = inward(ax, ay), fb = inward(bx, by);
    if (fa <= 0 && fb <= 0) return false;
    if (fa < 0) lo = std::max(lo, fa / (fa - fb));
    if (fb < 0) hi = std::min(hi, fa / (fa - fb));
  }
  return (hi - lo) * std::hypot(bx - ax, by - ay) > SPLIT_EPS;
}

/* the manifold's triangles into the polyhedron, one polyhedron vertex per
 * manifold vertex (the manifold's mesh is a manifold with shared vertex
 * indices; welding by position would merge vertices that only float
 * precision tells apart and break that). Facet faces (normal on a facet
 * direction) plain, everything else flagged as bevel.
 *
 * Every face is tagged with a voxel (the 3D view removes it on a click)
 * and a facet face with the cell face it was inset from (the view adds
 * the neighbour across it). A facet face lies g inside the exposed cell
 * faces of its plane and belongs to the one it was inset from: moved
 * back by g onto the plane, its centroid lies in that face's polygon,
 * even where the face itself passes through a neighbouring cell (the
 * tips of the prism grid's downward triangles above a row's bottom,
 * tetra-octa's slanted neighbours). Beyond them (the strip a concave
 * edge leaves, g cot(a / 2) wide at angle a) it belongs to the nearest
 * exposed face its own flat face lies over somewhere. A flat face over
 * none (the floor the bit's face leaves at a concave vertex or edge), and
 * a bevel, belong to the cell holding their centroid, the one whose
 * planes it violates least, with no cell face.
 *
 * The rebuild merges coplanar faces across cells, so one triangle can
 * span several cell faces. With split (the view), every facet triangle
 * is cut along the lines of the exposed cell faces' edges that cross it,
 * so that each piece lies over one of them, and every triangle gets the
 * cut points on its edges (the mesh stays closed). */
bool addFaces(Polyhedron * poly, const Manifold & m, const std::vector<vec3> & facets, const std::vector<cellFaces_s> & cells, double gAxial, double gSide, bool split, std::string & err) {
  using manifold::la::dot;
  using manifold::la::cross;
  using manifold::la::length;
  manifold::MeshGL64 mesh = m.GetMeshGL64();
  const size_t np = mesh.numProp;
  std::vector<vec3> pos;
  for (size_t i = 0; i * np < mesh.vertProperties.size(); i++) pos.push_back(vec3(mesh.vertProperties[i * np], mesh.vertProperties[i * np + 1], mesh.vertProperties[i * np + 2]));
  const std::vector<uint64_t> & tv = mesh.triVerts;
  const size_t nt = tv.size() / 3;

  /* per triangle: its corners, its unit normal and a 2D frame of its
   * plane, and for a facet triangle the exposed cell faces it was inset
   * from and the pieces it is cut into */
  struct tri_s {
    size_t v[3] = { 0, 0, 0 };
    vec3 n{}, u{}, w{};
    bool facet = false;
    std::vector<std::pair<size_t, size_t> > faces;   /* (cell, face) */
    std::vector<std::vector<splitPt_s> > pieces;
  };
  std::vector<tri_s> tris(nt);
  for (size_t t = 0; t < nt; t++) {
    tri_s & T = tris[t];
    for (int k = 0; k < 3; k++) T.v[k] = tv[3 * t + k];
    const vec3 & p0 = pos[T.v[0]], & p1 = pos[T.v[1]], & p2 = pos[T.v[2]];
    vec3 n = cross(p1 - p0, p2 - p0);
    const double l = length(n);
    if (l < 1e-14) {
      char why[400];
      snprintf(why, sizeof why, "the Minkowski construction left a face without area (%.12g %.12g %.12g) (%.12g %.12g %.12g) (%.12g %.12g %.12g)", p0.x, p0.y, p0.z, p1.x, p1.y, p1.z, p2.x, p2.y, p2.z);
      err = why; return false;
    }
    T.n = n / l;
    for (const vec3 & d : facets) if (dot(d, T.n) > 1 - 1e-4) T.facet = true;
    const vec3 a = std::fabs(T.n.x) < 0.5 ? vec3(1, 0, 0) : vec3(0, 1, 0);
    T.u = cross(T.n, a); T.u = T.u / length(T.u);
    T.w = cross(T.n, T.u);
    if (!T.facet) continue;
    const vec3 c = (p0 + p1 + p2) / 3.0;
    for (size_t k = 0; k < cells.size(); k++)
      for (size_t f = 0; f < cells[k].faces.size(); f++) {
        const cellFaces_s::face_s & cf = cells[k].faces[f];
        if (cf.exposed && dot(cf.normal, T.n) > 1 - 1e-4 &&
            std::fabs(dot(cf.normal, c) - (cf.d - (std::fabs(cf.normal.z) > 0.5 ? gAxial : gSide))) < 1e-6)
          T.faces.push_back(std::make_pair(k, f));
      }
  }

  /* the points cut into the mesh's edges, by edge (lower vertex first),
   * with their parameter from the lower vertex */
  std::map<std::pair<size_t, size_t>, std::vector<std::pair<double, size_t> > > cuts;
  auto edgePoint = [&](size_t a, size_t b, double t) -> size_t {
    if (a > b) { std::swap(a, b); t = 1 - t; }
    std::vector<std::pair<double, size_t> > & on = cuts[std::make_pair(a, b)];
    const double len = length(pos[b] - pos[a]);
    for (const auto & c : on) if (std::fabs(c.first - t) * len < SPLIT_EPS) return c.second;
    const vec3 p = pos[a] + (pos[b] - pos[a]) * t;
    pos.push_back(p);
    on.push_back(std::make_pair(t, pos.size() - 1));
    return pos.size() - 1;
  };
  auto corners = [&](const tri_s & T) {
    std::vector<splitPt_s> P(3);
    for (int k = 0; k < 3; k++) {
      P[k].id = T.v[k];
      P[k].x = dot(pos[T.v[k]], T.u); P[k].y = dot(pos[T.v[k]], T.w);
      P[k].edges = (1u << k) | (1u << ((k + 2) % 3));
      P[k].t[k] = 0; P[k].t[(k + 2) % 3] = 1;
    }
    return P;
  };

  /* the cuts */
  for (size_t t = 0; split && t < nt; t++) {
    tri_s & T = tris[t];
    if (T.faces.empty()) continue;
    const std::vector<splitPt_s> P = corners(T);
    /* the lines of the exposed faces' edges through the triangle's inside,
     * as (normal, offset) with the normal's sign fixed */
    std::vector<std::array<double, 3> > lines;
    for (const auto & fc : T.faces) {
      const std::vector<vec3> & C = cells[fc.first].faces[fc.second].corners;
      for (size_t i = 0; i < C.size(); i++) {
        const double ax = dot(C[i], T.u), ay = dot(C[i], T.w);
        const double bx = dot(C[(i + 1) % C.size()], T.u), by = dot(C[(i + 1) % C.size()], T.w);
        if (!crossesInside(ax, ay, bx, by, P.data())) continue;
        const double l = std::hypot(bx - ax, by - ay);
        double nx = (ay - by) / l, ny = (bx - ax) / l;
        if (nx < -1e-9 || (std::fabs(nx) <= 1e-9 && ny < 0)) { nx = -nx; ny = -ny; }
        const double off = nx * ax + ny * ay;
        bool seen = false;
        for (const auto & L : lines) if (nx * L[0] + ny * L[1] > 1 - 1e-12 && std::fabs(off - L[2]) < SPLIT_EPS) seen = true;
        if (!seen) lines.push_back({ nx, ny, off });
      }
    }
    if (lines.empty()) continue;
    const double off = dot(T.n, pos[T.v[0]]);
    std::vector<splitPt_s> inner;   /* the points cut inside the triangle */
    auto crossing = [&](const splitPt_s & a, const splitPt_s & b, double f) {
      splitPt_s c;
      const unsigned common = a.edges & b.edges;
      if (common) {
        const int k = common & 1 ? 0 : common & 2 ? 1 : 2;
        c.edges = 1u << k;
        c.t[k] = a.t[k] + f * (b.t[k] - a.t[k]);
        c.id = edgePoint(T.v[k], T.v[(k + 1) % 3], c.t[k]);
        c.x = dot(pos[c.id], T.u); c.y = dot(pos[c.id], T.w);
        return c;
      }
      c.x = a.x + f * (b.x - a.x); c.y = a.y + f * (b.y - a.y);
      for (const splitPt_s & q : inner) if (std::hypot(q.x - c.x, q.y - c.y) < SPLIT_EPS) return q;
      const vec3 p = T.u * c.x + T.w * c.y + T.n * off;
      pos.push_back(p);
      c.id = pos.size() - 1;
      inner.push_back(c);
      return c;
    };
    std::vector<std::vector<splitPt_s> > pieces(1, P);
    for (const auto & L : lines) {
      std::vector<std::vector<splitPt_s> > next;
      for (const std::vector<splitPt_s> & Q : pieces) {
        const size_t n = Q.size();
        std::vector<double> s(n);
        bool above = false, below = false;
        for (size_t i = 0; i < n; i++) {
          s[i] = L[0] * Q[i].x + L[1] * Q[i].y - L[2];
          if (s[i] > SPLIT_EPS) above = true;
          if (s[i] < -SPLIT_EPS) below = true;
        }
        if (!above || !below) { next.push_back(Q); continue; }
        std::vector<splitPt_s> A, B;
        for (size_t i = 0; i < n; i++) {
          const size_t j = (i + 1) % n;
          if (s[i] >= -SPLIT_EPS) A.push_back(Q[i]);
          if (s[i] <= SPLIT_EPS) B.push_back(Q[i]);
          if ((s[i] > SPLIT_EPS && s[j] < -SPLIT_EPS) || (s[i] < -SPLIT_EPS && s[j] > SPLIT_EPS)) {
            const splitPt_s c = crossing(Q[i], Q[j], s[i] / (s[i] - s[j]));
            A.push_back(c); B.push_back(c);
          }
        }
        next.push_back(A); next.push_back(B);
      }
      pieces.swap(next);
    }
    T.pieces = pieces;
  }

  /* the output: every piece (or uncut triangle) with the cut points on
   * the triangle's edges added, triangulated as a fan from a corner where
   * no fan triangle is flat, else from its centre */
  std::vector<std::array<size_t, 3> > out;
  std::vector<size_t> parent;
  for (size_t t = 0; t < nt; t++) {
    const tri_s & T = tris[t];
    std::vector<std::vector<splitPt_s> > pieces = T.pieces;
    if (pieces.empty()) pieces.push_back(corners(T));
    for (const std::vector<splitPt_s> & Q : pieces) {
      std::vector<size_t> ids;
      for (size_t i = 0; i < Q.size(); i++) {
        const splitPt_s & a = Q[i], & b = Q[(i + 1) % Q.size()];
        ids.push_back(a.id);
        const unsigned common = a.edges & b.edges;
        if (!common) continue;
        const int k = common & 1 ? 0 : common & 2 ? 1 : 2;
        const size_t e0 = T.v[k], e1 = T.v[(k + 1) % 3];
        auto it = cuts.find(std::make_pair(std::min(e0, e1), std::max(e0, e1)));
        if (it == cuts.end()) continue;
        const double len = length(pos[e1] - pos[e0]);
        std::vector<std::pair<double, size_t> > on;
        for (const auto & c : it->second) {
          const double tk = e0 < e1 ? c.first : 1 - c.first;
          if (std::min(tk - a.t[k], b.t[k] - tk) * len > SPLIT_EPS) on.push_back(std::make_pair(tk, c.second));
          else if (std::min(tk - b.t[k], a.t[k] - tk) * len > SPLIT_EPS) on.push_back(std::make_pair(-tk, c.second));
        }
        std::sort(on.begin(), on.end());
        for (const auto & c : on) ids.push_back(c.second);
      }
      const size_t n = ids.size();
      for (size_t i = 0; i < n; i++)
        if (ids[i] == ids[(i + 1) % n]) { err = "the view's split of the Minkowski mesh made a piece with a repeated corner"; return false; }
      size_t from = n;
      for (size_t s = 0; s < n && from == n && n > 3; s++) {
        bool flat = false;
        for (size_t i = 1; i + 1 < n; i++)
          if (length(cross(pos[ids[(s + i) % n]] - pos[ids[s]], pos[ids[(s + i + 1) % n]] - pos[ids[s]])) < 1e-12) flat = true;
        if (!flat) from = s;
      }
      if (n == 3) from = 0;
      if (from < n) {
        for (size_t i = 1; i + 1 < n; i++) { out.push_back({ ids[from], ids[(from + i) % n], ids[(from + i + 1) % n] }); parent.push_back(t); }
      } else {
        vec3 c(0, 0, 0);
        for (size_t id : ids) c = c + pos[id];
        pos.push_back(c / (double)n);
        for (size_t i = 0; i < n; i++) { out.push_back({ pos.size() - 1, ids[i], ids[(i + 1) % n] }); parent.push_back(t); }
      }
    }
  }

  /* closed: every edge once each way */
  if (split) {
    std::map<std::pair<size_t, size_t>, int> edges;
    for (const auto & o : out) for (int k = 0; k < 3; k++) edges[std::make_pair(o[k], o[(k + 1) % 3])]++;
    for (const auto & e : edges)
      if (e.second != 1 || edges.find(std::make_pair(e.first.second, e.first.first)) == edges.end()) {
        err = "the view's split of the Minkowski mesh left it open"; return false;
      }
  }

  /* the flat faces: facet triangles joined across their shared edges
   * when coplanar */
  std::vector<size_t> region(nt);
  for (size_t t = 0; t < nt; t++) region[t] = t;
  std::function<size_t(size_t)> root = [&](size_t t) { return region[t] == t ? t : region[t] = root(region[t]); };
  {
    std::map<std::pair<size_t, size_t>, size_t> owner;
    for (size_t t = 0; t < nt; t++) for (int k = 0; k < 3; k++) owner[std::make_pair(tris[t].v[k], tris[t].v[(k + 1) % 3])] = t;
    for (size_t t = 0; t < nt; t++) {
      if (!tris[t].facet) continue;
      for (int k = 0; k < 3; k++) {
        auto it = owner.find(std::make_pair(tris[t].v[(k + 1) % 3], tris[t].v[k]));
        if (it == owner.end() || !tris[it->second].facet || dot(tris[it->second].n, tris[t].n) < 1 - 1e-9) continue;
        region[root(t)] = root(it->second);
      }
    }
  }

  /* the exposed faces each flat face lies over somewhere */
  auto polygon2 = [&](const tri_s & T, const std::vector<vec3> & corners) {
    std::vector<std::pair<double, double> > q;
    for (const vec3 & p : corners) q.push_back(std::make_pair(dot(p, T.u), dot(p, T.w)));
    return q;
  };
  std::map<size_t, std::set<std::pair<size_t, size_t> > > covers;   /* region -> (cell, face) */
  for (size_t t = 0; t < nt; t++) {
    const tri_s & T = tris[t];
    const std::vector<std::pair<double, double> > a = polygon2(T, { pos[T.v[0]], pos[T.v[1]], pos[T.v[2]] });
    for (const auto & fc : T.faces)
      if (overlaps(a, polygon2(T, cells[fc.first].faces[fc.second].corners))) covers[root(t)].insert(fc);
  }

  std::vector<int> id(pos.size());
  std::vector<Vector3Df> fp(pos.size());
  for (size_t i = 0; i < pos.size(); i++) {
    fp[i] = Vector3Df((float)pos[i].x, (float)pos[i].y, (float)pos[i].z);
    id[i] = poly->addVertex(fp[i])->index();
  }
  for (size_t o = 0; o < out.size(); o++) {
    const tri_s & T = tris[parent[o]];
    const vec3 & p0 = pos[out[o][0]], & p1 = pos[out[o][1]], & p2 = pos[out[o][2]];
    /* the polyhedron holds single precision, and a face that has no area
     * there has no normal (the STL writer asserts on it) */
    Vector3Df fn = (fp[out[o][1]] - fp[out[o][0]]) ^ (fp[out[o][2]] - fp[out[o][0]]);
    if (length(cross(p1 - p0, p2 - p0)) < 1e-14 || fn.squaredModule() == 0) {
      char why[400];
      snprintf(why, sizeof why, "the Minkowski mesh has a face without area (%.12g %.12g %.12g) (%.12g %.12g %.12g) (%.12g %.12g %.12g)", p0.x, p0.y, p0.z, p1.x, p1.y, p1.z, p2.x, p2.y, p2.z);
      err = why; return false;
    }
    size_t best = 0;
    int side = -1;
    /* the nearest exposed face its flat face covers */
    const vec3 c = (p0 + p1 + p2) / 3.0;
    auto it = covers.find(root(parent[o]));
    if (it != covers.end()) {
      double bestDistance = 1e300;
      for (const auto & fc : it->second) {
        const double d = polygonDistance(dot(c, T.u), dot(c, T.w), polygon2(T, cells[fc.first].faces[fc.second].corners));
        if (d < bestDistance) { bestDistance = d; best = fc.first; side = cells[fc.first].faces[fc.second].n; }
      }
    } else {
      /* the cell holding the centroid: the one it violates least */
      double bestViolation = 1e300;
      for (size_t k = 0; k < cells.size(); k++) {
        double violation = -1e300;
        for (const cellFaces_s::face_s & cf : cells[k].faces) violation = std::max(violation, dot(cf.normal, c) - cf.d);
        if (violation < bestViolation) { bestViolation = violation; best = k; }
      }
    }
    Face * f = poly->addFace(id[out[o][0]], id[out[o][1]], id[out[o][2]]);
    f->_fb_face = side; f->_fb_index = cells.empty() ? 0 : (int)cells[best].index;
    f->_flags = T.facet ? 0 : FF_BEVEL_FACE;
  }
  return true;
}

typedef __int128 i128;

/* ---------------------------------------------------------------------
 * The exact construction.
 *
 * Everything happens in an integer frame: one cell is FRAME units (on the
 * prism grid, one lattice step is PRISM_FRAME units, see below), the
 * gap and the bevel are rounded once to whole units, every step size is
 * derived from them exactly, and every true vertex of every step is then
 * a whole number of units (the planes all have small integer normals and
 * integer offsets, and FRAME carries the factors their intersections
 * divide by). The booleans are Manifold's; around them nothing is
 * measured against a tolerance:
 *  - each piece of the surface is swept with Manifold::Hull, whose
 *    corners are exact since the points are whole numbers of units;
 *  - each boolean result is rebuilt from exact planes: every triangle is
 *    put on its integer plane (normal recovered as a small integer
 *    vector, offset rounded to a whole unit), every vertex on three or
 *    more planes is replaced by their exact intersection, every vertex
 *    on only two planes (a point along a straight edge) is dropped, and
 *    each planar face is re-triangulated from integer coordinates, with
 *    its area checked exactly.
 * Anything that does not fit (a triangle on no integer plane, a corner
 * that is not a whole unit, a face whose triangulation does not cover
 * it) is an error, never patched over.
 * ------------------------------------------------------------------- */

/* units per cell: 12 for the thirds and quarters plane intersections
 * divide by, 10^4 so that a gap or bevel with four decimals of a cell is
 * exact, 2^10 for headroom */
const double FRAME = 12.0 * 10000.0 * 1024.0;

/* a size in whole units, rounded to a multiple of 12, so that the planes
 * it moves meet in whole units (their intersections divide by 1, 2, 3 or 4) */
int64_t units(double x) { return 12 * std::llround(x / 12); }

struct iplane_s { int64_t a = 0, b = 0, c = 0, d = 0; };
inline bool operator<(const iplane_s & x, const iplane_s & y) { return std::tie(x.a, x.b, x.c, x.d) < std::tie(y.a, y.b, y.c, y.d); }

int64_t gcd64(int64_t a, int64_t b) { a = a < 0 ? -a : a; b = b < 0 ? -b : b; while (b) { int64_t t = a % b; a = b; b = t; } return a; }

/* the small integer vector a float normal is a multiple of: components up
 * to a thousand (the prism's bevel planes have components in the
 * hundreds). Two such vectors differ in direction by at least a millionth,
 * far above the noise of a triangle normal, which is only taken from
 * triangles with an altitude of a thousand units or more. */
bool intNormal(const vec3 & n, int64_t out[3]) {
  double mx = std::max(std::fabs(n.x), std::max(std::fabs(n.y), std::fabs(n.z)));
  if (mx <= 0) return false;
  const double v[3] = { n.x / mx, n.y / mx, n.z / mx };
  for (int q = 1; q <= 1000; q++) {
    int64_t r[3]; bool ok = true;
    for (int k = 0; k < 3; k++) { r[k] = std::llround(v[k] * q); if (std::fabs(v[k] * q - r[k]) > 1e-5) ok = false; }
    if (!ok) continue;
    int64_t g = gcd64(gcd64(r[0], r[1]), r[2]);
    for (int k = 0; k < 3; k++) out[k] = r[k] / g;
    return true;
  }
  return false;
}

struct ipt { int64_t x, y, z; };
inline i128 planeValue(const iplane_s & P, const ipt & p) { return (i128)P.a * p.x + (i128)P.b * p.y + (i128)P.c * p.z - P.d; }

/* convex pieces of a face from its triangulation, in exact integer 2D
 * coordinates (counterclockwise): triangles merged across shared edges
 * while the union stays convex (collinear corners allowed) */
void convexPiecesExact(const std::vector<std::array<int64_t, 2> > & p2, const std::vector<int> & tt, std::vector<std::vector<int> > & out) {
  auto cross = [&](int a, int b, int c) { return (i128)(p2[b][0] - p2[a][0]) * (p2[c][1] - p2[a][1]) - (i128)(p2[b][1] - p2[a][1]) * (p2[c][0] - p2[a][0]); };
  std::vector<std::vector<int> > poly;
  for (size_t t = 0; t + 2 < tt.size(); t += 3) poly.push_back({ tt[t], tt[t + 1], tt[t + 2] });
  std::vector<bool> alive(poly.size(), true);
  std::map<std::pair<int, int>, size_t> edgeOwner;
  for (size_t i = 0; i < poly.size(); i++) for (size_t c = 0; c < 3; c++) edgeOwner[std::make_pair(poly[i][c], poly[i][(c + 1) % 3])] = i;
  auto convex = [&](const std::vector<int> & p) {
    std::set<int> seen;
    for (int v : p) if (!seen.insert(v).second) return false;
    for (size_t i = 0; i < p.size(); i++) if (cross(p[i], p[(i + 1) % p.size()], p[(i + 2) % p.size()]) < 0) return false;
    return true;
  };
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t i = 0; i < poly.size(); i++) {
      if (!alive[i]) continue;
      for (size_t c = 0; c < poly[i].size(); c++) {
        int a = poly[i][c], b = poly[i][(c + 1) % poly[i].size()];
        auto it = edgeOwner.find(std::make_pair(b, a));
        if (it == edgeOwner.end() || it->second == i || !alive[it->second]) continue;
        const std::vector<int> & q = poly[it->second];
        size_t ia = 0;
        while (q[ia] != a) ia++;
        std::vector<int> merged;
        for (size_t d = 1; d <= poly[i].size(); d++) merged.push_back(poly[i][(c + d) % poly[i].size()]);
        for (size_t d = 1; d + 1 < q.size(); d++) merged.push_back(q[(ia + d) % q.size()]);
        if (!convex(merged)) continue;
        size_t j = it->second;
        for (size_t d = 0; d < q.size(); d++) edgeOwner.erase(std::make_pair(q[d], q[(d + 1) % q.size()]));
        for (size_t d = 0; d < poly[i].size(); d++) edgeOwner.erase(std::make_pair(poly[i][d], poly[i][(d + 1) % poly[i].size()]));
        poly[i] = merged; alive[j] = false;
        for (size_t d = 0; d < merged.size(); d++) edgeOwner[std::make_pair(merged[d], merged[(d + 1) % merged.size()])] = i;
        changed = true;
        break;
      }
    }
  }
  for (size_t i = 0; i < poly.size(); i++) if (alive[i]) out.push_back(poly[i]);
}

/* the exact rebuild of a boolean result: see the comment at the top */
bool exactFaces(const Manifold & m, faces_s & out, std::string & err) {
  out.pos.clear(); out.tri.clear(); out.pieces.clear();
  manifold::MeshGL64 G = m.GetMeshGL64();
  const size_t np = G.numProp;
  std::vector<vec3> & pos = out.pos;
  for (size_t i = 0; i * np < G.vertProperties.size(); i++) pos.push_back(vec3(G.vertProperties[i * np], G.vertProperties[i * np + 1], G.vertProperties[i * np + 2]));
  const std::vector<uint64_t> & tv = G.triVerts;
  const size_t nt = tv.size() / 3, nv = pos.size();
  if (!nt) return true;
  auto P = [&](size_t t, int c) -> const vec3 & { return pos[tv[3 * t + c]]; };
  std::map<std::pair<uint64_t, uint64_t>, size_t> owner;
  for (size_t t = 0; t < nt; t++) for (int c = 0; c < 3; c++) owner[std::make_pair(tv[3 * t + c], tv[3 * t + (c + 1) % 3])] = t;
  auto neighbour = [&](size_t t, int c) -> long {
    auto it = owner.find(std::make_pair(tv[3 * t + (c + 1) % 3], tv[3 * t + c]));
    return it == owner.end() ? -1 : (long)it->second;
  };
  /* A point on a plane leaves a residual of rounding only: the booleans'
   * new vertices and the evaluation itself err relative to the size of
   * the terms (measured up to about 120 epsilon of |normal| * |coordinate|),
   * so the tolerance is relative too, with four times that margin. Planes
   * have integer offsets, so anything below half a unit is unambiguous; a
   * shape so large that the tolerance gets near that is refused. */
  double maxCoord = 0;
  for (const vec3 & p : pos) maxCoord = std::max(maxCoord, std::max(std::fabs(p.x), std::max(std::fabs(p.y), std::fabs(p.z))));
  bool tooLarge = false;
  auto onPlane = [&](const iplane_s & pl, const vec3 & p) {
    const double tol = 512 * std::numeric_limits<double>::epsilon() * (double)(std::llabs(pl.a) + std::llabs(pl.b) + std::llabs(pl.c)) * maxCoord;
    if (tol > 0.4) tooLarge = true;
    long double v = (long double)pl.a * p.x + (long double)pl.b * p.y + (long double)pl.c * p.z - pl.d;
    return std::fabs((double)v) < tol;
  };
  /* 1. every triangle with a decent altitude on its integer plane */
  std::vector<iplane_s> planes;
  std::map<iplane_s, int> planeId;
  std::vector<int> plane(nt, -1);
  for (size_t t = 0; t < nt; t++) {
    vec3 n = manifold::la::cross(P(t, 1) - P(t, 0), P(t, 2) - P(t, 0));
    double l = manifold::la::length(n), longest = 0;
    for (int c = 0; c < 3; c++) longest = std::max(longest, manifold::la::length(P(t, (c + 1) % 3) - P(t, c)));
    if (longest <= 0 || l / longest < 1000) continue;            /* altitude under 1e-5 of a cell: its normal is noise */
    int64_t in[3];
    if (!intNormal(n / l, in)) {
      char why[200];
      snprintf(why, sizeof why, "a face of a Minkowski step is not on a plane with a small integer normal (normal %.9f %.9f %.9f, altitude %.3g units)", n.x / l, n.y / l, n.z / l, l / longest);
      err = why; return false;
    }
    iplane_s pl; pl.a = in[0]; pl.b = in[1]; pl.c = in[2];
    long double dv = (long double)pl.a * P(t, 0).x + (long double)pl.b * P(t, 0).y + (long double)pl.c * P(t, 0).z;
    pl.d = std::llround((double)dv);
    for (int c = 0; c < 3; c++) if (!onPlane(pl, P(t, c))) { err = "a triangle of a Minkowski step is not on a whole-unit plane"; return false; }
    if (tooLarge) { err = "the shape is too large for the exact Minkowski construction (rounding near half a unit)"; return false; }
    auto it = planeId.find(pl);
    if (it == planeId.end()) { it = planeId.insert(std::make_pair(pl, (int)planes.size())).first; planes.push_back(pl); }
    plane[t] = it->second;
  }
  /* 2. every sliver on the plane of a neighbour that all its corners are on */
  for (bool changed = true; changed;) {
    changed = false;
    for (size_t t = 0; t < nt; t++) {
      if (plane[t] >= 0) continue;
      for (int c = 0; c < 3 && plane[t] < 0; c++) {
        long u = neighbour(t, c);
        if (u < 0 || plane[u] < 0) continue;
        const iplane_s & pl = planes[plane[u]];
        if (onPlane(pl, P(t, 0)) && onPlane(pl, P(t, 1)) && onPlane(pl, P(t, 2))) { plane[t] = plane[u]; changed = true; }
      }
    }
  }
  for (size_t t = 0; t < nt; t++) if (plane[t] < 0) { err = "a sliver of a Minkowski step lies on no plane of its neighbours"; return false; }
  /* 3. vertices: the planes around each; three independent ones make a
   * corner, put at their exact intersection; two make a point along a
   * straight edge, dropped */
  std::vector<std::set<int> > vplanes(nv);
  for (size_t t = 0; t < nt; t++) for (int c = 0; c < 3; c++) vplanes[tv[3 * t + c]].insert(plane[t]);
  enum { UNUSED, CORNER, EDGE, INSIDE };
  std::vector<int> kind(nv, UNUSED);
  std::vector<ipt> exact(nv);
  for (size_t v = 0; v < nv; v++) {
    if (vplanes[v].empty()) continue;
    std::vector<int> ps(vplanes[v].begin(), vplanes[v].end());
    int found[3] = { -1, -1, -1 }; i128 det = 0;
    for (size_t i = 0; i < ps.size() && found[0] < 0; i++)
      for (size_t j = i + 1; j < ps.size() && found[0] < 0; j++)
        for (size_t k = j + 1; k < ps.size() && found[0] < 0; k++) {
          const iplane_s & A = planes[ps[i]], & B = planes[ps[j]], & C = planes[ps[k]];
          i128 dd = (i128)A.a * ((i128)B.b * C.c - (i128)B.c * C.b) - (i128)A.b * ((i128)B.a * C.c - (i128)B.c * C.a) + (i128)A.c * ((i128)B.a * C.b - (i128)B.b * C.a);
          if (dd != 0) { found[0] = ps[i]; found[1] = ps[j]; found[2] = ps[k]; det = dd; }
        }
    if (found[0] < 0) {
      /* the normals span a plane (an edge) or a line (inside a face) */
      bool line = false;
      for (size_t i = 0; i < ps.size() && !line; i++)
        for (size_t j = i + 1; j < ps.size() && !line; j++) {
          const iplane_s & A = planes[ps[i]], & B = planes[ps[j]];
          if ((i128)A.b * B.c - (i128)A.c * B.b != 0 || (i128)A.c * B.a - (i128)A.a * B.c != 0 || (i128)A.a * B.b - (i128)A.b * B.a != 0) line = true;
        }
      kind[v] = line ? EDGE : INSIDE;
      continue;
    }
    /* Cramer */
    const iplane_s & A = planes[found[0]], & B = planes[found[1]], & C = planes[found[2]];
    i128 dx = (i128)A.d * ((i128)B.b * C.c - (i128)B.c * C.b) - (i128)A.b * ((i128)B.d * C.c - (i128)B.c * C.d) + (i128)A.c * ((i128)B.d * C.b - (i128)B.b * C.d);
    i128 dy = (i128)A.a * ((i128)B.d * C.c - (i128)B.c * C.d) - (i128)A.d * ((i128)B.a * C.c - (i128)B.c * C.a) + (i128)A.c * ((i128)B.a * C.d - (i128)B.d * C.a);
    i128 dz = (i128)A.a * ((i128)B.b * C.d - (i128)B.d * C.b) - (i128)A.b * ((i128)B.a * C.d - (i128)B.d * C.a) + (i128)A.d * ((i128)B.a * C.b - (i128)B.b * C.a);
    if (dx % det || dy % det || dz % det) {
      auto g128 = [](i128 a, i128 b) { if (a < 0) a = -a; if (b < 0) b = -b; while (b) { i128 t = a % b; a = b; b = t; } return a; };
      i128 den = det / g128(g128(g128(dx, det), dy), dz);
      if (den < 0) den = -den;
      char why[400];
      snprintf(why, sizeof why, "a corner of a Minkowski step is not a whole number of units (denominator %lld; planes (%lld %lld %lld) (%lld %lld %lld) (%lld %lld %lld); %zu planes there)",
               (long long)den, (long long)A.a, (long long)A.b, (long long)A.c, (long long)B.a, (long long)B.b, (long long)B.c, (long long)C.a, (long long)C.b, (long long)C.c, ps.size());
      err = why; return false;
    }
    ipt q{ (int64_t)(dx / det), (int64_t)(dy / det), (int64_t)(dz / det) };
    for (int p : ps) if (planeValue(planes[p], q) != 0) { err = "a corner of a Minkowski step is not on all the planes around it"; return false; }
    if (std::fabs(pos[v].x - q.x) > 1 || std::fabs(pos[v].y - q.y) > 1 || std::fabs(pos[v].z - q.z) > 1) { err = "a corner of a Minkowski step is far from its exact place"; return false; }
    kind[v] = CORNER; exact[v] = q;
  }
  /* 4. faces: triangles on one plane joined across shared edges */
  std::vector<size_t> parent(nt);
  for (size_t t = 0; t < nt; t++) parent[t] = t;
  std::function<size_t(size_t)> find = [&](size_t i) { while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; } return i; };
  for (size_t t = 0; t < nt; t++) for (int c = 0; c < 3; c++) { long u = neighbour(t, c); if (u >= 0 && plane[u] == plane[t]) parent[find(t)] = find((size_t)u); }
  std::map<size_t, std::vector<size_t> > groups;
  for (size_t t = 0; t < nt; t++) groups[find(t)].push_back(t);
  /* exact positions out, for the corners */
  for (size_t v = 0; v < nv; v++) if (kind[v] == CORNER) pos[v] = vec3((double)exact[v].x, (double)exact[v].y, (double)exact[v].z);
  for (const auto & g : groups) {
    const std::vector<size_t> & members = g.second;
    const iplane_s & pl = planes[plane[members[0]]];
    const vec3 n = manifold::la::normalize(vec3((double)pl.a, (double)pl.b, (double)pl.c));
    /* the outline loops, taking the sharpest left turn where one touches itself */
    std::set<size_t> inGroup(members.begin(), members.end());
    std::multimap<uint64_t, uint64_t> next;
    for (size_t t : members) for (int c = 0; c < 3; c++) { long u = neighbour(t, c); if (u >= 0 && inGroup.count((size_t)u)) continue; next.insert(std::make_pair(tv[3 * t + c], tv[3 * t + (c + 1) % 3])); }
    std::vector<std::vector<uint64_t> > loops;
    std::set<std::pair<uint64_t, uint64_t> > used;
    for (const auto & e0 : next) {
      if (used.count(e0)) continue;
      std::vector<uint64_t> loop;
      std::pair<uint64_t, uint64_t> e = e0;
      bool closed = false;
      for (size_t guard = 0; guard <= next.size() + 1; guard++) {
        used.insert(e); loop.push_back(e.first);
        uint64_t v = e.second;
        if (v == e0.first) { closed = true; break; }
        auto range = next.equal_range(v);
        std::pair<uint64_t, uint64_t> pick(0, 0); bool have = false; double bestAng = -10;
        vec3 din = pos[v] - pos[e.first];
        for (auto it = range.first; it != range.second; ++it) {
          std::pair<uint64_t, uint64_t> cand(it->first, it->second);
          if (used.count(cand)) continue;
          vec3 dout = pos[cand.second] - pos[v];
          double ang = std::atan2(manifold::la::dot(manifold::la::cross(din, dout), n), manifold::la::dot(din, dout));
          if (!have || ang > bestAng) { bestAng = ang; pick = cand; have = true; }
        }
        if (!have) break;
        e = pick;
      }
      if (!closed) { err = "the outline of a face of a Minkowski step does not close"; return false; }
      loops.push_back(loop);
    }
    /* only the corners: points along straight edges and inside the face go */
    std::vector<std::vector<uint64_t> > kept;
    for (const std::vector<uint64_t> & loop : loops) {
      std::vector<uint64_t> l;
      for (uint64_t v : loop) {
        if (kind[v] == CORNER) l.push_back(v);
        else if (kind[v] == INSIDE) { err = "a face outline of a Minkowski step passes a point with only that face around it"; return false; }
      }
      if (l.size() < 3) { err = "a face outline of a Minkowski step has fewer than three corners"; return false; }
      kept.push_back(l);
    }
    /* exact 2D coordinates: drop the axis the normal leans on most, keep counterclockwise */
    const int64_t nn[3] = { pl.a, pl.b, pl.c };
    int ax = 0; for (int k = 1; k < 3; k++) if (std::llabs(nn[k]) > std::llabs(nn[ax])) ax = k;
    int u0 = (ax + 1) % 3, u1 = (ax + 2) % 3;
    if (nn[ax] < 0) std::swap(u0, u1);
    std::vector<std::array<int64_t, 2> > p2;
    std::vector<uint64_t> flat;
    std::vector<std::vector<int> > idx(kept.size());
    for (size_t l = 0; l < kept.size(); l++)
      for (uint64_t v : kept[l]) {
        const int64_t c3[3] = { exact[v].x, exact[v].y, exact[v].z };
        idx[l].push_back((int)p2.size()); p2.push_back({ c3[u0], c3[u1] }); flat.push_back(v);
      }
    auto cross2 = [&](int a, int b, int c) { return (i128)(p2[b][0] - p2[a][0]) * (p2[c][1] - p2[a][1]) - (i128)(p2[b][1] - p2[a][1]) * (p2[c][0] - p2[a][0]); };
    i128 outline = 0;
    for (size_t l = 0; l < kept.size(); l++)
      for (size_t q = 0; q < idx[l].size(); q++) { const auto & a = p2[idx[l][q]], & b = p2[idx[l][(q + 1) % idx[l].size()]]; outline += (i128)a[0] * b[1] - (i128)a[1] * b[0]; }
    if (outline <= 0) { err = "a face of a Minkowski step has no area (or runs backwards)"; return false; }
    std::vector<int> tt;
    {
      /* Where cells touch along an edge (no gap), a face's outline can pass
       * one point twice through two different corners: a slit, with the
       * face on both sides. Each such corner is moved a quarter unit into
       * the face on its own side (the face lies left of its outline) for
       * the triangulator, which would otherwise close the slit with a
       * triangle of no area; every other corner is a whole unit away. The
       * exact checks below use the true coordinates. */
      std::map<std::array<int64_t, 2>, int> seenAt;
      for (const auto & q : p2) seenAt[q]++;
      manifold::Polygons polys(kept.size());
      for (size_t l = 0; l < kept.size(); l++)
        for (size_t q = 0; q < idx[l].size(); q++) {
          const int i = idx[l][q];
          manifold::vec2 at((double)p2[i][0], (double)p2[i][1]);
          if (seenAt[p2[i]] > 1) {
            const auto & a = p2[idx[l][(q + idx[l].size() - 1) % idx[l].size()]], & c = p2[idx[l][(q + 1) % idx[l].size()]];
            const manifold::vec2 din = manifold::la::normalize(manifold::vec2((double)(p2[i][0] - a[0]), (double)(p2[i][1] - a[1])));
            const manifold::vec2 dout = manifold::la::normalize(manifold::vec2((double)(c[0] - p2[i][0]), (double)(c[1] - p2[i][1])));
            const manifold::vec2 left = manifold::vec2(-din.y, din.x) + manifold::vec2(-dout.y, dout.x);
            if (manifold::la::length(left) > 1e-9) at += manifold::la::normalize(left) * 0.25;
          }
          polys[l].push_back(at);
        }
      std::vector<manifold::ivec3> tri3;
      try { tri3 = manifold::Triangulate(polys, 0.01); }
      catch (...) { err = "a face of a Minkowski step could not be triangulated"; return false; }
      for (const manifold::ivec3 & t3 : tri3) { tt.push_back(t3.x); tt.push_back(t3.y); tt.push_back(t3.z); }
    }
    i128 covered = 0;
    for (size_t k = 0; k + 2 < tt.size(); k += 3) {
      i128 a2 = cross2(tt[k], tt[k + 1], tt[k + 2]);
      /* a triangle without area (an ear across a corner on a straight run
       * of the outline) is flipped away in rebuiltExact */
      if (a2 < 0) { err = "the triangulation of a face of a Minkowski step has a triangle turned over"; return false; }
      covered += a2;
    }
    if (covered != outline) { err = "the triangulation of a face of a Minkowski step does not cover it exactly"; return false; }
    for (int i : tt) out.tri.push_back(flat[i]);
    std::vector<std::vector<int> > cp;
    convexPiecesExact(p2, tt, cp);
    for (const std::vector<int> & p : cp) {
      std::vector<vec3> piece;
      for (int i : p) piece.push_back(pos[flat[i]]);
      out.pieces.push_back(piece);
    }
  }
  return true;
}

/* zero-area triangles (three exactly collinear corners, where a corner of
 * other faces sits on a straight run of this face's outline) flipped with
 * their neighbour across the longest edge, which is on the same plane */
void flipFlatExact(const std::vector<vec3> & pos, std::vector<uint64_t> & tri) {
  auto I = [&](uint64_t v) { return ipt{ (int64_t)pos[v].x, (int64_t)pos[v].y, (int64_t)pos[v].z }; };
  auto flat = [&](size_t t) {
    ipt a = I(tri[3 * t]), b = I(tri[3 * t + 1]), c = I(tri[3 * t + 2]);
    i128 ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    return uy * vz - uz * vy == 0 && uz * vx - ux * vz == 0 && ux * vy - uy * vx == 0;
  };
  std::map<std::pair<uint64_t, uint64_t>, size_t> owner;
  auto own = [&](size_t t) { for (int c = 0; c < 3; c++) owner[std::make_pair(tri[3 * t + c], tri[3 * t + (c + 1) % 3])] = t; };
  auto disown = [&](size_t t) { for (int c = 0; c < 3; c++) owner.erase(std::make_pair(tri[3 * t + c], tri[3 * t + (c + 1) % 3])); };
  for (size_t t = 0; t * 3 < tri.size(); t++) own(t);
  for (int round = 0; round < 16; round++) {
    bool any = false;
    for (size_t t = 0; t * 3 < tri.size(); t++) {
      if (!flat(t)) continue;
      int m = 0; double longest = -1;
      for (int c = 0; c < 3; c++) { double e = manifold::la::length(pos[tri[3 * t + (c + 1) % 3]] - pos[tri[3 * t + (c + 2) % 3]]); if (e > longest) { longest = e; m = c; } }
      uint64_t M = tri[3 * t + m], A = tri[3 * t + (m + 1) % 3], B = tri[3 * t + (m + 2) % 3];
      auto it = owner.find(std::make_pair(B, A));
      if (it == owner.end() || it->second == t) continue;
      size_t u = it->second;
      uint64_t D = 0;
      for (int c = 0; c < 3; c++) if (tri[3 * u + c] != A && tri[3 * u + c] != B) D = tri[3 * u + c];
      disown(t); disown(u);
      tri[3 * t] = A; tri[3 * t + 1] = D; tri[3 * t + 2] = M;
      tri[3 * u] = D; tri[3 * u + 1] = B; tri[3 * u + 2] = M;
      own(t); own(u);
      any = true;
    }
    if (!any) break;
  }
}

bool rebuiltExact(faces_s & f, Manifold & out, std::string & err) {
  flipFlatExact(f.pos, f.tri);
  std::vector<bool> used(f.pos.size(), false);
  for (size_t t = 0; t + 2 < f.tri.size(); t += 3) {
    const vec3 & a = f.pos[f.tri[t]], & b = f.pos[f.tri[t + 1]], & c = f.pos[f.tri[t + 2]];
    const i128 ux = (i128)(b.x - a.x), uy = (i128)(b.y - a.y), uz = (i128)(b.z - a.z);
    const i128 vx = (i128)(c.x - a.x), vy = (i128)(c.y - a.y), vz = (i128)(c.z - a.z);
    if (uy * vz - uz * vy == 0 && uz * vx - ux * vz == 0 && ux * vy - uy * vx == 0) {
      err = "a face of a Minkowski step kept a triangle without area"; return false;
    }
    for (int k = 0; k < 3; k++) used[f.tri[t + k]] = true;
  }
  out = fromTriangles(f.pos, f.tri);
  if (out.Status() != Manifold::Error::NoError) { err = "the exact rebuild of a Minkowski step is not a valid solid"; return false; }
  /* the next step sweeps the pieces of f, so the solid must be exactly the
   * mesh they came from: nothing merged or dropped on the way in */
  size_t nv = 0;
  for (bool u : used) nv += u;
  if (out.NumTri() != f.tri.size() / 3 || out.NumVert() != nv) {
    err = "the exact rebuild of a Minkowski step was changed on its way into Manifold"; return false;
  }
  return true;
}

/* one step: the solid plus or minus the sweep of its surface by the bit
 * (size in whole units) */
bool stepExact(Manifold & m, faces_s & f, const std::vector<vec3> & unit, int64_t size, bool add, std::string & err) {
  if (size <= 0) return true;
  std::vector<Manifold> hulls;
  for (const std::vector<vec3> & piece : f.pieces) {
    std::vector<vec3> pts;
    for (const vec3 & p : piece) for (const vec3 & b : unit) pts.push_back(p + b * (double)size);
    hulls.push_back(Manifold::Hull(pts));
    if (hulls.back().Status() != Manifold::Error::NoError) { err = "the hull of a piece of a Minkowski step is not a valid solid"; return false; }
    if (hulls.back().IsEmpty()) { err = "a piece of a Minkowski step sweeps into nothing"; return false; }
  }
  Manifold tube = Manifold::BatchBoolean(hulls, manifold::OpType::Add);
  Manifold u = outerShells(add ? m + tube : m - tube);
  faces_s g;
  if (!exactFaces(u, g, err)) return false;
  if (!rebuiltExact(g, m, err)) return false;
  /* the next step's pieces: the faces just rebuilt, which are exactly m's
   * (rebuiltExact checks that) */
  f = std::move(g);
  return true;
}

/* The bevel slope: the bipyramid's height over its hexagon's apothem must
 * be 1 for 45 degrees, i.e. its height over the hexagon's circumradius
 * sqrt3/2, which no small fraction is. 181/209 is the continued-fraction
 * convergent just above it: 45.0001 degrees, on the steep side, so never an
 * overhang. Both bits are whole multiples of this hexagon and height, so
 * every step size is one integer and the steps cancel exactly. */
const int64_t PRISM_HEX = 209, PRISM_HEIGHT = 181;

/* gap and bevel in multiples of the unit bits: 12 * 10^4 of them per
 * cell, so that a gap or bevel with four decimals of a cell is a whole
 * multiple of 12 bits (FRAME's 2^10 of headroom is for the irrational step
 * sizes of the rhombic bits; the prism's step sizes are all whole multiples
 * of the gap and the bevel, and its plane normals run into the hundreds, so
 * it does without the headroom and keeps that much more precision) */
const double PRISM_SCALE = 12.0 * 10000.0;

/* units per lattice step: the bits reach PRISM_HEIGHT units, 1 / PRISM_SCALE
 * of a cell */
const double PRISM_FRAME = PRISM_SCALE * PRISM_HEIGHT;

/* The prism grid in its lattice frame: coordinates (u, v, w) along
 * (1, 0, 0), (1/2, sqrt3/2, 0) and the axis (0, 0, 1), PRISM_FRAME units
 * per lattice step. Every cell corner is then a whole lattice point, and so,
 * with the bits below, is every true vertex of every step. Planes and
 * convexity survive the change of basis, so the booleans and the exact
 * rebuild work there unchanged; only the way back multiplies by sqrt3. */
const double PRISM_ROW = std::sqrt(3.0) / 2;

bool prismToLattice(const Manifold & body, Manifold & out, std::string & err) {
  manifold::MeshGL64 g = body.GetMeshGL64();
  const size_t np = g.numProp;
  for (size_t i = 0; i * np < g.vertProperties.size(); i++) {
    double * p = &g.vertProperties[i * np];
    /* x is a multiple of 1/2, y of sqrt3/2, z whole */
    const double i2 = 2 * p[0], j = p[1] / PRISM_ROW, k = p[2];
    const int64_t I = std::llround(i2), J = std::llround(j), K = std::llround(k);
    if (std::fabs(i2 - I) > 1e-4 || std::fabs(j - J) > 1e-4 || std::fabs(k - K) > 1e-4 || ((I - J) & 1)) {
      err = "prism grid: a cell corner is not on the triangular lattice"; return false;
    }
    p[0] = (double)((I - J) / 2) * PRISM_FRAME; p[1] = (double)J * PRISM_FRAME; p[2] = (double)K * PRISM_FRAME;
  }
  out = Manifold(g);
  if (out.Status() != Manifold::Error::NoError) { err = "prism grid: the shape in the lattice frame is not a valid solid"; return false; }
  return true;
}

Manifold prismFromLattice(const Manifold & m) {
  /* (u, v, w) -> (u + v / 2, v sqrt3 / 2, w), all over PRISM_FRAME */
  const double f = PRISM_FRAME;
  const manifold::mat3x4 t({ 1 / f, 0, 0 }, { 0.5 / f, PRISM_ROW / f, 0 }, { 0, 0, 1 / f }, { 0, 0, 0 });
  return m.Transform(t);
}

/* the hexagonal prism (the offset bit) and the hexagonal bipyramid (the
 * chamfer bit) at unit size, in lattice coordinates */
void prismLatticeBits(bits_s & b) {
  const int hex[6][2] = { { 1, 0 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { 0, -1 }, { 1, -1 } };
  b.offset.clear(); b.chamfer.clear();
  for (int s = -1; s <= 1; s += 2)
    for (const auto & h : hex) b.offset.push_back(vec3((double)(h[0] * PRISM_HEX), (double)(h[1] * PRISM_HEX), (double)(s * PRISM_HEIGHT)));
  for (const auto & h : hex) b.chamfer.push_back(vec3((double)(h[0] * PRISM_HEX), (double)(h[1] * PRISM_HEX), 0.0));
  b.chamfer.push_back(vec3(0.0, 0.0, (double)PRISM_HEIGHT));
  b.chamfer.push_back(vec3(0.0, 0.0, (double)-PRISM_HEIGHT));
  b.chamferFill = b.chamfer;
}

/* the prism grid's construction, on a body already in the lattice frame,
 * with gap and bevel as multiples of the unit bits */
bool constructPrismExact(const Manifold & body, const bits_s & b, int64_t g, int64_t r, bool fills, Manifold & out, std::string & err) {
  faces_s f;
  if (!exactFaces(body, f, err)) return false;
  Manifold m;
  if (!rebuiltExact(f, m, err)) return false;
  auto go = [&](const std::vector<vec3> & unit, int64_t size, bool add) { return stepExact(m, f, unit, size, add, err); };
  if (fills) {
    /* prism in 2b, bipyramid out 2b, bipyramid in b, prism out b - g (in
     * by the difference when the gap is the larger) */
    if (!go(b.offset, 2 * r, false)) return false;
    if (m.IsEmpty()) { err = "nothing is left of the shape after the bevel step (twice the bevel too large for its cells)"; return false; }
    if (!go(b.chamfer, 2 * r, true)) return false;
    if (!go(b.chamfer, r, false)) return false;
    if (r > g) { if (!go(b.offset, r - g, true)) return false; }
    else if (g > r) { if (!go(b.offset, g - r, false)) return false; }
  } else {
    /* prism in b + g, bipyramid out b */
    if (!go(b.offset, r + g, false)) return false;
    if (m.IsEmpty()) { err = "nothing is left of the shape after the gap step (gap + bevel too large for its cells)"; return false; }
    if (!go(b.chamfer, r, true)) return false;
  }
  if (m.IsEmpty()) { err = "nothing is left of the shape after the gap step"; return false; }
  out = m;
  return true;
}

/* the construction for the cube-and-octahedron bits, on a body already in
 * the frame and gap and bevel in whole units */
bool constructExact(const Manifold & body, const bits_s & b, int64_t g, int64_t r, double rCells, bool fills, Manifold & out, std::string & err) {
  faces_s f;
  if (!exactFaces(body, f, err)) return false;
  Manifold m;
  if (!rebuiltExact(f, m, err)) return false;
  auto go = [&](const std::vector<vec3> & unit, int64_t size, bool add) { return stepExact(m, f, unit, size, add, err); };
  /* the chamfer bit is the Minkowski sum of a cube of half-size sqrt2 - 1
   * and an octahedron of radius 2 - sqrt2 (in the same proportions as its
   * vertices (+-1, +-(sqrt2-1), +-(sqrt2-1))): one step with each */
  auto goChamfer = [&](int64_t size, bool add) {
    if (!go(b.offset, units((SQRT2 - 1) * (double)size), add)) return false;
    return go(b.chamferFill, units((2 - SQRT2) * (double)size), add);
  };
  if (fills) {
    const int64_t bp = r;                                                   /* kFill = 1 */
    if (!go(b.offset, 2 * bp, false)) return false;
    if (m.IsEmpty()) { err = "nothing is left of the shape after the bevel step (twice the bevel too large for its cells)"; return false; }
    if (!go(b.chamferFill, 2 * bp, true)) return false;
    if (!go(b.chamferFill, bp, false)) return false;
    if (!go(b.offset, bp, true)) return false;
    if (!goChamfer(g, false)) return false;
  } else {
    const int64_t bp = units(b.k * rCells * FRAME);
    if (!go(b.offset, bp, false)) return false;
    if (m.IsEmpty()) { err = "nothing is left of the shape after the bevel step (bevel too large for its cells)"; return false; }
    if (bp > g) { if (!goChamfer(bp - g, true)) return false; }
    else if (g > bp) { if (!goChamfer(g - bp, false)) return false; }
  }
  if (m.IsEmpty()) { err = "nothing is left of the shape after the gap step"; return false; }
  out = m;
  return true;
}

} // namespace

bool handles(const voxel_c & v) {
  gridType_c::gridType t = v.getGridType()->getType();
  return t == gridType_c::GT_TRIANGULAR_PRISM || t == gridType_c::GT_RHOMBIC || t == gridType_c::GT_TETRA_OCTA;
}

Polyhedron * polyhedron(const voxel_c & v, double g, double r, bool fills, bool split, std::string & err)
{
  if (!handles(v)) { err = "grid not handled by the Minkowski mesher"; return 0; }
  const bool prism = v.getGridType()->getType() == gridType_c::GT_TRIANGULAR_PRISM;
  /* the cells, the facet directions, the bits */
  std::vector<vec3> facets;
  std::vector<cellFaces_s> cellFaces;
  for (unsigned z = 0; z < v.getZ(); z++)
    for (unsigned y = 0; y < v.getY(); y++)
      for (unsigned x = 0; x < v.getX(); x++) {
        if (!v.validCoordinate(x, y, z) || v.getState(x, y, z) != voxel_c::VX_FILLED) continue;
        cellNormals(v, x, y, z, facets);
        /* the cell's faces as outward planes */
        cellFaces_s cf;
        cf.index = v.getIndex(x, y, z);
        std::vector<std::pair<int, std::vector<vec3> > > polys;
        vec3 centre(0, 0, 0); size_t np = 0;
        std::vector<float> corners;
        int nx, ny, nz;
        for (unsigned n = 0; v.getNeighbor(n, 0, x, y, z, &nx, &ny, &nz); n++) {
          corners.clear();
          v.getConnectionFace(x, y, z, (int)n, 0, 0, corners);
          std::vector<vec3> pts;
          for (unsigned k = 0; k + 2 < corners.size(); k += 3) {
            /* the corners come in single precision: on the prism grid, back
             * onto the lattice the mesh was built on (x a multiple of 1/2,
             * y of sqrt3/2, z whole), elsewhere they are exact */
            vec3 q(corners[k], corners[k + 1], corners[k + 2]);
            if (prism) q = vec3(std::round(2 * q.x) / 2, std::round(q.y / PRISM_ROW) * PRISM_ROW, std::round(q.z));
            pts.push_back(q); centre = centre + q; np++;
          }
          polys.push_back(std::make_pair((int)n, pts));
        }
        centre = centre / (double)np;
        for (const auto & pn : polys) {
          const std::vector<vec3> & pts = pn.second;
          if (pts.size() < 3) continue;
          vec3 nrm(0, 0, 0);
          for (size_t k = 0; k < pts.size(); k++) nrm = nrm + manifold::la::cross(pts[k], pts[(k + 1) % pts.size()]);
          double ln = manifold::la::length(nrm);
          if (ln < 1e-300) continue;
          nrm = nrm / ln;
          if (manifold::la::dot(nrm, pts[0] - centre) < 0) nrm = nrm * -1.0;
          /* exposed: the neighbour across is outside the voxel space or not filled */
          bool exposed = true;
          if (v.getNeighbor((unsigned)pn.first, 0, x, y, z, &nx, &ny, &nz) && nx >= 0 && ny >= 0 && nz >= 0 &&
              (unsigned)nx < v.getX() && (unsigned)ny < v.getY() && (unsigned)nz < v.getZ() &&
              v.validCoordinate(nx, ny, nz) && v.getState(nx, ny, nz) == voxel_c::VX_FILLED)
            exposed = false;
          cf.faces.push_back({ pn.first, nrm, manifold::la::dot(nrm, pts[0]), exposed, pts });
        }
        cellFaces.push_back(cf);
      }
  if (cellFaces.empty()) { err = "the shape has no voxels"; return 0; }
  for (size_t i = 0, n = facets.size(); i < n; i++) {
    vec3 m = facets[i] * -1.0;
    bool seen = false;
    for (const vec3 & o : facets) if (manifold::la::dot(o, m) > 1 - 1e-6) seen = true;
    if (!seen) facets.push_back(m);
  }
  Manifold body = exactBody(v, err);
  if (body.IsEmpty()) return 0;
  Manifold outer;
  /* how far inside its cell a facet face lands: the gap, except that on
   * the prism grid the bits' hexagon reaches 209 (sqrt3/2) / 181 of their
   * height sideways, so side faces land a hair (3.7e-6 g) less far in */
  double gapAxial = g, gapSide = g;
  if (!prism) {
    /* the integer frame */
    const int64_t gu = units(g * FRAME), ru = units(r * FRAME);
    if (!constructExact(body.Scale(vec3(FRAME)), cubeBits(), gu, ru, r, fills, outer, err)) return 0;
    outer = outer.Scale(vec3(1 / FRAME));
  } else {
    /* the lattice frame; gap and bevel as multiples of the unit bits,
     * whose reach is 1 / PRISM_SCALE of a cell */
    Manifold lattice;
    if (!prismToLattice(body, lattice, err)) return 0;
    bits_s b;
    prismLatticeBits(b);
    const int64_t gs = units(g * PRISM_SCALE), rs = units(r * PRISM_SCALE);
    if (!constructPrismExact(lattice, b, gs, rs, fills, outer, err)) return 0;
    outer = prismFromLattice(outer);
    gapAxial = (double)(gs * PRISM_HEIGHT) / PRISM_FRAME;
    gapSide = (double)(gs * PRISM_HEX) * PRISM_ROW / PRISM_FRAME;
  }
  Polyhedron * poly = new Polyhedron();
  if (!addFaces(poly, outer, facets, cellFaces, gapAxial, gapSide, split, err)) { delete poly; return 0; }
  poly->finalize();
  return poly;
}

} // namespace minkMesh
