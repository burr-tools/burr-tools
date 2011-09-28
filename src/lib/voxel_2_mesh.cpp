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

#include "puzzle.h"

#include "../halfedge/polyhedron.h"

#include "math.h"

/* this file contains the mesh generation part of voxel_2. This is so big for the spheres
 * that I didn't want to put it into the normal file
 */

#define Epsilon 1.0e-6

/* makes a sphere */

/* These are the points where the spheres touch (called touch point),
 * corresponding to the faces of a rhombic dodecahedron.
 * The order must be the same as in neighbor calculation.
 */
static int connectionPoints[12][3] = {
  {-1,-1, 0}, {-1,+1, 0}, {+1,-1, 0}, {+1,+1, 0},
  {-1, 0,-1}, {-1, 0,+1}, {+1, 0,-1}, {+1, 0,+1},
  { 0,-1,-1}, { 0,-1,+1}, { 0,+1,-1}, { 0,+1,+1}
};

/* Six points surrounding each touch point that
 * correspond (roughly) to the connecting cylinder.
 * they are all on a circle around the touch point but not equally space, only roughly equally
 *
 * As the touch point surrounding circles do have their maximal radius the circles touch, that
 * is where those point given below are.
 *
 * We do not really give the real point but only a vector that is a direction seen from the centre
 * of the sphere, when you scale that vector to the radius of the sphere you will end up at the points
 *
 * As those points are exactly at the middle between 2 touching points all we need to do is take the average
 * of the corresponding 2 touching points, the comment behind a line shows which 2 touching points are taken
 *
 * For the remaining 2 of the 6 points the other point we take the main axis to do the averaging with, this
 * is not really on the circle but only in the direction where we want the point to be. It works anyway because
 * before using a point we shift coordinates until they are on the circle
 *
 * if you cut the sphere in half so that a touching point is on the cutting plane and then connect centre of
 * the sphere with the 2 points on the circle of the cut sphere where the touching circle lies you will get
 * an equilateral triangle, to the angle of the circle is 60 degree
 */
static int holeTouchPoints[12][6][3] = {
  {
    {-1+0, -1-1, 0},
    {-1+0, -1-1, 0+1}, // 0+9
    {-1-1, -1+0, 0+1}, // 0+5
    {-1-1, -1+0, 0},
    {-1-1, -1+0, 0-1}, // 0+4
    {-1+0, -1-1, 0-1}, // 0+8
  },{
    {-1+0, +1+1, 0},
    {-1+0, +1+1, 0-1}, // 1+10
    {-1-1, +1+0, 0-1}, // 1+4
    {-1-1, +1+0, 0},
    {-1-1, +1+0, 0+1}, // 1+5
    {-1+0, +1+1, 0+1}, // 1+11
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
    {+1+1, 0, +1+0},
    {+1+1, 0+1, +1+0}, // 7+3
    {+1+0, 0+1, +1+1}, // 7+11
    {+1+0, 0, +1+1},
    {+1+0, 0-1, +1+1}, // 7+9
    {+1+1, 0-1, +1+0} // 7+2
  },{
    {0, -1+0, -1-1},
    {0+1, -1+0, -1-1}, // 8+6
    {0+1, -1-1, -1+0}, // 8+2
    {0, -1-1, -1+0},
    {0-1, -1-1, -1+0}, // 8+0
    {0-1, -1+0, -1-1}, // 8+4
  },{
    {0+1, -1-1, +1+0}, // 9+2
    {0+1, -1+0, +1+1}, // 9+7
    {0, -1+0, +1+1},
    {0-1, -1+0, +1+1}, // 9+5
    {0-1, -1-1, +1+0}, // 9+0
    {0, -1-1, +1+0}
  },{
    {0, +1+0, -1-1},
    {0-1, +1+0, -1-1}, // 10+4
    {0-1, +1+1, -1+0}, // 10+1
    {0, +1+1, -1+0},
    {0+1, +1+1, -1+0}, // 10+3
    {0+1, +1+0, -1-1} // 10+6
  },{
    {0-1, +1+1, +1+0}, // 11+1
    {0-1, +1+0, +1+1}, // 11+5
    {0, +1+0, +1+1},
    {0+1, +1+0, +1+1}, // 11+7
    {0+1, +1+1, +1+0}, // 11+3
    {0, +1+1, +1+0},
  }
};

/* These correspond to 3 triangles around the eight obtuse
 * vertices of a rhombic dodecahedron (3 faces meet).
 *
 * we have the point defined as above for the circles and one additional value
 * that defines, what is the touching point that lies on the edge towards the next point in the list
 * (or the first point for the last entry)
 *
 * important to note is that the point must be in counter clockwise order, because we need
 * to generate all triangles in counter clockwise order so that we know where the normal points to
 */
static int trianglePoints[8][3][3+1] = {
  {
    {+1+0, 0+1, +1+1,  7}, // 7+11
    {+1+1, +1+0, 0+1,  3}, // 3+7
    {0+1, +1+1, +1+0, 11} // 11+3
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
    {0-1, +1+0, +1+1, 11}, // 11+5
    {-1+0, +1+1, 0+1,  1}, // 1+11
    {-1-1, 0+1, +1+0,  5} // 5+1
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

/* These correspond to 6 triangles around the six acute
 * vertices of a rhombic dodecahedron (4 faces meet).
 * Two of these triangles form a square, the other four
 * share an edge with each side of the square.
 * The shape of all 6 triangles form a star-shape.
 *
 * otherwise the same notes apply as for the trianglePoints array above
 */
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
    {+1+1, 0+1, +1+0}, // 7+3
    {+1+1, 0, +1+0,    7},
    {+1+1, -1+0, 0+1}, // 2+7
    {+1+1, -1+0, 0,    2},
    {+1+1, 0-1, -1+0}, // 6+2
    {+1+1, 0, -1+0,    6},
    {+1+1, +1+0, 0-1}, // 3+6
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
    {+1+0, +1+1, 0+1}, // 3+11
    {+1+0, +1+1, 0,    3},
    {0+1, +1+1, -1+0}, // 10+3
    {0, +1+1, -1+0,   10},
    {-1+0, +1+1, 0-1}, // 1+10
    {-1+0, +1+1, 0,    1},
    {0-1, +1+1, +1+0}, // 11+1
  }
};

// this structure contains all parameters that are used for
// the mesh generation, there are so many of them that it became
// unhandy to deliver them all as parameters to the different
// functions, instead we will just hand around a (const where possible) reference
typedef struct
{
  float sphere_rad;    // Radius of the sphere (outer layer)
  float inner_rad;     // Radius of the sphere (inner layer, radius of the hole)
  float offset;        // offset by which the sphere radiuses are made smaller (subtraction)
  bool outside;        // is currently drawn sphere the outer or inner sphere
  float hole_diam;     // diameter of the hole between inside and outside > 0 round hole, <0 square hole
  float connection_rad;// radius of the connection between spheres (1 = maximal possible radius)
  float xc, yc, zc;    // centre of the current sphere

  // some internal values used to calculate the curve that makes up the transition between
  // the sphere surface and the connection cylinder
  float curvX;
  float curvY;
  float curvRad;
  float holeStart;
  float lineEnd;
  float curvEnd;

  // the vertex list, we add the triangles to
  vertexList_c * vl;

  // the flags to use for the triangles
  int flags;
  int fb_index;
  int fb_face;
  int color;

} genPar;


// add the triangle to the vertexList (and thus to the polyhedron)
//
static void outTriangle(
        /* the 3 vertexes of the triangle */
        float x1, float y1, float z1,
        float x2, float y2, float z2,
        float x3, float y3, float z3,
        genPar & par)
{

  // when outputting the vertices, we round the values to
  // a multiple of 1/256. This will help us properly align vertices up to a very high degree
  // and the power of 2 factor will make exact division possible for the floats
  const float roundfac = 256;

  x1 = roundf((par.xc+x1)*roundfac)/roundfac;
  y1 = roundf((par.yc+y1)*roundfac)/roundfac;
  z1 = roundf((par.zc+z1)*roundfac)/roundfac;

  x2 = roundf((par.xc+x2)*roundfac)/roundfac;
  y2 = roundf((par.yc+y2)*roundfac)/roundfac;
  z2 = roundf((par.zc+z2)*roundfac)/roundfac;

  x3 = roundf((par.xc+x3)*roundfac)/roundfac;
  y3 = roundf((par.yc+y3)*roundfac)/roundfac;
  z3 = roundf((par.zc+z3)*roundfac)/roundfac;

  std::vector<int> face3(3);
  face3[0] = par.vl->get(x1, y1, z1);
  face3[1] = par.vl->get(x2, y2, z2);
  face3[2] = par.vl->get(x3, y3, z3);

  // don't add degenerate triangles
  if (face3[0] == face3[1] || face3[0] == face3[2] || face3[1] == face3[2])
    return;

  Face * f = par.vl->addFace(face3);

  f->_flags = par.flags;
  f->_fb_index = par.fb_index;
  f->_fb_face = par.fb_face;
  f->_color = par.color;
}

// normalise a vector
static void normalize(float *x, float *y, float *z)
{
  float l = sqrt(*x * *x + *y * *y + *z * *z);
  *x /= l;
  *y /= l;
  *z /= l;
}

/* The given point is shifted away from the given touching
 * point until it is on the 60 degree circle around that
 * touching point.
 * This gives the maximum size cylinder that can be used to
 * connect the spheres. Such cylinders touch at the sphere
 * surface, and have a diameter equal to the radius of the sphere.
 */
static void shiftToHoleBorder(int nr, float *x, float *y, float *z)
{
  float l = sqrt(*x * *x + *y * *y + *z * *z);

  /* ok some names
   * M is the middle of the sphere (which is at the origin btw.)
   * T is out touching point
   * P is the input point
   */

  // calculate the angle at PMT
  float a = acos((
        connectionPoints[nr][0] * *x +
        connectionPoints[nr][1] * *y +
        connectionPoints[nr][2] * *z) / (l * sqrt(2)));

  // what the heck, I don't understand any more what I did in here... bahh next time
  // I will need to comment it as soon as it works
  float b = 105*M_PI/180 - a;

  float xl = sin(75*M_PI/180) / sin(b);

  *x *= xl/l;
  *y *= xl/l;
  *z *= xl/l;

  *x -= connectionPoints[nr][0]/sqrt(2);
  *y -= connectionPoints[nr][1]/sqrt(2);
  *z -= connectionPoints[nr][2]/sqrt(2);

  normalize(x, y, z);

  *x *= sin(30*M_PI/180)/sin(75*M_PI/180);
  *y *= sin(30*M_PI/180)/sin(75*M_PI/180);
  *z *= sin(30*M_PI/180)/sin(75*M_PI/180);

  *x += connectionPoints[nr][0]/sqrt(2);
  *y += connectionPoints[nr][1]/sqrt(2);
  *z += connectionPoints[nr][2]/sqrt(2);
}

// recursively draw a spherical triangle on the perimeter of of
// a sphere, additionally it is possible to shift the edges of the
// triangle to that they are on the circle around the touching
// points of the sphere
static void drawTriangle(
    // direction vectors for the 3 corners, the length is not important
    // as the points will eventually be on the sphere
    float x1, float y1, float z1,
    float x2, float y2, float z2,
    float x3, float y3, float z3,
    // which touching circle to use for which edge, if negative no connection circle is used
    // the recursion level tells how many times the triangle is subdivided multiplying
    // the number of triangles by 4 each time
    int edge12, int edge23, int edge31, int rec,
    genPar & par)
{
  // make the vectors unit length
  normalize(&x1, &y1, &z1);
  normalize(&x2, &y2, &z2);
  normalize(&x3, &y3, &z3);

  if (rec > 0)
  {
    // find the middle of the 3 triangle sides
    float x12 = (x1+x2)/2;
    float y12 = (y1+y2)/2;
    float z12 = (z1+z2)/2;

    float x23 = (x2+x3)/2;
    float y23 = (y2+y3)/2;
    float z23 = (z2+z3)/2;

    float x31 = (x3+x1)/2;
    float y31 = (y3+y1)/2;
    float z31 = (z3+z1)/2;

    // shift to the connection circles
    if (edge12 >= 0) shiftToHoleBorder(edge12, &x12, &y12, &z12);
    if (edge23 >= 0) shiftToHoleBorder(edge23, &x23, &y23, &z23);
    if (edge31 >= 0) shiftToHoleBorder(edge31, &x31, &y31, &z31);

    // recursively output the triangles, we need to keep the edge flags only for
    // the sides of the triangles that are still on the outside
    drawTriangle(x1, y1, z1, x12, y12, z12, x31, y31, z31, edge12, -1, edge31, rec-1, par);
    drawTriangle(x2, y2, z2, x23, y23, z23, x12, y12, z12, edge23, -1, edge12, rec-1, par);
    drawTriangle(x3, y3, z3, x31, y31, z31, x23, y23, z23, edge31, -1, edge23, rec-1, par);
    drawTriangle(x12, y12, z12, x23, y23, z23, x31, y31, z31, -1, -1, -1, rec-1, par);
  }
  else
  {

    // final output, first find out the radius of
    // the sphere we are on
    float mult;

    if (par.outside)
      mult = par.sphere_rad-par.offset;
    else
      mult = par.inner_rad-par.offset;

    // scale the triangle point onto the sphere
    x1 *= mult;
    y1 *= mult;
    z1 *= mult;

    x2 *= mult;
    y2 *= mult;
    z2 *= mult;

    x3 *= mult;
    y3 *= mult;
    z3 *= mult;

    // if not outside reverse the order of points to flip surface
    if (par.outside)
      outTriangle(x1, y1, z1, x2, y2, z2, x3, y3, z3, par);
    else
      outTriangle(x1, y1, z1, x3, y3, z3, x2, y2, z2, par);
  }
}


/* when drawing a connection this function calculates
 * the radius (meaning how far is the surface away from the centre of the sphere
 * for a certain angle from the touching point
 *
 * this value is the sphere radius for the outer rim and will increase as the
 * angle gets smaller and we get closer to the point, where the cylinders touch
 * inside of the cylinder the value is not defined
 */
static float radius(float a, genPar & par)
{
  /* this is a bit like raytracing of the situation transformed to 2d
   *   we are calculating 3 segments that construct the shape of the hole piece:
   *     - line where 2 spheres tough
   *     - circle of the curvature
   *     - arc of the circle of the sphere
   *   and then we intersect and find out on which piece we are
   *   and calculate the distance...
   */

  float m = tan(90*M_PI/180-a);

  float linex = par.connection_rad*(par.sphere_rad-par.offset)/2;
  float liney = m*linex;

  float circlex = (par.sphere_rad-par.offset)*sin(a);
  float circley = (par.sphere_rad-par.offset)*cos(a);

  float curvex, curvey;

  float ap = 1+m*m;
  float bp = -2*par.curvX-2*m*par.curvY;
  float cp = par.curvX*par.curvX+par.curvY*par.curvY-par.curvRad*par.curvRad;

  if (fabs(ap) < Epsilon)
  {
    curvex = curvey = 1000000;
  }
  else
  {
    float p = bp/ap;
    float q = cp/ap;

    if (p*p/4-q >= 0)
    {
      curvex = -p/2-sqrt(p*p/4-q);
      curvey = m*curvex;
    }
    else
    {
      curvex = curvey = 1000000;
    }
  }

  float px = linex;
  float py = liney;

  if (liney < curvey && curvey < 10000)
  {
    px = curvex;
    py = curvey;
  }

  if (m*par.curvX < par.curvY)
  {
    px = circlex;
    py = circley;
  }

  return sqrt(px*px + py*py);
}


static void findPointOnArc(float xs, float ys, float zs, float xe, float ye, float ze, float arc,
    float *x, float *y, float *z )
{
  float ls = sqrt(xs*xs+ys*ys+zs*zs);
  float le = sqrt(xe*xe+ye*ye+ze*ze);

  float a = (xs*xe+ys*ye+zs*ze)/(ls*le);

  if (arc >= a)
  {
    *x = xe;
    *y = ye;
    *z = ze;

    return;
  }

  float ws = 0;
  float we = 1;

  while (fabs(ws-we) > Epsilon)
  {
    float w = (ws+we)/2;

    *x = (1-w)*xs+w*xe;
    *y = (1-w)*ys+w*ye;
    *z = (1-w)*zs+w*ze;

    float l = sqrt(*x * *x + *y * *y + *z * *z);
    a = acos((*x * xs + *y * ys + *z * zs)/(l*ls));

    if (arc < a)
    {
      we = w;
    }
    else
    {
      ws = w;
    }
  }
}

static void drawHolePiece(
    int i,
    float start, float end,
    float x1, float y1, float z1,
    float x2, float y2, float z2,
    int rec,
    genPar & par)
{
  if (rec > 0 && fabs(start-end) > Epsilon)
  {
    drawHolePiece(i, start, (start+end)/2, x1, y1, z1, x2, y2, z2, rec-1, par);
    drawHolePiece(i, (start+end)/2, end, x1, y1, z1, x2, y2, z2, rec-1, par);
  }
  else
  {
    float x1s, y1s, z1s, x2s, y2s, z2s, x1e, y1e, z1e, x2e, y2e, z2e;

    findPointOnArc(connectionPoints[i][0], connectionPoints[i][1], connectionPoints[i][2], x1, y1, z1, start, &x1s, &y1s, &z1s);
    findPointOnArc(connectionPoints[i][0], connectionPoints[i][1], connectionPoints[i][2], x2, y2, z2, start, &x2s, &y2s, &z2s);
    findPointOnArc(connectionPoints[i][0], connectionPoints[i][1], connectionPoints[i][2], x1, y1, z1, end, &x1e, &y1e, &z1e);
    findPointOnArc(connectionPoints[i][0], connectionPoints[i][1], connectionPoints[i][2], x2, y2, z2, end, &x2e, &y2e, &z2e);

    float rs = radius(start, par);
    float re = radius(end, par);

    float l;
    l = sqrt(x1s*x1s+y1s*y1s+z1s*z1s); x1s *= rs/l; y1s *= rs/l; z1s *= rs/l;
    l = sqrt(x2s*x2s+y2s*y2s+z2s*z2s); x2s *= rs/l; y2s *= rs/l; z2s *= rs/l;

    l = sqrt(x1e*x1e+y1e*y1e+z1e*z1e); x1e *= re/l; y1e *= re/l; z1e *= re/l;
    l = sqrt(x2e*x2e+y2e*y2e+z2e*z2e); x2e *= re/l; y2e *= re/l; z2e *= re/l;

    outTriangle(x2s, y2s, z2s, x1s, y1s, z1s, x2e, y2e, z2e, par);
    outTriangle(x1s, y1s, z1s, x1e, y1e, z1e, x2e, y2e, z2e, par);
  }
}

/* generate the connection between 2 spheres
 * this connection starts on the sphere surface and slowly (via a rounding circle)
 * warps into the connection cylinder
 *
 * the function really only draws a pie shaped piece of the connection that
 * may be recursively split into smaller pie shaped pieces
 */
static void drawHole(
    /* which touching point */
    int i,
    /* the 2 corners of the pie shaped piece where the hole is placed in between */
    float x1, float y1, float z1, float x2, float y2, float z2,
    /* number of recursion levels for this pie division, and number of recursion levels for
     * when the pie piece is drawn
     */
    int rec, int rec2,
    genPar & par)
{
  if (rec > 0)
  {
    // find the middle between the 2 points
    normalize(&x1, &y1, &z1);
    normalize(&x2, &y2, &z2);

    float px = (x1+x2)/2;
    float py = (y1+y2)/2;
    float pz = (z1+z2)/2;

    // shift that point to the edge of the touching circle
    shiftToHoleBorder(i, &px, &py, &pz);

    // draw the 2 pies to the left and the right
    drawHole(i, x1, y1, z1, px, py, pz, rec-1, rec2, par);
    drawHole(i, px, py, pz, x2, y2, z2, rec-1, rec2, par);
  }
  else
  {
    // Outermost section of the connection (farthest from centre)
    if (par.holeStart < par.lineEnd)
      drawHolePiece(i, par.holeStart, par.lineEnd, x1, y1, z1, x2, y2, z2, 0, par);

    // Curvature transition
    if (par.lineEnd < par.curvEnd)
      drawHolePiece(i, par.lineEnd, par.curvEnd, x1, y1, z1, x2, y2, z2, rec2, par);

    // Innermost, transition to the connector
    if (par.curvEnd < 30*M_PI/180)
      drawHolePiece(i, par.curvEnd, 30*M_PI/180, x1, y1, z1, x2, y2, z2, rec2, par);
  }
}


/* Shift the given point along the line connecting (x, y, z) to (px, py, pz) until
 * it is a distance rad from this point (px, py, pz).
 */
static void shiftToConnectingHole(float px, float py, float pz, float rad,
    float *x, float *y, float *z)
{
  float diff = sqrt((px-*x)*(px-*x)+(py-*y)*(py-*y)+(pz-*z)*(pz-*z));

  if (diff < Epsilon) return;

  float s = 1.0 - rad/diff;

  *x = *x + s*(px-*x);
  *y = *y + s*(py-*y);
  *z = *z + s*(pz-*z);

  normalize(x, y, z);
}

/* Draw the connecting faces between the inside and outside of the sphere.
 * Simply draw two triangles between two pairs of inside and outside points.
 */
static void drawConnectingTriangle(
    float x1, float y1, float z1,
    float x2, float y2, float z2,
    genPar & par)
{
  normalize(&x1, &y1, &z1);
  normalize(&x2, &y2, &z2);

  // calculate the points that are on the outer sphere
  float x3 = x2*(par.sphere_rad-par.offset);
  float y3 = y2*(par.sphere_rad-par.offset);
  float z3 = z2*(par.sphere_rad-par.offset);

  float x4 = x1*(par.sphere_rad-par.offset);
  float y4 = y1*(par.sphere_rad-par.offset);
  float z4 = z1*(par.sphere_rad-par.offset);

  // the points for the inner sphere
  float mult = par.inner_rad-par.offset;

  x1 *= mult;
  y1 *= mult;
  z1 *= mult;

  x2 *= mult;
  y2 *= mult;
  z2 *= mult;

  outTriangle(x1, y1, z1, x3, y3, z3, x2, y2, z2, par);
  outTriangle(x1, y1, z1, x4, y4, z4, x3, y3, z3, par);
}

/* The pattern of squares by variable name (x1,y1,z1), (x5,y5,z5), etc.
   1  5  2
   6  7  8
   3  9  4

   edgeflag & 1 means this square includes the top edge
   edgeflag & 2 means this square includes the bottom edge
*/
static void drawConnectingHole(
    float hx, float hy, float hz,
    float x1, float y1, float z1, float x2, float y2, float z2,
    float x3, float y3, float z3, float x4, float y4, float z4,
    int edgeflag, int rec,
    genPar & par)
{
  normalize(&x1, &y1, &z1);
  normalize(&x2, &y2, &z2);
  normalize(&x3, &y3, &z3);
  normalize(&x4, &y4, &z4);

  if (rec > 0)
  {
    float x9 = (x3 + x4)/2;
    float y9 = (y3 + y4)/2;
    float z9 = (z3 + z4)/2;

    // Do these next lines to make round (circular) holes
    // otherwise they will be square.
    if (edgeflag & 2 && par.hole_diam > 0)
    {
      float connectingHoleRadius = 0.5 * par.hole_diam / par.sphere_rad;
      shiftToConnectingHole(hx, hy, hz, connectingHoleRadius, &x9, &y9, &z9);
    }

    float x5 = (x1 + x2)/2;
    float y5 = (y1 + y2)/2;
    float z5 = (z1 + z2)/2;

    float x6 = (x1 + x3)/2;
    float y6 = (y1 + y3)/2;
    float z6 = (z1 + z3)/2;

    float x8 = (x2 + x4)/2;
    float y8 = (y2 + y4)/2;
    float z8 = (z2 + z4)/2;

    float x7 = (x5 + x9)/2;
    float y7 = (y5 + y9)/2;
    float z7 = (z5 + z9)/2;

    drawConnectingHole(hx, hy, hz,
      x1, y1, z1, x5, y5, z5, x6, y6, z6, x7, y7, z7, edgeflag & 1, rec-1, par);
    drawConnectingHole(hx, hy, hz,
      x5, y5, z5, x2, y2, z2, x7, y7, z7, x8, y8, z8, edgeflag & 1, rec-1, par);
    drawConnectingHole(hx, hy, hz,
      x6, y6, z6, x7, y7, z7, x3, y3, z3, x9, y9, z9, edgeflag & 2, rec-1, par);
    drawConnectingHole(hx, hy, hz,
      x7, y7, z7, x8, y8, z8, x9, y9, z9, x4, y4, z4, edgeflag & 2, rec-1, par);

  }
  else
  {
    float diag23 = sqrt((x2-x3)*(x2-x3)+(y2-y3)*(y2-y3)+(z2-z3)*(z2-z3));
    float diag14 = sqrt((x1-x4)*(x1-x4)+(y1-y4)*(y1-y4)+(z1-z4)*(z1-z4));

    if (diag23 < diag14)
    {
      drawTriangle(x1, y1, z1, x2, y2, z2, x3, y3, z3, 0, 0, 0, 0, par);
      drawTriangle(x2, y2, z2, x4, y4, z4, x3, y3, z3, 0, 0, 0, 0, par);
    }
    else
    {
      drawTriangle(x1, y1, z1, x2, y2, z2, x4, y4, z4, 0, 0, 0, 0, par);
      drawTriangle(x1, y1, z1, x4, y4, z4, x3, y3, z3, 0, 0, 0, 0, par);
    }

    if ((edgeflag & 2) && par.outside)
    {
      drawConnectingTriangle(x3, y3, z3, x4, y4, z4, par);
    }
  }
}

/* create one sphere.
 * neighbours shows, where we need connections to the neighbours (each bit set means that
 * there must be a connection cylinder, no bit set close the sphere) the bits are in
 * the same order as the neighbours or connection points
 *
 * hollow specifies, whether the sphere is supposed to be hollow
 * variable, shows, whether the sphere is supposed to be a variable sphere (use the bevel face as variable marker)
 */
static void makeSphere(uint16_t neighbors, int recursion, bool hollow, bool variable, genPar & par)
{
  /* first make the holes, or hole caps */
  for (int i = 0; i < 12; i++)
  {
    float p1x = connectionPoints[i][0];
    float p1y = connectionPoints[i][1];
    float p1z = connectionPoints[i][2];

    normalize(&p1x, &p1y, &p1z);

    /* close the hole using 6 triangles */

    for (int t = 0; t < 6; t++)
    {
      float p2x = holeTouchPoints[i][(t+0)%6][0];
      float p2y = holeTouchPoints[i][(t+0)%6][1];
      float p2z = holeTouchPoints[i][(t+0)%6][2];

      float p3x = holeTouchPoints[i][(t+1)%6][0];
      float p3y = holeTouchPoints[i][(t+1)%6][1];
      float p3z = holeTouchPoints[i][(t+1)%6][2];

      shiftToHoleBorder(i, &p2x, &p2y, &p2z);
      shiftToHoleBorder(i, &p3x, &p3y, &p3z);

      if ((neighbors & (1<<i)) && par.outside && (par.connection_rad > Epsilon))
      {
        /* make a proper hole (connection to next sphere) */
        par.flags = FF_COLOR_LIGHT;
        drawHole(i, p2x, p2y, p2z, p3x, p3y, p3z, (int)recursion, (int)recursion, par);
      }
      else
      {
        /* simply close the sphere with a sphere surface section
         * those faces are the sensitive faces for appending another sphere
         * they are drawn a bit darker
         */
        par.fb_face = i;
        par.flags = 0;
        drawTriangle(p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, -1, i, -1, (int)recursion, par);
      }
    } // end loop over t
  } // end loop over i

  // when the sphere is a variable sphere, we use the variable face marker for the filling faces, this will end up with
  // black faces, otherwise we use the light marker to get lighter faces than the clickable faces
  if (variable)
    par.flags = FF_VARIABLE_FACE;
  else
    par.flags = FF_COLOR_LIGHT;

  // following faces are supposed to stay, when in wire frame mode
  par.flags |= FF_WIREFRAME;

  /* fill the 8 triangular gaps */
  for (int i = 0; i < 8; i++)
  {
    float p1x = trianglePoints[i][0][0];
    float p1y = trianglePoints[i][0][1];
    float p1z = trianglePoints[i][0][2];

    float p2x = trianglePoints[i][1][0];
    float p2y = trianglePoints[i][1][1];
    float p2z = trianglePoints[i][1][2];

    float p3x = trianglePoints[i][2][0];
    float p3y = trianglePoints[i][2][1];
    float p3z = trianglePoints[i][2][2];

    shiftToHoleBorder(trianglePoints[i][0][3], &p1x, &p1y, &p1z);
    shiftToHoleBorder(trianglePoints[i][1][3], &p2x, &p2y, &p2z);
    shiftToHoleBorder(trianglePoints[i][2][3], &p3x, &p3y, &p3z);

    drawTriangle(p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z,
        trianglePoints[i][0][3],
        trianglePoints[i][1][3],
        trianglePoints[i][2][3],
        (int)recursion, par);
  }

  /* finally the 6 square gaps */
  for (int i = 0; i < 6; i++)
  {
    /* first fill the corners */
    for (int k = 0; k < 4; k++) {
      float p1x = squarePoints[i][(2*k+0)%8][0];
      float p1y = squarePoints[i][(2*k+0)%8][1];
      float p1z = squarePoints[i][(2*k+0)%8][2];

      float p2x = squarePoints[i][(2*k+1)%8][0];
      float p2y = squarePoints[i][(2*k+1)%8][1];
      float p2z = squarePoints[i][(2*k+1)%8][2];

      float p3x = squarePoints[i][(2*k+2)%8][0];
      float p3y = squarePoints[i][(2*k+2)%8][1];
      float p3z = squarePoints[i][(2*k+2)%8][2];

      shiftToHoleBorder(squarePoints[i][(2*k+0)%8][3], &p1x, &p1y, &p1z);
      shiftToHoleBorder(squarePoints[i][(2*k+0)%8][3], &p2x, &p2y, &p2z);
      shiftToHoleBorder(squarePoints[i][(2*k+2)%8][3], &p3x, &p3y, &p3z);

      drawTriangle(p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z,
          squarePoints[i][(2*k+0)%8][3],
          squarePoints[i][(2*k+2)%8][3],
          -1,
          (int)recursion, par);
    }

    /* Then the centre square */
    if (!hollow || fabs(par.hole_diam) < Epsilon)
    { // not hollow or no holes
      for (int k = 0; k < 2; k++)
      {
        float p1x = squarePoints[i][(4*k+0)%8][0];
        float p1y = squarePoints[i][(4*k+0)%8][1];
        float p1z = squarePoints[i][(4*k+0)%8][2];

        float p2x = squarePoints[i][(4*k+2)%8][0];
        float p2y = squarePoints[i][(4*k+2)%8][1];
        float p2z = squarePoints[i][(4*k+2)%8][2];

        float p3x = squarePoints[i][(4*k+4)%8][0];
        float p3y = squarePoints[i][(4*k+4)%8][1];
        float p3z = squarePoints[i][(4*k+4)%8][2];

        shiftToHoleBorder(squarePoints[i][(4*k+0)%8][3], &p1x, &p1y, &p1z);
        shiftToHoleBorder(squarePoints[i][(4*k+2)%8][3], &p2x, &p2y, &p2z);
        shiftToHoleBorder(squarePoints[i][(4*k+4)%8][3], &p3x, &p3y, &p3z);

        drawTriangle(p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z,
            -1, -1, -1, (int)recursion, par);
      }
    }
    else
    { //hollow with holes

      // hx,hy,hz, location of the centre of the hole into the interior
      float hx = 0.125*(squarePoints[i][0][0] + squarePoints[i][2][0] +
                      squarePoints[i][4][0] + squarePoints[i][6][0]);
      float hy = 0.125*(squarePoints[i][0][1] + squarePoints[i][2][1] +
                      squarePoints[i][4][1] + squarePoints[i][6][1]);
      float hz = 0.125*(squarePoints[i][0][2] + squarePoints[i][2][2] +
                      squarePoints[i][4][2] + squarePoints[i][6][2]);

      /* Extend the inner square to the hole */
      for (int k=0; k < 4; k++)
      { //Once per side of the square
        float p1x = squarePoints[i][(2*k+0)%8][0];
        float p1y = squarePoints[i][(2*k+0)%8][1];
        float p1z = squarePoints[i][(2*k+0)%8][2];
        shiftToHoleBorder(squarePoints[i][(2*k+0)%8][3], &p1x, &p1y, &p1z);

        float p2x = squarePoints[i][(2*k+2)%8][0];
        float p2y = squarePoints[i][(2*k+2)%8][1];
        float p2z = squarePoints[i][(2*k+2)%8][2];
        shiftToHoleBorder(squarePoints[i][(2*k+2)%8][3], &p2x, &p2y, &p2z);

        float p3x = p1x;
        float p3y = p1y;
        float p3z = p1z;

        float p4x = p2x;
        float p4y = p2y;
        float p4z = p2z;

        float connectingHoleRadius = 0.5 / par.sphere_rad;

        // connectingHoleRadius is a diagonal measurement for square holes.
        // Thus, enlarge them by sqrt(2) for a side measurement.
        if (par.hole_diam < 0)
          connectingHoleRadius *= - (sqrt(2.0) * par.hole_diam);
        else
          connectingHoleRadius *= par.hole_diam;

        shiftToConnectingHole(hx, hy, hz, connectingHoleRadius, &p3x, &p3y, &p3z);
        shiftToConnectingHole(hx, hy, hz, connectingHoleRadius, &p4x, &p4y, &p4z);

        drawConnectingHole(hx, hy, hz, p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z, 3, (int)recursion, par);
      }
    } // End else
  } // End of loop over i
}

static bool curvOk(float curvrad, float cnrad, float sprad, float offset)
{
  float curvx = cnrad*(sprad-offset)/2+curvrad;
  float curvy = sqrt((sprad-offset+curvrad)*(sprad-offset+curvrad)-curvx*curvx);

  if (curvy > sprad) return false;
  if (curvy/curvx < tan(60*M_PI/180)) return false;

  return true;
}

Polyhedron * voxel_2_c::getMeshInternal(float sphere_rad, float connection_rad, float round, float offset, int recursion, float inner_rad, float hole_diam, bool fast) const
{
  Polyhedron * poly = new Polyhedron;
  vertexList_c vl(poly);

  float maxcurv = 10;
  float maxcurv2 = 0;

  while (curvOk(maxcurv, connection_rad, sphere_rad, offset))
  {
    maxcurv2 = maxcurv;
    maxcurv *= 2;
  }

  while (fabs(maxcurv2-maxcurv) > Epsilon)
  {
    if (curvOk((maxcurv+maxcurv2)/2, connection_rad, sphere_rad, offset))
    {
      maxcurv2 = (maxcurv+maxcurv2)/2;
    }
    else
    {
      maxcurv = (maxcurv+maxcurv2)/2;
    }
  }

  genPar par;

  par.sphere_rad = sphere_rad;
  par.inner_rad = inner_rad;
  par.offset = offset;
  par.hole_diam = hole_diam;
  par.connection_rad = connection_rad;
  par.curvRad = maxcurv * round;
  par.curvX = connection_rad*(sphere_rad - offset)/2 + par.curvRad;
  par.curvY = sqrt((sphere_rad-offset+par.curvRad)*(sphere_rad-offset+par.curvRad)-par.curvX*par.curvX);
  par.vl = &vl;
  par.holeStart = atan((par.connection_rad*(par.sphere_rad-par.offset)/2)/par.sphere_rad);
  par.lineEnd = M_PI/2-atan2(par.curvY, par.curvX-par.curvRad);
  par.curvEnd = M_PI/2-atan2(par.curvY, par.curvX);

  if (par.lineEnd < par.holeStart) par.lineEnd = par.holeStart;
  if (par.curvEnd < par.lineEnd) par.curvEnd = par.lineEnd;

  for (unsigned int x = 0; x < getX(); x++)
    for (unsigned int y = 0; y < getY(); y++)
      for (unsigned int z = 0; z < getZ(); z++) {
        if (validCoordinate(x, y, z) && !isEmpty(x, y, z))
        {
          /* collect neighbours for a bitmask */
          uint16_t neighbors = 0;

          int nx, ny, nz;
          int idx = 0;

          while (getNeighbor(idx, 0, x, y, z, &nx, &ny, &nz))
          {
            if (validCoordinate(nx, ny, nz) && !isEmpty2(nx, ny, nz))
              neighbors |= 1<<idx;
            idx++;
          }

          bool hollow = false;
          if (inner_rad > Epsilon) hollow = true;

          par.fb_index = getIndex(x, y, z);
          bool variable = isVariable(x, y, z);
          par.color = getColor(x, y, z);

          par.xc = 2*sphere_rad*(x)*sqrt(0.5)+sphere_rad;
          par.yc = 2*sphere_rad*(y)*sqrt(0.5)+sphere_rad;
          par.zc = 2*sphere_rad*(z)*sqrt(0.5)+sphere_rad;

          // Draw the outside of the sphere.
          par.outside = true;
          makeSphere(neighbors, recursion, hollow, variable, par);

          // In the sphere is hollow, draw the inside of the sphere.
          // The connection between the inner and outer halves is done with the outside.
          if (hollow)
          {
            par.outside = false;
            par.fb_index = 0;
            par.flags = false;
            makeSphere(neighbors, recursion, hollow, variable, par);
          }
        }
      }

  if (!fast)
  {
    poly->finalize();
  }

  return poly;
}

Polyhedron * voxel_2_c::getMesh(float sphere_rad, float connection_rad, float round, float offset, int recursion, float inner_rad, float hole_diam) const
{
  return getMeshInternal(
      sphere_rad, connection_rad, round, offset, recursion, inner_rad, hole_diam, false);
}

Polyhedron * voxel_2_c::getDrawingMesh(void) const
{
  return getMeshInternal(
      0.5,  // sphere radius
      0.7,  // connection radius
      1,    // round
      0.01, // offset
      1,    // recursion
      0,    // inner_rad
      0,    // hole_diam
      true  // fast generation
      );
}

Polyhedron * voxel_2_c::getWireframeMesh(void) const
{
  return getDrawingMesh();
}

void voxel_2_c::getConnectionFace(int x, int y, int z, int n, double /*bevel*/, double /*offset*/, std::vector<float> & faceCorners) const
{
  static const float A = sqrt(0.5);
  static const float B = sqrt(0.125);

  /* array of
   * - of the 12 neighbours
   * - of the 4 points of a rhombus
   * - x, y, z
   */
  static const float faces[12][4][3] =
  {
    /* neighbour at -1, -1,  0 */ { {0, -A, 0}, {-B, -B,  B}, {-A, 0, 0}, {-B, -B, -B} },
    /* neighbour at -1,  1,  0 */ { {0,  A, 0}, {-B,  B, -B}, {-A, 0, 0}, {-B,  B,  B} },
    /* neighbour at  1, -1,  0 */ { {0, -A, 0}, { B, -B, -B}, { A, 0, 0}, { B, -B,  B} },
    /* neighbour at  1,  1,  0 */ { {0,  A, 0}, { B,  B,  B}, { A, 0, 0}, { B,  B, -B} },

    /* neighbour at -1,  0, -1 */ { {-A, 0, 0}, {-B,  B, -B}, {0, 0, -A}, {-B, -B, -B} },
    /* neighbour at -1,  0,  1 */ { {-A, 0, 0}, {-B, -B,  B}, {0, 0,  A}, {-B,  B,  B} },
    /* neighbour at  1,  0, -1 */ { { A, 0, 0}, { B, -B, -B}, {0, 0, -A}, { B,  B, -B} },
    /* neighbour at  1,  0,  1 */ { { A, 0, 0}, { B,  B,  B}, {0, 0,  A}, { B, -B,  B} },

    /* neighbour at  0, -1, -1 */ { {0, 0, -A}, { B, -B, -B}, {0, -A, 0}, {-B, -B, -B} },
    /* neighbour at  0, -1,  1 */ { {0, 0,  A}, {-B, -B,  B}, {0, -A, 0}, { B, -B,  B} },
    /* neighbour at  0,  1, -1 */ { {0, 0, -A}, {-B,  B, -B}, {0,  A, 0}, { B,  B, -B} },
    /* neighbour at  0,  1,  1 */ { {0, 0,  A}, { B,  B,  B}, {0,  A, 0}, {-B,  B,  B} },
  };

  bt_assert(n < 12);

  float xc = x*sqrt(0.5)+0.5;
  float yc = y*sqrt(0.5)+0.5;
  float zc = z*sqrt(0.5)+0.5;

  faceCorners.push_back(xc+faces[n][0][0]);
  faceCorners.push_back(yc+faces[n][0][1]);
  faceCorners.push_back(zc+faces[n][0][2]);

  faceCorners.push_back(xc+faces[n][1][0]);
  faceCorners.push_back(yc+faces[n][1][1]);
  faceCorners.push_back(zc+faces[n][1][2]);

  faceCorners.push_back(xc+faces[n][2][0]);
  faceCorners.push_back(yc+faces[n][2][1]);
  faceCorners.push_back(zc+faces[n][2][2]);

  faceCorners.push_back(xc+faces[n][3][0]);
  faceCorners.push_back(yc+faces[n][3][1]);
  faceCorners.push_back(zc+faces[n][3][2]);
}

void voxel_2_c::calculateSize(float * x, float * y, float * z) const
{
  *x = 1 + (getX()-1)*sqrt(0.5);
  *y = 1 + (getY()-1)*sqrt(0.5);
  *z = 1 + (getZ()-1)*sqrt(0.5);
}

void voxel_2_c::recalcSpaceCoordinates(float * x, float * y, float * z) const
{
  *x = *x * sqrt(0.5);
  *y = *y * sqrt(0.5);
  *z = *z * sqrt(0.5);
}

