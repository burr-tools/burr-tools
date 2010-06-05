/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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
#include "voxel_1.h"

#include <math.h>

void voxel_1_c::minimizePiece(void) {

  bool move_again = (bx1 + by1) & 1;

  voxel_c::minimizePiece();

  if (move_again) {
    resize(sx+1, sy, sz, 0);
    translate(1, 0, 0, 0);
  }
}

#include "tabs_1/tablesizes.inc"

static double rotationMatrices[NUM_TRANSFORMATIONS_MIRROR][9] = {
#include "tabs_1/rotmatrix.inc"
};

#include "tabs_1/meshverts.inc"

bool voxel_1_c::transform(unsigned int nr) {

  if (nr == 0) return true;

  bt_assert(nr < NUM_TRANSFORMATIONS_MIRROR);

  int minx = 100000;
  int miny = 100000;
  int minz = 100000;
  int maxx = -100000;
  int maxy = -100000;
  int maxz = -100000;

  unsigned int index = 0;
  for (unsigned int z = 0; z < sz; z++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int x = 0; x < sx; x++) {
        if (!isEmpty(index)) {

          double xp = 0.5 + 0.5 * x;
          double yp = y * sqrt(0.75);
          double zp = z;

          if ((x + y) & 1) {
            yp += sqrt(1.0/3);
          } else {
            yp += sqrt(1.0/12);
          }

          // rotate using the rotation matrices from the top

          double xpn = rotationMatrices[nr][0]*xp + rotationMatrices[nr][1]*yp + rotationMatrices[nr][2]*zp;
          double ypn = rotationMatrices[nr][3]*xp + rotationMatrices[nr][4]*yp + rotationMatrices[nr][5]*zp;
          double zpn = rotationMatrices[nr][6]*xp + rotationMatrices[nr][7]*yp + rotationMatrices[nr][8]*zp;

          xpn = (xpn - 0.5)*2;
          ypn /= sqrt(0.75);

          int xn = (int)(xpn+(xpn<0?-0.5:0.5));
          int yn = (int)floor(ypn);
          int zn = (int)(zpn+(zpn<0?-0.5:0.5));

          if (xn > maxx) maxx = xn;
          if (yn > maxy) maxy = yn;
          if (zn > maxz) maxz = zn;

          if (xn < minx) minx = xn;
          if (yn < miny) miny = yn;
          if (zn < minz) minz = zn;
        }
        index++;
      }

  // empty space, do nothing
  if (minx == 100000)
    return true;

  if ((minx+miny) & 1)
    minx--;

  unsigned int nsx = maxx-minx+1;
  unsigned int nsy = maxy-miny+1;
  unsigned int nsz = maxz-minz+1;

  // don't make the new space smaller than the old one
  // if the old one was larger center the object inside it
  if (nsz < sz) {
    minz -= (sz-nsz)/2;
    nsz = sz;
  }

  if ((nsx < sx) || (nsy < sy)) {
    // we must make sure that we shift so that we don't change the
    // state of the voxels, do (dx+dy)&1 must be 0

    int dx = (nsx < sx) ? (sx-nsx)/2 : 0;
    int dy = (nsy < sy) ? (sy-nsy)/2 : 0;

    if ((dx+dy) & 1) {

      if (dx > 0)
        dx--;
      else
        dy--;
    }

    minx -= dx;
    miny -= dy;
    nsx = (sx > nsx) ? sx : nsx;
    nsy = (sy > nsy) ? sy : nsy;
  }

  int voxelsn = nsx*nsy*nsz;

  voxel_type *s = new voxel_type[voxelsn];
  memset(s, VX_EMPTY, voxelsn);

  index = 0;
  for (unsigned int z = 0; z < sz; z++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int x = 0; x < sx; x++) {
        if (!isEmpty(index)) {

          double xp = 0.5 + 0.5 * x;
          double yp = y * sqrt(0.75);
          double zp = z;

          if ((x + y) & 1) {
            yp += sqrt(1.0/3);
          } else {
            yp += sqrt(1.0/12);
          }

          // rotate using the rotation matrices from the top

          double xpn = rotationMatrices[nr][0]*xp + rotationMatrices[nr][1]*yp + rotationMatrices[nr][2]*zp;
          double ypn = rotationMatrices[nr][3]*xp + rotationMatrices[nr][4]*yp + rotationMatrices[nr][5]*zp;
          double zpn = rotationMatrices[nr][6]*xp + rotationMatrices[nr][7]*yp + rotationMatrices[nr][8]*zp;

          xpn = (xpn - 0.5)*2;
          ypn /= sqrt(0.75);

          int xn = (int)(xpn+(xpn<0?-0.5:0.5));
          int yn = (int)floor(ypn);
          int zn = (int)(zpn+(zpn<0?-0.5:0.5));

          s[(xn-minx) + nsx*((yn-miny) + nsy*(zn-minz))] = space[index];
        }
        index++;
      }

  // calculate the new hotspot position
  double xp = 0.5 +  hx * 0.5;
  double yp = hy * sqrt(0.75);
  double zp = hz;

  if ((hx + hy) & 1) {
    yp += sqrt(1.0/3);
  } else {
    yp += sqrt(1.0/12);
  }

  double xpn = rotationMatrices[nr][0]*xp + rotationMatrices[nr][1]*yp + rotationMatrices[nr][2]*zp;
  double ypn = rotationMatrices[nr][3]*xp + rotationMatrices[nr][4]*yp + rotationMatrices[nr][5]*zp;
  double zpn = rotationMatrices[nr][6]*xp + rotationMatrices[nr][7]*yp + rotationMatrices[nr][8]*zp;

  xpn = (xpn - 0.5)*2;
  ypn /= sqrt(0.75);

  hx = (int)(xpn+(xpn<0?-0.5:0.5)) - minx;
  hy = (int)floor(ypn) - miny;
  hz = (int)(zpn+(zpn<0?-0.5:0.5)) - minz;

  // take over new space and new size
  sx = nsx;
  sy = nsy;
  sz = nsz;

  delete [] space;
  space = s;

  voxels = voxelsn;

  recalcBoundingBox();

  symmetries = symmetryInvalid();

  return true;
}




bool voxel_1_c::identicalInBB(const voxel_c * op, bool includeColors) const {
  return (((bx1+by1) & 1) == ((op->boundX1() + op->boundY1()) & 1)) && voxel_c::identicalInBB(op, includeColors);
}

void voxel_1_c::transformPoint(int * x, int * y, int * z, unsigned int trans) const {

  bt_assert(trans < NUM_TRANSFORMATIONS_MIRROR);

  double xs = 0.5 + 0.5* *x;
  double ys = *y * sqrt(0.75);
  double zs = *z;

  if ((*x + *y) & 1) {
    ys += sqrt(1.0/3);
  } else {
    ys += sqrt(1.0/12);
  }

  double xpn = rotationMatrices[trans][0]*xs + rotationMatrices[trans][1]*ys + rotationMatrices[trans][2]*zs;
  double ypn = rotationMatrices[trans][3]*xs + rotationMatrices[trans][4]*ys + rotationMatrices[trans][5]*zs;
  double zpn = rotationMatrices[trans][6]*xs + rotationMatrices[trans][7]*ys + rotationMatrices[trans][8]*zs;

  xpn = (xpn - 0.5)*2;
  ypn /= sqrt(0.75);

  *z = (int)(zpn+(zpn<0?-0.5:0.5));
  *y = (int)floor(ypn);
  *x = (int)(xpn+(xpn<0?-0.5:0.5));
}

bool voxel_1_c::getNeighbor(unsigned int idx, unsigned int typ, int x, int y, int z, int * xn, int *yn, int *zn) const {

  int t = ((x+y) & 1) ? -1 : 1;  // -1 for top down, 1 for base triangle

  switch (typ) {
    case 0:
      switch (idx) {
        case 0: *xn = x-1; *yn = y;   *zn = z;   break;
        case 1: *xn = x+1; *yn = y;   *zn = z;   break;
        case 2: *xn = x;   *yn = y;   *zn = z-1; break;
        case 3: *xn = x;   *yn = y;   *zn = z+1; break;
        case 4: *xn = x;   *yn = y-t; *zn = z;   break;
        default: return false;
      }
      break;
    case 1:
      switch (idx) {
        case  0: *xn = x-2; *yn = y  ; *zn = z;   break;
        case  1: *xn = x+2; *yn = y  ; *zn = z;   break;
        case  2: *xn = x-1; *yn = y+1; *zn = z;   break;
        case  3: *xn = x-1; *yn = y-1; *zn = z;   break;
        case  4: *xn = x+1; *yn = y+1; *zn = z;   break;
        case  5: *xn = x+1; *yn = y-1; *zn = z;   break;
        case  6: *xn = x-2; *yn = y-t; *zn = z;   break;
        case  7: *xn = x+2; *yn = y-t; *zn = z;   break;
        case  8: *xn = x;   *yn = y+t; *zn = z;   break;

        case  9: *xn = x-1; *yn = y;   *zn = z+1; break;
        case 10: *xn = x-1; *yn = y;   *zn = z-1; break;
        case 11: *xn = x+1; *yn = y;   *zn = z+1; break;
        case 12: *xn = x+1; *yn = y;   *zn = z-1; break;

        case 13: *xn = x;   *yn = y-t; *zn = z-1; break;
        case 14: *xn = x;   *yn = y-t; *zn = z+1; break;
        default: return false;
      }
      break;
    case 2:
      switch (idx) {
        case  0: *xn = x-2; *yn = y  ; *zn = z-1; break;
        case  1: *xn = x+2; *yn = y  ; *zn = z-1; break;
        case  2: *xn = x-1; *yn = y+1; *zn = z-1; break;
        case  3: *xn = x-1; *yn = y-1; *zn = z-1; break;
        case  4: *xn = x+1; *yn = y+1; *zn = z-1; break;
        case  5: *xn = x+1; *yn = y-1; *zn = z-1; break;
        case  6: *xn = x-2; *yn = y-t; *zn = z-1; break;
        case  7: *xn = x+2; *yn = y-t; *zn = z-1; break;
        case  8: *xn = x;   *yn = y+t; *zn = z-1; break;

        case  9: *xn = x-2; *yn = y  ; *zn = z+1; break;
        case 10: *xn = x+2; *yn = y  ; *zn = z+1; break;
        case 11: *xn = x-1; *yn = y+1; *zn = z+1; break;
        case 12: *xn = x-1; *yn = y-1; *zn = z+1; break;
        case 13: *xn = x+1; *yn = y+1; *zn = z+1; break;
        case 14: *xn = x+1; *yn = y-1; *zn = z+1; break;
        case 15: *xn = x-2; *yn = y-t; *zn = z+1; break;
        case 16: *xn = x+2; *yn = y-t; *zn = z+1; break;
        case 17: *xn = x;   *yn = y+t; *zn = z+1; break;
        default: return false;
      }
      break;
  }
  return true;
}

void voxel_1_c::scale(unsigned int amount, bool grid)
{
  unsigned int nsx = amount-1 + sx*amount;
  unsigned int nsy = sy*amount;
  unsigned int nsz = sz*amount;
  voxel_type * s2 = new voxel_type[nsx*nsy*nsz];
  memset(s2, VX_EMPTY, nsx*nsy*nsz);

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)

        for (unsigned int ax = 0; ax < 2*amount-1; ax++)
          for (unsigned int ay = 0; ay < amount; ay++)
            for (unsigned int az = 0; az < amount; az++)
            {
              voxel_type value = 0;

              if (!grid)
              {
                 value = get(x, y, z);
              }
              else
              {
                if (((az == 0 || az == amount-1) && (ay == 0 || ay == ax || ay+1 == ax || ay == 2*amount-2-ax || ay == 2*amount-3-ax))
                    || (ax == 0 && ay == 0) || (ax == amount-1 && ay == amount-1) || (ax == 2*amount-2 && ay == 0)
                    || (az == 0        && isEmpty2(x, y, z-1))
                    || (az == amount-1 && isEmpty2(x, y, z+1))
                    || (ay == 0        && (((x+y) & 1) == 0 && isEmpty2(x, y-1, z) || ((x+y) & 1) == 1 && isEmpty2(x, y+1, z)))
                    || ((ay == ax || ay+1 == ax) && isEmpty2(x-1, y, z))
                    || ((ay == 2*amount-2-ax || ay == 2*amount-3-ax) && isEmpty2(x+1, y, z))
                   )
                  value = get(x, y, z);
                else
                  value = 0;
              }
              if ((ay <= ax) && (ay < 2*amount-1-ax))
              {
                if (((x+y) & 1) == 0)
                {
                  s2[(x*amount+ax) + nsx * ((y*amount+ay) + nsy * (z*amount+az))] = value;
                }
                else
                {
                  s2[(x*amount+ax) + nsx * ((y*amount+amount-1-ay) + nsy * (z*amount+az))] = value;
                }
              }
            }

  delete [] space;
  space = s2;

  sx = nsx;
  sy = nsy;
  sz = nsz;

  hx = hy = hz = 0;

  voxels = sx*sy*sz;

  recalcBoundingBox();
}

void voxel_1_c::resizeInclude(int & px, int & py, int & pz) {

  int nsx = getX();
  int nsy = getY();
  int nsz = getZ();
  int tx = 0;
  int ty = 0;
  int tz = 0;

  if (px < 0) {

    nsx -= px;
    tx -= px;
    if (px & 1) {
      nsx ++;
      tx ++;
    }
  }
  if (py < 0) {

    nsy -= py;
    ty -= py;
    if (py & 1) {
      nsy ++;
      ty ++;
    }
  }
  if (pz < 0) {

    nsz -= pz;
    tz -= pz;
  }
  if (px >= (int)getX()) nsx += (px-getX()+1);
  if (py >= (int)getY()) nsy += (py-getY()+1);
  if (pz >= (int)getZ()) nsz += (pz-getZ()+1);

  resize(nsx, nsy, nsz, 0);
  translate(tx, ty, tz, 0);

  px += tx;
  py += ty;
  pz += tz;
}

bool voxel_1_c::validCoordinate(int /*x*/, int /*y*/, int /*z*/) const {

  return true;

}

bool voxel_1_c::onGrid(int x, int y, int /*z*/) const {

  // the shape doesn't fit, when the lower left corner doesn't have the right parity
  if ((x+y) & 1) return false;

  return true;
}

#define HEIGHT (sqrt(3)/2)

void voxel_1_c::getConnectionFace(int x, int y, int z, int n, double bevel, double offset, std::vector<float> & faceCorners) const
{
  int p;

  if (((x+y) & 1) == 0)
  {
    p = 0;
  }
  else
  {
    p = 1;
  }

  if (n < 0)
  {
    n = -1-n;

    if (n < 15)
    {
      for (int i = 0; i < bevelFaces[p][n][0]; i++)
      {
        int v = bevelFaces[p][n][i+1];
        faceCorners.push_back(   0.5*x+vertices[v][0][0] + offset*vertices[v][1][0] + bevel*vertices[v][2][0]);
        faceCorners.push_back(HEIGHT*y+vertices[v][0][1] + offset*vertices[v][1][1] + bevel*vertices[v][2][1]);
        faceCorners.push_back(       z+vertices[v][0][2] + offset*vertices[v][1][2] + bevel*vertices[v][2][2]);
      }
    }
  }
  else
  {
    if (n < 5)
    {
      for (int i = 0; i < normalFaces[p][n][0]; i++)
      {
        int v = normalFaces[p][n][i+1];
        faceCorners.push_back(   0.5*x+vertices[v][0][0] + offset*vertices[v][1][0] + bevel*vertices[v][2][0]);
        faceCorners.push_back(HEIGHT*y+vertices[v][0][1] + offset*vertices[v][1][1] + bevel*vertices[v][2][1]);
        faceCorners.push_back(       z+vertices[v][0][2] + offset*vertices[v][1][2] + bevel*vertices[v][2][2]);
      }
    }
  }
}

void voxel_1_c::calculateSize(float * x, float * y, float * z) const {
  *x = 1 + (getX()-1)*0.5;
  *y = getY()*HEIGHT;
  *z = getZ();
}

void voxel_1_c::recalcSpaceCoordinates(float * x, float * y, float * /*z*/) const {
  *x = *x * 0.5;
  *y = *y * HEIGHT;
}

