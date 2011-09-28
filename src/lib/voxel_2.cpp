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
#include "voxel_2.h"

#include <math.h>

#include "tabs_2/tablesizes.inc"

static double rotationMatrices[NUM_TRANSFORMATIONS_MIRROR][9] = {
#include "tabs_2/rotmatrix.inc"
};

bool voxel_2_c::transform(unsigned int nr) {

  if (nr == 0) return true;

  bt_assert(nr < NUM_TRANSFORMATIONS_MIRROR);

  int minx = 100000;
  int miny = 100000;
  int minz = 100000;
  int maxx = -100000;
  int maxy = -100000;
  int maxz = -100000;

  bool first = true;
  double shx, shy, shz;
  shx = shy = shz = 0;

  unsigned int index = 0;
  for (unsigned int z = 0; z < sz; z++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int x = 0; x < sx; x++) {

        if (((x+y+z) & 1) == 0) {

          if (!isEmpty(index)) {

            double xp = x * sqrt(0.5);
            double yp = y * sqrt(0.5);
            double zp = z * sqrt(0.5);

            // rotate using the rotation matrices from the top

            double xpn = rotationMatrices[nr][0]*xp + rotationMatrices[nr][1]*yp + rotationMatrices[nr][2]*zp;
            double ypn = rotationMatrices[nr][3]*xp + rotationMatrices[nr][4]*yp + rotationMatrices[nr][5]*zp;
            double zpn = rotationMatrices[nr][6]*xp + rotationMatrices[nr][7]*yp + rotationMatrices[nr][8]*zp;

            xpn /= sqrt(0.5);
            ypn /= sqrt(0.5);
            zpn /= sqrt(0.5);

            if (first) {
              shx = xpn;
              shy = ypn;
              shz = zpn;
              xpn = ypn = zpn = 0;
              first = false;
            } else {
              xpn -= shx;
              ypn -= shy;
              zpn -= shz;
            }

            int xn = (int)(xpn+(xpn<0?-0.5:0.5));
            int yn = (int)(ypn+(ypn<0?-0.5:0.5));
            int zn = (int)(zpn+(zpn<0?-0.5:0.5));

            // check errors, it should be small enough, so the calculated
            // new position should fall on a grid position
            if ((fabs(xpn-xn) > 0.01) ||
                (fabs(ypn-yn) > 0.01) ||
                (fabs(zpn-zn) > 0.01)) {
              return false;
            }

            // is should fall on a valid grid position
            if (((xn+yn+zn) & 1) != 0) {
              return false;
            }

            if (xn > maxx) maxx = xn;
            if (yn > maxy) maxy = yn;
            if (zn > maxz) maxz = zn;

            if (xn < minx) minx = xn;
            if (yn < miny) miny = yn;
            if (zn < minz) minz = zn;
          }
        }
        index++;
      }

  /* when there are no voxels, we do nothing */
  if (first)
    return true;

  if ((minx+miny+minz) & 1)
    minx--;

  unsigned int nsx = maxx-minx+1;
  unsigned int nsy = maxy-miny+1;
  unsigned int nsz = maxz-minz+1;

  // don't make the new space smaller than the old one
  // if the old one was larger centre the object inside it
  if ((nsx < sx) || (nsy < sy) || (nsz < sz)) {
    // we must make sure that we shift so that we don't change the
    // state of the voxels, do (dx+dy+dz)&1 must be 0

    int dx = (nsx < sx) ? (sx-nsx)/2 : 0;
    int dy = (nsy < sy) ? (sy-nsy)/2 : 0;
    int dz = (nsz < sz) ? (sz-nsz)/2 : 0;

    if ((dx+dy+dz) & 1) {

      if (dx > 0)
        dx--;
      else if (dy > 0)
        dy--;
      else
        dz--;
    }

    minx -= dx;
    miny -= dy;
    minz -= dz;
    nsx = (sx > nsx) ? sx : nsx;
    nsy = (sy > nsy) ? sy : nsy;
    nsz = (sz > nsz) ? sz : nsz;
  }

  int voxelsn = nsx*nsy*nsz;

  voxel_type *s = new voxel_type[voxelsn];
  memset(s, VX_EMPTY, voxelsn);

  index = 0;
  for (unsigned int z = 0; z < sz; z++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int x = 0; x < sx; x++) {

        if (((x+y+z) & 1) == 0) {

          if (!isEmpty(index)) {

            double xp = x * sqrt(0.5);
            double yp = y * sqrt(0.5);
            double zp = z * sqrt(0.5);

            // rotate using the rotation matrices from the top

            double xpn = rotationMatrices[nr][0]*xp + rotationMatrices[nr][1]*yp + rotationMatrices[nr][2]*zp;
            double ypn = rotationMatrices[nr][3]*xp + rotationMatrices[nr][4]*yp + rotationMatrices[nr][5]*zp;
            double zpn = rotationMatrices[nr][6]*xp + rotationMatrices[nr][7]*yp + rotationMatrices[nr][8]*zp;

            xpn /= sqrt(0.5);
            ypn /= sqrt(0.5);
            zpn /= sqrt(0.5);

            xpn -= shx;
            ypn -= shy;
            zpn -= shz;

            int xn = (int)(xpn+(xpn<0?-0.5:0.5));
            int yn = (int)(ypn+(ypn<0?-0.5:0.5));
            int zn = (int)(zpn+(zpn<0?-0.5:0.5));

            s[(xn-minx) + nsx*((yn-miny) + nsy*(zn-minz))] = space[index];
          }
        }
        index++;
      }


  // calculate the new hotspot position
  bt_assert(((hx+hy+hz) & 1) == 0);

  double xp = hx * sqrt(0.5);
  double yp = hy * sqrt(0.5);
  double zp = hz * sqrt(0.5);

  double xpn = rotationMatrices[nr][0]*xp + rotationMatrices[nr][1]*yp + rotationMatrices[nr][2]*zp;
  double ypn = rotationMatrices[nr][3]*xp + rotationMatrices[nr][4]*yp + rotationMatrices[nr][5]*zp;
  double zpn = rotationMatrices[nr][6]*xp + rotationMatrices[nr][7]*yp + rotationMatrices[nr][8]*zp;

  xpn /= sqrt(0.5);
  ypn /= sqrt(0.5);
  zpn /= sqrt(0.5);

  xpn -= shx;
  ypn -= shy;
  zpn -= shz;

  hx = (int)(xpn+(xpn<0?-0.5:0.5)) - minx;
  hy = (int)(ypn+(ypn<0?-0.5:0.5)) - miny;
  hz = (int)(zpn+(zpn<0?-0.5:0.5)) - minz;

  bt_assert(((hx+hy+hz) & 1) == 0);

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

void voxel_2_c::transformPoint(int * x, int * y, int * z, unsigned int trans) const {

  bt_assert(trans < NUM_TRANSFORMATIONS_MIRROR);
  bt_assert(((*x+*y+*z) & 1) == 0);

  double xp = *x * sqrt(0.5);
  double yp = *y * sqrt(0.5);
  double zp = *z * sqrt(0.5);

  double xpn = rotationMatrices[trans][0]*xp + rotationMatrices[trans][1]*yp + rotationMatrices[trans][2]*zp;
  double ypn = rotationMatrices[trans][3]*xp + rotationMatrices[trans][4]*yp + rotationMatrices[trans][5]*zp;
  double zpn = rotationMatrices[trans][6]*xp + rotationMatrices[trans][7]*yp + rotationMatrices[trans][8]*zp;

  xpn /= sqrt(0.5);
  ypn /= sqrt(0.5);
  zpn /= sqrt(0.5);

  // it is possible that the resulting coordinates are invalid (not on a whole coordinate or at
  // an invalid coordinate, but we assume that the error we introduce here will be cancelled out
  // later on, when other coordinates are transformed
  //
  // this function is only used in the assembly class to transform an assembly in there we use
  // this function twice, once to remove hotspot movement and once to transform the anchor point
  // or pieces. These 2 values are subtracted. The assumption is that both time we introduce the
  // same error with the rounding below and due to subtraction the error cancels itself out
  // lets cross fingers....

  *x = (int)floor(xpn+0.5);
  *y = (int)floor(ypn+0.5);
  *z = (int)floor(zpn+0.5);
}

bool voxel_2_c::getNeighbor(unsigned int idx, unsigned int typ, int x, int y, int z, int * xn, int *yn, int *zn) const {

  // spheres have only one type of neighbour
  switch (typ) {
    case 0:
      switch (idx) {
        case  0: *xn = x-1; *yn = y-1; *zn = z;   break;
        case  1: *xn = x-1; *yn = y+1; *zn = z;   break;
        case  2: *xn = x+1; *yn = y-1; *zn = z;   break;
        case  3: *xn = x+1; *yn = y+1; *zn = z;   break;
        case  4: *xn = x-1; *yn = y;   *zn = z-1; break;
        case  5: *xn = x-1; *yn = y;   *zn = z+1; break;
        case  6: *xn = x+1; *yn = y;   *zn = z-1; break;
        case  7: *xn = x+1; *yn = y;   *zn = z+1; break;
        case  8: *xn = x;   *yn = y-1; *zn = z-1; break;
        case  9: *xn = x;   *yn = y-1; *zn = z+1; break;
        case 10: *xn = x;   *yn = y+1; *zn = z-1; break;
        case 11: *xn = x;   *yn = y+1; *zn = z+1; break;
        default: return false;
      }
      break;
    default:
      return false;
  }
  return true;
}


void voxel_2_c::minimizePiece(void) {
  int move_again = (bx1 + by1 + bz1) & 1;

  voxel_c::minimizePiece();

  if (move_again) {
    resize(sx+1, sy, sz, 0);
    translate(1, 0, 0, 0);
  }
}

void voxel_2_c::initHotspot(void) {

  unsigned long best = getX()*getX() + getY()*getY() + getZ()*getZ();

  // find a sphere as near as possible to the source
  for (unsigned int z = 0; z < getZ(); z++)
    for (unsigned int y = 0; y < getY(); y++)
      for (unsigned int x = 0; x < getX(); x++)
        if (!isEmpty(x, y, z)) {
          unsigned long diff = x*x + y*y + z*z;
          if (diff < best) {
            best = diff;
            setHotspot(x, y, z);
          }
        }
}

void voxel_2_c::resizeInclude(int & px, int & py, int & pz) {

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
    if (pz & 1) {
      nsz ++;
      tz ++;
    }
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

bool voxel_2_c::validCoordinate(int x, int y, int z) const {

  return ((x+y+z) & 1) == 0;

}

bool voxel_2_c::onGrid(int x, int y, int z) const {

  // the shape doesn't fit, when the lower left corner doesn't have the right parity
  return ((x+y+z) & 1) == 0;
}

