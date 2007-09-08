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

#include "voxeldrawer_2.h"

#include "piececolor.h"

#include "../lib/voxel.h"

#include <math.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

// this is used to shift one side of the cubes so that they slightly differ
// from the side of the next cube, so that (in case of frames) the sides
// are clearly separated and don't interlock when drawing
#define MY 0.005f


#define SPHERE_COORDINATES 8
static int vi[SPHERE_COORDINATES][3][3] = {
  {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
  {{1, 0, 0}, {0, 0, 1}, {0, -1, 0}},
  {{1, 0, 0}, {0, -1, 0}, {0, 0, -1}},
  {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},
  {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
  {{-1, 0, 0}, {0, -1, 0}, {0, 0, 1}},
  {{-1, 0, 0}, {0, 0, -1}, {0, -1, 0}},
  {{-1, 0, 0}, {0, 1, 0}, {0, 0, -1}}};

static int att[12][3] = {
  {-1, -1, 0}, {-1, 1, 0}, {1, -1, 0}, {1, 1, 0},
  {-1, 0, -1}, {-1, 0, 1}, {1, 0, -1}, {1, 0, 1},
  {0, -1, -1}, {0, -1, 1}, {0, 1, -1}, {0, 1, 1}};

int atatchment_check(double &x, double &y, double &z) {

  for (unsigned int i = 0; i < 12; i++) {

    double d = (x-att[i][0])*(x-att[i][0]) + (y-att[i][1])*(y-att[i][1]) + (z-att[i][2])*(z-att[i][2]);

    if (d < 0.20)
      return i;
  }

  return 12;
}


// some functions to draw sections of spheres

// this draws a triangular sections of the sphere with radius 0.5
// given the 3 coordinates of the 3 end points and the recursion level requested
static void draw_sphere_section(double x1, double y1, double z1,
                                double x2, double y2, double z2,
                                double x3, double y3, double z3, int rec) {

  if (rec > 0) {

    double x12 = (x1+x2)/2;
    double y12 = (y1+y2)/2;
    double z12 = (z1+z2)/2;

    double l12 = sqrt(x12*x12 + y12*y12 + z12*z12);
    x12 /= l12;
    y12 /= l12;
    z12 /= l12;

    double x23 = (x2+x3)/2;
    double y23 = (y2+y3)/2;
    double z23 = (z2+z3)/2;

    double l23 = sqrt(x23*x23 + y23*y23 + z23*z23);
    x23 /= l23;
    y23 /= l23;
    z23 /= l23;

    double x31 = (x3+x1)/2;
    double y31 = (y3+y1)/2;
    double z31 = (z3+z1)/2;

    double l31 = sqrt(x31*x31 + y31*y31 + z31*z31);
    x31 /= l31;
    y31 /= l31;
    z31 /= l31;

    draw_sphere_section(x1, y1, z1, x12, y12, z12, x31, y31, z31, rec-1);
    draw_sphere_section(x2, y2, z2, x23, y23, z23, x12, y12, z12, rec-1);
    draw_sphere_section(x3, y3, z3, x31, y31, z31, x23, y23, z23, rec-1);
    draw_sphere_section(x12, y12, z12, x23, y23, z23, x31, y31, z31, rec-1);

  } else {

    /* check, if we are close to a attatchment point */
    int a1 = atatchment_check(x1, y1, z1);
    int a2 = atatchment_check(x2, y2, z2);
    int a3 = atatchment_check(x3, y3, z3);

    /* count number of points in atachment area */
    int act = 0;
    if (a1 != 12) act++;
    if (a2 != 12) act++;
    if (a2 != 12) act++;

    if (act < 1) {

      /* draw the normal triangle with the points */
      glLoadName(12);

      glBegin(GL_TRIANGLES);

      // calculate normal for the triangle

      double v1x = x2-x1; double v1y = y2-y1; double v1z = z2-z1;
      double v2x = x3-x1; double v2y = y3-y1; double v2z = z3-z1;

      double nx = v1y*v2z - v1z*v2y;
      double ny = v1z*v2x - v1x*v2z;
      double nz = v1x*v2y - v1y*v2x;

      double l = sqrt(nx*nx + ny*ny + nz*nz);
      nx /= l; ny /= l; nz /= l;

      glNormal3f(nx, ny, nz);

      glVertex3f(x1, y1, z1);
      glVertex3f(x2, y2, z2);
      glVertex3f(x3, y3, z3);

      glEnd();

    } else {

      int a = 12;

      if (a1 != 12) a = a1;
      if (a2 != 12) a = a2;
      if (a3 != 12) a = a3;

      glLoadName(a);

      glBegin(GL_TRIANGLES);

      glNormal3f(att[a][0], att[a][1], att[a][2]);

      glVertex3f(x1, y1, z1);
      glVertex3f(x2, y2, z2);
      glVertex3f(x3, y3, z3);

      glEnd();
    }
  }
}

// draws a sphere using the sphere section function to draw all
// triangles of a icosahedron
static void draw_sphere(void) {

  for (int i = 0; i < SPHERE_COORDINATES; i++)
    draw_sphere_section(
        vi[i][0][0], vi[i][0][1], vi[i][0][2],
        vi[i][1][0], vi[i][1][1], vi[i][1][2],
        vi[i][2][0], vi[i][2][1], vi[i][2][2],
        3);
}

// draws markers for a variable sphere by taking the faces of a icosahedron and
// moving the corners a bit to the center of the triangle and then
// use the section draw function
static void draw_markers(void) {

  static double vec[SPHERE_COORDINATES][3][3];
  static bool init = false;

  if (!init) {

    // let us calculate the necessary vectors for the triangles
    for (int i = 0; i < SPHERE_COORDINATES; i++) {

      // take the middle of 2 vectors and then move the 3rd vector a bit in that direction

      for (int v = 0; v < 3; v++) {

        double xm = vi[i][(v+2)%3][0] + vi[i][(v+1)%3][0];
        double ym = vi[i][(v+2)%3][1] + vi[i][(v+1)%3][1];
        double zm = vi[i][(v+2)%3][2] + vi[i][(v+1)%3][2];

        double lm = sqrt(xm*xm + ym*ym + zm*zm);
        xm /= lm;
        ym /= lm;
        zm /= lm;

        xm = 5*vi[i][v][0]+xm;
        ym = 5*vi[i][v][1]+ym;
        zm = 5*vi[i][v][2]+zm;

        lm = sqrt(xm*xm + ym*ym + zm*zm);
        xm /= lm;
        ym /= lm;
        zm /= lm;

        vec[i][v][0] = xm;
        vec[i][v][1] = ym;
        vec[i][v][2] = zm;
      }
    }

    init = true;
  }

  for (int i = 0; i < SPHERE_COORDINATES; i++)
    draw_sphere_section(
        vec[i][0][0], vec[i][0][1], vec[i][0][2],
        vec[i][1][0], vec[i][1][1], vec[i][1][2],
        vec[i][2][0], vec[i][2][1], vec[i][2][2],
        3);
}

static void draw_sphere_line(double x1, double y1, double z1, double x2, double y2, double z2, unsigned int rec) {

  if (rec > 0) {

    double x = (x1+x2)/2;
    double y = (y1+y2)/2;
    double z = (z1+z2)/2;

    double l = sqrt(x*x + y*y + z*z);
    x /= l;
    y /= l;
    z /= l;

    draw_sphere_line(x1, y1, z1, x, y, z, rec-1);
    draw_sphere_line(x, y, z, x2, y2, z2, rec-1);

  } else {

    // p2-p1 is the direction vector, p1 and p2 are normal vectors so
    // (p2-p1) x p1 or (p2-p1) x p2 are vectors to make some width to the
    // line

    double xd = x2-x1;
    double yd = y2-y1;
    double zd = z2-z1;

    double xc = yd*z1-zd*y1;
    double yc = zd*x1-xd*z1;
    double zc = xd*y1-yd*x1;

    // normalize to a proper length

    double cl = sqrt(xc*xc + yc*yc + zc*zc);
    cl *= 10;

    xc /= cl;
    yc /= cl;
    zc /= cl;

    glBegin(GL_QUADS);

    glNormal3f(x1, y1, z1);
    glVertex3f(x1-xc, y1-yc, z1-zc);
    glVertex3f(x1+xc, y1+yc, z1+zc);

    glNormal3f(x2, y2, z2);
    glVertex3f(x2+xc, y2+yc, z2+zc);
    glVertex3f(x2-xc, y2-yc, z2-zc);

    glEnd();
  }
}


// draws a wireframe sphere by taking the corners of an icosahedron and
// drawing lines lines along the edges but along the perimeter of the
// sphere
static void draw_wire_sphere(void) {

  draw_sphere_line(1, 0, 0, 0, 1, 0, 3);
  draw_sphere_line(1, 0, 0, 0, 0, 1, 3);
  draw_sphere_line(1, 0, 0, 0, -1, 0, 3);
  draw_sphere_line(1, 0, 0, 0, 0, -1, 3);

  draw_sphere_line(-1, 0, 0, 0, 1, 0, 3);
  draw_sphere_line(-1, 0, 0, 0, 0, 1, 3);
  draw_sphere_line(-1, 0, 0, 0, -1, 0, 3);
  draw_sphere_line(-1, 0, 0, 0, 0, -1, 3);

  draw_sphere_line(0, 1, 0, 0, 0, 1, 3);
  draw_sphere_line(0, 0, 1, 0, -1, 0, 3);
  draw_sphere_line(0, -1, 0, 0, 0, -1, 3);
  draw_sphere_line(0, 0, -1, 0, 1, 0, 3);
}

// draws a wire frame box depending on the neighbours
void voxelDrawer_2_c::drawFrame(const voxel_c * space, int x, int y, int z, float edge) {

  if (((x+y+z) & 1) != 0) return;

  // first calculate center

  float cx = 0.5 + x*sqrt(0.5);
  float cy = 0.5 + y*sqrt(0.5);
  float cz = 0.5 + z*sqrt(0.5);

  glPushMatrix();
  glTranslatef(cx, cy, cz);
  glScalef(0.5, 0.5, 0.5);

  draw_wire_sphere();

  glPopMatrix();
}

// draws a box with borders depending on the neighbour boxes
void voxelDrawer_2_c::drawNormalVoxel(const voxel_c * space, int x, int y, int z, float alpha, float edge) {

  glPushName(12);

  if (((x+y+z) & 1) != 0) return;

  // first calculate center

  float cx = 0.5 + x*sqrt(0.5);
  float cy = 0.5 + y*sqrt(0.5);
  float cz = 0.5 + z*sqrt(0.5);

  glPushMatrix();
  glTranslatef(cx, cy, cz);
  glScalef(0.5, 0.5, 0.5);

  draw_sphere();

  glPopMatrix();

  glPopName();
}

// draw a cube that is smaller than 1
void voxelDrawer_2_c::drawVariableMarkers(const voxel_c * space, int x, int y, int z) {

  glPushName(12);

  if (((x+y+z) & 1) != 0) return;

  // first calculate center

  float cx = 0.5 + x*sqrt(0.5);
  float cy = 0.5 + y*sqrt(0.5);
  float cz = 0.5 + z*sqrt(0.5);

  glPushMatrix();
  glTranslatef(cx, cy, cz);
  glScalef(0.5+MY, 0.5+MY, 0.5+MY);

  draw_markers();

  glPopName();

  glPopMatrix();
}

void voxelDrawer_2_c::drawCursor(const voxel_c * space, unsigned int sx, unsigned int sy, unsigned int sz) {
  // draw the cursor, this is done by iterating over all
  // cubes and checking for the 3 directions (in one direction only as the other
  // direction is done with the next cube), if there is a border in the cursor
  // between these 2 cubes, if so draw the cursor grid
  for (unsigned int x = 0; x <= sx; x++)
    for (unsigned int y = 0; y <= sy; y++)
      for (unsigned int z = 0; z <= sz; z++) {
        bool ins = inRegion(x, y, z, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType);

        if (ins && (((x+y+z) & 1) == 0)) {

          // first calculate center

          float cx = 0.5 + x*sqrt(0.5);
          float cy = 0.5 + y*sqrt(0.5);
          float cz = 0.5 + z*sqrt(0.5);

          glPushMatrix();
          glTranslatef(cx, cy, cz);
          glScalef(0.5+MY, 0.5+MY, 0.5+MY);

          draw_wire_sphere();

          glPopMatrix();
        }
      }
}


void voxelDrawer_2_c::gridTypeChanged(void) {

  GLfloat sx, sy, sz;
  sx = sy = sz = 1;

  GLfloat m[16] = {
    sx, 0, 0, 0,
    0, sy, 0, 0,
    0, 0, sz, 0,
    0, 0, 0,  1 };

  setTransformationMatrix(m);
}

void voxelDrawer_2_c::calculateSize(const voxel_c * shape, float * x, float * y, float * z) {

  *x = 1 + (shape->getX()-1)*sqrt(0.5);
  *y = 1 + (shape->getY()-1)*sqrt(0.5);
  *z = 1 + (shape->getZ()-1)*sqrt(0.5);
}

void voxelDrawer_2_c::recalcSpaceCoordinates(float * x, float * y, float * z) {
  *x = *x * sqrt(0.5);
  *y = *y * sqrt(0.5);
  *z = *z * sqrt(0.5);
}

