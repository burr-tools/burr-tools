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
#include <map>
#include <set>
#include <vector>

namespace minkMesh {

namespace {

using manifold::Manifold;
using manifold::vec3;

const double SQRT2 = 1.4142135623730951;

/* The shape's surface, exactly: the cell faces whose neighbour is not
 * filled (at bevel 0 and offset 0, the same enumeration
 * voxel_c::getMeshInternal uses). The faces between two filled cells
 * cancel by identity here and never reach a boolean, whose float
 * arithmetic would have to rediscover them and now and then leaves a
 * seam. Corners are shared between a cell's own faces and across the
 * faces shared with filled neighbours (their corner coordinates agree
 * exactly); two cells that touch only along an edge or at a corner keep
 * separate vertices there, which is what a manifold mesh needs. */
Manifold exactBody(const voxel_c & v, std::string & err) {
  struct corner_s { vec3 p; };
  std::vector<corner_s> corners;
  std::vector<size_t> parent;
  auto find = [&](size_t i) { while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; } return i; };
  std::map<std::pair<unsigned, std::tuple<float, float, float> >, size_t> slot;   /* (cell, exact position) -> corner */
  auto corner = [&](unsigned cell, const vec3 & p) {
    auto key = std::make_pair(cell, std::make_tuple((float)p.x, (float)p.y, (float)p.z));
    auto it = slot.find(key);
    if (it != slot.end()) return it->second;
    corners.push_back({ p }); parent.push_back(corners.size() - 1);
    slot[key] = corners.size() - 1;
    return corners.size() - 1;
  };
  auto filled = [&](int x, int y, int z) {
    return x >= 0 && y >= 0 && z >= 0 && x < (int)v.getX() && y < (int)v.getY() && z < (int)v.getZ()
      && v.validCoordinate(x, y, z) && v.getState(x, y, z) == voxel_c::VX_FILLED;
  };
  std::vector<std::vector<size_t> > faces;
  std::vector<float> fc;
  int nx, ny, nz;
  for (unsigned z = 0; z < v.getZ(); z++)
    for (unsigned y = 0; y < v.getY(); y++)
      for (unsigned x = 0; x < v.getX(); x++) {
        if (!filled(x, y, z)) continue;
        unsigned cell = v.getIndex(x, y, z);
        for (unsigned n = 0; v.getNeighbor(n, 0, x, y, z, &nx, &ny, &nz); n++) {
          fc.clear();
          v.getConnectionFace(x, y, z, (int)n, 0, 0, fc);
          if (fc.size() < 9) continue;
          std::vector<size_t> face;
          for (unsigned k = 0; k + 2 < fc.size(); k += 3) face.push_back(corner(cell, vec3(fc[k], fc[k + 1], fc[k + 2])));
          if (filled(nx, ny, nz)) {
            /* an interior face: its corners are the neighbour's too */
            unsigned other = v.getIndex(nx, ny, nz);
            for (size_t c : face) {
              size_t o = corner(other, corners[c].p);
              parent[find(c)] = find(o);
            }
          } else {
            faces.push_back(face);
          }
        }
      }
  manifold::MeshGL64 mesh;
  mesh.numProp = 3;
  std::vector<uint64_t> index(corners.size(), ~(uint64_t)0);
  auto vertex = [&](size_t c) {
    size_t r = find(c);
    if (index[r] == ~(uint64_t)0) {
      index[r] = mesh.vertProperties.size() / 3;
      mesh.vertProperties.push_back(corners[r].p.x); mesh.vertProperties.push_back(corners[r].p.y); mesh.vertProperties.push_back(corners[r].p.z);
    }
    return index[r];
  };
  for (const std::vector<size_t> & face : faces)
    for (size_t k = 1; k + 1 < face.size(); k++) {
      mesh.triVerts.push_back(vertex(face[0])); mesh.triVerts.push_back(vertex(face[k])); mesh.triVerts.push_back(vertex(face[k + 1]));
    }
  Manifold m(mesh);
  if (m.Status() != Manifold::Error::NoError) { err = "the shape's cell faces do not close up into a solid"; return Manifold(); }
  if (m.Volume() < 0) {
    /* the faces were wound the other way round */
    for (size_t t = 0; t + 2 < mesh.triVerts.size(); t += 3) std::swap(mesh.triVerts[t + 1], mesh.triVerts[t + 2]);
    m = Manifold(mesh);
  }
  return m;
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
  bool prism = false;      /* the prism grid's bits, with their own sequences */
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
  b.prism = false;
  return b;
}

/* prism: the hexagonal prism with faces on the grid's facet directions
 * and the hexagonal bipyramid aligned to it, from the in-plane facet
 * normals of a cell and the prism axis */
bool prismBits(const std::vector<vec3> & normals, bits_s & b, std::string & err) {
  /* the axis: the coordinate direction every facet normal is either
   * parallel or perpendicular to (an in-plane facet can be axis aligned
   * too, but the other in-plane facets are not perpendicular to it) */
  int axis = -1;
  for (int a = 0; a < 3 && axis < 0; a++) {
    bool ok = true;
    for (const vec3 & n : normals) {
      double d = std::fabs(n[a]);
      if (d > 1e-6 && std::fabs(d - 1) > 1e-6) ok = false;
    }
    if (ok) axis = a;
  }
  if (axis < 0) { err = "prism grid: no prism axis among the facet normals"; return false; }
  int u = (axis + 1) % 3, w = (axis + 2) % 3;
  std::vector<double> az;                                   /* in-plane facet azimuths, both signs */
  for (const vec3 & n : normals) {
    if (std::fabs(n[axis]) > 1e-6) continue;
    for (int s = -1; s <= 1; s += 2) {
      double a = std::atan2(s * n[w], s * n[u]);
      bool seen = false;
      for (double o : az) if (std::fabs(std::remainder(o - a, 2 * M_PI)) < 1e-6) seen = true;
      if (!seen) az.push_back(a);
    }
  }
  if (az.size() != 6) { err = "prism grid: expected 6 in-plane facet directions"; return false; }
  std::sort(az.begin(), az.end());
  const double c30 = std::cos(M_PI / 6);
  for (int s = -1; s <= 1; s += 2)
    for (double a : az) {
      /* the prism's vertices lie between adjacent facet directions */
      double m = a + M_PI / 6, r = 1 / c30;
      double p[3]; p[u] = r * std::cos(m); p[w] = r * std::sin(m); p[axis] = s;
      b.offset.push_back(vec3(p[0], p[1], p[2]));
    }
  for (double a : az) {
    /* the bipyramid's equatorial vertices between the facet directions,
     * apexes at the equatorial apothem: faces on the facet directions at
     * 45 degrees, reach 1 along every facet normal and along the axis */
    double m = a + M_PI / 6, r = 1 / c30;
    double p[3]; p[u] = r * std::cos(m); p[w] = r * std::sin(m); p[axis] = 0;
    b.chamfer.push_back(vec3(p[0], p[1], p[2]));
  }
  for (int s = -1; s <= 1; s += 2) { double p[3] = {0, 0, 0}; p[axis] = s; b.chamfer.push_back(vec3(p[0], p[1], p[2])); }
  b.k = 1;
  /* the bipyramid carries no face on a facet direction, so it is the
   * chamfer bit of both variants */
  b.chamferFill = b.chamfer;
  b.kFill = 1;
  b.prism = true;
  return true;
}

/* The bit's vertices at the given size, turned by a hundredth of a
 * microradian about a skew axis. Untouched, the bits' faces are exactly
 * parallel to the grid's facets, and the sweeps of different pieces
 * pile up several exactly coincident faces on one plane wherever faces
 * meet; Manifold's boolean then leaves garbage there now and then:
 * slivers facing each other across nothing, which the next step opens
 * into a slice through the shape, and unions whose volume depends on
 * the order of the operands. Turned, no two sweeps of different kinds
 * are coplanar any more, the unions agree to a part in a billion and
 * carry no such slivers; the turn moves nothing by more than a
 * nanometre per metre, far inside what the rebuild snaps away. */
std::vector<vec3> bitPoints(const std::vector<vec3> & unit, double size, int attempt) {
  /* a later attempt turns a little more, and about a different axis,
   * which changes every tie the boolean has to break */
  const double angle = 1e-8 * (1 + attempt);
  const vec3 axis = manifold::la::normalize(vec3(0.3 + 0.17 * attempt, 0.5 - 0.11 * attempt, 0.8 + 0.07 * attempt));
  const double c = std::cos(angle), s = std::sin(angle);
  std::vector<vec3> pts;
  for (const vec3 & p : unit) {
    /* Rodrigues */
    vec3 r = p * c + manifold::la::cross(axis, p) * s + axis * (manifold::la::dot(axis, p) * (1 - c));
    pts.push_back(r * size);
  }
  return pts;
}

/* The planar faces of a solid, from its triangle mesh, and the solid
 * rebuilt from them.
 *
 * The booleans leave every face shattered: slivers along the seams where
 * the hulls of the sweep meet in one plane, vertices a few nanometres
 * apart, triangles whose normals mean nothing. Fed back into the next
 * step as they are, the slivers sweep into garbage-direction facets and
 * the fragmentation compounds (a small prism shape reached forty
 * thousand triangles). So after every boolean the solid is put back
 * onto its planes: vertices welded, the planes taken from the fat
 * triangles, every triangle assigned to the plane it lies on (a sliver
 * to its neighbour's), every vertex snapped onto the intersection of
 * its planes, and each planar face re-triangulated from its outline
 * loops. All loop vertices are kept, so the faces stay edge-consistent
 * with their neighbours and the rebuilt mesh is a manifold; the slivers
 * have become exactly flat and are gone.
 *
 * The same faces give the sweep of the boundary by a convex bit: the
 * union of one hull per convex piece of each face, of which a face has
 * few (one hull per triangle of the shattered mesh bogs the union down;
 * Manifold's built-in Minkowski does exactly that). */
struct faces_s {
  std::vector<vec3> pos;                       /* welded, snapped onto their planes */
  std::vector<uint64_t> tri;                   /* the rebuilt triangulation, index triples */
  std::vector<uint64_t> orig;                  /* the input's own triangulation, welded */
  std::vector<std::vector<vec3> > pieces;      /* convex pieces of the faces */
};

/* triangles of one planar face (CCW in the 2D frame) merged greedily
 * across shared edges while the union stays convex; collinear outline
 * vertices are allowed */
void convexPieces(const std::vector<manifold::vec2> & p2, const std::vector<int> & tt, std::vector<std::vector<int> > & out) {
  std::vector<std::vector<int> > poly;
  for (size_t t = 0; t + 2 < tt.size(); t += 3) {
    std::vector<int> p = { tt[t], tt[t + 1], tt[t + 2] };
    double a = (p2[p[1]].x - p2[p[0]].x) * (p2[p[2]].y - p2[p[0]].y) - (p2[p[1]].y - p2[p[0]].y) * (p2[p[2]].x - p2[p[0]].x);
    if (std::fabs(a) < 1e-18) continue;
    if (a < 0) std::swap(p[1], p[2]);
    poly.push_back(p);
  }
  std::vector<bool> alive(poly.size(), true);
  std::map<std::pair<int, int>, size_t> edgeOwner;
  for (size_t i = 0; i < poly.size(); i++)
    for (size_t c = 0; c < 3; c++) edgeOwner[std::make_pair(poly[i][c], poly[i][(c + 1) % 3])] = i;
  auto convex = [&](const std::vector<int> & p) {
    std::set<int> seen;
    for (int v : p) if (!seen.insert(v).second) return false;
    double scale = 0;
    for (size_t i = 0; i < p.size(); i++) scale = std::max(scale, std::fabs(p2[p[i]].x) + std::fabs(p2[p[i]].y));
    for (size_t i = 0; i < p.size(); i++) {
      const manifold::vec2 & a = p2[p[i]], & b = p2[p[(i + 1) % p.size()]], & c = p2[p[(i + 2) % p.size()]];
      double cr = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
      if (cr < -1e-12 * scale * scale) return false;
    }
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
        /* q runs b -> a -> q1 .. qk -> b; the merged outline replaces a -> b
         * in poly[i] by a -> q1 .. qk -> b */
        size_t ia = 0;
        while (q[ia] != a) ia++;
        std::vector<int> merged;
        for (size_t d = 1; d <= poly[i].size(); d++) merged.push_back(poly[i][(c + d) % poly[i].size()]);   /* b .. a */
        for (size_t d = 1; d + 1 < q.size(); d++) merged.push_back(q[(ia + d) % q.size()]);                  /* q1 .. qk */
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

void extractFaces(const Manifold & m, faces_s & out) {
  std::vector<vec3> & pos = out.pos;
  std::vector<uint64_t> & tv = out.orig;
  out.tri.clear(); out.orig.clear(); out.pieces.clear(); pos.clear();
  {
    manifold::MeshGL64 mesh = m.GetMeshGL64();
    const size_t np = mesh.numProp;
    for (size_t i = 0; i * np < mesh.vertProperties.size(); i++)
      pos.push_back(vec3(mesh.vertProperties[i * np], mesh.vertProperties[i * np + 1], mesh.vertProperties[i * np + 2]));
    tv = mesh.triVerts;
  }
  /* short edges contracted: WELD is far above the noise the booleans
   * leave (nanometres) and far below any feature of a chamfer. Contracting
   * edges keeps the mesh a manifold; welding by proximity alone does
   * not, since two vertices of a crack a few nanometres wide are not
   * joined by an edge, and merging them folds the surface. */
  const double WELD = 2e-6;
  auto contract = [&]() {
    const size_t nv = pos.size();
    std::vector<uint64_t> parentV(nv);
    for (size_t i = 0; i < nv; i++) parentV[i] = i;
    auto findV = [&](uint64_t i) { while (parentV[i] != i) { parentV[i] = parentV[parentV[i]]; i = parentV[i]; } return i; };
    /* an edge is contracted only when its two vertices have exactly the
     * two apexes of its triangles as common neighbours (the link
     * condition): otherwise the collapse folds the surface, and two
     * triangles end up sharing a directed edge, which cuts the faces
     * apart in everything that follows */
    std::vector<std::set<uint64_t> > adj(nv);
    for (size_t k = 0; k + 2 < tv.size(); k += 3)
      for (int c = 0; c < 3; c++) { adj[tv[k + c]].insert(tv[k + (c + 1) % 3]); adj[tv[k + (c + 1) % 3]].insert(tv[k + c]); }
    for (size_t k = 0; k + 2 < tv.size(); k += 3)
      for (int c = 0; c < 3; c++) {
        uint64_t a = findV(tv[k + c]), b = findV(tv[k + (c + 1) % 3]);
        if (a == b || manifold::la::length(pos[a] - pos[b]) > WELD) continue;
        size_t common = 0;
        for (uint64_t x : adj[a]) if (x != b && adj[b].count(x)) common++;
        if (common != 2) continue;
        /* b into a */
        for (uint64_t x : adj[b]) {
          adj[x].erase(b);
          if (x != a) { adj[x].insert(a); adj[a].insert(x); }
        }
        adj[a].erase(b);
        adj[b].clear();
        parentV[b] = a;
      }
    std::vector<uint64_t> remap(nv, ~(uint64_t)0);
    std::vector<vec3> np;
    for (size_t i = 0; i < nv; i++) {
      uint64_t r = findV(i);
      if (remap[r] == ~(uint64_t)0) { remap[r] = np.size(); np.push_back(pos[r]); }
      remap[i] = remap[r];
    }
    std::vector<uint64_t> nt;
    for (size_t k = 0; k + 2 < tv.size(); k += 3) {
      uint64_t a = remap[tv[k]], b = remap[tv[k + 1]], c = remap[tv[k + 2]];
      if (a == b || b == c || a == c) continue;
      nt.push_back(a); nt.push_back(b); nt.push_back(c);
    }
    pos.swap(np); tv.swap(nt);
  };
  size_t nt = 0;
  auto P = [&](size_t t, int c) -> const vec3 & { return pos[tv[3 * t + c]]; };
  std::vector<double> area, altitude;
  std::vector<vec3> nrm;
  struct plane_s { vec3 n{}; double d = 0; };
  std::vector<plane_s> planes;
  const double TOL = 1e-6;
  std::map<std::pair<uint64_t, uint64_t>, size_t> owner;
  std::vector<int> plane;
  auto onPlane = [&](size_t t, const plane_s & p) {
    for (int c = 0; c < 3; c++) if (std::fabs(manifold::la::dot(p.n, P(t, c)) - p.d) > TOL) return false;
    return true;
  };
  auto neighbour = [&](size_t t, int c) -> long {
    auto rv = owner.find(std::make_pair(tv[3 * t + (c + 1) % 3], tv[3 * t + c]));
    return rv == owner.end() ? -1 : (long)rv->second;
  };
  /* the planes, each from its largest triangle; only triangles with a
   * decent altitude define one (a sliver's normal is noise), and the
   * planes of a Minkowski result on a grid are degrees apart, so a
   * plane already seen is recognised generously. Every triangle to the
   * plane it lies on, nearest its own normal; a triangle on no plane (a
   * sliver whose vertices are on several) takes the plane of a neighbour
   * across an edge that it does lie on. A triangle on no plane at all
   * (a spike the boolean left, with a vertex well off every plane) is
   * kept as it is: put into a planar group it would make that group's
   * outline leave the plane, and the re-triangulation folds. Then the
   * vertices snapped onto their planes:
   * the least displacement that puts the vertex on every plane it is
   * already close to (up to three independent ones; the rest agree at a
   * true vertex). A vertex that would have to move far is left alone. */
  auto fit = [&]() {
    nt = tv.size() / 3;
    area.assign(nt, 0); altitude.assign(nt, 0); nrm.assign(nt, vec3(0, 0, 0));
    std::vector<size_t> order;
    for (size_t t = 0; t < nt; t++) {
      vec3 n = manifold::la::cross(P(t, 1) - P(t, 0), P(t, 2) - P(t, 0));
      double l = manifold::la::length(n);
      area[t] = l / 2; nrm[t] = l > 1e-300 ? n / l : vec3(0, 0, 0);
      double longest = 0;
      for (int c = 0; c < 3; c++) longest = std::max(longest, manifold::la::length(P(t, (c + 1) % 3) - P(t, c)));
      altitude[t] = longest > 0 ? l / longest : 0;
      order.push_back(t);
    }
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return area[a] > area[b]; });
    planes.clear();
    for (size_t t : order) {
      if (altitude[t] < 1e-4) continue;
      double d = manifold::la::dot(nrm[t], P(t, 0));
      bool found = false;
      for (const plane_s & p : planes) if (manifold::la::dot(p.n, nrm[t]) > 1 - 1e-6 && std::fabs(p.d - d) < TOL) { found = true; break; }
      if (!found) planes.push_back({ nrm[t], d });
    }
    owner.clear();
    for (size_t t = 0; t < nt; t++)
      for (int c = 0; c < 3; c++) owner[std::make_pair(tv[3 * t + c], tv[3 * t + (c + 1) % 3])] = t;
    plane.assign(nt, -1);
    for (size_t t = 0; t < nt; t++) {
      double best = -2; int bk = -1;
      for (size_t k = 0; k < planes.size(); k++) {
        double d = manifold::la::dot(planes[k].n, nrm[t]);
        if (d > best && onPlane(t, planes[k])) { best = d; bk = (int)k; }
      }
      plane[t] = bk;
    }
    {
      bool changed = true;
      while (changed) {
        changed = false;
        for (size_t t = 0; t < nt; t++) {
          if (plane[t] >= 0) continue;
          for (int c = 0; c < 3 && plane[t] < 0; c++) {
            long u = neighbour(t, c);
            if (u < 0 || plane[u] < 0) continue;
            if (onPlane(t, planes[plane[u]])) { plane[t] = plane[u]; changed = true; }
          }
        }
      }
    }
    /* every plane the vertex is close to counts, not only those of its
     * own triangles: a vertex on the line of two faces whose own
     * triangles happen to lie on only one of them would otherwise be
     * left a few nanometres off that line, and the triangle along the
     * line stays a sliver instead of going flat */
    for (size_t v = 0; v < pos.size(); v++) {
      std::vector<vec3> N; std::vector<double> D; std::vector<vec3> basis;
      for (size_t k = 0; k < planes.size(); k++) {
        if (std::fabs(manifold::la::dot(planes[k].n, pos[v]) - planes[k].d) > TOL) continue;
        vec3 r = planes[k].n;
        for (const vec3 & e : basis) r = r - e * manifold::la::dot(e, r);
        if (manifold::la::length(r) < 0.1) continue;
        basis.push_back(r / manifold::la::length(r));
        N.push_back(planes[k].n); D.push_back(planes[k].d);
        if (N.size() == 3) break;
      }
      if (N.empty()) continue;
      /* (N N^T) lambda = D - N v, x = v + N^T lambda; Gaussian elimination, r <= 3 */
      size_t r = N.size();
      double A[3][4];
      for (size_t i = 0; i < r; i++) {
        for (size_t j = 0; j < r; j++) A[i][j] = manifold::la::dot(N[i], N[j]);
        A[i][r] = D[i] - manifold::la::dot(N[i], pos[v]);
      }
      for (size_t i = 0; i < r; i++) {
        size_t piv = i;
        for (size_t j = i + 1; j < r; j++) if (std::fabs(A[j][i]) > std::fabs(A[piv][i])) piv = j;
        for (size_t j = 0; j <= r; j++) std::swap(A[i][j], A[piv][j]);
        for (size_t j = i + 1; j < r; j++) {
          double f = A[j][i] / A[i][i];
          for (size_t k = i; k <= r; k++) A[j][k] -= f * A[i][k];
        }
      }
      double lambda[3] = {0.0, 0.0, 0.0};
      for (size_t i = r; i-- > 0;) {
        double s = A[i][r];
        for (size_t j = i + 1; j < r; j++) s -= A[i][j] * lambda[j];
        lambda[i] = s / A[i][i];
      }
      vec3 x = pos[v];
      for (size_t i = 0; i < r; i++) x = x + N[i] * lambda[i];
      if (manifold::la::length(x - pos[v]) > 1e-5) continue;
      pos[v] = x;
    }
  };
  /* twice: snapping can put two vertices of a sliver onto the same point,
   * and the second contraction merges them */
  for (int pass = 0; pass < 2; pass++) { contract(); fit(); }
  /* union-find over shared edges between triangles on the same plane */
  std::vector<size_t> parent(nt);
  for (size_t t = 0; t < nt; t++) parent[t] = t;
  auto find = [&](size_t i) { while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; } return i; };
  for (size_t t = 0; t < nt; t++)
    for (int c = 0; c < 3; c++) {
      long u = neighbour(t, c);
      if (u >= 0 && plane[t] >= 0 && plane[u] == plane[t]) parent[find(t)] = find((size_t)u);
    }
  std::map<size_t, std::vector<size_t> > groups;
  for (size_t t = 0; t < nt; t++) groups[find(t)].push_back(t);
  /* a group that cannot be rebuilt keeps its own triangles (still
   * consistent with its neighbours) and hulls each of them */
  auto raw = [&](const std::vector<size_t> & members) {
    for (size_t t : members) {
      for (int c = 0; c < 3; c++) out.tri.push_back(tv[3 * t + c]);
      vec3 n = manifold::la::cross(P(t, 1) - P(t, 0), P(t, 2) - P(t, 0));
      if (manifold::la::length(n) < 2e-10) continue;      /* flat: its neighbours' hulls cover it */
      std::vector<vec3> piece;
      for (int c = 0; c < 3; c++) piece.push_back(P(t, c));
      out.pieces.push_back(piece);
    }
  };
  for (const auto & g : groups) {
    const std::vector<size_t> & members = g.second;
    if (plane[members[0]] < 0) { raw(members); continue; }
    const vec3 n = planes[plane[members[0]]].n;
    /* the outline: directed edges without their reverse inside the group,
     * walked into loops; where the outline touches itself a vertex has
     * several outgoing edges and the walk takes the sharpest left turn
     * (about the normal), which keeps each loop simple */
    std::set<size_t> inGroup(members.begin(), members.end());
    std::multimap<uint64_t, uint64_t> next;
    for (size_t t : members)
      for (int c = 0; c < 3; c++) {
        long u = neighbour(t, c);
        if (u >= 0 && inGroup.count((size_t)u)) continue;
        next.insert(std::make_pair(tv[3 * t + c], tv[3 * t + (c + 1) % 3]));
      }
    std::vector<std::vector<uint64_t> > loops;
    std::set<std::pair<uint64_t, uint64_t> > used;
    bool bad = false;
    for (const auto & e0 : next) {
      if (used.count(e0)) continue;
      std::vector<uint64_t> loop;
      std::pair<uint64_t, uint64_t> e = e0;
      size_t guard = 0;
      bool closed = false;
      while (true) {
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
        if (++guard > next.size() + 1) break;
      }
      if (!closed) { bad = true; break; }
      loops.push_back(loop);
    }
    if (bad || loops.empty()) { raw(members); continue; }
    /* loops too short to bound anything mean a walk gone wrong; a group
     * whose loops all run backwards (a sheet facing away from its plane's
     * normal, a sliver artefact) is kept as it is; otherwise every loop
     * goes to the triangulator, which sorts outlines from holes by their
     * orientation, and accepts several outlines touching at a vertex */
    bool degenerate = false, forward = false;
    std::vector<std::vector<uint64_t> > kept;
    for (size_t l = 0; l < loops.size(); l++) {
      if (loops[l].size() < 3) { degenerate = true; break; }
      vec3 a(0, 0, 0);
      for (size_t q = 0; q < loops[l].size(); q++) a = a + manifold::la::cross(pos[loops[l][q]], pos[loops[l][(q + 1) % loops[l].size()]]);
      if (manifold::la::dot(a, n) > 1e-14) forward = true;
      kept.push_back(loops[l]);
    }
    if (degenerate || !forward) { raw(members); continue; }
    /* the loops must enclose exactly the group's own area, or the walk
     * has gone wrong somewhere and the pieces would sweep phantom
     * surface into the solid */
    {
      double own = 0, enclosed = 0;
      for (size_t t : members) own += manifold::la::length(manifold::la::cross(P(t, 1) - P(t, 0), P(t, 2) - P(t, 0))) / 2;
      for (const std::vector<uint64_t> & loop : kept) {
        vec3 a(0, 0, 0);
        for (size_t q = 0; q < loop.size(); q++) a = a + manifold::la::cross(pos[loop[q]], pos[loop[(q + 1) % loop.size()]]);
        enclosed += manifold::la::dot(a, n) / 2;
      }
      if (std::fabs(enclosed - own) > 1e-9 * std::max(1.0, own)) { raw(members); continue; }
    }
    /* 2D projection and triangulation */
    vec3 ax(1, 0, 0);
    if (std::fabs(n.y) < std::fabs(n.x)) ax = vec3(0, 1, 0);
    if (std::fabs(n.z) < std::fabs(manifold::la::dot(ax, n))) ax = vec3(0, 0, 1);
    vec3 u1 = ax - n * manifold::la::dot(ax, n); u1 = u1 / manifold::la::length(u1);
    vec3 u2 = manifold::la::cross(n, u1);
    std::vector<manifold::vec2> p2;
    std::vector<uint64_t> flat;
    std::vector<std::vector<int> > idx(kept.size());
    for (size_t l = 0; l < kept.size(); l++)
      for (uint64_t v : kept[l]) {
        manifold::vec2 q(manifold::la::dot(pos[v], u1), manifold::la::dot(pos[v], u2));
        idx[l].push_back((int)p2.size()); p2.push_back(q); flat.push_back(v);
      }
    /* Manifold's own polygon triangulator: it keeps every loop edge (a
     * collinear vertex gives a flat triangle, flipped away below), so
     * the face stays consistent with its neighbours across the outline */
    std::vector<int> tt;
    {
      manifold::Polygons polys(kept.size());
      for (size_t l = 0; l < kept.size(); l++)
        for (int i : idx[l]) polys[l].push_back(p2[i]);
      std::vector<manifold::ivec3> tri3;
      try { tri3 = manifold::Triangulate(polys, 1e-9); }
      catch (...) { raw(members); continue; }
      if (tri3.empty()) { raw(members); continue; }
      for (const manifold::ivec3 & t3 : tri3) { tt.push_back(t3.x); tt.push_back(t3.y); tt.push_back(t3.z); }
    }
    /* and the triangulation must cover exactly that area */
    {
      double covered = 0;
      for (size_t k = 0; k + 2 < tt.size(); k += 3) {
        const manifold::vec2 & a = p2[tt[k]], & b = p2[tt[k + 1]], & c = p2[tt[k + 2]];
        covered += ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) / 2;
      }
      double own = 0;
      for (size_t t : members) own += manifold::la::length(manifold::la::cross(P(t, 1) - P(t, 0), P(t, 2) - P(t, 0))) / 2;
      if (std::fabs(covered - own) > 1e-9 * std::max(1.0, own)) { raw(members); continue; }
    }
    for (int i : tt) out.tri.push_back(flat[i]);
    std::vector<std::vector<int> > cp;
    convexPieces(p2, tt, cp);
    for (const std::vector<int> & p : cp) {
      std::vector<vec3> piece;
      for (int i : p) piece.push_back(pos[flat[i]]);
      out.pieces.push_back(piece);
    }
  }
}

/* flat triangles flipped away: the triangulation of a face with a vertex
 * on a straight run of its outline can carry a triangle with no area
 * across that vertex (a needle along the line), which single precision
 * cannot tell from nothing and the STL writer chokes on. The edge
 * opposite the middle vertex is flipped with the neighbour across it,
 * which is on the same plane, giving two triangles with area. A flat
 * triangle whose neighbour is not coplanar is left (and reported later). */
void flipFlat(const std::vector<vec3> & pos, std::vector<uint64_t> & tri) {
  std::map<std::pair<uint64_t, uint64_t>, size_t> owner;
  auto own = [&](size_t t) { for (int c = 0; c < 3; c++) owner[std::make_pair(tri[3 * t + c], tri[3 * t + (c + 1) % 3])] = t; };
  auto disown = [&](size_t t) { for (int c = 0; c < 3; c++) owner.erase(std::make_pair(tri[3 * t + c], tri[3 * t + (c + 1) % 3])); };
  for (size_t t = 0; t + 2 < tri.size(); t += 3) own(t / 3);
  auto flat = [&](size_t t) {
    const vec3 & a = pos[tri[3 * t]], & b = pos[tri[3 * t + 1]], & c = pos[tri[3 * t + 2]];
    double l = manifold::la::length(manifold::la::cross(b - a, c - a));
    double e = std::max(manifold::la::length(b - a), std::max(manifold::la::length(c - b), manifold::la::length(a - c)));
    Vector3Df fa((float)a.x, (float)a.y, (float)a.z), fb((float)b.x, (float)b.y, (float)b.z), fc((float)c.x, (float)c.y, (float)c.z);
    return l < 1e-12 * e * e || ((fb - fa) ^ (fc - fa)).squaredModule() == 0;
  };
  for (int round = 0; round < 8; round++) {
    bool any = false;
    for (size_t t = 0; t + 2 < tri.size(); t += 3) {
      size_t ti = t / 3;
      if (!flat(ti)) continue;
      /* the middle vertex is opposite the longest edge */
      int m = 0; double longest = -1;
      for (int c = 0; c < 3; c++) {
        double e = manifold::la::length(pos[tri[t + (c + 1) % 3]] - pos[tri[t + (c + 2) % 3]]);
        if (e > longest) { longest = e; m = c; }
      }
      uint64_t M = tri[t + m], A = tri[t + (m + 1) % 3], B = tri[t + (m + 2) % 3];
      auto it = owner.find(std::make_pair(B, A));
      if (it == owner.end() || it->second == ti) continue;
      size_t u = it->second;
      uint64_t D = 0;
      for (int c = 0; c < 3; c++) if (tri[3 * u + c] != A && tri[3 * u + c] != B) D = tri[3 * u + c];
      /* the neighbour on the same plane, and the flip giving two triangles with area */
      vec3 n = manifold::la::cross(pos[B] - pos[A], pos[D] - pos[A]);
      double ln = manifold::la::length(n);
      if (ln < 1e-300) continue;
      n = n / ln;
      if (std::fabs(manifold::la::dot(n, pos[M] - pos[A])) > 1e-6) continue;
      disown(ti); disown(u);
      tri[t] = A; tri[t + 1] = D; tri[t + 2] = M;
      tri[3 * u] = D; tri[3 * u + 1] = B; tri[3 * u + 2] = M;
      own(ti); own(u);
      any = true;
    }
    if (!any) break;
  }
}

Manifold fromTriangles(const std::vector<vec3> & pos, const std::vector<uint64_t> & tri) {
  manifold::MeshGL64 mesh;
  mesh.numProp = 3;
  mesh.vertProperties.reserve(pos.size() * 3);
  for (const vec3 & p : pos) { mesh.vertProperties.push_back(p.x); mesh.vertProperties.push_back(p.y); mesh.vertProperties.push_back(p.z); }
  mesh.triVerts = tri;
  return Manifold(mesh);
}

/* the solid rebuilt from its faces; if that is not a valid manifold the
 * snapped original triangulation, and failing that the input itself */
Manifold rebuilt(faces_s & f, const Manifold & input) {
  if (f.tri.empty()) return input;
  flipFlat(f.pos, f.tri);
  Manifold r = fromTriangles(f.pos, f.tri);
  if (r.Status() == Manifold::Error::NoError && !r.IsEmpty()) return r;
  r = fromTriangles(f.pos, f.orig);
  if (r.Status() == Manifold::Error::NoError && !r.IsEmpty()) return r;
  return input;
}

/* the sweep of the solid's boundary by the bit: one hull per convex
 * piece of its planar faces */
Manifold boundaryTube(const faces_s & f, const std::vector<vec3> & bitPts) {
  std::vector<Manifold> hulls;
  for (const std::vector<vec3> & piece : f.pieces) {
    /* a sliver piece (a notch or a seam a boolean left, thinner than any
     * feature) sweeps into a wedge whose faces are a hair off its
     * neighbours' and can end up facing each other across nothing; its
     * neighbours' hulls cover its sweep to within that hair, so it is
     * left out. Width: twice the area over the perimeter. */
    vec3 c(0, 0, 0);
    for (const vec3 & p : piece) c = c + p;
    c = c / (double)piece.size();
    double area2 = 0, perimeter = 0;
    for (size_t i = 0; i < piece.size(); i++) {
      const vec3 & a = piece[i], & b = piece[(i + 1) % piece.size()];
      area2 += manifold::la::length(manifold::la::cross(a - c, b - c));
      perimeter += manifold::la::length(b - a);
    }
    if (perimeter <= 0 || area2 / perimeter < 1e-5) continue;
    std::vector<vec3> pts;
    for (const vec3 & p : piece) for (const vec3 & b : bitPts) pts.push_back(p + b);
    hulls.push_back(Manifold::Hull(pts));
  }
  return Manifold::BatchBoolean(hulls, manifold::OpType::Add);
}

/* A fold in a solid: two of its triangles nearly face to face, within a
 * micron of one plane, one's centroid inside the other, both wider than
 * a thousandth of a cell (a needle lying on a face is not a fold, only
 * a hair). No feature of a chamfered shape is that thin; a boolean
 * leaves such pleats now and then where many coplanar faces met. */
bool folded(const Manifold & m) {
  manifold::MeshGL64 g = m.GetMeshGL64();
  const size_t np = g.numProp;
  std::vector<vec3> n, c, p0, p1, p2;
  std::vector<double> d;
  for (size_t t = 0; t + 2 < g.triVerts.size(); t += 3) {
    vec3 q[3];
    for (int k = 0; k < 3; k++) q[k] = vec3(g.vertProperties[g.triVerts[t + k] * np], g.vertProperties[g.triVerts[t + k] * np + 1], g.vertProperties[g.triVerts[t + k] * np + 2]);
    vec3 nn = manifold::la::cross(q[1] - q[0], q[2] - q[0]);
    double l = manifold::la::length(nn);
    double longest = std::max(manifold::la::length(q[1] - q[0]), std::max(manifold::la::length(q[2] - q[1]), manifold::la::length(q[0] - q[2])));
    if (l < 1e-16 || l / longest < 1e-3) continue;
    n.push_back(nn / l); c.push_back((q[0] + q[1] + q[2]) / 3.0); d.push_back(manifold::la::dot(nn / l, q[0]));
    p0.push_back(q[0]); p1.push_back(q[1]); p2.push_back(q[2]);
  }
  auto inside = [&](size_t i, const vec3 & q) {
    return manifold::la::dot(manifold::la::cross(p1[i] - p0[i], q - p0[i]), n[i]) >= -1e-12
        && manifold::la::dot(manifold::la::cross(p2[i] - p1[i], q - p1[i]), n[i]) >= -1e-12
        && manifold::la::dot(manifold::la::cross(p0[i] - p2[i], q - p2[i]), n[i]) >= -1e-12;
  };
  /* candidates by a coarse grid, so this stays cheap */
  const double CELL = 0.1;
  std::map<std::tuple<long, long, long>, std::vector<size_t> > grid;
  for (size_t i = 0; i < c.size(); i++) grid[std::make_tuple((long)std::floor(c[i].x / CELL), (long)std::floor(c[i].y / CELL), (long)std::floor(c[i].z / CELL))].push_back(i);
  for (size_t i = 0; i < c.size(); i++) {
    long cx = (long)std::floor(c[i].x / CELL), cy = (long)std::floor(c[i].y / CELL), cz = (long)std::floor(c[i].z / CELL);
    for (long dx = -1; dx <= 1; dx++) for (long dy = -1; dy <= 1; dy++) for (long dz = -1; dz <= 1; dz++) {
      auto it = grid.find(std::make_tuple(cx + dx, cy + dy, cz + dz));
      if (it == grid.end()) continue;
      for (size_t j : it->second) {
        if (j <= i || manifold::la::dot(n[i], n[j]) > -0.999) continue;
        if (std::fabs(manifold::la::dot(n[i], c[j]) - d[i]) > 1e-4) continue;
        if (inside(i, c[j]) || inside(j, c[i])) return true;
      }
    }
  }
  return false;
}

/* one step: the solid plus or minus the sweep of its boundary by the bit
 * at the given size, the result put back onto its planes; f carries the
 * faces of m in and of the result out. A step of size zero does nothing
 * (and is skipped outright: gap or bevel zero must not depend on how a
 * hull of a flat point set behaves). */
Manifold step(const Manifold & m, faces_s & f, const std::vector<vec3> & unit, double size, bool add, std::string & err) {
  if (size <= 0) return m;
  /* a fold in the rebuilt result means the boolean broke a tie badly:
   * the step is redone with the bit turned a little differently, which
   * changes every tie; a few times, then it is an error */
  const int ATTEMPTS = 6;
  Manifold r;
  int attempt = 0;
  for (; attempt < ATTEMPTS; attempt++) {
    Manifold tube = boundaryTube(f, bitPoints(unit, size, attempt));
    Manifold u = (add ? m + tube : m - tube).Simplify(1e-5);
    faces_s g;
    extractFaces(u, g);
    r = rebuilt(g, u);
    if (!folded(r)) break;
  }
  if (attempt == ATTEMPTS) { err = "a Minkowski step left a fold in the surface, even with the bit turned"; return Manifold(); }
  /* the pieces for the next sweep come from the solid as rebuilt, not
   * from the faces it was rebuilt from: Manifold drops what it cannot
   * hold (a zero-thickness flap the boolean left, say), and a piece of
   * that would sweep a spike into the solid */
  extractFaces(r, f);
  return r;
}

/* the construction for one body */
bool construct(const Manifold & body, const bits_s & b, double g, double r, bool fills, Manifold & out, std::string & err) {
  faces_s f;
  extractFaces(body, f);
  Manifold m = rebuilt(f, body).Simplify(1e-7);
  extractFaces(m, f);
  /* one step of the sequence; false when the step failed (an empty
   * result without a message is a shape that vanished, checked where
   * that can happen) */
  auto go = [&](const std::vector<vec3> & unit, double size, bool add) {
    m = step(m, f, unit, size, add, err);
    return !(m.IsEmpty() && !err.empty());
  };
  if (!b.prism) {
    if (fills) {
      /* cube in 2b, octahedron out 2b and in b, cube out b, then the gap
       * with the cuboctahedron */
      double bp = b.kFill * r;
      if (!go(b.offset, 2 * bp, false)) return false;
      if (m.IsEmpty()) { err = "nothing is left of the shape after the bevel step (twice the bevel too large for its cells)"; return false; }
      if (!go(b.chamferFill, 2 * bp, true)) return false;
      if (!go(b.chamferFill, bp, false)) return false;
      if (!go(b.offset, bp, true)) return false;
      if (!go(b.chamfer, g, false)) return false;
      if (m.IsEmpty()) { err = "nothing is left of the shape after the gap step"; return false; }
    } else {
      /* cube in b', cuboctahedron out b' - g (in by the difference when
       * the gap is the larger, nothing when they are equal) */
      double bp = b.k * r;
      if (!go(b.offset, bp, false)) return false;
      if (m.IsEmpty()) { err = "nothing is left of the shape after the bevel step (bevel too large for its cells)"; return false; }
      if (bp > g) { if (!go(b.chamfer, bp - g, true)) return false; }
      else if (g > bp) { if (!go(b.chamfer, g - bp, false)) return false; }
      if (m.IsEmpty()) { err = "nothing is left of the shape after the gap step"; return false; }
    }
  } else {
    if (fills) {
      /* prism in 2b, bipyramid out 2b, bipyramid in b, prism out b - g
       * (in by the difference when the gap is the larger, nothing when
       * they are equal) */
      if (!go(b.offset, 2 * r, false)) return false;
      if (m.IsEmpty()) { err = "nothing is left of the shape after the bevel step (twice the bevel too large for its cells)"; return false; }
      if (!go(b.chamfer, 2 * r, true)) return false;
      if (!go(b.chamfer, r, false)) return false;
      if (r > g) { if (!go(b.offset, r - g, true)) return false; }
      else if (g > r) { if (!go(b.offset, g - r, false)) return false; }
      if (m.IsEmpty()) { err = "nothing is left of the shape after the gap step"; return false; }
    } else {
      /* prism in b + g, bipyramid out b */
      if (!go(b.offset, r + g, false)) return false;
      if (m.IsEmpty()) { err = "nothing is left of the shape after the gap step (gap + bevel too large for its cells)"; return false; }
      if (!go(b.chamfer, r, true)) return false;
    }
  }
  /* the result simplified (collinear vertices and features below a
   * millionth of a cell removed) and put back onto its planes once more,
   * so that nothing thinner than single precision resolves is left */
  if (!m.IsEmpty()) {
    m = m.Simplify(1e-6);
    extractFaces(m, f);
    m = rebuilt(f, m);
  }
  if (m.Status() != Manifold::Error::NoError) { err = "the Minkowski construction produced an invalid solid"; return false; }
  out = m;
  return true;
}

/* one cell of the shape for tagging the faces: its index and its faces
 * as outward planes, in getNeighbor's numbering */
struct cellFaces_s {
  unsigned index = 0;
  struct face_s { int n = 0; vec3 normal{}; double d = 0; };
  std::vector<face_s> faces;
};

/* the manifold's triangles into the polyhedron, one polyhedron vertex per
 * manifold vertex (the manifold's mesh is a manifold with shared vertex
 * indices; welding by position would merge vertices that only float
 * precision tells apart and break that). Facet faces (normal on a facet
 * direction) plain, everything else flagged as bevel. Every face is
 * tagged with the voxel whose
 * cell holds its centroid (the 3D view removes that voxel on a click),
 * and a face lying on one of that cell's faces, g inside it, with the
 * number of that cell face (the view adds the neighbour across it). */
bool addFaces(Polyhedron * poly, const Manifold & m, const std::vector<vec3> & facets, const std::vector<cellFaces_s> & cells, double g, std::string & err) {
  manifold::MeshGL64 mesh = m.GetMeshGL64();
  const size_t np = mesh.numProp;
  const size_t nv = mesh.vertProperties.size() / np;
  std::vector<int> id(nv);
  std::vector<Vector3Df> fp(nv);
  for (size_t i = 0; i < nv; i++) {
    fp[i] = Vector3Df((float)mesh.vertProperties[i * np], (float)mesh.vertProperties[i * np + 1], (float)mesh.vertProperties[i * np + 2]);
    id[i] = poly->addVertex(fp[i])->index();
  }
  for (size_t t = 0; t + 2 < mesh.triVerts.size(); t += 3) {
    size_t i0 = mesh.triVerts[t], i1 = mesh.triVerts[t + 1], i2 = mesh.triVerts[t + 2];
    vec3 p0(mesh.vertProperties[i0 * np], mesh.vertProperties[i0 * np + 1], mesh.vertProperties[i0 * np + 2]);
    vec3 p1(mesh.vertProperties[i1 * np], mesh.vertProperties[i1 * np + 1], mesh.vertProperties[i1 * np + 2]);
    vec3 p2(mesh.vertProperties[i2 * np], mesh.vertProperties[i2 * np + 1], mesh.vertProperties[i2 * np + 2]);
    vec3 n = manifold::la::cross(p1 - p0, p2 - p0);
    double l = manifold::la::length(n);
    /* the polyhedron holds single precision, and a face that has no area
     * there has no normal (the STL writer asserts on it) */
    Vector3Df fn = (fp[i1] - fp[i0]) ^ (fp[i2] - fp[i0]);
    if (l < 1e-14 || fn.squaredModule() == 0) { err = "the Minkowski construction left a face without area"; return false; }
    bool facet = false;
    n = n / l;
    for (const vec3 & d : facets) if (manifold::la::dot(d, n) > 1 - 1e-4) facet = true;
    /* the cell holding the centroid: the one it violates least */
    vec3 c = (p0 + p1 + p2) / 3.0;
    size_t best = 0; double bestViolation = 1e300;
    for (size_t k = 0; k < cells.size(); k++) {
      double violation = -1e300;
      for (const cellFaces_s::face_s & cf : cells[k].faces) violation = std::max(violation, manifold::la::dot(cf.normal, c) - cf.d);
      if (violation < bestViolation) { bestViolation = violation; best = k; }
    }
    int side = -1;
    if (!cells.empty())
      for (const cellFaces_s::face_s & cf : cells[best].faces)
        if (manifold::la::dot(cf.normal, n) > 1 - 1e-4 && std::fabs(manifold::la::dot(cf.normal, c) - (cf.d - g)) < 1e-6) side = cf.n;
    Face * f = poly->addFace(id[i0], id[i1], id[i2]);
    f->_fb_face = side; f->_fb_index = cells.empty() ? 0 : (int)cells[best].index;
    f->_flags = facet ? 0 : FF_BEVEL_FACE;
  }
  return true;
}

typedef __int128 i128;

/* ---------------------------------------------------------------------
 * The exact path (cube and octahedron bits: the rhombic and tetra-octa
 * grids).
 *
 * Everything happens in an integer frame: one cell is FRAME units, the
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

/* the small integer vector a float normal is a multiple of */
bool intNormal(const vec3 & n, int64_t out[3]) {
  double mx = std::max(std::fabs(n.x), std::max(std::fabs(n.y), std::fabs(n.z)));
  if (mx <= 0) return false;
  const double v[3] = { n.x / mx, n.y / mx, n.z / mx };
  for (int q = 1; q <= 12; q++) {
    int64_t r[3]; bool ok = true;
    for (int k = 0; k < 3; k++) { r[k] = std::llround(v[k] * q); if (std::fabs(v[k] * q - r[k]) > 1e-7 * q) ok = false; }
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
  out.pos.clear(); out.tri.clear(); out.orig.clear(); out.pieces.clear();
  manifold::MeshGL64 G = m.GetMeshGL64();
  const size_t np = G.numProp;
  std::vector<vec3> & pos = out.pos;
  for (size_t i = 0; i * np < G.vertProperties.size(); i++) pos.push_back(vec3(G.vertProperties[i * np], G.vertProperties[i * np + 1], G.vertProperties[i * np + 2]));
  const std::vector<uint64_t> & tv = G.triVerts;
  out.orig = tv;
  const size_t nt = tv.size() / 3, nv = pos.size();
  if (!nt) return true;
  auto P = [&](size_t t, int c) -> const vec3 & { return pos[tv[3 * t + c]]; };
  std::map<std::pair<uint64_t, uint64_t>, size_t> owner;
  for (size_t t = 0; t < nt; t++) for (int c = 0; c < 3; c++) owner[std::make_pair(tv[3 * t + c], tv[3 * t + (c + 1) % 3])] = t;
  auto neighbour = [&](size_t t, int c) -> long {
    auto it = owner.find(std::make_pair(tv[3 * t + (c + 1) % 3], tv[3 * t + c]));
    return it == owner.end() ? -1 : (long)it->second;
  };
  auto onPlane = [&](const iplane_s & pl, const vec3 & p) {
    long double v = (long double)pl.a * p.x + (long double)pl.b * p.y + (long double)pl.c * p.z - pl.d;
    return std::fabs((double)v) < 0.05;
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
    if (dx % det || dy % det || dz % det) { err = "a corner of a Minkowski step is not a whole number of units"; return false; }
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
    /* the outline loops (as in extractFaces: sharpest left turn where it touches itself) */
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
      manifold::Polygons polys(kept.size());
      for (size_t l = 0; l < kept.size(); l++) for (int i : idx[l]) polys[l].push_back(manifold::vec2((double)p2[i][0], (double)p2[i][1]));
      std::vector<manifold::ivec3> tri3;
      try { tri3 = manifold::Triangulate(polys, 0.5); }
      catch (...) { err = "a face of a Minkowski step could not be triangulated"; return false; }
      for (const manifold::ivec3 & t3 : tri3) { tt.push_back(t3.x); tt.push_back(t3.y); tt.push_back(t3.z); }
    }
    i128 covered = 0;
    for (size_t k = 0; k + 2 < tt.size(); k += 3) {
      i128 a2 = cross2(tt[k], tt[k + 1], tt[k + 2]);
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
  out = fromTriangles(f.pos, f.tri);
  if (out.Status() != Manifold::Error::NoError) { err = "the exact rebuild of a Minkowski step is not a valid solid"; return false; }
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
  Manifold u = add ? m + tube : m - tube;
  faces_s g;
  if (!exactFaces(u, g, err)) return false;
  if (!rebuiltExact(g, m, err)) return false;
  /* the next step's pieces: the faces just rebuilt, which are exactly m's */
  f = g;
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

Polyhedron * polyhedron(const voxel_c & v, double g, double r, bool fills, std::string & err)
{
  if (!handles(v)) { err = "grid not handled by the Minkowski mesher"; return 0; }
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
          for (unsigned k = 0; k + 2 < corners.size(); k += 3) { pts.push_back(vec3(corners[k], corners[k + 1], corners[k + 2])); centre = centre + pts.back(); np++; }
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
          cf.faces.push_back({ pn.first, nrm, manifold::la::dot(nrm, pts[0]) });
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
  bits_s b;
  if (v.getGridType()->getType() == gridType_c::GT_TRIANGULAR_PRISM) { if (!prismBits(facets, b, err)) return 0; }
  else b = cubeBits();
  Manifold body = exactBody(v, err);
  if (body.IsEmpty()) return 0;
  Manifold outer;
  if (!b.prism) {
    /* the exact path, in the integer frame */
    const int64_t gu = units(g * FRAME), ru = units(r * FRAME);
    if (!constructExact(body.Scale(vec3(FRAME)), b, gu, ru, r, fills, outer, err)) return 0;
    outer = outer.Scale(vec3(1 / FRAME));
  } else if (!construct(body, b, g, r, fills, outer, err)) return 0;
  Polyhedron * poly = new Polyhedron();
  if (!addFaces(poly, outer, facets, cellFaces, g, err)) { delete poly; return 0; }
  poly->finalize();
  return poly;
}

} // namespace minkMesh
