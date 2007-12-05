/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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

#include "voxeldrawer_3.h"

#include "piececolor.h"

#include "../lib/voxel.h"

#include <math.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

// this is used to shift one side of the cubes so that they slightly differ
// from the side of the next cube, so that (in case of frames) the sides
// are clearly separated and don't interlock when drawing
#define MY 0.005f

static bool getTetrahedron(int x, int y, int z,
    int *x1, int *y1, int *z1,
    int *x2, int *y2, int *z2,
    int *x3, int *y3, int *z3,
    int *x4, int *y4, int *z4) {

  int xc = (x+100) / 5 -20;   // this is supposed to make shure we round towards -inf. It works up to -100
  int yc = (y+100) / 5 -20;
  int zc = (z+100) / 5 -20;

  int xs = x - 5*xc;
  int ys = y - 5*yc;
  int zs = z - 5*zc;

  /* the center is always used */
  *x1 = *y1 = *z1 = 1;

  /* the snd point is in the center of the face, where the voxel is within its 5x5x5 grid */
  *x2 = *y2 = *z2 = 1;
  if (xs == 0 || xs == 4)      *x2 = xs/2;
  else if (ys == 0 || ys == 4) *y2 = ys/2;
  else if (zs == 0 || zs == 4) *z2 = zs/2;
  else return false;

  /* the last 2 points are in the 2 corners closest to the point */
  if (xs == 2) {
    *x3 = 0;
    *x4 = 2;

    *y3 = *y4 = (ys<2)?0:2;
    *z3 = *z4 = (zs<2)?0:2;
  } else if (ys == 2) {
    *y3 = 0;
    *y4 = 2;

    *x3 = *x4 = (xs<2)?0:2;
    *z3 = *z4 = (zs<2)?0:2;
  } else if (zs == 2) {
    *z3 = 0;
    *z4 = 2;

    *x3 = *x4 = (xs<2)?0:2;
    *y3 = *y4 = (ys<2)?0:2;
  } else return false;

  *x1 += 2*xc; *x2 += 2*xc; *x3 += 2*xc; *x4 += 2*xc;
  *y1 += 2*yc; *y2 += 2*yc; *y3 += 2*yc; *y4 += 2*yc;
  *z1 += 2*zc; *z2 += 2*zc; *z3 += 2*zc; *z4 += 2*zc;

  /* final check for invalid voxels */
  if (xs > 2) xs = 4-xs;
  if (ys > 2) ys = 4-ys;
  if (zs > 2) zs = 4-zs;

  if (xs != 0 && ys != 0 && zs != 0) return false;
  if (xs != 1 && ys != 1 && zs != 1) return false;

  return true;
}


static bool sameEdge(int xa, int ya, int za, int xb, int yb, int zb,
                     int xc, int yc, int zc, int xd, int yd, int zd) {

  return (xa == xc && ya == yc && za == zc && xb == xd && yb == yd && zb == zd ||
          xa == xd && ya == yd && za == zd && xb == xc && yb == yc && zb == zc);
}

/* for a given voxel it returns the 2 faces that are at a given edge with 2 coodinates */

static bool getEdgeFaces(int x, int y, int z, int xa, int ya, int za, int xb, int yb, int zb, int *f1, int *f2) {

  int x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4;
  bt_assert(getTetrahedron(x, y, z, &x1, &y1, &z1, &x2, &y2, &z2, &x3, &y3, &z3, &x4, &y4, &z4));

  int xt1, xt2, xt3, yt1, yt2, yt3, zt1, zt2, zt3;
  bool foundFirst = false;

  for (int f = 0; f < 4; f++) {

    switch (f) {
      case 0: xt1 = x1; yt1 = y1; zt1 = z1; xt2 = x2; yt2 = y2; zt2 = z2; xt3 = x3; yt3 = y3; zt3 = z3; break;
      case 1: xt1 = x1; yt1 = y1; zt1 = z1; xt2 = x2; yt2 = y2; zt2 = z2; xt3 = x4; yt3 = y4; zt3 = z4; break;
      case 2: xt1 = x2; yt1 = y2; zt1 = z2; xt2 = x3; yt2 = y3; zt2 = z3; xt3 = x4; yt3 = y4; zt3 = z4; break;
      case 3: xt1 = x3; yt1 = y3; zt1 = z3; xt2 = x1; yt2 = y1; zt2 = z1; xt3 = x4; yt3 = y4; zt3 = z4; break;
      default: bt_assert(0);
    }

    for (int e = 0; e < 3; e++) {
      int xea, yea, zea, xeb, yeb, zeb;

      switch (e) {
        case 0: xea = xt1; yea = yt1; zea = zt1; xeb = xt2; yeb = yt2; zeb = zt2; break;
        case 1: xea = xt2; yea = yt2; zea = zt2; xeb = xt3; yeb = yt3; zeb = zt3; break;
        case 2: xea = xt3; yea = yt3; zea = zt3; xeb = xt1; yeb = yt1; zeb = zt1; break;
        default: bt_assert(0);
      }

      if (sameEdge(xa, ya, za, xb, yb, zb, xea, yea, zea, xeb, yeb, zeb)) {

        if (!foundFirst) {
          *f1 = f;
          foundFirst = true;
        } else {
          *f2 = f;
          return true;
        }
      }
    }
  }

  return false;
}

/* this function finds all the neighbors that tough the voxel at the given edge of the givne face
 * there can be up to 7 neighbors. The concrete number is given in the return value the coordinates
 * in the field in tx1, y1, z1, x2, y2, z2, ... order
 */

static int getEdgeNeighbors(const voxel_c * space, int x, int y, int z, int face, int edge, int vx[8*3]) {

  bt_assert(face < 4);
  bt_assert(edge < 3);

  int nCnt = 0;

  int xa, ya, za, xb, yb, zb;

  {
    /* get current tetrahedron and extract the edge we are interested in */
    int x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4;
    bt_assert(getTetrahedron(x, y, z, &x1, &y1, &z1, &x2, &y2, &z2, &x3, &y3, &z3, &x4, &y4, &z4));

    int xt1, xt2, xt3, yt1, yt2, yt3, zt1, zt2, zt3;

    switch (face) {
      case 0: xt1 = x1; yt1 = y1; zt1 = z1; xt2 = x2; yt2 = y2; zt2 = z2; xt3 = x3; yt3 = y3; zt3 = z3; break;
      case 1: xt1 = x1; yt1 = y1; zt1 = z1; xt2 = x2; yt2 = y2; zt2 = z2; xt3 = x4; yt3 = y4; zt3 = z4; break;
      case 2: xt1 = x2; yt1 = y2; zt1 = z2; xt2 = x3; yt2 = y3; zt2 = z3; xt3 = x4; yt3 = y4; zt3 = z4; break;
      case 3: xt1 = x3; yt1 = y3; zt1 = z3; xt2 = x1; yt2 = y1; zt2 = z1; xt3 = x4; yt3 = y4; zt3 = z4; break;
      default: bt_assert(0);
    }
    switch (edge) {
      case 0: xa = xt1; ya = yt1; za = zt1; xb = xt2; yb = yt2; zb = zt2; break;
      case 1: xa = xt2; ya = yt2; za = zt2; xb = xt3; yb = yt3; zb = zt3; break;
      case 2: xa = xt3; ya = yt3; za = zt3; xb = xt1; yb = yt1; zb = zt1; break;
      default: bt_assert(0);
    }
  }

  vx[3*nCnt+0] = x;
  vx[3*nCnt+1] = y;
  vx[3*nCnt+2] = z;
  nCnt++;

  int nx, ny, nz, f1, f2;
  space->getNeighbor(face, 0, x, y, z, &nx, &ny, &nz);

  while (true) {

    if (x == nx && y == ny && z == nz)
      break;

    vx[3*nCnt+0] = nx;
    vx[3*nCnt+1] = ny;
    vx[3*nCnt+2] = nz;

    bt_assert(getEdgeFaces(nx, ny, nz, xa, ya, za, xb, yb, zb, &f1, &f2));

    bt_assert(space->getNeighbor(f1, 0, vx[3*nCnt+0], vx[3*nCnt+1], vx[3*nCnt+2], &nx, &ny, &nz));

    if (nx == vx[3*nCnt-3] && ny == vx[3*nCnt-2] && nz == vx[3*nCnt-1])
      bt_assert(space->getNeighbor(f2, 0, vx[3*nCnt+0], vx[3*nCnt+1], vx[3*nCnt+2], &nx, &ny, &nz));

    bt_assert(nx != vx[3*nCnt-3] || ny != vx[3*nCnt-2] || nz != vx[3*nCnt-1]);

    nCnt++;
  }

  return nCnt;
}

/* to find out, if an edge has to be drawn we need to find all the other voxels that touch
 * the current voxel at the current edge.
 *
 * if of all those voxels, only voxels _below_ the plane of the current face are existing and
 * the neighbor voxel that is on the same plane is existing, then we don't draw the edge
 *
 * this function calculates exactly this information
 */

static bool edgeVisible(const voxel_c * space, int x, int y, int z, int face, int edge) {


#if 0 // this code is used to generate the bit matrix in voxel_3_c::getNeighbor for
      // the edge neighbors

  static int tttt = 0;

  if (tttt == 0) {
    tttt = 1;

    for (int xp = 0; xp < 5; xp++)
      for (int yp = 0; yp < 5; yp++)
        for (int zp = 0; zp < 5; zp++)
          if (space->validCoordinate(xp, yp, zp)) {

            printf(" neighbors of %i %i %i:\n", xp, yp, zp);

            for (int ff = 0; ff < 4; ff++)
              for (int ed = 0; ed < 3; ed++) {

                int nb[8*3];
                int cnt = getEdgeNeighbors(space, xp, yp, zp, ff, ed, nb);


                for (int res = 1; res < cnt; res++) {

                  bool faceN = false;

                  int idx = 0;
                  int nx, ny, nz;

                  while (space->getNeighbor(idx, 0, xp, yp, zp, &nx, &ny, &nz)) {
                    if (nx == nb[3*res] && ny == nb[3*res+1] && nz == nb[3*res+2]) {
                      faceN = true;
                      break;
                    }
                    idx++;
                  }

                  if (!faceN)
                    printf("  %i %i %i\n", nb[3*res]-xp, nb[3*res+1]-yp, nb[3*res+2]-zp);
                }
              }
          }
  }
#endif

  int nb[8*3];
  int cnt = getEdgeNeighbors(space, x, y, z, face, edge, nb);

  bt_assert(cnt == 4 || cnt == 6|| cnt == 8);

  int f = 1;

  // the first neibor must be empty, otherwise the face is invisible and the
  // edge is _not_ drawn
  if (!space->isEmpty2(nb[3*f], nb[3*f+1], nb[3*f+2]))
    return false;

  f++;
  while (f <= cnt/2) {
    if (!space->isEmpty2(nb[3*f], nb[3*f+1], nb[3*f+2]))
      return true;
    f++;
  }

  if (space->isEmpty2(nb[3*(cnt/2+1)], nb[3*(cnt/2+1)+1], nb[3*(cnt/2+1)+2]))
    return true;

  return false;
}

/* moves a line of a triangle:
 * the line 1-2 is moved by amount in direction of point 3
 */

static void moveLine(float *x1, float *y1, float *z1, float *x2, float *y2, float *z2, float x3, float y3, float z3, float amount) {

  /* the triangle is x1 x2 x3 the moved points are x1 and x2. Let's see how it is done for x1:
   *   take the angle between vextors x1->x3 ad x1->x2. use this angle to calculate the
   *   length of all sides of the triangle x1 x1n x1b, where x1b is the point of a right angle
   *   of....
   */

  float v1x = *x2 - *x1;
  float v1y = *y2 - *y1;
  float v1z = *z2 - *z1;

  float len1 = sqrt(v1x*v1x+v1y*v1y+v1z*v1z);

  float v2x = x3 - *x1;
  float v2y = y3 - *y1;
  float v2z = z3 - *z1;

  float len = sqrt(v2x*v2x+v2y*v2y+v2z*v2z);

  float cosa = (v1x*v2x + v1y*v2y + v1z*v2z) / (len1 * len);
  float sina = sqrt(1-cosa*cosa);
  float scale = amount/(sina*len);


  *x1 += v2x * scale;
  *y1 += v2y * scale;
  *z1 += v2z * scale;

  v1x *= -1;
  v1y *= -1;
  v1z *= -1;

  v2x = x3 - *x2;
  v2y = y3 - *y2;
  v2z = z3 - *z2;

  len = sqrt(v2x*v2x+v2y*v2y+v2z*v2z);

  cosa = (v1x*v2x + v1y*v2y + v1z*v2z) / (len1 * len);
  sina = sqrt(1-cosa*cosa);
  scale = amount/(sina*len);

  *x2 += v2x * scale;
  *y2 += v2y * scale;
  *z2 += v2z * scale;
}

static void drawFrameTriangle(int x1, int y1, int z1, int x2, int y2, int z2, int x3, int y3, int z3, int px, int py, int pz,
    float edge, uint8_t edgemask) {

  int sx = x1;
  int sy = y1;
  int sz = z1;
  int v1x = x2-x1;
  int v1y = y2-y1;
  int v1z = z2-z1;
  int v2x = x3-x1;
  int v2y = y3-y1;
  int v2z = z3-z1;

  float nx = v1y*v2z - v1z*v2y;
  float ny = v1z*v2x - v1x*v2z;
  float nz = v1x*v2y - v1y*v2x;

  float l = sqrt(nx*nx+ny*ny+nz*nz);
  nx /= l;
  ny /= l;
  nz /= l;

  if (nx*(px-sx)+ny*(py-sy)+nz*(pz-sz) > 0) {
    nx = -nx;
    ny = -ny;
    nz = -nz;
  }

  float px1, px2, px3, py1, py2, py3, pz1, pz2, pz3;

  px1 = x1; px2 = x2; px3 = x3;
  py1 = y1; py2 = y2; py3 = y3;
  pz1 = z1; pz2 = z2; pz3 = z3;

  glBegin(GL_TRIANGLES);
  glNormal3f(nx, ny, nz);

  if (edgemask & 1) {
    moveLine(&px1, &py1, &pz1, &px2, &py2, &pz2, px3, py3, pz3, edge);

    glVertex3f(x1, y1, z1); glVertex3f(x2, y2, z2); glVertex3f(px1, py1, pz1);
    glVertex3f(x2, y2, z2); glVertex3f(px1, py1, pz1); glVertex3f(px2, py2, pz2);

    px1 = x1; px2 = x2;
    py1 = y1; py2 = y2;
    pz1 = z1; pz2 = z2;
  }

  if (edgemask & 2) {
    moveLine(&px2, &py2, &pz2, &px3, &py3, &pz3, px1, py1, pz1, edge);

    glVertex3f(x2, y2, z2); glVertex3f(x3, y3, z3); glVertex3f(px2, py2, pz2);
    glVertex3f(x3, y3, z3); glVertex3f(px2, py2, pz2); glVertex3f(px3, py3, pz3);

    px2 = x2; px3 = x3;
    py2 = y2; py3 = y3;
    pz2 = z2; pz3 = z3;
  }

  if (edgemask & 4) {
    moveLine(&px3, &py3, &pz3, &px1, &py1, &pz1, px2, py2, pz2, edge);

    glVertex3f(x3, y3, z3); glVertex3f(x1, y1, z1); glVertex3f(px3, py3, pz3);
    glVertex3f(x1, y1, z1); glVertex3f(px3, py3, pz3); glVertex3f(px1, py1, pz1);
  }

  glEnd();
}


static void drawFrameTetrahedron(int x1, int y1, int z1,
                                 int x2, int y2, int z2,
                                 int x3, int y3, int z3,
                                 int x4, int y4, int z4, uint8_t mask, float edge, uint16_t edgemask) {

  if (mask & 1) {
    glLoadName(0);
    drawFrameTriangle(x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4, edge, edgemask & 0x7);
  }

  if (mask & 2) {
    glLoadName(1);
    drawFrameTriangle(x1, y1, z1, x2, y2, z2, x4, y4, z4, x3, y3, z3, edge, (edgemask >> 3) & 0x7);
  }

  if (mask & 4) {
    glLoadName(2);
    drawFrameTriangle(x2, y2, z2, x3, y3, z3, x4, y4, z4, x1, y1, z1, edge, (edgemask >> 6) & 0x7);
  }

  if (mask & 8) {
    glLoadName(3);
    drawFrameTriangle(x3, y3, z3, x1, y1, z1, x4, y4, z4, x2, y2, z2, edge, (edgemask >> 9)  & 0x7);
  }
}

// draws a wire frame box depending on the neighbours
void voxelDrawer_3_c::drawFrame(const voxel_c * space, int x, int y, int z, float edge) {

  if (fabs(edge) < 0.00001) return;

  int x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4;

  if (!getTetrahedron(x, y, z, &x1, &y1, &z1, &x2, &y2, &z2, &x3, &y3, &z3, &x4, &y4, &z4)) return;

  uint8_t mask = 0;

  int nx, ny, nz;
  for (int i = 0; i < 4; i++) {
    space->getNeighbor(i, 0, x, y, z, &nx, &ny, &nz);
    if (space->isEmpty2(nx, ny, nz)) mask |= (1 << i);
  }

  uint16_t edgemask = 0;
  int p = 0;

  for (int f = 0; f < 4; f++)
    for (int e = 0; e < 3; e++) {
      if (edgeVisible(space, x, y, z, f, e))
        edgemask |= (1 << p);
      p++;
    }

  glPushName(0);
  drawFrameTetrahedron(x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4, mask, edge, edgemask);
  glPopName();
}

/* draw a trianle, set the normal so that it is orthogonal and pointing away from p */
static void drawEdgeTriangle(int x1, int y1, int z1, int x2, int y2, int z2, int x3, int y3, int z3, int px, int py, int pz,
    float edge, uint8_t edgemask) {

  float px1, px2, px3, py1, py2, py3, pz1, pz2, pz3;

  px1 = x1; px2 = x2; px3 = x3;
  py1 = y1; py2 = y2; py3 = y3;
  pz1 = z1; pz2 = z2; pz3 = z3;

  if (edgemask & 1) moveLine(&px1, &py1, &pz1, &px2, &py2, &pz2, px3, py3, pz3, edge);
  if (edgemask & 2) moveLine(&px2, &py2, &pz2, &px3, &py3, &pz3, px1, py1, pz1, edge);
  if (edgemask & 4) moveLine(&px3, &py3, &pz3, &px1, &py1, &pz1, px2, py2, pz2, edge);

  drawTriangle(px1, py1, pz1, px2, py2, pz2, px3, py3, pz3, px, py, pz);
}

static void drawTetrahedron(int x1, int y1, int z1,
                            int x2, int y2, int z2,
                            int x3, int y3, int z3,
                            int x4, int y4, int z4, float edge, uint8_t mask, uint16_t edgemask) {

  if (mask & 1) {
    glLoadName(0);
    drawEdgeTriangle(x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4, edge, edgemask & 0x7);
  }

  if (mask & 2) {
    glLoadName(1);
    drawEdgeTriangle(x1, y1, z1, x2, y2, z2, x4, y4, z4, x3, y3, z3, edge, (edgemask >> 3) & 0x7);
  }

  if (mask & 4) {
    glLoadName(2);
    drawEdgeTriangle(x2, y2, z2, x3, y3, z3, x4, y4, z4, x1, y1, z1, edge, (edgemask >> 6) & 0x7);
  }

  if (mask & 8) {
    glLoadName(3);
    drawEdgeTriangle(x3, y3, z3, x1, y1, z1, x4, y4, z4, x2, y2, z2, edge, (edgemask >> 9)  & 0x7);
  }
}

/* draw a triangle, set the normal so that it is orthogonal and pointing away from p the triangle is
 * shrunk so that a new smaller triangle whose sides are 0.2 units from the given one and also
 * moved by MY to the outside along the normal */
static void drawShrinkTriangle(int x1, int y1, int z1, int x2, int y2, int z2, int x3, int y3, int z3, int px, int py, int pz) {

  int sx = x1;
  int sy = y1;
  int sz = z1;
  int v1x = x2-x1;
  int v1y = y2-y1;
  int v1z = z2-z1;
  int v2x = x3-x1;
  int v2y = y3-y1;
  int v2z = z3-z1;

  float nx = v1y*v2z - v1z*v2y;
  float ny = v1z*v2x - v1x*v2z;
  float nz = v1x*v2y - v1y*v2x;

  float l = sqrt(nx*nx+ny*ny+nz*nz);
  nx /= l;
  ny /= l;
  nz /= l;

  if (nx*(px-sx)+ny*(py-sy)+nz*(pz-sz) > 0) {
    nx = -nx;
    ny = -ny;
    nz = -nz;
  }

  float c = sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)+(z2-z1)*(z2-z1));
  float b = sqrt((x3-x1)*(x3-x1)+(y3-y1)*(y3-y1)+(z3-z1)*(z3-z1));
  float a = sqrt((x2-x3)*(x2-x3)+(y2-y3)*(y2-y3)+(z2-z3)*(z2-z3));

  float cx = x1*a/(a+b+c) + x2*b/(a+b+c) + x3*c/(a+b+c);
  float cy = y1*a/(a+b+c) + y2*b/(a+b+c) + y3*c/(a+b+c);
  float cz = z1*a/(a+b+c) + z2*b/(a+b+c) + z3*c/(a+b+c);

  float s = (a+b+c)/2;
  float r = sqrt(((s-a)*(s-b)*(s-c))/s);

  float p = (r-0.2)/r;

  glBegin(GL_TRIANGLES);
  glNormal3f(nx, ny, nz);

  glVertex3f(cx+p*(x1-cx)+MY*nx, cy+p*(y1-cy)+MY*ny, cz+p*(z1-cz)+MY*nz);
  glVertex3f(cx+p*(x2-cx)+MY*nx, cy+p*(y2-cy)+MY*ny, cz+p*(z2-cz)+MY*nz);
  glVertex3f(cx+p*(x3-cx)+MY*nx, cy+p*(y3-cy)+MY*ny, cz+p*(z3-cz)+MY*nz);
  glEnd();
}

static void drawGapTetrahedron(int x1, int y1, int z1,
                               int x2, int y2, int z2,
                               int x3, int y3, int z3,
                               int x4, int y4, int z4, uint8_t mask) {

  if (mask & 1) {
    glLoadName(0);
    drawShrinkTriangle(x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4);
  }

  if (mask & 2) {
    glLoadName(1);
    drawShrinkTriangle(x1, y1, z1, x2, y2, z2, x4, y4, z4, x3, y3, z3);
  }

  if (mask & 4) {
    glLoadName(2);
    drawShrinkTriangle(x2, y2, z2, x3, y3, z3, x4, y4, z4, x1, y1, z1);
  }

  if (mask & 8) {
    glLoadName(3);
    drawShrinkTriangle(x3, y3, z3, x1, y1, z1, x4, y4, z4, x2, y2, z2);
  }
}

// draws a box with borders depending on the neighbour boxes
void voxelDrawer_3_c::drawNormalVoxel(const voxel_c * space, int x, int y, int z, float alpha, float edge) {

  /* calculate the drawing mask for the tetrahedron */
  uint8_t mask = 0;

  int nx, ny, nz;
  for (int i = 0; i < 4; i++) {
    space->getNeighbor(i, 0, x, y, z, &nx, &ny, &nz);
    if (space->isEmpty2(nx, ny, nz)) mask |= (1 << i);
  }

  uint16_t edgemask = 0;
  int p = 0;

  for (int f = 0; f < 4; f++)
    for (int e = 0; e < 3; e++) {
      if (edgeVisible(space, x, y, z, f, e))
        edgemask |= 1 << p;
      p++;
    }

  // when no fase needs to be painted exit
  if (mask == 0) return;

  int x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4;

  if (!getTetrahedron(x, y, z, &x1, &y1, &z1, &x2, &y2, &z2, &x3, &y3, &z3, &x4, &y4, &z4)) return;

  glPushName(2);
  drawTetrahedron(x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4, edge, mask, edgemask);
  glPopName();

  glColor4f(0, 0, 0, alpha);
  drawFrame(space, x, y, z, edge);
}

// draw a cube that is smaller than 1
void voxelDrawer_3_c::drawVariableMarkers(const voxel_c * space, int x, int y, int z) {

  /* calculate the drawing mask for the tetrahedron */
  uint8_t mask = 0;

  int nx, ny, nz;
  for (int i = 0; i < 4; i++) {
    space->getNeighbor(i, 0, x, y, z, &nx, &ny, &nz);
    if (space->isEmpty2(nx, ny, nz)) mask |= (1 << i);
  }

  // when no fase needs to be painted exit
  if (mask == 0) return;

  int x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4;

  if (!getTetrahedron(x, y, z, &x1, &y1, &z1, &x2, &y2, &z2, &x3, &y3, &z3, &x4, &y4, &z4)) return;

  glPushName(2);
  drawGapTetrahedron(x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4, mask);
  glPopName();
}

void voxelDrawer_3_c::drawCursor(const voxel_c * space, int mX1, int mX2, int mY1, int mY2, int mZ, int mode) {

  unsigned int sx = space->getX();
  unsigned int sy = space->getY();
  unsigned int sz = space->getZ();

  // draw the cursor, this is done by iterating over all
  // cubes and checking for the 3 directions (in one direction only as the other
  // direction is done with the next cube), if there is a border in the cursor
  // between these 2 cubes, if so draw the cursor grid
  for (unsigned int x = 0; x <= sx; x++)
    for (unsigned int y = 0; y <= sy; y++)
      for (unsigned int z = 0; z <= sz; z++) {
        bool ins = inRegion(x, y, z, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, mode);

        uint8_t mask = 0;

        int nx, ny, nz;
        for (int i = 0; i < 4; i++) {
          space->getNeighbor(i, 0, x, y, z, &nx, &ny, &nz);
          if (ins ^ inRegion(nx, ny, nz, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, mode)) mask |= (1 << i);
        }

        if (mask && ins) {
          int x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4;

          if (getTetrahedron(x, y, z, &x1, &y1, &z1, &x2, &y2, &z2, &x3, &y3, &z3, &x4, &y4, &z4)) {
            if (mask & 1) drawGridTriangle(x1, y1, z1, x2-x1, y2-y1, z2-z1, x3-x1, y3-y1, z3-z1, 4);
            if (mask & 2) drawGridTriangle(x1, y1, z1, x2-x1, y2-y1, z2-z1, x4-x1, y4-y1, z4-z1, 4);
            if (mask & 4) drawGridTriangle(x2, y2, z2, x3-x2, y3-y2, z3-z2, x4-x2, y4-y2, z4-z2, 4);
            if (mask & 8) drawGridTriangle(x3, y3, z3, x1-x3, y1-y3, z1-z3, x4-x3, y4-y4, z4-z3, 4);
          }
        }
      }
}

void voxelDrawer_3_c::calculateSize(const voxel_c * shape, float * x, float * y, float * z) {
  *x = 2*((shape->getX()+4)/5);
  *y = 2*((shape->getY()+4)/5);
  *z = 2*((shape->getZ()+4)/5);
}

void voxelDrawer_3_c::recalcSpaceCoordinates(float * x, float * y, float * z) {
  *x *= 0.4;
  *y *= 0.4;
  *z *= 0.4;
}

