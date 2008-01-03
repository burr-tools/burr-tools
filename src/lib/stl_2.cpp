/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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
#include "stl_2.h"
#include "math.h"

#include "puzzle.h"
#include "voxel.h"

#define Epsilon 1.0e-5

/* makes a sphere */


/* the order must be the same as in neighbor calculation */
static int connectionPoints[12][3] = {
  {-1,-1, 0}, {-1,+1, 0}, {+1,-1, 0}, {+1,+1, 0},
  {-1, 0,-1}, {-1, 0,+1}, {+1, 0,-1}, {+1, 0,+1},
  { 0,-1,-1}, { 0,-1,+1}, { 0,+1,-1}, { 0,+1,+1}
};

static int holeTouchPoints[12][6][3] = {

  {
    {-1+0, -1-1, 0-1}, // 0+8
    {-1-1, -1+0, 0-1}, // 0+4
    {-1-1, -1+0, 0},
    {-1-1, -1+0, 0+1}, // 0+5
    {-1+0, -1-1, 0+1}, // 0+9
    {-1+0, -1-1, 0}
  },{
    {-1+0, +1+1, 0+1}, // 1+11
    {-1-1, +1+0, 0+1}, // 1+5
    {-1-1, +1+0, 0},
    {-1-1, +1+0, 0-1}, // 1+4
    {-1+0, +1+1, 0-1}, // 1+10
    {-1+0, +1+1, 0}
  },{
    {+1+1, -1+0, 0+1}, // 2+7
    {+1+0, -1-1, 0+1}, // 2+9
    {+1+0, -1-1, 0},
    {+1+0, -1-1, 0-1}, // 2+8
    {+1+1, -1+0, 0-1}, // 2+6
    {+1+1, -1+0, 0}
  },{
    {+1+1, +1+0, 0-1}, // 3+6
    {+1+0, +1+1, 0-1}, // 3+10
    {+1+0, +1+1, 0},
    {+1+0, +1+1, 0+1}, // 3+11
    {+1+1, +1+0, 0+1}, // 3+7
    {+1+1, +1+0, 0}
  },{
    {-1+0, 0-1, -1-1}, // 4+8
    {-1-1, 0-1, -1+0}, // 4+0
    {-1-1, 0, -1+0},
    {-1-1, 0+1, -1+0}, // 4+1
    {-1+0, 0+1, -1-1}, // 4+10
    {-1+0, 0, -1-1}
  },{
    {-1-1, 0-1, +1+0}, // 5+0
    {-1+0, 0-1, +1+1}, // 5+9
    {-1+0, 0, +1+1},
    {-1+0, 0+1, +1+1}, // 5+11
    {-1-1, 0+1, +1+0}, // 5+1
    {-1-1, 0, +1+0}
  },{
    {+1+1, 0-1, -1+0}, // 6+2
    {+1+0, 0-1, -1-1}, // 6+8
    {+1+0, 0, -1-1},
    {+1+0, 0+1, -1-1}, // 6+10
    {+1+1, 0+1, -1+0}, // 6+3
    {+1+1, 0, -1+0}
  },{
    {+1+1, 0-1, +1+0}, // 7+2
    {+1+0, 0-1, +1+1}, // 7+9
    {+1+0, 0, +1+1},
    {+1+0, 0+1, +1+1}, // 7+11
    {+1+1, 0+1, +1+0}, // 7+3
    {+1+1, 0, +1+0}
  },{
    {0-1, -1+0, -1-1}, // 8+4
    {0-1, -1-1, -1+0}, // 8+0
    {0, -1-1, -1+0},
    {0+1, -1-1, -1+0}, // 8+2
    {0+1, -1+0, -1-1}, // 8+6
    {0, -1+0, -1-1}
  },{
    {0+1, -1-1, +1+0}, // 9+2
    {0+1, -1+0, +1+1}, // 9+7
    {0, -1+0, +1+1},
    {0-1, -1+0, +1+1}, // 9+5
    {0-1, -1-1, +1+0}, // 9+0
    {0, -1-1, +1+0}
  },{
    {0+1, +1+0, -1-1}, // 10+6
    {0+1, +1+1, -1+0}, // 10+3
    {0, +1+1, -1+0},
    {0-1, +1+1, -1+0}, // 10+1
    {0-1, +1+0, -1-1}, // 10+4
    {0, +1+0, -1-1}
  },{
    {0-1, +1+1, +1+0}, // 11+1
    {0-1, +1+0, +1+1}, // 11+5
    {0, +1+0, +1+1},
    {0+1, +1+0, +1+1}, // 11+7
    {0+1, +1+1, +1+0}, // 11+3
    {0, +1+1, +1+0}
  }
};

static int trianglePoints[8][3][3+1] = {
  {
    {+1+1, +1+0, 0+1,  7}, // 3+7
    {+1+0, 0+1, +1+1, 11}, // 7+11
    {0+1, +1+1, +1+0,  3} // 11+3
  },{
    {+1+1, +1+0, 0-1,  6}, // 3+6
    {+1+0, 0+1, -1-1, 10}, // 6+10
    {0+1, +1+1, -1+0,  3} // 10+3
  },{
    {+1+1, 0-1, -1+0,  2}, // 6+2
    {+1+0, -1-1, 0-1,  8}, // 2+8
    {0+1, -1+0, -1-1,  6} // 8+6
  },{
    {0-1, +1+0, -1-1,  4}, // 10+4
    {-1-1, 0+1, -1+0,  1}, // 4+1
    {-1+0, +1+1, 0-1, 10} // 1+10
  },{
    {-1+0, 0-1, -1-1,  8}, // 4+8
    {0-1, -1-1, -1+0,  0}, // 8+0
    {-1-1, -1+0, 0-1,  4} // 0+4
  },{
    {-1+0, +1+1, 0+1, 11}, // 1+11
    {0-1, +1+0, +1+1,  5}, // 11+5
    {-1-1, 0+1, +1+0,  1} // 5+1
  },{
    {-1+0, -1-1, 0+1,  9}, // 0+9
    {0-1, -1+0, +1+1,  5}, // 9+5
    {-1-1, 0-1, +1+0,  0} // 5+0
  },{
    {+1+1, -1+0, 0+1,  7}, // 2+7
    {+1+0, 0-1, +1+1,  9}, // 7+9
    {0+1, -1-1, +1+0,  2} // 9+2
  }
};

static int squarePoints[6][8][4] = {

  {
    {0, +1+0, -1-1,   10},
    {0+1, +1+0, -1-1}, // 10+6
    {+1+0, 0, -1-1,    6},
    {+1+0, 0-1, -1-1}, // 6+8
    {0, -1+0, -1-1,    8},
    {0-1, -1+0, -1-1}, // 8+4
    {-1+0, 0, -1-1,    4},
    {-1+0, 0+1, -1-1}, // 4+10
  },{
    {-1-1, 0, -1+0,    4},
    {-1-1, 0-1, -1+0}, // 4+0
    {-1-1, -1+0, 0,    0},
    {-1-1, -1+0, 0+1}, // 0+5
    {-1-1, 0, +1+0,    5},
    {-1-1, 0+1, +1+0}, // 5+1
    {-1-1, +1+0, 0,    1},
    {-1-1, +1+0, 0-1}, // 1+4
  },{
    {+1+0, -1-1, 0,    2},
    {+1+0, -1-1, 0+1}, // 2+9
    {0, -1-1, +1+0,    9},
    {0-1, -1-1, +1+0}, // 9+0
    {-1+0, -1-1, 0,    0},
    {-1+0, -1-1, 0-1}, // 0+8
    {0, -1-1, -1+0,    8},
    {0+1, -1-1, -1+0}, // 8+2
  },{
    {+1+1, +1+0, 0,    3},
    {+1+1, +1+0, 0-1}, // 3+6
    {+1+1, 0, -1+0,    6},
    {+1+1, 0-1, -1+0}, // 6+2
    {+1+1, -1+0, 0,    2},
    {+1+1, -1+0, 0+1}, // 2+7
    {+1+1, 0, +1+0,    7},
    {+1+1, 0+1, +1+0}, // 7+3
  },{
    {0, +1+0, +1+1,   11},
    {0-1, +1+0, +1+1}, // 11+5
    {-1+0, 0, +1+1,    5},
    {-1+0, 0-1, +1+1}, // 5+9
    {0, -1+0, +1+1,    9},
    {0+1, -1+0, +1+1}, // 9+7
    {+1+0, 0, +1+1,    7},
    {+1+0, 0+1, +1+1}, // 7+11
  },{
    {0, +1+1, +1+0,   11},
    {0-1, +1+1, +1+0}, // 11+1
    {-1+0, +1+1, 0,    1},
    {-1+0, +1+1, 0-1}, // 1+10
    {0, +1+1, -1+0,   10},
    {0+1, +1+1, -1+0}, // 10+3
    {+1+0, +1+1, 0,    3},
    {+1+0, +1+1, 0+1}, // 3+11
  }
};


/* the given point is shifted away from the given touching
 * point until it is on the 60 degree circle around that
 * touching point
 */
static void shiftToHoleBorder(int nr, double *x, double *y, double *z) {

  double l = sqrt(*x * *x + *y * *y + *z * *z);

  double a = acos((
        connectionPoints[nr][0] * *x +
        connectionPoints[nr][1] * *y +
        connectionPoints[nr][2] * *z) / (l * sqrt(2)));

  double b = 105*M_PI/180 - a;

  double xl = sin(75*M_PI/180) / sin(b);

  *x *= xl/l;
  *y *= xl/l;
  *z *= xl/l;

  *x -= connectionPoints[nr][0]/sqrt(2);
  *y -= connectionPoints[nr][1]/sqrt(2);
  *z -= connectionPoints[nr][2]/sqrt(2);

  l = sqrt(*x * *x + *y * *y + *z * *z);

  *x /= l;
  *y /= l;
  *z /= l;

  *x *= sin(30*M_PI/180)/sin(75*M_PI/180);
  *y *= sin(30*M_PI/180)/sin(75*M_PI/180);
  *z *= sin(30*M_PI/180)/sin(75*M_PI/180);

  *x += connectionPoints[nr][0]/sqrt(2);
  *y += connectionPoints[nr][1]/sqrt(2);
  *z += connectionPoints[nr][2]/sqrt(2);
}

void stlExporter_2_c::drawTriangle(
    int x, int y, int z,
    double x1, double y1, double z1,
    double x2, double y2, double z2,
    double x3, double y3, double z3,
    int edge12, int edge23, int edge31, int rec) {

  double l;

  l = sqrt(x1*x1+y1*y1+z1*z1); x1 /= l; y1 /= l; z1 /= l;
  l = sqrt(x2*x2+y2*y2+z2*z2); x2 /= l; y2 /= l; z2 /= l;
  l = sqrt(x3*x3+y3*y3+z3*z3); x3 /= l; y3 /= l; z3 /= l;

  if (rec > 0) {

    double x12 = (x1+x2)/2;
    double y12 = (y1+y2)/2;
    double z12 = (z1+z2)/2;

    double x23 = (x2+x3)/2;
    double y23 = (y2+y3)/2;
    double z23 = (z2+z3)/2;

    double x31 = (x3+x1)/2;
    double y31 = (y3+y1)/2;
    double z31 = (z3+z1)/2;

    if (edge12 >= 0) shiftToHoleBorder(edge12, &x12, &y12, &z12);
    if (edge23 >= 0) shiftToHoleBorder(edge23, &x23, &y23, &z23);
    if (edge31 >= 0) shiftToHoleBorder(edge31, &x31, &y31, &z31);

    drawTriangle(x, y, z, x1, y1, z1, x12, y12, z12, x31, y31, z31, edge12, -1, edge31, rec-1);
    drawTriangle(x, y, z, x2, y2, z2, x23, y23, z23, x12, y12, z12, edge23, -1, edge12, rec-1);
    drawTriangle(x, y, z, x3, y3, z3, x31, y31, z31, x23, y23, z23, edge31, -1, edge23, rec-1);
    drawTriangle(x, y, z, x12, y12, z12, x23, y23, z23, x31, y31, z31, -1, -1, -1, rec-1);

  } else {

    double xc = 2*sphere_rad*(x+2)*sqrt(0.5);
    double yc = 2*sphere_rad*(y+2)*sqrt(0.5);
    double zc = 2*sphere_rad*(z+2)*sqrt(0.5);

    x1 *= (sphere_rad-offset);
    y1 *= (sphere_rad-offset);
    z1 *= (sphere_rad-offset);

    x2 *= (sphere_rad-offset);
    y2 *= (sphere_rad-offset);
    z2 *= (sphere_rad-offset);

    x3 *= (sphere_rad-offset);
    y3 *= (sphere_rad-offset);
    z3 *= (sphere_rad-offset);

    outTriangle(xc+x1, yc+y1, zc+z1, xc+x2, yc+y2, zc+z2, xc+x3, yc+y3, zc+z3, xc, yc, zc);
  }
}

static double radius(double a, double cr, double sphere_rad, double offset, double curvX, double curvY, double curvRad) {

  /* this is a bit like raytracing of the situation transformed to 2d
   *   we are calculating 3 segments that construct the shape of the hole piece:
   *     - line where 2 spheres tough
   *     - circle of the curvature
   *     - arc of the circle of the sphere
   *   and then we intersect and find out on which piece we are
   *   and calculate the distance...
   */

  double m = tan(90*M_PI/180-a);

  double linex = cr*(sphere_rad-offset)/2;
  double liney = m*linex;

  double circlex = (sphere_rad-offset)*sin(a);
  double circley = (sphere_rad-offset)*cos(a);

  double curvex, curvey;

  double ap = 1+m*m;
  double bp = -2*curvX-2*m*curvY;
  double cp = curvX*curvX+curvY*curvY-curvRad*curvRad;

  if (fabs(ap) < Epsilon) {

    curvex = curvey = 1000000;


  } else {

    double p = bp/ap;
    double q = cp/ap;

    if (p*p/4-q >= 0) {

      curvex = -p/2-sqrt(p*p/4-q);
      curvey = m*curvex;

    } else {

      curvex = curvey = 1000000;

    }
  }

  double px = linex;
  double py = liney;

  if (liney < curvey && curvey < 10000) {
    px = curvex;
    py = curvey;
  }

  if (m*curvX < curvY) {
    px = circlex;
    py = circley;
  }

  return sqrt(px*px + py*py);
}

static void findPointOnArc(double xs, double ys, double zs, double xe, double ye, double ze, double arc,
    double *x, double *y, double *z ) {

  double ls = sqrt(xs*xs+ys*ys+zs*zs);
  double le = sqrt(xe*xe+ye*ye+ze*ze);

  double a = (xs*xe+ys*ye+zs*ze)/(ls*le);

  if (arc >= a) {

    *x = xe;
    *y = ye;
    *z = ze;

    return;
  }

  double ws = 0;
  double we = 1;

  while (fabs(ws-we) > Epsilon*Epsilon) {

    double w = (ws+we)/2;

    *x = (1-w)*xs+w*xe;
    *y = (1-w)*ys+w*ye;
    *z = (1-w)*zs+w*ze;

    double l = sqrt(*x * *x + *y * *y + *z * *z);
    a = acos((*x * xs + *y * ys + *z * zs)/(l*ls));

    if (arc < a) {
      we = w;
    } else {
      ws = w;
    }
  }
}

void stlExporter_2_c::drawHolePiece(int i, int x, int y, int z,
    double start, double end,
    double x1, double y1, double z1,
    double x2, double y2, double z2,
    int rec) {

  if (rec > 0 && fabs(start-end) > Epsilon) {

    drawHolePiece(i, x, y, z, start, (start+end)/2, x1, y1, z1, x2, y2, z2, rec-1);
    drawHolePiece(i, x, y, z, (start+end)/2, end, x1, y1, z1, x2, y2, z2, rec-1);

  } else {

    double xc = 2*sphere_rad*(x+2)*sqrt(0.5);
    double yc = 2*sphere_rad*(y+2)*sqrt(0.5);
    double zc = 2*sphere_rad*(z+2)*sqrt(0.5);

    double x1s, y1s, z1s, x2s, y2s, z2s, x1e, y1e, z1e, x2e, y2e, z2e;

    findPointOnArc(connectionPoints[i][0], connectionPoints[i][1], connectionPoints[i][2], x1, y1, z1, start, &x1s, &y1s, &z1s);
    findPointOnArc(connectionPoints[i][0], connectionPoints[i][1], connectionPoints[i][2], x2, y2, z2, start, &x2s, &y2s, &z2s);
    findPointOnArc(connectionPoints[i][0], connectionPoints[i][1], connectionPoints[i][2], x1, y1, z1, end, &x1e, &y1e, &z1e);
    findPointOnArc(connectionPoints[i][0], connectionPoints[i][1], connectionPoints[i][2], x2, y2, z2, end, &x2e, &y2e, &z2e);

    double rs = radius(start, connection_rad, sphere_rad, offset, curvX, curvY, curvRad);
    double re = radius(end, connection_rad, sphere_rad, offset, curvX, curvY, curvRad);

    double l;
    l = sqrt(x1s*x1s+y1s*y1s+z1s*z1s); x1s *= rs/l; y1s *= rs/l; z1s *= rs/l;
    l = sqrt(x2s*x2s+y2s*y2s+z2s*z2s); x2s *= rs/l; y2s *= rs/l; z2s *= rs/l;

    l = sqrt(x1e*x1e+y1e*y1e+z1e*z1e); x1e *= re/l; y1e *= re/l; z1e *= re/l;
    l = sqrt(x2e*x2e+y2e*y2e+z2e*z2e); x2e *= re/l; y2e *= re/l; z2e *= re/l;

    outTriangle(xc+x1s, yc+y1s, zc+z1s, xc+x2s, yc+y2s, zc+z2s, xc+x2e, yc+y2e, zc+z2e, xc, yc, zc);
    outTriangle(xc+x1s, yc+y1s, zc+z1s, xc+x1e, yc+y1e, zc+z1e, xc+x2e, yc+y2e, zc+z2e, xc, yc, zc);
  }
}

void stlExporter_2_c::drawHole(
    int x, int y, int z, int i,
    double x1, double y1, double z1,
    double x2, double y2, double z2, int rec, int rec2) {

  if (rec > 0) {
    double l;

    l = sqrt(x1*x1+y1*y1+z1*z1); x1 /= l; y1 /= l; z1 /= l;
    l = sqrt(x2*x2+y2*y2+z2*z2); x2 /= l; y2 /= l; z2 /= l;

    double px = (x1+x2)/2;
    double py = (y1+y2)/2;
    double pz = (z1+z2)/2;

    shiftToHoleBorder(i, &px, &py, &pz);

    drawHole(x, y, z, i, x1, y1, z1, px, py, pz, rec-1, rec2);
    drawHole(x, y, z, i, px, py, pz, x2, y2, z2, rec-1, rec2);

  } else {

    double holeStart = atan((connection_rad*(sphere_rad-offset)/2)/sphere_rad);
    double lineEnd = M_PI/2-atan2(curvY, curvX-curvRad);
    double curvEnd = M_PI/2-atan2(curvY, curvX);

    if (lineEnd < holeStart) lineEnd = holeStart;
    if (curvEnd < lineEnd) curvEnd = lineEnd;

    if (holeStart < lineEnd)
      drawHolePiece(i, x, y, z, holeStart, lineEnd, x1, y1, z1, x2, y2, z2, 0);
    if (lineEnd < curvEnd)
      drawHolePiece(i, x, y, z, lineEnd, curvEnd, x1, y1, z1, x2, y2, z2, rec2);
    if (curvEnd < 30*M_PI/180)
      drawHolePiece(i, x, y, z, curvEnd, 30*M_PI/180, x1, y1, z1, x2, y2, z2, rec2);
  }

}

void stlExporter_2_c::makeSphere(int x, int y, int z, uint16_t neighbors) {

  /* first make the holes, or hole caps */

  for (int i = 0; i < 12; i++) {

    double p1x = connectionPoints[i][0];
    double p1y = connectionPoints[i][1];
    double p1z = connectionPoints[i][2];

    double l = sqrt(p1x*p1x+p1y*p1y+p1z*p1z); p1x /= l; p1y /= l; p1z /= l;

    /* close the hole using 6 triangles */

    for (int t = 0; t < 6; t++) {

      double p2x = holeTouchPoints[i][(t+0)%6][0];
      double p2y = holeTouchPoints[i][(t+0)%6][1];
      double p2z = holeTouchPoints[i][(t+0)%6][2];

      double p3x = holeTouchPoints[i][(t+1)%6][0];
      double p3y = holeTouchPoints[i][(t+1)%6][1];
      double p3z = holeTouchPoints[i][(t+1)%6][2];

      shiftToHoleBorder(i, &p2x, &p2y, &p2z);
      shiftToHoleBorder(i, &p3x, &p3y, &p3z);

      if (neighbors & (1<<i)) {

        /* make a proper hole */
        drawHole(x, y, z, i, p2x, p2y, p2z, p3x, p3y, p3z, (int)recursion, (int)recursion);

      } else {

        /* close the hole using 6 triangles */
        drawTriangle(x, y, z, p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, -1, i, -1, (int)recursion);
      }
    }
  }

  /* fill the 8 triangular gaps */
  for (int i = 0; i < 8; i++) {
    double p1x = trianglePoints[i][0][0];
    double p1y = trianglePoints[i][0][1];
    double p1z = trianglePoints[i][0][2];

    double p2x = trianglePoints[i][1][0];
    double p2y = trianglePoints[i][1][1];
    double p2z = trianglePoints[i][1][2];

    double p3x = trianglePoints[i][2][0];
    double p3y = trianglePoints[i][2][1];
    double p3z = trianglePoints[i][2][2];

    shiftToHoleBorder(trianglePoints[i][0][3], &p1x, &p1y, &p1z);
    shiftToHoleBorder(trianglePoints[i][1][3], &p2x, &p2y, &p2z);
    shiftToHoleBorder(trianglePoints[i][2][3], &p3x, &p3y, &p3z);

    drawTriangle(x, y, z, p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z,
        trianglePoints[i][0][3],
        trianglePoints[i][1][3],
        trianglePoints[i][2][3],
        (int)recursion);
  }

  /* finally the 6 square gaps */
  for (int i = 0; i < 6; i++) {

    /* first fill the corners */
    for (int k = 0; k < 4; k++) {
      double p1x = squarePoints[i][(2*k+0)%8][0];
      double p1y = squarePoints[i][(2*k+0)%8][1];
      double p1z = squarePoints[i][(2*k+0)%8][2];

      double p2x = squarePoints[i][(2*k+1)%8][0];
      double p2y = squarePoints[i][(2*k+1)%8][1];
      double p2z = squarePoints[i][(2*k+1)%8][2];

      double p3x = squarePoints[i][(2*k+2)%8][0];
      double p3y = squarePoints[i][(2*k+2)%8][1];
      double p3z = squarePoints[i][(2*k+2)%8][2];

      shiftToHoleBorder(squarePoints[i][(2*k+0)%8][3], &p1x, &p1y, &p1z);
      shiftToHoleBorder(squarePoints[i][(2*k+0)%8][3], &p2x, &p2y, &p2z);
      shiftToHoleBorder(squarePoints[i][(2*k+2)%8][3], &p3x, &p3y, &p3z);

      drawTriangle(x, y, z, p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z,
          squarePoints[i][(2*k+0)%8][3],
          squarePoints[i][(2*k+2)%8][3],
          -1,
          (int)recursion);
    }

    /* then the center square */
    for (int k = 0; k < 2; k++) {
      double p1x = squarePoints[i][(4*k+0)%8][0];
      double p1y = squarePoints[i][(4*k+0)%8][1];
      double p1z = squarePoints[i][(4*k+0)%8][2];

      double p2x = squarePoints[i][(4*k+2)%8][0];
      double p2y = squarePoints[i][(4*k+2)%8][1];
      double p2z = squarePoints[i][(4*k+2)%8][2];

      double p3x = squarePoints[i][(4*k+4)%8][0];
      double p3y = squarePoints[i][(4*k+4)%8][1];
      double p3z = squarePoints[i][(4*k+4)%8][2];

      shiftToHoleBorder(squarePoints[i][(4*k+0)%8][3], &p1x, &p1y, &p1z);
      shiftToHoleBorder(squarePoints[i][(4*k+2)%8][3], &p2x, &p2y, &p2z);
      shiftToHoleBorder(squarePoints[i][(4*k+4)%8][3], &p3x, &p3y, &p3z);

      drawTriangle(x, y, z, p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z,
          -1, -1, -1, (int)recursion);
    }
  }
}

static bool curvOk(double curvrad, double cnrad, double sprad, double offset) {
  double curvx = cnrad*(sprad-offset)/2+curvrad;
  double curvy = sqrt((sprad-offset+curvrad)*(sprad-offset+curvrad)-curvx*curvx);

  if (curvy > sprad) return false;
  if (curvy/curvx < tan(60*M_PI/180)) return false;

  return true;
}

void stlExporter_2_c::write(const char * fname, voxel_c * v) {

  if (v->countState(voxel_c::VX_VARIABLE)) throw new stlException_c("Shapes with variable voxels cannot be exported");
  if (sphere_rad < Epsilon) throw new stlException_c("Sphere size too small");
  if (offset < 0) throw new stlException_c("Offset cannot be negative");
  if (round < 0) throw new stlException_c("Curvature radius cannot be negative");
  if (offset > sphere_rad) throw new stlException_c("Offset must be smaller than sphere radius");
  if (round > 1) throw new stlException_c("The curvature radius is relative and must be between 0 and 1");
  if (connection_rad < 0 || connection_rad > 1)
    throw new stlException_c("The connection radius is relative and must be between 0 and 1");

  int cost = (int)ceilf(v->countState(voxel_c::VX_FILLED) * sphere_rad*sphere_rad*sphere_rad*M_PI*4/3 / 1000.0);

  char name[1000];
  snprintf(name, 1000, "%s_%03i", fname, cost);
  open(name);

  double maxcurv = 10;
  double maxcurv2 = 0;

  while (curvOk(maxcurv, connection_rad, sphere_rad, offset)) {
    maxcurv2 = maxcurv;
    maxcurv *= 2;
  }

  while (fabs(maxcurv2-maxcurv) > Epsilon) {

    if (curvOk((maxcurv+maxcurv2)/2, connection_rad, sphere_rad, offset)) {
      maxcurv2 = (maxcurv+maxcurv2)/2;
    } else {
      maxcurv = (maxcurv+maxcurv2)/2;
    }
  }

  curvRad = maxcurv * round;
  curvX = connection_rad*(sphere_rad - offset)/2 + curvRad;
  curvY = sqrt((sphere_rad-offset+curvRad)*(sphere_rad-offset+curvRad)-curvX*curvX);

  for (unsigned int x = 0; x < v->getX(); x++)
    for (unsigned int y = 0; y < v->getY(); y++)
      for (unsigned int z = 0; z < v->getZ(); z++) {
        if (v->validCoordinate(x, y, z) && !v->isEmpty(x, y, z)) {

          /* collect neighbors for a bitmask */
          uint16_t neighbors = 0;

          int nx, ny, nz;
          int idx = 0;

          while (v->getNeighbor(idx, 0, x, y, z, &nx, &ny, &nz)) {
            if (v->validCoordinate(nx, ny, nz) && !v->isEmpty2(nx, ny, nz))
              neighbors |= 1<<idx;
            idx++;
          }

          makeSphere(x, y, z, neighbors);
        }
      }

  close();
}


const char * stlExporter_2_c::getParameterName(unsigned int idx) const {

  switch (idx) {

    case 0: return "Sphere radius";
    case 1: return "Connection radius";
    case 2: return "Curvature radius";
    case 3: return "Offset";
    case 4: return "Recursions";
    default: return 0;
  }
}

double stlExporter_2_c::getParameter(unsigned int idx) const {

  switch (idx) {
    case 0: return sphere_rad;
    case 1: return connection_rad;
    case 2: return round;
    case 3: return offset;
    case 4: return recursion;
    default: return 0;
  }
}

void stlExporter_2_c::setParameter(unsigned int idx, double value) {

  switch (idx) {
    case 0: sphere_rad = value; return;
    case 1: connection_rad = value; return;
    case 2: round = value; return;
    case 3: offset = value; return;
    case 4: recursion = value; return;
    default: return;
  }
}

