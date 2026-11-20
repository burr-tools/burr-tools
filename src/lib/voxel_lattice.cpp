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
#include <climits>

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
