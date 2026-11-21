/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "voxel_lattice.h"

#include "latticegridinfo.h"
#include "gridtype.h"
#include "bt_assert.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <map>
#include <mutex>

voxel_lattice_c::voxel_lattice_c(unsigned int x, unsigned int y, unsigned int z, const gridType_c * g, voxel_type init)
  : voxel_c(x, y, z, g, init), inf(latticeGridInfo_c::get(g->getType())) {
  // only voxels may be filled
  if (init)
    for (unsigned int k = 0; k < voxels; k++) {
      unsigned int cx, cy, cz;
      indexToXYZ(k, &cx, &cy, &cz);
      if (!validCoordinate(cx, cy, cz)) space[k] = 0;
    }
  recalcBoundingBox();
  initHotspot();
}

voxel_lattice_c::voxel_lattice_c(xmlParser_c & pars, const gridType_c * g)
  : voxel_c(pars, g), inf(latticeGridInfo_c::get(g->getType())) {
  // only voxels may be filled, as in the constructor above: a file with
  // more would load and then fail in transform()
  bool cleared = false;
  for (unsigned int k = 0; k < voxels; k++)
    if (space[k]) {
      unsigned int cx, cy, cz;
      indexToXYZ(k, &cx, &cy, &cz);
      if (!validCoordinate(cx, cy, cz)) {
        space[k] = 0;
        cleared = true;
      }
    }
  if (cleared)
    recalcBoundingBox();
  if (!validCoordinate(hx, hy, hz))
    initHotspot();
}

voxel_lattice_c::voxel_lattice_c(const voxel_c & orig)
  : voxel_c(orig), inf(latticeGridInfo_c::get(orig.getGridType()->getType())) { }

voxel_lattice_c::voxel_lattice_c(const voxel_c * orig)
  : voxel_c(orig), inf(latticeGridInfo_c::get(orig->getGridType()->getType())) { }

void voxel_lattice_c::rebuild(const std::vector<cell_s> & cells, int vx, int vy, int vz) {

  int mx = 0, my = 0, mz = 0;
  for (const auto & c : cells) {
    mx = std::max(mx, c.x - vx);
    my = std::max(my, c.y - vy);
    mz = std::max(mz, c.z - vz);
  }

  sx = mx + 1;
  sy = my + 1;
  sz = mz + 1;
  voxels = sx * sy * sz;
  space.assign(voxels, 0);

  for (const auto & c : cells) {
    bt_assert(c.x - vx >= 0 && c.y - vy >= 0 && c.z - vz >= 0);
    space[(c.x - vx) + sx * ((c.y - vy) + sy * (c.z - vz))] = c.v;
  }

  hx -= vx;
  hy -= vy;
  hz -= vz;

  recalcBoundingBox();
  symmetries = symmetryInvalid();
}

bool voxel_lattice_c::transform(unsigned int nr) {

  // input validation, as in voxel_0_c::transform
  if (nr >= inf.transformations())
    bt_te("voxel_lattice_c::transform: nr < getNumTransformationsMirror()");

  /* The identity leaves the space where it is, as on every grid:
   * assembly_c::transform takes the result's box under transformation 0
   * (getBoundingBox(0)) as the original's position, and a space moved to
   * the smallest position here would shift every transformed assembly of a
   * result that does not sit there, so that none matched itself. */
  if (nr == 0) return true;

  std::vector<cell_s> cells;
  int mx = INT_MAX, my = INT_MAX, mz = INT_MAX;

  for (unsigned int z = 0; z < sz; z++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int x = 0; x < sx; x++) {
        const voxel_type v = space[x + sx * (y + sy * z)];
        if (!v) continue;
        int nx = x, ny = y, nz = z;
        if (!inf.apply(nr, nx, ny, nz))
          bt_te("voxel_lattice_c::transform: a filled cell that is not a voxel");
        cells.push_back({nx, ny, nz, v});
        mx = std::min(mx, nx);
        my = std::min(my, ny);
        mz = std::min(mz, nz);
      }

  int h[3] = {hx, hy, hz};
  if (!inf.apply(nr, h[0], h[1], h[2]))
    bt_te("voxel_lattice_c::transform: the hotspot is not a voxel position");
  hx = h[0]; hy = h[1]; hz = h[2];

  // an empty space keeps its position relative to the hotspot
  if (cells.empty()) {
    mx = hx; my = hy; mz = hz;
  }

  int vx, vy, vz;
  inf.alignDown(mx, my, mz, vx, vy, vz);
  rebuild(cells, vx, vy, vz);

  return true;
}

void voxel_lattice_c::transformPoint(int * x, int * y, int * z, unsigned int trans) const {
  if (trans >= inf.transformations())
    bt_te("voxel_lattice_c::transformPoint: trans < getNumTransformationsMirror()");
  int a = *x, b = *y, c = *z;
  // the positions handed in are voxel positions (placements are hotspots), which always map
  if (!inf.apply(trans, a, b, c))
    bt_te("voxel_lattice_c::transformPoint: not a voxel position");
  *x = a; *y = b; *z = c;
}

void voxel_lattice_c::getTransformMatrix(unsigned int trans, double m[9]) const {
  if (trans >= inf.transformations())
    bt_te("voxel_lattice_c::getTransformMatrix: trans < getNumTransformationsMirror()");
  inf.physicalMatrix(trans, m);
}

bool voxel_lattice_c::getNeighbor(unsigned int idx, unsigned int typ, int x, int y, int z, int * xn, int *yn, int *zn) const {

  if (typ == 0)
    return inf.faceNeighbour(x, y, z, idx, *xn, *yn, *zn);

  if (typ > 2) return false;

  const int b = inf.base(x, y, z);
  if (b < 0) return false;
  const auto & list = inf.otherNeighbours(b, typ);
  if (idx >= list.size()) return false;

  *xn = x + list[idx][0];
  *yn = y + list[idx][1];
  *zn = z + list[idx][2];
  return true;
}

void voxel_lattice_c::resizeInclude(int & px, int & py, int & pz) {

  /* move the contents by a lattice vector -v so that the point and
   * everything there is lands at non negative coordinates */
  int vx = 0, vy = 0, vz = 0;
  if (px < 0 || py < 0 || pz < 0)
    inf.alignDown(std::min(px, 0), std::min(py, 0), std::min(pz, 0), vx, vy, vz);

  const int nsx = std::max((int)sx - vx, px - vx + 1);
  const int nsy = std::max((int)sy - vy, py - vy + 1);
  const int nsz = std::max((int)sz - vz, pz - vz + 1);

  resize(nsx, nsy, nsz, 0);
  if (vx || vy || vz)
    translate(-vx, -vy, -vz, 0);

  px -= vx;
  py -= vy;
  pz -= vz;
}

void voxel_lattice_c::minimizePiece(void) {

  std::vector<cell_s> cells;
  for (unsigned int z = 0; z < sz; z++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int x = 0; x < sx; x++) {
        const voxel_type v = space[x + sx * (y + sy * z)];
        if (v) cells.push_back({(int)x, (int)y, (int)z, v});
      }

  if (cells.empty()) {
    voxel_c::minimizePiece();
    return;
  }

  int vx, vy, vz;
  inf.alignDown((int)bx1, (int)by1, (int)bz1, vx, vy, vz);
  rebuild(cells, vx, vy, vz);
}

void voxel_lattice_c::initHotspot(void) {

  /* the first voxel position of the space, else one near it: any voxel
   * position works, it only has to stay whole-numbered under every
   * transformation */
  for (unsigned int z = 0; z < sz; z++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int x = 0; x < sx; x++)
        if (validCoordinate(x, y, z)) {
          setHotspot(x, y, z);
          return;
        }

  for (int r = 0; r < 8; r++)
    for (int z = 0; z <= r; z++)
      for (int y = 0; y <= r; y++)
        for (int x = 0; x <= r; x++)
          if (validCoordinate(x, y, z)) {
            setHotspot(x, y, z);
            return;
          }

  bt_te("voxel_lattice_c::initHotspot: no voxel position near the origin");
}

bool voxel_lattice_c::validCoordinate(int x, int y, int z) const {
  return inf.isVoxel(x, y, z);
}

bool voxel_lattice_c::identicalInBB(const voxel_c * op, bool includeColors) const {
  return inf.isLatticeVector((int)bx1 - (int)op->boundX1(), (int)by1 - (int)op->boundY1(), (int)bz1 - (int)op->boundZ1()) &&
         voxel_c::identicalInBB(op, includeColors);
}

bool voxel_lattice_c::onGrid(int x, int y, int z) const {
  return inf.isLatticeVector(x, y, z);
}

void voxel_lattice_c::getConnectionFace(int x, int y, int z, int n, double bevel, double offset, std::vector<float> & faceCorners) const {
  // n < 0 asks for bevel faces, which described grids do not draw yet
  if (n < 0) return;
  inf.faceCorners(x, y, z, (unsigned int)n, bevel, offset, faceCorners);
}

void voxel_lattice_c::drawingBounds(float lo[3], float hi[3]) const {

  // the image of the space's box, and how far a voxel reaches past its point
  for (int k = 0; k < 3; k++) { lo[k] = 1e30f; hi[k] = -1e30f; }
  for (int c = 0; c < 8; c++) {
    float p[3] = {(c & 1) ? (float)(sx - 1) : 0.0f, (c & 2) ? (float)(sy - 1) : 0.0f, (c & 4) ? (float)(sz - 1) : 0.0f};
    inf.toPhysical(p[0], p[1], p[2]);
    for (int k = 0; k < 3; k++) {
      lo[k] = std::min(lo[k], p[k]);
      hi[k] = std::max(hi[k], p[k]);
    }
  }
  float rlo[3], rhi[3];
  inf.voxelReach(rlo, rhi);
  for (int k = 0; k < 3; k++) {
    lo[k] += rlo[k];
    hi[k] += rhi[k];
  }
}

void voxel_lattice_c::calculateSize(float * x, float * y, float * z) const {
  float lo[3], hi[3];
  drawingBounds(lo, hi);
  *x = hi[0] - lo[0];
  *y = hi[1] - lo[1];
  *z = hi[2] - lo[2];
}

void voxel_lattice_c::recalcSpaceCoordinates(float * x, float * y, float * z) const {
  inf.toPhysical(*x, *y, *z);
}

namespace {

  /* a convex cell as its corners and the planes of its faces (outward
   * normal n, offset d: inside is n.p <= d) */
  struct poly_s {
    std::vector<std::array<double, 3>> corners;
    std::vector<std::array<double, 4>> planes;
    std::array<double, 3> centre{};
  };

  constexpr double kGeoEps = 1e-5;

  /* the cell at voxel (x, y, z), grown by factor about the physical point c */
  poly_s cellPoly(const latticeGridInfo_c & inf, int x, int y, int z, double factor,
                  const std::array<double, 3> & c0 = {0, 0, 0}) {
    poly_s p;
    std::vector<float> c;
    std::vector<std::vector<std::array<double, 3>>> faces;
    for (unsigned int f = 0; f < inf.faces(x, y, z); f++) {
      c.clear();
      inf.faceCorners(x, y, z, f, 0, 0, c);
      std::vector<std::array<double, 3>> face;
      for (size_t i = 0; i + 2 < c.size(); i += 3) {
        face.push_back({c0[0] + factor * (c[i] - c0[0]), c0[1] + factor * (c[i+1] - c0[1]), c0[2] + factor * (c[i+2] - c0[2])});
        p.corners.push_back(face.back());
      }
      faces.push_back(std::move(face));
    }
    for (const auto & q : p.corners)
      for (int k = 0; k < 3; k++) p.centre[k] += q[k] / p.corners.size();
    for (const auto & face : faces) {
      // Newell's normal, then turned outwards
      double n[3] = {0, 0, 0};
      for (size_t i = 0; i < face.size(); i++) {
        const auto & a = face[i];
        const auto & b = face[(i + 1) % face.size()];
        n[0] += (a[1] - b[1]) * (a[2] + b[2]);
        n[1] += (a[2] - b[2]) * (a[0] + b[0]);
        n[2] += (a[0] - b[0]) * (a[1] + b[1]);
      }
      const double len = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
      if (len < 1e-12) continue;
      for (double & v : n) v /= len;
      double d = n[0]*face[0][0] + n[1]*face[0][1] + n[2]*face[0][2];
      if (n[0]*p.centre[0] + n[1]*p.centre[1] + n[2]*p.centre[2] > d) {
        for (double & v : n) v = -v;
        d = -d;
      }
      p.planes.push_back({n[0], n[1], n[2], d});
    }
    return p;
  }

  /* the cells of one grid, cheaply: one polyhedron per cell kind, moved by
   * the lattice vector from its base position (computing a cell from its
   * faces each time made scaling a large shape take minutes) */
  class polyCache_c {
    const latticeGridInfo_c & inf;
    std::vector<poly_s> base;
    std::vector<std::array<double, 3>> anchor;  // per kind: face 0's first corner
    mutable std::vector<float> buf;
  public:
    explicit polyCache_c(const latticeGridInfo_c & i) : inf(i) {
      for (unsigned int b = 0; b < inf.baseCount(); b++) {
        const std::array<int, 3> p = inf.basePosition(b);
        base.push_back(cellPoly(inf, p[0], p[1], p[2], 1.0));
        buf.clear();
        inf.faceCorners(p[0], p[1], p[2], 0, 0, 0, buf);
        anchor.push_back({buf[0], buf[1], buf[2]});
      }
    }
    poly_s get(int x, int y, int z) const {
      const int b = inf.base(x, y, z);
      if (b < 0) return {};
      /* the cell is its kind's base cell moved by a lattice vector; the
       * offset in physical space is where face 0's first corner went (the
       * faces start at their smallest corner, so the same corner) */
      buf.clear();
      inf.faceCorners(x, y, z, 0, 0, 0, buf);
      const double o[3] = {buf[0] - anchor[b][0], buf[1] - anchor[b][1], buf[2] - anchor[b][2]};
      poly_s r = base[b];
      for (auto & q : r.corners)
        for (int k = 0; k < 3; k++) q[k] += o[k];
      for (int k = 0; k < 3; k++) r.centre[k] += o[k];
      for (auto & pl : r.planes) pl[3] += pl[0]*o[0] + pl[1]*o[1] + pl[2]*o[2];
      return r;
    }
  };

  // p grown by k about the physical point c
  poly_s grown(poly_s p, double k, const std::array<double, 3> & c) {
    for (auto & q : p.corners)
      for (int i = 0; i < 3; i++) q[i] = c[i] + k * (q[i] - c[i]);
    for (int i = 0; i < 3; i++) p.centre[i] = c[i] + k * (p.centre[i] - c[i]);
    for (auto & pl : p.planes) {
      const double nc = pl[0]*c[0] + pl[1]*c[1] + pl[2]*c[2];
      pl[3] = nc + k * (pl[3] - nc);
    }
    return p;
  }

  // the largest signed distance of q past a face of p: < 0 strictly inside
  double outside(const poly_s & p, const std::array<double, 3> & q) {
    double m = -1e30;
    for (const auto & pl : p.planes)
      m = std::max(m, pl[0]*q[0] + pl[1]*q[1] + pl[2]*q[2] - pl[3]);
    return m;
  }

  void invert3(const double m[9], double r[9]) {
    const double det = m[0]*(m[4]*m[8]-m[5]*m[7]) - m[1]*(m[3]*m[8]-m[5]*m[6]) + m[2]*(m[3]*m[7]-m[4]*m[6]);
    r[0] = (m[4]*m[8]-m[5]*m[7])/det; r[1] = (m[2]*m[7]-m[1]*m[8])/det; r[2] = (m[1]*m[5]-m[2]*m[4])/det;
    r[3] = (m[5]*m[6]-m[3]*m[8])/det; r[4] = (m[0]*m[8]-m[2]*m[6])/det; r[5] = (m[2]*m[3]-m[0]*m[5])/det;
    r[6] = (m[3]*m[7]-m[4]*m[6])/det; r[7] = (m[1]*m[6]-m[0]*m[7])/det; r[8] = (m[0]*m[4]-m[1]*m[3])/det;
  }

  /* physical to voxel coordinates, measured from the cells' geometry: how a
   * cell's centre moves when its voxel position moves by each lattice
   * column (toPhysical() is the drawing's map, which on some grids is not
   * where faceCorners() puts the cells) */
  std::array<double, 3> toVoxel(const latticeGridInfo_c & inf, const std::array<double, 3> & q) {
    struct map_s { double inv[9]; std::array<double, 3> c0; std::array<int, 3> p0; };
    static std::mutex mtx;
    static std::map<const latticeGridInfo_c *, map_s> known;
    map_s m;
    {
      std::lock_guard<std::mutex> lock(mtx);
      auto it = known.find(&inf);
      if (it != known.end()) m = it->second;
      else {
        const std::array<int, 3> p0 = inf.basePosition(0);
        const std::array<double, 3> c0 = cellPoly(inf, p0[0], p0[1], p0[2], 1.0).centre;
        double L[9], M[9];  // columns: lattice vectors, and the moves of the centre
        for (int j = 0; j < 3; j++) {
          int lx, ly, lz;
          inf.latticeColumn(j, lx, ly, lz);
          L[j] = lx; L[3+j] = ly; L[6+j] = lz;
          const std::array<double, 3> cj = cellPoly(inf, p0[0] + lx, p0[1] + ly, p0[2] + lz, 1.0).centre;
          for (int i = 0; i < 3; i++) M[3*i+j] = cj[i] - c0[i];
        }
        // voxel = p0 + L M^-1 (q - c0)
        double Mi[9];
        invert3(M, Mi);
        for (int i = 0; i < 3; i++)
          for (int j = 0; j < 3; j++)
            m.inv[3*i+j] = L[3*i]*Mi[j] + L[3*i+1]*Mi[3+j] + L[3*i+2]*Mi[6+j];
        m.c0 = c0;
        m.p0 = p0;
        known[&inf] = m;
      }
    }
    const double d[3] = {q[0] - m.c0[0], q[1] - m.c0[1], q[2] - m.c0[2]};
    return {m.p0[0] + m.inv[0]*d[0] + m.inv[1]*d[1] + m.inv[2]*d[2],
            m.p0[1] + m.inv[3]*d[0] + m.inv[4]*d[1] + m.inv[5]*d[2],
            m.p0[2] + m.inv[6]*d[0] + m.inv[7]*d[1] + m.inv[8]*d[2]};
  }

  /* how far, in voxel coordinates, a cell's voxel position can lie from its
   * own corners: on some grids the position is no point of the cell */
  int voxelMargin(const latticeGridInfo_c & inf) {
    double m = 0;
    for (unsigned int b = 0; b < inf.baseCount(); b++) {
      const std::array<int, 3> p = inf.basePosition(b);
      for (const auto & q : cellPoly(inf, p[0], p[1], p[2], 1.0).corners) {
        const std::array<double, 3> v = toVoxel(inf, q);
        for (int k = 0; k < 3; k++) m = std::max(m, std::fabs(v[k] - p[k]));
      }
    }
    return (int)std::ceil(m) + 1;
  }

  // the cells of the grid that can meet the grown cell big: those whose
  // voxel position is in the voxel box around its corners, widened by the
  // grid's margin
  template <class F>
  void forFineCells(const latticeGridInfo_c & inf, int margin, const poly_s & big, F f) {
    int lo[3] = {INT_MAX, INT_MAX, INT_MAX}, hi[3] = {INT_MIN, INT_MIN, INT_MIN};
    for (const auto & q : big.corners) {
      const std::array<double, 3> v = toVoxel(inf, q);
      for (int k = 0; k < 3; k++) {
        lo[k] = std::min(lo[k], (int)std::floor(v[k]) - margin);
        hi[k] = std::max(hi[k], (int)std::ceil(v[k]) + margin);
      }
    }
    for (int z = lo[2]; z <= hi[2]; z++)
      for (int y = lo[1]; y <= hi[1]; y++)
        for (int x = lo[0]; x <= hi[0]; x++)
          if (inf.isVoxel(x, y, z)) f(x, y, z);
  }

  /* whether growing every cell by k about c gives unions of cells: for a
   * cell of each kind, no cell straddles the grown cell's border. A cell
   * straddles when points strictly inside it (its centre, and the points
   * halfway from there to each corner) are not all strictly inside or all
   * strictly outside the grown cell; corners alone do not tell, as on the
   * triangles, where every small corner lies on the big one's edges. A
   * grown cell's corner strictly inside a cell also means it is cut. One
   * cell per kind is enough: the other cells are lattice moves of it, and
   * growing moves them by k times a lattice vector, which keeps the grid. */
  bool exactAbout(const latticeGridInfo_c & inf, unsigned int k, const std::array<double, 3> & c) {
    const polyCache_c cells(inf);
    const int margin = voxelMargin(inf);
    for (unsigned int b = 0; b < inf.baseCount(); b++) {
      const std::array<int, 3> p = inf.basePosition(b);
      const poly_s big = cellPoly(inf, p[0], p[1], p[2], k, c);
      bool exact = true;
      forFineCells(inf, margin, big, [&](int x, int y, int z) {
        if (!exact) return;
        const poly_s small = cells.get(x, y, z);
        int in = 0, out = 0;
        auto sample = [&](const std::array<double, 3> & q) {
          const double o = outside(big, q);
          if (o < -kGeoEps) in++;
          else if (o > kGeoEps) out++;
          else exact = false;  // the border runs through the cell
        };
        sample(small.centre);
        for (const auto & q : small.corners)
          sample({(q[0] + small.centre[0]) / 2, (q[1] + small.centre[1]) / 2, (q[2] + small.centre[2]) / 2});
        if (in && out) exact = false;
        for (const auto & q : big.corners)
          if (outside(small, q) < -kGeoEps) exact = false;
      });
      if (!exact) return false;
    }
    return true;
  }

  struct scaleRule_s { bool exact; std::array<double, 3> centre; };

  /* the point to grow about so that growing by k is exact, if there is one:
   * the origin or a corner of a cell (on grids whose cells sit centred on
   * the lattice, growing by an even factor about the origin cuts cells) */
  scaleRule_s scaleRule(const latticeGridInfo_c & inf, unsigned int k) {
    static std::mutex mtx;
    static std::map<std::pair<const latticeGridInfo_c *, unsigned int>, scaleRule_s> known;
    {
      std::lock_guard<std::mutex> lock(mtx);
      auto it = known.find({&inf, k});
      if (it != known.end()) return it->second;
    }
    std::vector<std::array<double, 3>> centres{{0, 0, 0}};
    for (unsigned int b = 0; b < inf.baseCount(); b++) {
      const std::array<int, 3> p = inf.basePosition(b);
      for (const auto & q : cellPoly(inf, p[0], p[1], p[2], 1.0).corners)
        centres.push_back(q);
    }
    scaleRule_s rule{false, {0, 0, 0}};
    for (const auto & c : centres)
      if (exactAbout(inf, k, c)) {
        rule = {true, c};
        break;
      }
    std::lock_guard<std::mutex> lock(mtx);
    known[{&inf, k}] = rule;
    return rule;
  }

}

bool voxel_lattice_c::canScale(unsigned int amount) const {
  if (amount < 2) return amount == 1;
  return scaleRule(inf, amount).exact;
}

void voxel_lattice_c::scale(unsigned int amount, bool grid) {

  if (amount < 2 || !canScale(amount)) return;
  const scaleRule_s rule = scaleRule(inf, amount);
  const polyCache_c geo(inf);
  const int margin = voxelMargin(inf);

  std::vector<cell_s> cells;
  int mx = INT_MAX, my = INT_MAX, mz = INT_MAX;

  for (unsigned int z = 0; z < sz; z++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int x = 0; x < sx; x++) {
        const voxel_type v = space[x + sx * (y + sy * z)];
        if (!v) continue;
        const poly_s big = grown(geo.get(x, y, z), amount, rule.centre);

        /* for the grid scale: which faces of the grown cell are open, that
         * is lie towards an empty neighbour: the face neighbour whose cell
         * holds a point just outside the face's middle */
        std::vector<bool> open;
        if (grid) {
          const poly_s me = geo.get(x, y, z);
          std::vector<std::pair<std::array<int, 3>, poly_s>> nbs;
          int nx, ny, nz;
          for (unsigned int n = 0; inf.faceNeighbour(x, y, z, n, nx, ny, nz); n++)
            nbs.push_back({{nx, ny, nz}, geo.get(nx, ny, nz)});
          for (const auto & pl : me.planes) {
            // the middle of the face: the mean of the corners on its plane
            std::array<double, 3> m{0, 0, 0};
            int cnt = 0;
            for (const auto & q : me.corners)
              if (std::fabs(pl[0]*q[0] + pl[1]*q[1] + pl[2]*q[2] - pl[3]) < kGeoEps) {
                for (int k = 0; k < 3; k++) m[k] += q[k];
                cnt++;
              }
            for (int k = 0; k < 3; k++) m[k] = m[k] / std::max(cnt, 1) + 1e-3 * pl[k];
            bool empty = true;
            for (const auto & [pos, poly] : nbs)
              if (outside(poly, m) < 0) {
                empty = isEmpty2(pos[0], pos[1], pos[2]);
                break;
              }
            open.push_back(empty);
          }
        }

        forFineCells(inf, margin, big, [&](int fx, int fy, int fz) {
          const poly_s small = geo.get(fx, fy, fz);
          if (outside(big, small.centre) >= 0) return;
          if (grid) {
            /* the faces of the grown cell the small one touches: on two or
             * more it lies along an edge; on an open one it is part of the
             * shape's surface */
            unsigned int touched = 0;
            bool onOpen = false;
            for (size_t f = 0; f < big.planes.size(); f++) {
              const auto & pl = big.planes[f];
              bool touches = false;
              for (const auto & q : small.corners)
                touches |= std::fabs(pl[0]*q[0] + pl[1]*q[1] + pl[2]*q[2] - pl[3]) < kGeoEps;
              if (touches) {
                touched++;
                onOpen |= f < open.size() && open[f];
              }
            }
            if (touched < 2 && !onOpen) return;
          }
          cells.push_back({fx, fy, fz, v});
          mx = std::min(mx, fx);
          my = std::min(my, fy);
          mz = std::min(mz, fz);
        });
      }

  if (cells.empty()) return;

  int vx, vy, vz;
  inf.alignDown(mx, my, mz, vx, vy, vz);
  hx = vx; hy = vy; hz = vz;  // rebuild moves it with the cells; initHotspot sets it
  rebuild(cells, vx, vy, vz);
  initHotspot();
}

void voxel_lattice_c::moveToOrigin(void) {
  if (boundX2() < boundX1()) return;  // empty
  int vx, vy, vz;
  inf.alignDown((int)boundX1(), (int)boundY1(), (int)boundZ1(), vx, vy, vz);
  translate(-vx, -vy, -vz, 0);
}

void voxel_lattice_c::centreInBox(void) {
  if (boundX2() < boundX1()) return;  // empty

  // where the contents' box would sit centred, in voxel coordinates
  const double want[3] = {
    ((double)sx - 1 - boundX2() - boundX1()) / 2.0,
    ((double)sy - 1 - boundY2() - boundY1()) / 2.0,
    ((double)sz - 1 - boundZ2() - boundZ1()) / 2.0,
  };

  // of the lattice vectors that keep the contents in the box, the one nearest
  // to that, measured in physical space
  int L[9];
  for (int j = 0; j < 3; j++) inf.latticeColumn(j, L[j], L[3+j], L[6+j]);
  const int R = (int)std::max({sx, sy, sz}) + 1;
  bool found = false;
  double best = 0;
  int bx = 0, by = 0, bz = 0;
  for (int n0 = -R; n0 <= R; n0++)
    for (int n1 = -R; n1 <= R; n1++)
      for (int n2 = -R; n2 <= R; n2++) {
        const int v[3] = {L[0]*n0 + L[1]*n1 + L[2]*n2, L[3]*n0 + L[4]*n1 + L[5]*n2, L[6]*n0 + L[7]*n1 + L[8]*n2};
        if ((int)boundX1() + v[0] < 0 || (int)boundX2() + v[0] >= (int)sx ||
            (int)boundY1() + v[1] < 0 || (int)boundY2() + v[1] >= (int)sy ||
            (int)boundZ1() + v[2] < 0 || (int)boundZ2() + v[2] >= (int)sz)
          continue;
        float d[3] = {(float)(v[0] - want[0]), (float)(v[1] - want[1]), (float)(v[2] - want[2])};
        inf.toPhysical(d[0], d[1], d[2]);
        const double dist = (double)d[0]*d[0] + (double)d[1]*d[1] + (double)d[2]*d[2];
        if (!found || dist < best - 1e-9) {
          found = true;
          best = dist;
          bx = v[0]; by = v[1]; bz = v[2];
        }
      }
  if (found && (bx || by || bz))
    translate(bx, by, bz, 0);
}
