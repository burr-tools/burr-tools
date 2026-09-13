/* see cubemesh.h */
#include "cubemesh.h"
#include "triangulate.h"
#include "cubetable.h"
#include "cubetable_nofill.h"

#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace cubeMesh {

namespace {

/* the regime of a case for this ratio: the interval containing it, or
 * at a tie ratio (skipped by the table generator) the neighbour whose
 * boundary is nearest. The last regime runs to infinity: every regime
 * boundary is a finite ratio, so beyond the last one the structure is
 * the g = 0 structure, whatever ratio the table generation stopped at. */
const cubeRegime_s * regimeOf(const cubeCase_s & c, double ratio) {
  const cubeRegime_s * best = 0;
  double bd = 1e100;
  const cubeRegime_s * last = 0;
  for (int i = 0; i < c.nRegime; i++) if (!last || c.regime[i].hi > last->hi) last = &c.regime[i];
  if (last && ratio >= last->lo) return last;
  for (int i = 0; i < c.nRegime; i++) {
    const cubeRegime_s & R = c.regime[i];
    if (ratio >= R.lo && ratio <= R.hi) return &R;
    double d = ratio < R.lo ? R.lo - ratio : ratio - R.hi;
    if (d < bd) { bd = d; best = &R; }
  }
  return bd < 2e-3 ? best : 0;
}

/* vertex welding by proximity: a point within TOL of a stored point is
 * that point. Buckets of TOL*4, a lookup checks the 27 around it. */
struct weld_s {
  static constexpr double TOL = 1e-7;
  std::vector<vec3> pts;
  std::map<long long, std::vector<int> > buckets;
  static long long key(long long a, long long b, long long c) {
    return ((a & 0x1fffffLL) << 42) | ((b & 0x1fffffLL) << 21) | (c & 0x1fffffLL);
  }
  int add(const vec3 & p) {
    long long ix = (long long)floor(p.x / (TOL * 4)), iy = (long long)floor(p.y / (TOL * 4)), iz = (long long)floor(p.z / (TOL * 4));
    for (long long dx = -1; dx <= 1; dx++)
      for (long long dy = -1; dy <= 1; dy++)
        for (long long dz = -1; dz <= 1; dz++) {
          std::map<long long, std::vector<int> >::iterator it = buckets.find(key(ix + dx, iy + dy, iz + dz));
          if (it == buckets.end()) continue;
          for (unsigned k = 0; k < it->second.size(); k++) {
            const vec3 & q = pts[it->second[k]];
            if (fabs(q.x - p.x) < TOL && fabs(q.y - p.y) < TOL && fabs(q.z - p.z) < TOL) return it->second[k];
          }
        }
    int id = (int)pts.size();
    pts.push_back(p);
    buckets[key(ix, iy, iz)].push_back(id);
    return id;
  }
};

struct v2_s { double x, y; };

/* the 48 symmetries of the cube about the vertex, the canonical case of
 * every vertex mask and the transform that takes the case's body to it -
 * computed at first use (the tables hold the geometry, the
 * combinatorics is code). A transform maps a point p of the 2x2x2
 * body about (1,1,1) to q[a] = flip[a] ? 2 - p[perm[a]] : p[perm[a]],
 * and a cell (bit 4x+2y+z) the same way. */
struct sym_s {
  struct T { int perm[3]; int flip[3]; };
  T t[48];
  int canonOf[256];       /* the canonical mask, 0 when the mask has no case (not face-connected, or empty) */
  int transformOf[256];
  static int apply(int mask, const T & tr) {
    int out = 0;
    for (int c = 0; c < 8; c++) {
      if (!(mask & (1 << c))) continue;
      int p[3] = { (c >> 2) & 1, (c >> 1) & 1, c & 1 }, q[3];
      for (int a = 0; a < 3; a++) q[a] = tr.flip[a] ? 1 - p[tr.perm[a]] : p[tr.perm[a]];
      out |= 1 << (4 * q[0] + 2 * q[1] + q[2]);
    }
    return out;
  }
  sym_s() {
    int n = 0;
    int perms[6][3] = { {0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0} };
    for (int p = 0; p < 6; p++)
      for (int f = 0; f < 8; f++) {
        for (int a = 0; a < 3; a++) { t[n].perm[a] = perms[p][a]; t[n].flip[a] = (f >> (2 - a)) & 1; }
        n++;
      }
    for (int mask = 0; mask < 256; mask++) {
      int best = mask, bt = 0;
      for (int k = 0; k < 48; k++) { int m = apply(mask, t[k]); if (m < best) best = m; }
      canonOf[mask] = 0; transformOf[mask] = 0;
      for (int c = 0; c < cubeNumCases; c++) if (cubeCases[c].mask == best) canonOf[mask] = best;
      if (!canonOf[mask]) continue;
      for (int k = 0; k < 48; k++) if (apply(best, t[k]) == mask) { bt = k; break; }
      transformOf[mask] = bt;
    }
  }
};

const sym_s & sym() { static sym_s s; return s; }

/* the face-connected pieces of a vertex's cells: cells touching only at
 * an edge or a corner are separate bodies 2g apart and the chamfer
 * follows the rules for each on its own */
static int pieces(int mask, int out[8]) {
  int n = 0, left = mask;
  while (left) {
    int seed = left & -left, comp = seed, grow = seed;
    while (grow) {
      int c = 0; while (!((grow >> c) & 1)) c++;
      grow &= ~(1 << c);
      for (int d = 1; d <= 4; d <<= 1) {
        int nb = c ^ d;
        if ((left >> nb) & 1 && !((comp >> nb) & 1)) { comp |= 1 << nb; grow |= 1 << nb; }
      }
    }
    out[n++] = comp;
    left &= ~comp;
  }
  return n;
}

/* a polygon as emitted for one dual cell: a loop of welded vertex ids,
 * and for a non-planar hole patch its triangulation (index triples into
 * the loop); planar polygons are triangulated after consolidation */
struct poly_s {
  std::vector<int> loop;
  std::vector<int> tris;
};

static vec3 sub(const vec3 & a, const vec3 & b) { vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z }; return r; }
static vec3 cross(const vec3 & a, const vec3 & b) { vec3 r = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; return r; }
static double dot(const vec3 & a, const vec3 & b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static double len(const vec3 & a) { return sqrt(dot(a, a)); }

/* Newell normal of a loop (unit length, or zero for a degenerate loop) */
static vec3 loopNormal(const std::vector<int> & loop, const std::vector<vec3> & pts) {
  vec3 n = {0, 0, 0};
  for (unsigned q = 0; q < loop.size(); q++) {
    const vec3 & a = pts[loop[q]], & b = pts[loop[(q + 1) % loop.size()]];
    n.x += (a.y - b.y) * (a.z + b.z); n.y += (a.z - b.z) * (a.x + b.x); n.z += (a.x - b.x) * (a.y + b.y);
  }
  double ln = len(n);
  if (ln < 1e-14) { vec3 z = {0, 0, 0}; return z; }
  n.x /= ln; n.y /= ln; n.z /= ln;
  return n;
}

/* ear clipping of a planar face (outer loop CCW about n, holes CW) into
 * index triples of vertex ids */
static bool triangulateFace(const std::vector<std::vector<int> > & loops, const vec3 & n, const std::vector<vec3> & pts,
                            std::vector<int> & tris, std::string & why) {
  vec3 ax = {1, 0, 0};
  if (fabs(n.y) < fabs(n.x)) ax = {0, 1, 0};
  if (fabs(n.z) < fabs(dot(ax, n))) ax = {0, 0, 1};
  double d = dot(ax, n);
  vec3 u1 = { ax.x - d * n.x, ax.y - d * n.y, ax.z - d * n.z };
  double l1 = len(u1);
  u1.x /= l1; u1.y /= l1; u1.z /= l1;
  vec3 u2 = cross(n, u1);
  std::vector<v2_s> p2;
  std::vector<int> flat;
  std::vector<std::vector<int> > idx(loops.size());
  for (unsigned l = 0; l < loops.size(); l++)
    for (unsigned q = 0; q < loops[l].size(); q++) {
      const vec3 & a = pts[loops[l][q]];
      v2_s v = { dot(a, u1), dot(a, u2) };
      idx[l].push_back((int)p2.size());
      p2.push_back(v); flat.push_back(loops[l][q]);
    }
  std::vector<int> tt;
  const char * reason;
  if (!triangulateLoops(p2, idx, tt, reason)) {
    char buf[160];
    snprintf(buf, sizeof(buf), "a face of %zu loops, %zu vertices would not triangulate: %s", loops.size(), flat.size(), reason);
    why = buf;
    return false;
  }
  for (unsigned q = 0; q < tt.size(); q++) tris.push_back(flat[tt[q]]);
  return true;
}

/* The consolidation pass: the dual cells' polygons merged into whole
 * faces. Coplanar polygons sharing an edge (the edge one way in one, the
 * other way in the other - the output is watertight, so the cut across
 * a dual-cell boundary is the same segment on both sides) are one face;
 * a face's outline is the edges of its polygons that are not shared
 * inside it, linked into loops (one outer, CCW, plus holes, CW). Then a
 * vertex that lies inside a straight edge of EVERY loop it is on (the
 * points where the dual-cell boundaries cut the edges) is dropped
 * everywhere - it was nobody's corner. Non-planar hole patches keep
 * their loops and triangulation; a face whose outline touches itself
 * at a vertex is left as its polygons. */
static bool consolidate(const std::vector<poly_s> & raw0, mesh_s & out, std::string & err) {
  const std::vector<vec3> & pts = out.verts;
  /* with r = 0 the bevel strips and corner cuts collapse: welded ids
   * repeat along a loop and whole polygons have no area. Repeats are
   * removed and empty polygons dropped (a patch's triangles with a
   * repeated id likewise); what remains is the gap step's surface */
  std::vector<poly_s> raw;
  for (unsigned p = 0; p < raw0.size(); p++) {
    poly_s P = raw0[p];
    std::vector<int> L;
    for (unsigned q = 0; q < P.loop.size(); q++) if (L.empty() || L.back() != P.loop[q]) L.push_back(P.loop[q]);
    while (L.size() > 1 && L.front() == L.back()) L.pop_back();
    if (!P.tris.empty()) {
      std::vector<int> T;
      for (unsigned t = 0; t + 2 < P.tris.size(); t += 3) {
        int a = P.loop[P.tris[t]], b = P.loop[P.tris[t + 1]], c = P.loop[P.tris[t + 2]];
        if (a == b || b == c || a == c) continue;
        T.push_back(a); T.push_back(b); T.push_back(c);
      }
      if (T.empty()) continue;
      P.tris = T;                                   /* now vertex ids, not loop indices */
      P.loop = L;
      raw.push_back(P);
      continue;
    }
    if (L.size() < 3) continue;
    P.loop = L;
    vec3 n = loopNormal(P.loop, pts);
    if (len(n) < 0.5) {
      /* zero area, or nearly: a collapsed strip (r = 0); anything else is an error */
      vec3 a = {0, 0, 0};
      for (unsigned q = 0; q < L.size(); q++) { vec3 c = cross(pts[L[q]], pts[L[(q + 1) % L.size()]]); a.x += c.x; a.y += c.y; a.z += c.z; }
      if (len(a) > 1e-12) { err = "a degenerate polygon"; return false; }
      continue;
    }
    raw.push_back(P);
  }
  const unsigned N = raw.size();
  std::vector<vec3> nrm(N);
  std::vector<char> planar(N, 0);
  for (unsigned p = 0; p < N; p++) {
    planar[p] = raw[p].tris.empty();
    if (planar[p]) nrm[p] = loopNormal(raw[p].loop, pts);
  }
  /* union-find over shared edges between coplanar polygons */
  std::vector<int> parent(N);
  for (unsigned p = 0; p < N; p++) parent[p] = (int)p;
  struct uf_s { static int find(std::vector<int> & pa, int i) { while (pa[i] != i) { pa[i] = pa[pa[i]]; i = pa[i]; } return i; } };
  std::map<std::pair<int,int>, int> owner;      /* directed edge -> polygon */
  for (unsigned p = 0; p < N; p++) {
    if (!planar[p]) continue;
    const std::vector<int> & L = raw[p].loop;
    for (unsigned q = 0; q < L.size(); q++) {
      std::pair<int,int> e(L[q], L[(q + 1) % L.size()]);
      if (owner.count(e)) { err = "a directed edge occurs twice"; return false; }
      owner[e] = (int)p;
    }
  }
  for (std::map<std::pair<int,int>, int>::iterator it = owner.begin(); it != owner.end(); ++it) {
    std::map<std::pair<int,int>, int>::iterator rv = owner.find(std::make_pair(it->first.second, it->first.first));
    if (rv == owner.end()) continue;
    int a = it->second, b = rv->second;
    if (dot(nrm[a], nrm[b]) < 1 - 1e-9) continue;
    int ra = uf_s::find(parent, a), rb = uf_s::find(parent, b);
    if (ra != rb) parent[ra] = rb;
  }
  std::map<int, std::vector<int> > groups;
  for (unsigned p = 0; p < N; p++) if (planar[p]) groups[uf_s::find(parent, (int)p)].push_back((int)p);
  /* faces: loops (outer first) + the normal; patches go through as they are */
  struct face_s { std::vector<std::vector<int> > loops; vec3 n{}; std::vector<int> tris; };
  std::vector<face_s> faces;
  for (unsigned p = 0; p < N; p++)
    if (!planar[p]) {
      face_s f; f.loops.push_back(raw[p].loop); f.n = {0, 0, 0};
      f.tris = raw[p].tris;
      faces.push_back(f);
    }
  for (std::map<int, std::vector<int> >::iterator g = groups.begin(); g != groups.end(); ++g) {
    const std::vector<int> & members = g->second;
    std::set<int> inGroup(members.begin(), members.end());
    std::map<int, int> next;                       /* outline: vertex -> next vertex */
    bool pinch = false;
    for (unsigned m = 0; m < members.size() && !pinch; m++) {
      const std::vector<int> & L = raw[members[m]].loop;
      for (unsigned q = 0; q < L.size(); q++) {
        int a = L[q], b = L[(q + 1) % L.size()];
        std::map<std::pair<int,int>, int>::iterator rv = owner.find(std::make_pair(b, a));
        if (rv != owner.end() && inGroup.count(rv->second)) continue;    /* shared inside the face */
        if (next.count(a)) { pinch = true; break; }
        next[a] = b;
      }
    }
    face_s f; f.n = nrm[members[0]];
    if (pinch) {
      /* the outline touches itself: keep the polygons as they are */
      for (unsigned m = 0; m < members.size(); m++) { face_s s; s.n = nrm[members[m]]; s.loops.push_back(raw[members[m]].loop); faces.push_back(s); }
      continue;
    }
    std::set<int> seen;
    for (std::map<int, int>::iterator it = next.begin(); it != next.end(); ++it) {
      if (seen.count(it->first)) continue;
      std::vector<int> loop;
      int v = it->first;
      while (!seen.count(v)) { seen.insert(v); loop.push_back(v); v = next[v]; }
      if (v != it->first) { err = "a face outline does not close"; return false; }
      f.loops.push_back(loop);
    }
    /* the outer loop first: the one with positive area about the normal */
    int outer = -1;
    for (unsigned l = 0; l < f.loops.size(); l++) {
      vec3 a = {0, 0, 0};
      const std::vector<int> & L = f.loops[l];
      for (unsigned q = 0; q < L.size(); q++) {
        vec3 c = cross(pts[L[q]], pts[L[(q + 1) % L.size()]]);
        a.x += c.x; a.y += c.y; a.z += c.z;
      }
      if (dot(a, f.n) > 0) { if (outer >= 0) { err = "a face with two outer loops"; return false; } outer = (int)l; }
    }
    if (outer < 0) { err = "a face without an outer loop"; return false; }
    std::swap(f.loops[0], f.loops[outer]);
    faces.push_back(f);
  }
  /* corners: a vertex is kept where it is a corner of some loop; patch
   * vertices always are */
  std::vector<char> corner(pts.size(), 0);
  for (unsigned fi = 0; fi < faces.size(); fi++)
    for (unsigned l = 0; l < faces[fi].loops.size(); l++) {
      const std::vector<int> & L = faces[fi].loops[l];
      for (unsigned q = 0; q < L.size(); q++) {
        int v = L[q];
        if (!faces[fi].tris.empty()) { corner[v] = 1; continue; }
        const vec3 & a = pts[L[(q + L.size() - 1) % L.size()]], & b = pts[v], & c = pts[L[(q + 1) % L.size()]];
        vec3 d = sub(c, a); double ld = len(d);
        if (ld < 1e-12) { corner[v] = 1; continue; }
        double off = len(cross(sub(b, a), d)) / ld;         /* distance of b from the line a-c */
        double t = dot(sub(b, a), d);
        if (off > 1e-6 || t <= 0 || t >= ld * ld) corner[v] = 1;
      }
    }
  out.tris.clear();
  for (unsigned fi = 0; fi < faces.size(); fi++) {
    face_s & f = faces[fi];
    for (unsigned l = 0; l < f.loops.size(); l++) {
      std::vector<int> kept;
      for (unsigned q = 0; q < f.loops[l].size(); q++) if (corner[f.loops[l][q]]) kept.push_back(f.loops[l][q]);
      if (kept.size() < 3) { err = "a face loop lost its corners"; return false; }
      f.loops[l] = kept;
    }
    if (!f.tris.empty()) { out.tris.insert(out.tris.end(), f.tris.begin(), f.tris.end()); continue; }
    std::string why;
    if (!triangulateFace(f.loops, f.n, pts, out.tris, why)) { err = why; return false; }
  }
  return true;
}

/* g = r = 0: every exposed cell face as one quad, welded; no table */
static bool plainSurface(int nx, int ny, int nz, const std::vector<char> & filled, mesh_s & out, std::string & /*err*/) {
  weld_s weld;
  const int d[6][3] = { {-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1} };
  for (int z = 0; z < nz; z++)
    for (int y = 0; y < ny; y++)
      for (int x = 0; x < nx; x++) {
        if (!filled[x + nx * (y + ny * z)]) continue;
        for (int f = 0; f < 6; f++) {
          int xn = x + d[f][0], yn = y + d[f][1], zn = z + d[f][2];
          if (xn >= 0 && yn >= 0 && zn >= 0 && xn < nx && yn < ny && zn < nz && filled[xn + nx * (yn + ny * zn)]) continue;
          /* the face's corners, counter-clockwise seen from outside */
          int a = f / 2, s = f % 2;              /* axis, side (0 = low, 1 = high) */
          int u = (a + 1) % 3, v = (a + 2) % 3;
          std::vector<int> loop;
          for (int k = 0; k < 4; k++) {
            int cu = (k == 1 || k == 2), cv = (k >= 2);
            if (!s) std::swap(cu, cv);
            double p[3]; p[0] = x; p[1] = y; p[2] = z;
            p[a] += s; p[u] += cu; p[v] += cv;
            vec3 w = { p[0], p[1], p[2] };
            loop.push_back(weld.add(w));
          }
          out.tris.push_back(loop[0]); out.tris.push_back(loop[1]); out.tris.push_back(loop[2]);
          out.tris.push_back(loop[0]); out.tris.push_back(loop[2]); out.tris.push_back(loop[3]);
        }
      }
  out.verts = weld.pts;
  return true;
}

} // namespace

bool generate(int nx, int ny, int nz, const std::vector<char> & filled,
              double g, double r, bool fills, mesh_s & out, std::string & err) {
  out.verts.clear(); out.tris.clear();
  /* the table for the variant: with interior (concave) chamfers, or without */
  const cubeCase_s * cases = fills ? cubeCases : cubeNFCases;
  const int nCases = fills ? cubeNumCases : cubeNFNumCases;
  if ((int)filled.size() != nx * ny * nz) { err = "filled has the wrong size"; return false; }
  if (!(g >= 0) || !(r >= 0)) { err = "need g >= 0 and r >= 0"; return false; }
  if (g + r > 0.5) { err = "gap + bevel must stay below half a cell"; return false; }
  /* the special cases: r = 0 is the gap step alone and goes through the tables at ratio 0, where every
   * regime starts and the collapsed strips are dropped in consolidate();
   * g = 0 is the r/g -> infinity limit: the last regime of each case
   * evaluated at g = 0 (its formulas are affine, and every regime
   * boundary is a finite ratio, all below 5, so the structure beyond the
   * last boundary is the g = 0 structure); g = r = 0 is the plain cell
   * surface and is built without the tables */
  if (g == 0 && r == 0) return plainSurface(nx, ny, nz, filled, out, err);
  double ratio = g > 0 ? r / g : 1e300;
  /* a case's regime is looked up when the shape first needs the case;
   * a shape without a given vertex body is not held to that body's
   * table */
  std::vector<const cubeRegime_s *> regime(nCases, (const cubeRegime_s *)0);
  std::vector<char> resolved(nCases, 0);
  weld_s weld;
  std::vector<vec3> pv;
  std::vector<int> ids;
  std::vector<poly_s> raw;          /* every dual cell's polygons, merged afterwards */
  for (int k = 0; k <= nz; k++)
    for (int j = 0; j <= ny; j++)
      for (int i = 0; i <= nx; i++) {
        /* the 8 cells about vertex (i,j,k): bit 4cx + 2cy + cz for the
         * cell at (i-1+cx, j-1+cy, k-1+cz) */
        int mask = 0;
        for (int cx = 0; cx < 2; cx++)
          for (int cy = 0; cy < 2; cy++)
            for (int cz = 0; cz < 2; cz++) {
              int x = i - 1 + cx, y = j - 1 + cy, z = k - 1 + cz;
              if (x < 0 || y < 0 || z < 0 || x >= nx || y >= ny || z >= nz) continue;
              if (filled[x + nx * (y + ny * z)]) mask |= 1 << (4 * cx + 2 * cy + cz);
            }
        if (mask == 255) continue;         /* an interior vertex: no surface here */
        int comps[8];
        int nc = pieces(mask, comps);
        for (int pc = 0; pc < nc; pc++) {
        int piece = comps[pc];
        int ci = -1;
        for (int c = 0; c < nCases; c++) if (cases[c].mask == sym().canonOf[piece]) ci = c;
        if (ci < 0) {
          char buf[200];
          snprintf(buf, sizeof(buf), "no table for the vertex body at (%d,%d,%d) (mask %d, piece %d)", i, j, k, mask, piece);
          err = buf;
          return false;
        }
        if (!resolved[ci]) {
          resolved[ci] = 1;
          regime[ci] = regimeOf(cases[ci], ratio);
        }
        if (!regime[ci]) {
          char buf[300];
          const cubeCase_s & C = cases[ci];
          int n = 0;
          n += snprintf(buf + n, sizeof(buf) - n, "r/g = %.4f is outside the table for the vertex body at (%d,%d,%d) (mask %d, piece %d, case mask %d; tabulated:", ratio, i, j, k, mask, piece, C.mask);
          for (int q = 0; q < C.nRegime && n < (int)sizeof(buf) - 30; q++)
            n += snprintf(buf + n, sizeof(buf) - n, " %.3f..%.3f", C.regime[q].lo, C.regime[q].hi);
          snprintf(buf + n, sizeof(buf) - n, C.nRegime ? ")" : " nothing)");
          err = buf;
          return false;
        }
        const cubeRegime_s & R = *regime[ci];
        const sym_s::T & T = sym().t[sym().transformOf[piece]];
        /* orientation of the transform: a permutation's parity times
         * the flips' */
        int par = 1;
        for (int a = 0; a < 3; a++) for (int b = a + 1; b < 3; b++) if (T.perm[a] > T.perm[b]) par = -par;
        for (int a = 0; a < 3; a++) if (T.flip[a]) par = -par;
        /* the regime's vertices, placed */
        ids.resize(R.nVert);
        for (int v = 0; v < R.nVert; v++) {
          const double * c = R.vert + 9 * v;
          double p[3] = { c[0] + c[1] * g + c[2] * r, c[3] + c[4] * g + c[5] * r, c[6] + c[7] * g + c[8] * r };
          double q[3];
          for (int a = 0; a < 3; a++) q[a] = T.flip[a] ? 2.0 - p[T.perm[a]] : p[T.perm[a]];
          vec3 w = { q[0] + (i - 1), q[1] + (j - 1), q[2] + (k - 1) };
          ids[v] = weld.add(w);
        }
        for (int pi = 0; pi < R.nPoly; pi++) {
          int s = R.polyStart[pi], e = R.polyStart[pi + 1];
          poly_s P;
          for (int q = s; q < e; q++) P.loop.push_back(ids[R.polyVert[q]]);
          if (par < 0) std::reverse(P.loop.begin(), P.loop.end());
          int ts = R.triStart[pi], te = R.triStart[pi + 1];
          if (te > ts) {
            /* a non-planar hole patch: its own triangulation, as index
             * triples into the loop (the loop was reversed under a
             * reflection: index n-1-a, and the triangle's winding too) */
            int n = e - s;
            for (int t = ts; t + 2 < te; t += 3) {
              int a = R.triVert[t], b = R.triVert[t + 1], c2 = R.triVert[t + 2];
              if (par < 0) { a = n - 1 - a; b = n - 1 - b; c2 = n - 1 - c2; std::swap(b, c2); }
              P.tris.push_back(a); P.tris.push_back(b); P.tris.push_back(c2);
            }
          }
          raw.push_back(P);
        }
        }   /* pieces */
      }
  out.verts = weld.pts;
  return consolidate(raw, out, err);
}

} // namespace cubeMesh
