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

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "rotationrules.h"

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <set>

namespace {

struct cellLess {
  bool operator()(const rotationRules_c::cell_t & a, const rotationRules_c::cell_t & b) const {
    if (a.x != b.x) return a.x < b.x;
    if (a.y != b.y) return a.y < b.y;
    return a.z < b.z;
  }
};

typedef std::set<rotationRules_c::cell_t, cellLess> cellSet;

/* Mid-path samples along a 90° turn, matching the Fortran NrotSteps default. */
static const int ARC_STEPS = 24;
static const double BEVEL_REMOVE = 0.0405;
static const double HALFTHICK = 0.5 - BEVEL_REMOVE;

static double pivotWorld(int doubled) {
  return doubled * 0.5 + 0.5;
}

/* Voxel (i,j,k) occupies [i,i+1]×[j,j+1]×[k,k+1]. A unit cube centred at C
 * overlaps lattice voxels in each axis from floor(C-0.5) to floor(C+0.5-eps). */
static void addOverlappedCells(double cx, double cy, double cz, cellSet & out) {
  const double eps = 1e-9;
  int x0 = (int)floor(cx - 0.5 + eps);
  int x1 = (int)floor(cx + 0.5 - eps);
  int y0 = (int)floor(cy - 0.5 + eps);
  int y1 = (int)floor(cy + 0.5 - eps);
  int z0 = (int)floor(cz - 0.5 + eps);
  int z1 = (int)floor(cz + 0.5 - eps);

  for (int x = x0; x <= x1; x++)
    for (int y = y0; y <= y1; y++)
      for (int z = z0; z <= z1; z++)
        out.insert(rotationRules_c::cell_t(x, y, z));
}

/* Rotate in-plane coords (u,v) by angle θ toward a ±90° turn.
 * sense 0 = +90°, sense 1 = -90°. */
static void rotateInPlane(double u, double v, double theta, unsigned int sense,
                          double * uOut, double * vOut) {
  double c = cos(theta);
  double s = sin(theta);
  if (sense == 0) {
    *uOut = u * c - v * s;
    *vOut = u * s + v * c;
  } else {
    *uOut = u * c + v * s;
    *vOut = -u * s + v * c;
  }
}

/* Continuous centre of a voxel after rotating by theta about pivot centre. */
static void rotatedCenter(double dx, double dy, double dz,
                          unsigned int axis, unsigned int sense, double theta,
                          double * ox, double * oy, double * oz) {
  if (axis == 0) {
    double ny, nz;
    rotateInPlane(dy, dz, theta, sense, &ny, &nz);
    *ox = dx; *oy = ny; *oz = nz;
  } else if (axis == 1) {
    if (sense == 0) {
      *ox = dx * cos(theta) + dz * sin(theta);
      *oy = dy;
      *oz = -dx * sin(theta) + dz * cos(theta);
    } else {
      *ox = dx * cos(theta) - dz * sin(theta);
      *oy = dy;
      *oz = dx * sin(theta) + dz * cos(theta);
    }
  } else {
    double nx, ny;
    rotateInPlane(dx, dy, theta, sense, &nx, &ny);
    *ox = nx; *oy = ny; *oz = dz;
  }
}

/**
 * 2D SAT: true if the moving beveled square overlaps the static one.
 * Normals are axis-aligned plus the current rotation angle (Fortran rr=1..4).
 */
static bool bevelSquaresOverlap(const double mc[4][2], const double sc[4][2],
                                double theta) {
  const double nrm[4] = { 0.0, 1.5707963267948966, theta, theta + 1.5707963267948966 };
  for (int r = 0; r < 4; r++) {
    double c = cos(nrm[r]);
    double s = sin(nrm[r]);
    double min1 = 1e100, max1 = -1e100, min2 = 1e100, max2 = -1e100;
    for (int i = 0; i < 4; i++) {
      double d1 = mc[i][0] * c + mc[i][1] * s;
      double d2 = sc[i][0] * c + sc[i][1] * s;
      if (d1 < min1) min1 = d1;
      if (d1 > max1) max1 = d1;
      if (d2 < min2) min2 = d2;
      if (d2 > max2) max2 = d2;
    }
    if (max1 < min2 || max2 < min1)
      return false;
  }
  return true;
}

static void inPlaneFromWorld(double x, double y, double z, unsigned int axis,
                             double * u, double * v) {
  if (axis == 0) { *u = y; *v = z; }
  else if (axis == 1) { *u = x; *v = z; }
  else { *u = x; *v = y; }
}

static void bevelCornersWorld(const rotationRules_c::cell_t & cell, unsigned int axis,
                              double ht, double out[4][3]) {
  const double cx = cell.x + 0.5;
  const double cy = cell.y + 0.5;
  const double cz = cell.z + 0.5;
  const double su[4] = { -ht, -ht, ht, ht };
  const double sv[4] = { -ht, ht, -ht, ht };
  for (int i = 0; i < 4; i++) {
    if (axis == 0) {
      out[i][0] = cx; out[i][1] = cy + su[i]; out[i][2] = cz + sv[i];
    } else if (axis == 1) {
      out[i][0] = cx + su[i]; out[i][1] = cy; out[i][2] = cz + sv[i];
    } else {
      out[i][0] = cx + su[i]; out[i][1] = cy + sv[i]; out[i][2] = cz;
    }
  }
}

/**
 * Sample the 90° path with Fortran beveled squares. Optional in-plane wiggle
 * (dshift * sin(2θ) along one of 8 directions) matches SimpleRot.
 */
static bool bevelArcClearOne(const cellSet & occ,
                             const std::vector<rotationRules_c::cell_t> & startCells,
                             const rotationRules_c::pivot_t & pivot,
                             unsigned int axis, unsigned int sense,
                             double dshift, int shifti, int shiftj) {

  const double pcx = pivotWorld(pivot.hx);
  const double pcy = pivotWorld(pivot.hy);
  const double pcz = pivotWorld(pivot.hz);
  const double halfPi = 1.5707963267948966;

  for (int s = 1; s < ARC_STEPS; s++) {
    double theta = halfPi * (double)s / (double)ARC_STEPS;
    double w = dshift * sin(2.0 * theta);
    double wu = w * (double)shifti;
    double wv = w * (double)shiftj;
    double wx = 0, wy = 0, wz = 0;
    if (axis == 0) { wy = wu; wz = wv; }
    else if (axis == 1) { wx = wu; wz = wv; }
    else { wx = wu; wy = wv; }

    for (unsigned int ci = 0; ci < startCells.size(); ci++) {
      const rotationRules_c::cell_t & cell = startCells[ci];
      double corners[4][3];
      bevelCornersWorld(cell, axis, HALFTHICK, corners);
      double mc[4][2];
      for (int k = 0; k < 4; k++) {
        double dx = corners[k][0] - pcx;
        double dy = corners[k][1] - pcy;
        double dz = corners[k][2] - pcz;
        double ox, oy, oz;
        rotatedCenter(dx, dy, dz, axis, sense, theta, &ox, &oy, &oz);
        inPlaneFromWorld(pcx + ox + wx, pcy + oy + wy, pcz + oz + wz, axis,
                         &mc[k][0], &mc[k][1]);
      }

      int ck = (axis == 0) ? cell.x : ((axis == 1) ? cell.y : cell.z);
      for (cellSet::const_iterator it = occ.begin(); it != occ.end(); ++it) {
        int sk = (axis == 0) ? it->x : ((axis == 1) ? it->y : it->z);
        if (sk != ck)
          continue;
        double scorners[4][3];
        bevelCornersWorld(*it, axis, HALFTHICK, scorners);
        double sc[4][2];
        for (int k = 0; k < 4; k++)
          inPlaneFromWorld(scorners[k][0], scorners[k][1], scorners[k][2], axis,
                           &sc[k][0], &sc[k][1]);
        if (bevelSquaresOverlap(mc, sc, (sense == 0) ? theta : -theta))
          return false;
      }
    }
  }

  return true;
}

static bool arcClear(const cellSet & occ,
                     const std::vector<rotationRules_c::cell_t> & startCells,
                     const rotationRules_c::pivot_t & pivot,
                     unsigned int axis,
                     unsigned int sense) {

  if (bevelArcClearOne(occ, startCells, pivot, axis, sense, 0.0, 0, 0))
    return true;

  const double amounts[3] = {
    sqrt(2.0 * HALFTHICK * HALFTHICK) - 0.5,
    (sqrt(2.0) - 1.0) * 0.5 + 0.025,
    0.378680
  };
  static const int dirs[8][2] = {
    { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
    { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 }
  };

  for (int a = 0; a < 3; a++)
    for (int d = 0; d < 8; d++)
      if (bevelArcClearOne(occ, startCells, pivot, axis, sense,
                           amounts[a], dirs[d][0], dirs[d][1]))
        return true;

  return false;
}

static bool occupiedAt(const cellSet & occ, int x, int y, int z) {
  return occ.find(rotationRules_c::cell_t(x, y, z)) != occ.end();
}

static int axialCoord(const rotationRules_c::cell_t & c, unsigned int axis) {
  if (axis == 0) return c.x;
  if (axis == 1) return c.y;
  return c.z;
}

/* In-plane U,V for a rotation axis: X→(Y,Z), Y→(X,Z), Z→(X,Y). */
static void inPlaneUV(unsigned int axis,
                      int * ux, int * uy, int * uz,
                      int * vx, int * vy, int * vz) {
  if (axis == 0) {
    *ux = 0; *uy = 1; *uz = 0;
    *vx = 0; *vy = 0; *vz = 1;
  } else if (axis == 1) {
    *ux = 1; *uy = 0; *uz = 0;
    *vx = 0; *vy = 0; *vz = 1;
  } else {
    *ux = 1; *uy = 0; *uz = 0;
    *vx = 0; *vy = 1; *vz = 0;
  }
}

static int inPlaneU(const rotationRules_c::cell_t & c, unsigned int axis) {
  if (axis == 0) return c.y;
  return c.x;
}

static int inPlaneV(const rotationRules_c::cell_t & c, unsigned int axis) {
  if (axis == 2) return c.y;
  return c.z;
}

/* Moving voxel lies on the pivot column (same in-plane coords as pivot). */
static bool onPivotColumn(const rotationRules_c::cell_t & c,
                          const rotationRules_c::pivot_t & pivot,
                          unsigned int axis) {
  if (axis == 0)
    return c.y * 2 == pivot.hy && c.z * 2 == pivot.hz;
  if (axis == 1)
    return c.x * 2 == pivot.hx && c.z * 2 == pivot.hz;
  return c.x * 2 == pivot.hx && c.y * 2 == pivot.hy;
}

/* True if any moving voxel on layer `a` is face-adjacent to an other-piece voxel. */
static bool layerTouchesOther(const cellSet & occ,
                              const std::vector<rotationRules_c::cell_t> & startCells,
                              unsigned int axis,
                              int a) {
  static const int d[6][3] = {
    { -1, 0, 0 }, { 1, 0, 0 },
    { 0, -1, 0 }, { 0, 1, 0 },
    { 0, 0, -1 }, { 0, 0, 1 }
  };

  for (unsigned int i = 0; i < startCells.size(); i++) {
    const rotationRules_c::cell_t & c = startCells[i];
    if (axialCoord(c, axis) != a)
      continue;
    for (int k = 0; k < 6; k++) {
      if (occupiedAt(occ, c.x + d[k][0], c.y + d[k][1], c.z + d[k][2]))
        return true;
    }
  }
  return false;
}

/* Record ±U/±V face neighbours of one moving cell for axis-cross (in-plane only). */
static void axisCrossScanCell(const rotationRules_c::cell_t & c,
                              unsigned int axis,
                              const cellSet & occ,
                              const cellSet & start,
                              bool * negU, bool * posU, bool * negV, bool * posV,
                              cellSet * slotOccupied,
                              cellSet * restricted) {

  rotationRules_c::cell_t nu, pu, nv, pv;

  if (axis == 0) {
    nu = rotationRules_c::cell_t(c.x, c.y - 1, c.z);
    pu = rotationRules_c::cell_t(c.x, c.y + 1, c.z);
    nv = rotationRules_c::cell_t(c.x, c.y, c.z - 1);
    pv = rotationRules_c::cell_t(c.x, c.y, c.z + 1);
  } else if (axis == 1) {
    nu = rotationRules_c::cell_t(c.x - 1, c.y, c.z);
    pu = rotationRules_c::cell_t(c.x + 1, c.y, c.z);
    nv = rotationRules_c::cell_t(c.x, c.y, c.z - 1);
    pv = rotationRules_c::cell_t(c.x, c.y, c.z + 1);
  } else {
    nu = rotationRules_c::cell_t(c.x - 1, c.y, c.z);
    pu = rotationRules_c::cell_t(c.x + 1, c.y, c.z);
    nv = rotationRules_c::cell_t(c.x, c.y - 1, c.z);
    pv = rotationRules_c::cell_t(c.x, c.y + 1, c.z);
  }

  const rotationRules_c::cell_t slots[4] = { nu, pu, nv, pv };
  bool * flags[4] = { negU, posU, negV, posV };

  for (int i = 0; i < 4; i++) {
    if (start.find(slots[i]) != start.end())
      continue;
    if (occ.find(slots[i]) != occ.end()) {
      *flags[i] = true;
      if (slotOccupied)
        slotOccupied->insert(slots[i]);
    } else if (restricted) {
      restricted->insert(slots[i]);
    }
  }
}

/**
 * A moving voxel must not have static face-neighbours on both opposite
 * in-plane sides on the same axial layer (Fortran SimpleRot in-plane test).
 * Own-piece voxels are not in `occ`, so they do not count as a pinch.
 */
static bool inPlanePinchClear(const cellSet & occ,
                              const std::vector<rotationRules_c::cell_t> & cells,
                              unsigned int axis,
                              cellSet * blocking) {

  int ux, uy, uz, vx, vy, vz;
  inPlaneUV(axis, &ux, &uy, &uz, &vx, &vy, &vz);

  bool pinched = false;

  for (unsigned int i = 0; i < cells.size(); i++) {
    const rotationRules_c::cell_t & c = cells[i];
    rotationRules_c::cell_t nu(c.x - ux, c.y - uy, c.z - uz);
    rotationRules_c::cell_t pu(c.x + ux, c.y + uy, c.z + uz);
    rotationRules_c::cell_t nv(c.x - vx, c.y - vy, c.z - vz);
    rotationRules_c::cell_t pv(c.x + vx, c.y + vy, c.z + vz);

    bool negU = occ.find(nu) != occ.end();
    bool posU = occ.find(pu) != occ.end();
    bool negV = occ.find(nv) != occ.end();
    bool posV = occ.find(pv) != occ.end();

    if (negU && posU) {
      pinched = true;
      if (blocking) {
        blocking->insert(nu);
        blocking->insert(pu);
      } else {
        return false;
      }
    }
    if (negV && posV) {
      pinched = true;
      if (blocking) {
        blocking->insert(nv);
        blocking->insert(pv);
      } else {
        return false;
      }
    }
  }

  return !pinched;
}

/**
 * Perpendicular-plane capture (Fortran PerPplane 1 and 2).
 * On a slice of constant U (resp. V) that contains at least two moving and
 * two static voxels, opposite static walls in V (resp. U) at the same axial
 * coordinate reject the axis.
 */
static bool perpPlaneClear(const cellSet & occ,
                           const std::vector<rotationRules_c::cell_t> & startCells,
                           unsigned int axis,
                           cellSet * blocking) {

  if (startCells.size() < 2 || occ.size() < 2)
    return true;

  int uMin = inPlaneU(startCells[0], axis);
  int uMax = uMin;
  int vMin = inPlaneV(startCells[0], axis);
  int vMax = vMin;
  for (unsigned int i = 1; i < startCells.size(); i++) {
    int u = inPlaneU(startCells[i], axis);
    int v = inPlaneV(startCells[i], axis);
    if (u < uMin) uMin = u;
    if (u > uMax) uMax = u;
    if (v < vMin) vMin = v;
    if (v > vMax) vMax = v;
  }

  bool captured = false;

  /* PerPplane 1: slices of constant U, opposite walls in V */
  for (int u = uMin; u <= uMax; u++) {
    unsigned int nMove = 0, nStat = 0;
    for (unsigned int i = 0; i < startCells.size(); i++)
      if (inPlaneU(startCells[i], axis) == u)
        nMove++;
    for (cellSet::const_iterator it = occ.begin(); it != occ.end(); ++it)
      if (inPlaneU(*it, axis) == u)
        nStat++;
    if (nMove < 2 || nStat < 2)
      continue;

    bool neg = false, pos = false;
    cellSet hits;
    for (unsigned int i = 0; i < startCells.size(); i++) {
      const rotationRules_c::cell_t & c = startCells[i];
      if (inPlaneU(c, axis) != u)
        continue;
      int ck = axialCoord(c, axis);
      int cv = inPlaneV(c, axis);
      for (cellSet::const_iterator it = occ.begin(); it != occ.end(); ++it) {
        if (inPlaneU(*it, axis) != u)
          continue;
        if (axialCoord(*it, axis) != ck)
          continue;
        int sv = inPlaneV(*it, axis);
        if (sv == cv - 1) {
          neg = true;
          if (blocking) hits.insert(*it);
        } else if (sv == cv + 1) {
          pos = true;
          if (blocking) hits.insert(*it);
        }
      }
    }
    if (neg && pos) {
      captured = true;
      if (blocking) {
        for (cellSet::const_iterator it = hits.begin(); it != hits.end(); ++it)
          blocking->insert(*it);
      } else {
        return false;
      }
    }
  }

  /* PerPplane 2: slices of constant V, opposite walls in U */
  for (int v = vMin; v <= vMax; v++) {
    unsigned int nMove = 0, nStat = 0;
    for (unsigned int i = 0; i < startCells.size(); i++)
      if (inPlaneV(startCells[i], axis) == v)
        nMove++;
    for (cellSet::const_iterator it = occ.begin(); it != occ.end(); ++it)
      if (inPlaneV(*it, axis) == v)
        nStat++;
    if (nMove < 2 || nStat < 2)
      continue;

    bool neg = false, pos = false;
    cellSet hits;
    for (unsigned int i = 0; i < startCells.size(); i++) {
      const rotationRules_c::cell_t & c = startCells[i];
      if (inPlaneV(c, axis) != v)
        continue;
      int ck = axialCoord(c, axis);
      int cu = inPlaneU(c, axis);
      for (cellSet::const_iterator it = occ.begin(); it != occ.end(); ++it) {
        if (inPlaneV(*it, axis) != v)
          continue;
        if (axialCoord(*it, axis) != ck)
          continue;
        int su = inPlaneU(*it, axis);
        if (su == cu - 1) {
          neg = true;
          if (blocking) hits.insert(*it);
        } else if (su == cu + 1) {
          pos = true;
          if (blocking) hits.insert(*it);
        }
      }
    }
    if (neg && pos) {
      captured = true;
      if (blocking) {
        for (cellSet::const_iterator it = hits.begin(); it != hits.end(); ++it)
          blocking->insert(*it);
      } else {
        return false;
      }
    }
  }

  return !captured;
}

/**
 * Static ±U/±V neighbours face-adjacent to pivot-column moving voxels,
 * aggregated across layers along the rotation axis that face-touch the
 * other piece. Same-layer opposites are handled by inPlanePinchClear;
 * opposition across two different touching layers is rejected here.
 */
static bool axisCrossClear(const cellSet & occ,
                           const std::vector<rotationRules_c::cell_t> & startCells,
                           const rotationRules_c::pivot_t & pivot,
                           unsigned int axis) {

  if (startCells.empty())
    return true;

  cellSet start;
  for (unsigned int i = 0; i < startCells.size(); i++)
    start.insert(startCells[i]);

  int aMin = axialCoord(startCells[0], axis);
  int aMax = aMin;
  for (unsigned int i = 1; i < startCells.size(); i++) {
    int a = axialCoord(startCells[i], axis);
    if (a < aMin) aMin = a;
    if (a > aMax) aMax = a;
  }

  bool seenNegU = false, seenPosU = false;
  bool seenNegV = false, seenPosV = false;

  for (int a = aMin; a <= aMax; a++) {
    if (!layerTouchesOther(occ, startCells, axis, a))
      continue;

    bool layerNegU = false, layerPosU = false;
    bool layerNegV = false, layerPosV = false;

    for (unsigned int i = 0; i < startCells.size(); i++) {
      const rotationRules_c::cell_t & c = startCells[i];
      if (axialCoord(c, axis) != a)
        continue;
      if (!onPivotColumn(c, pivot, axis))
        continue;
      axisCrossScanCell(c, axis, occ, start,
                        &layerNegU, &layerPosU, &layerNegV, &layerPosV, 0, 0);
    }

    if ((layerPosU && seenNegU) || (layerNegU && seenPosU) ||
        (layerPosV && seenNegV) || (layerNegV && seenPosV)) {
      if (getenv("BT_ROT_DEBUG")) {
        const char * uName = (axis == 0) ? "Y" : "X";
        const char * vName = (axis == 0) ? "Z" : ((axis == 1) ? "Z" : "Y");
        fprintf(stderr,
                "ROT_AXIS_CROSS detail pivot=(%.1f,%.1f,%.1f) axis=%u "
                "touching-layer=%d cross-layer sides: -%s=%d +%s=%d -%s=%d +%s=%d "
                "(layer -%s=%d +%s=%d -%s=%d +%s=%d)\n",
                pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, a,
                uName, (int)seenNegU, uName, (int)seenPosU,
                vName, (int)seenNegV, vName, (int)seenPosV,
                uName, (int)layerNegU, uName, (int)layerPosU,
                vName, (int)layerNegV, vName, (int)layerPosV);
      }
      return false;
    }

    seenNegU |= layerNegU;
    seenPosU |= layerPosU;
    seenNegV |= layerNegV;
    seenPosV |= layerPosV;
  }

  return true;
}

}

bool rotationRules_c::allowRotation(const std::vector<cell_t> & occupied,
                                    const std::vector<cell_t> & startCells,
                                    const std::vector<cell_t> & endCells,
                                    const pivot_t & pivot,
                                    unsigned int axis,
                                    unsigned int sense) const {

  cellSet occ;
  for (unsigned int i = 0; i < occupied.size(); i++)
    occ.insert(occupied[i]);

  {
    const char * spec = getenv("BT_ROT_DUMP");
    int px, py, pz, a, s;
    if (spec && sscanf(spec, "%d,%d,%d,%u,%u", &px, &py, &pz, &a, &s) == 5 &&
        pivot.hx == px * 2 && pivot.hy == py * 2 && pivot.hz == pz * 2 &&
        axis == (unsigned)a && sense == (unsigned)s) {
      static bool dumped = false;
      if (!dumped) {
        dumped = true;
        fprintf(stderr, "ROT_DUMP moving cells (%zu):\n", startCells.size());
        for (unsigned int i = 0; i < startCells.size(); i++)
          fprintf(stderr, "  M %d %d %d\n", startCells[i].x, startCells[i].y, startCells[i].z);
        fprintf(stderr, "ROT_DUMP static/other cells (%zu):\n", occupied.size());
        for (unsigned int i = 0; i < occupied.size(); i++)
          fprintf(stderr, "  S %d %d %d\n", occupied[i].x, occupied[i].y, occupied[i].z);
      }
    }
  }

  /* End-position: final voxels must not overlap other pieces */
  for (unsigned int i = 0; i < endCells.size(); i++)
    if (occ.find(endCells[i]) != occ.end()) {
      if (getenv("BT_ROT_DEBUG"))
        fprintf(stderr,
                "ROT_REJECT end-overlap pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u end=(%d,%d,%d)\n",
                pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense,
                endCells[i].x, endCells[i].y, endCells[i].z);
      return false;
    }

  /* Same-layer sandwich at start (pinched voxel cannot turn in-plane) */
  if (!inPlanePinchClear(occ, startCells, axis, 0)) {
    if (getenv("BT_ROT_DEBUG"))
      fprintf(stderr,
              "ROT_REJECT sandwich-start pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u "
              "(moving voxel has static on both opposite in-plane sides)\n",
              pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense);
    return false;
  }

  /* Same-layer sandwich at the 90° pose */
  if (!inPlanePinchClear(occ, endCells, axis, 0)) {
    if (getenv("BT_ROT_DEBUG"))
      fprintf(stderr,
              "ROT_REJECT sandwich-end pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u "
              "(rotated voxel would have static on both opposite in-plane sides)\n",
              pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense);
    return false;
  }

  /* Opposite static walls in planes perpendicular to the rotation plane */
  if (!perpPlaneClear(occ, startCells, axis, 0)) {
    if (getenv("BT_ROT_DEBUG"))
      fprintf(stderr,
              "ROT_REJECT perp-plane pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u "
              "(opposite static walls on a U or V slice with 2+ moving and 2+ static voxels)\n",
              pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense);
    return false;
  }

  /* Opposite static neighbours forbidden across layers that touch the other piece */
  if (!axisCrossClear(occ, startCells, pivot, axis)) {
    if (getenv("BT_ROT_DEBUG"))
      fprintf(stderr,
              "ROT_REJECT axis-cross pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u "
              "(opposite ±in-plane static neighbours face-adjacent to moving piece on layers touching other piece)\n",
              pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense);
    return false;
  }

  /* Continuous path must not clip other pieces mid-turn */
  if (!arcClear(occ, startCells, pivot, axis, sense)) {
    if (getenv("BT_ROT_DEBUG"))
      fprintf(stderr,
              "ROT_REJECT arc-sweep pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u "
              "(mid-path beveled square overlaps other piece)\n",
              pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense);
    return false;
  }

  if (getenv("BT_ROT_DEBUG"))
    fprintf(stderr,
            "ROT_ALLOW pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u\n",
            pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense);

  return true;
}

bool rotationRules_c::axisBlocked(const std::vector<cell_t> & occupied,
                                  const std::vector<cell_t> & startCells,
                                  unsigned int axis) const {

  cellSet occ;
  for (unsigned int i = 0; i < occupied.size(); i++)
    occ.insert(occupied[i]);

  if (!inPlanePinchClear(occ, startCells, axis, 0))
    return true;
  if (!perpPlaneClear(occ, startCells, axis, 0))
    return true;
  return false;
}

void rotationRules_c::collectDebugConflictCells(
    const std::vector<cell_t> & occupied,
    const std::vector<cell_t> & startCells,
    const pivot_t & pivot,
    unsigned int axis,
    unsigned int sense,
    std::vector<cell_t> & outBlocking,
    std::vector<cell_t> & outClearance,
    std::vector<cell_t> & outRestricted) const {

  outBlocking.clear();
  outClearance.clear();
  outRestricted.clear();

  cellSet occ;
  for (unsigned int i = 0; i < occupied.size(); i++)
    occ.insert(occupied[i]);

  cellSet start;
  for (unsigned int i = 0; i < startCells.size(); i++)
    start.insert(startCells[i]);

  cellSet blocking;
  cellSet clearance;
  cellSet restricted;

  /* Arc-sweep samples: every overlapped lattice cell that is not a start
   * voxel is a clearance cell; those also in occ are hard blockers. */
  {
    const double pcx = pivotWorld(pivot.hx);
    const double pcy = pivotWorld(pivot.hy);
    const double pcz = pivotWorld(pivot.hz);
    const double halfPi = 1.5707963267948966;

    for (unsigned int ci = 0; ci < startCells.size(); ci++) {
      const cell_t & cell = startCells[ci];
      double dx = (cell.x + 0.5) - pcx;
      double dy = (cell.y + 0.5) - pcy;
      double dz = (cell.z + 0.5) - pcz;

      if (axis == 0 && dy == 0 && dz == 0) continue;
      if (axis == 1 && dx == 0 && dz == 0) continue;
      if (axis == 2 && dx == 0 && dy == 0) continue;

      for (int s = 1; s < ARC_STEPS; s++) {
        double theta = halfPi * (double)s / (double)ARC_STEPS;
        double ox, oy, oz;
        rotatedCenter(dx, dy, dz, axis, sense, theta, &ox, &oy, &oz);

        cellSet hit;
        addOverlappedCells(pcx + ox, pcy + oy, pcz + oz, hit);
        for (cellSet::const_iterator it = hit.begin(); it != hit.end(); ++it) {
          if (start.find(*it) != start.end())
            continue;
          clearance.insert(*it);
          if (occ.find(*it) != occ.end())
            blocking.insert(*it);
        }
      }
    }
  }

  /* Same-layer sandwich and perpendicular-plane capture: static cells that
   * currently pinch or wall-in the moving piece. */
  inPlanePinchClear(occ, startCells, axis, &blocking);
  perpPlaneClear(occ, startCells, axis, &blocking);

  /* Axis-cross: ±in-plane slots face-adjacent to pivot-column moving voxels on
   * touching layers. Cross-layer opposition marks hard blockers. */
  if (!startCells.empty()) {
    int aMin = axialCoord(startCells[0], axis);
    int aMax = aMin;
    for (unsigned int i = 1; i < startCells.size(); i++) {
      int a = axialCoord(startCells[i], axis);
      if (a < aMin) aMin = a;
      if (a > aMax) aMax = a;
    }

    bool seenNegU = false, seenPosU = false;
    bool seenNegV = false, seenPosV = false;
    cellSet slotOccupied;

    for (int a = aMin; a <= aMax; a++) {
      if (!layerTouchesOther(occ, startCells, axis, a))
        continue;

      bool layerNegU = false, layerPosU = false;
      bool layerNegV = false, layerPosV = false;

      for (unsigned int i = 0; i < startCells.size(); i++) {
        const cell_t & c = startCells[i];
        if (axialCoord(c, axis) != a)
          continue;
        if (!onPivotColumn(c, pivot, axis))
          continue;
        axisCrossScanCell(c, axis, occ, start,
                          &layerNegU, &layerPosU, &layerNegV, &layerPosV,
                          &slotOccupied, &restricted);
      }

      bool cross = (layerPosU && seenNegU) || (layerNegU && seenPosU) ||
                   (layerPosV && seenNegV) || (layerNegV && seenPosV);
      if (cross) {
        for (cellSet::const_iterator it = slotOccupied.begin(); it != slotOccupied.end(); ++it)
          blocking.insert(*it);
      }

      seenNegU |= layerNegU;
      seenPosU |= layerPosU;
      seenNegV |= layerNegV;
      seenPosV |= layerPosV;
    }

    for (cellSet::const_iterator it = slotOccupied.begin(); it != slotOccupied.end(); ++it)
      clearance.insert(*it);
  }

  for (cellSet::const_iterator it = blocking.begin(); it != blocking.end(); ++it)
    outBlocking.push_back(*it);
  for (cellSet::const_iterator it = clearance.begin(); it != clearance.end(); ++it)
    if (blocking.find(*it) == blocking.end())
      outClearance.push_back(*it);
  for (cellSet::const_iterator it = restricted.begin(); it != restricted.end(); ++it)
    if (blocking.find(*it) == blocking.end() && clearance.find(*it) == clearance.end())
      outRestricted.push_back(*it);
}
