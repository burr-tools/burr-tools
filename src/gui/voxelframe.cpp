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
#include "voxelframe.h"
#include "arcball.h"

#include "piececolor.h"
#include "configuration.h"
#include "grideditor.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"
#include "../lib/problem.h"
#include "../lib/assembly.h"
#include "../lib/disasmtomoves.h"
#include "../lib/solution.h"

#include "../halfedge/polyhedron.h"

#include <math.h>

#include <FL/Fl.H>

#ifdef WIN32
#include <GL/glext.h>
#endif

#include "gl2ps.h"

voxelFrame_c::voxelFrame_c(int x,int y,int w,int h) :
  Fl_Gl_Window(x,y,w,h),
  curAssembly(0),
  markerType(-1),
  size(10), cb(0),
  colors(pieceColor),
  _useLightning(true),
  pickx(-1),
  insideVisible(false)
{
  if (config.rotationMethod() == 0)
    rotater = new arcBall_c(w, h);
  else
    rotater = new method2_c(w, h);
  rotMethod = config.rotationMethod();
};

void voxelFrame_c::setRotaterMethod(int method)
{
  if (method == rotMethod) return;

  delete rotater;

  if (method == 0)
    rotater = new arcBall_c(w(), h());
  else
    rotater = new method2_c(w(), h());

  rotMethod = method;
}

voxelFrame_c::~voxelFrame_c(void) {
  clearSpaces();
  if (curAssembly) {
    delete curAssembly;
    curAssembly = 0;
  }
  delete rotater;
}

// this is used to shift one side of the cubes so that they slightly differ
// from the side of the next cube, so that (in case of frames) the sides
// are clearly separated and don't interlock when drawing
#define MY 0.005f

#define SHRINK 0.13

/* draw a triangle, set the normal so that it is orthogonal and pointing away from p the triangle is
 * shrunk so that a new smaller triangle whose sides are 0.2 units from the given one and also
 * moved by MY to the outside along the normal */
static void drawShrinkTriangle(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3)
{

  float v1x = x2-x1;
  float v1y = y2-y1;
  float v1z = z2-z1;
  float v2x = x3-x1;
  float v2y = y3-y1;
  float v2z = z3-z1;

  float nx = v1y*v2z - v1z*v2y;
  float ny = v1z*v2x - v1x*v2z;
  float nz = v1x*v2y - v1y*v2x;

  float l = sqrt(nx*nx+ny*ny+nz*nz);
  nx /= l;
  ny /= l;
  nz /= l;

  float c = sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)+(z2-z1)*(z2-z1));
  float b = sqrt((x3-x1)*(x3-x1)+(y3-y1)*(y3-y1)+(z3-z1)*(z3-z1));
  float a = sqrt((x2-x3)*(x2-x3)+(y2-y3)*(y2-y3)+(z2-z3)*(z2-z3));

  float cx = x1*a/(a+b+c) + x2*b/(a+b+c) + x3*c/(a+b+c);
  float cy = y1*a/(a+b+c) + y2*b/(a+b+c) + y3*c/(a+b+c);
  float cz = z1*a/(a+b+c) + z2*b/(a+b+c) + z3*c/(a+b+c);

  float s = (a+b+c)/2;
  float r = sqrt(((s-a)*(s-b)*(s-c))/s);

  float p = (r-SHRINK)/r;

  glVertex3f(cx+p*(x1-cx)+MY*nx, cy+p*(y1-cy)+MY*ny, cz+p*(z1-cz)+MY*nz);
  glVertex3f(cx+p*(x2-cx)+MY*nx, cy+p*(y2-cy)+MY*ny, cz+p*(z2-cz)+MY*nz);
  glVertex3f(cx+p*(x3-cx)+MY*nx, cy+p*(y3-cy)+MY*ny, cz+p*(z3-cz)+MY*nz);
}

static void drawShrinkQuadrilateral(float x1a, float y1a, float z1a, float x2a, float y2a, float z2a, float x3a, float y3a, float z3a, float x4a, float y4a, float z4a)
{

  float v1x = x2a-x1a;
  float v1y = y2a-y1a;
  float v1z = z2a-z1a;
  float v2x = x3a-x1a;
  float v2y = y3a-y1a;
  float v2z = z3a-z1a;

  float nx = v1y*v2z - v1z*v2y;
  float ny = v1z*v2x - v1x*v2z;
  float nz = v1x*v2y - v1y*v2x;

  float l = sqrt(nx*nx+ny*ny+nz*nz);
  nx /= l;
  ny /= l;
  nz /= l;

  float x1, y1, z1, x2, y2, z2, x3, y3, z3;
  float px1, py1, pz1, px2, py2, pz2, px3, py3, pz3, px4, py4, pz4;

  {
    x1 = x1a; y1 = y1a; z1 = z1a;
    x2 = x2a; y2 = y2a; z2 = z2a;
    x3 = x3a; y3 = y3a; z3 = z3a;

    float c = sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)+(z2-z1)*(z2-z1));
    float b = sqrt((x3-x1)*(x3-x1)+(y3-y1)*(y3-y1)+(z3-z1)*(z3-z1));
    float a = sqrt((x2-x3)*(x2-x3)+(y2-y3)*(y2-y3)+(z2-z3)*(z2-z3));

    float cx = x1*a/(a+b+c) + x2*b/(a+b+c) + x3*c/(a+b+c);
    float cy = y1*a/(a+b+c) + y2*b/(a+b+c) + y3*c/(a+b+c);
    float cz = z1*a/(a+b+c) + z2*b/(a+b+c) + z3*c/(a+b+c);

    float s = (a+b+c)/2;
    float r = sqrt(((s-a)*(s-b)*(s-c))/s);

    float p = (r-SHRINK)/r;

    px2 = cx+p*(x2-cx)+MY*nx;
    py2 = cy+p*(y2-cy)+MY*ny;
    pz2 = cz+p*(z2-cz)+MY*nz;
  }

  {
    x1 = x2a; y1 = y2a; z1 = z2a;
    x2 = x3a; y2 = y3a; z2 = z3a;
    x3 = x4a; y3 = y4a; z3 = z4a;

    float c = sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)+(z2-z1)*(z2-z1));
    float b = sqrt((x3-x1)*(x3-x1)+(y3-y1)*(y3-y1)+(z3-z1)*(z3-z1));
    float a = sqrt((x2-x3)*(x2-x3)+(y2-y3)*(y2-y3)+(z2-z3)*(z2-z3));

    float cx = x1*a/(a+b+c) + x2*b/(a+b+c) + x3*c/(a+b+c);
    float cy = y1*a/(a+b+c) + y2*b/(a+b+c) + y3*c/(a+b+c);
    float cz = z1*a/(a+b+c) + z2*b/(a+b+c) + z3*c/(a+b+c);

    float s = (a+b+c)/2;
    float r = sqrt(((s-a)*(s-b)*(s-c))/s);

    float p = (r-SHRINK)/r;

    px3 = cx+p*(x2-cx)+MY*nx;
    py3 = cy+p*(y2-cy)+MY*ny;
    pz3 = cz+p*(z2-cz)+MY*nz;
  }

  {
    x1 = x3a; y1 = y3a; z1 = z3a;
    x2 = x4a; y2 = y4a; z2 = z4a;
    x3 = x1a; y3 = y1a; z3 = z1a;

    float c = sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)+(z2-z1)*(z2-z1));
    float b = sqrt((x3-x1)*(x3-x1)+(y3-y1)*(y3-y1)+(z3-z1)*(z3-z1));
    float a = sqrt((x2-x3)*(x2-x3)+(y2-y3)*(y2-y3)+(z2-z3)*(z2-z3));

    float cx = x1*a/(a+b+c) + x2*b/(a+b+c) + x3*c/(a+b+c);
    float cy = y1*a/(a+b+c) + y2*b/(a+b+c) + y3*c/(a+b+c);
    float cz = z1*a/(a+b+c) + z2*b/(a+b+c) + z3*c/(a+b+c);

    float s = (a+b+c)/2;
    float r = sqrt(((s-a)*(s-b)*(s-c))/s);

    float p = (r-SHRINK)/r;

    px4 = cx+p*(x2-cx)+MY*nx;
    py4 = cy+p*(y2-cy)+MY*ny;
    pz4 = cz+p*(z2-cz)+MY*nz;
  }

  {
    x1 = x4a; y1 = y4a; z1 = z4a;
    x2 = x1a; y2 = y1a; z2 = z1a;
    x3 = x2a; y3 = y2a; z3 = z2a;

    float c = sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)+(z2-z1)*(z2-z1));
    float b = sqrt((x3-x1)*(x3-x1)+(y3-y1)*(y3-y1)+(z3-z1)*(z3-z1));
    float a = sqrt((x2-x3)*(x2-x3)+(y2-y3)*(y2-y3)+(z2-z3)*(z2-z3));

    float cx = x1*a/(a+b+c) + x2*b/(a+b+c) + x3*c/(a+b+c);
    float cy = y1*a/(a+b+c) + y2*b/(a+b+c) + y3*c/(a+b+c);
    float cz = z1*a/(a+b+c) + z2*b/(a+b+c) + z3*c/(a+b+c);

    float s = (a+b+c)/2;
    float r = sqrt(((s-a)*(s-b)*(s-c))/s);

    float p = (r-SHRINK)/r;

    px1 = cx+p*(x2-cx)+MY*nx;
    py1 = cy+p*(y2-cy)+MY*ny;
    pz1 = cz+p*(z2-cz)+MY*nz;
  }

  glVertex3f(px1, py1, pz1);
  glVertex3f(px2, py2, pz2);
  glVertex3f(px3, py3, pz3);

  glVertex3f(px3, py3, pz3);
  glVertex3f(px4, py4, pz4);
  glVertex3f(px1, py1, pz1);
}

static void drawGridRect(double x0, double y0, double z0,
                     double v1x, double v1y, double v1z,
                     double v2x, double v2y, double v2z, int diag) {

  glBegin(GL_LINES);
  glVertex3f(x0, y0, z0); glVertex3f(x0+v1x, y0+v1y, z0+v1z);
  glVertex3f(x0+v1x, y0+v1y, z0+v1z); glVertex3f(x0+v1x+v2x, y0+v1y+v2y, z0+v1z+v2z);
  glVertex3f(x0+v1x+v2x, y0+v1y+v2y, z0+v1z+v2z); glVertex3f(x0+v2x, y0+v2y, z0+v2z);
  glVertex3f(x0+v2x, y0+v2y, z0+v2z); glVertex3f(x0, y0, z0);

  int state1 = 0;
  int state2 = 0;


  float x1 = x0 + v1x;
  float y1 = y0 + v1y;
  float z1 = z0 + v1z;

  float x2 = x0 + v1x;
  float y2 = y0 + v1y;
  float z2 = z0 + v1z;

  float xe = x0 + v2x;
  float ye = y0 + v2y;
  float ze = z0 + v2z;

  while ((fabs(x1 - xe) > 0.01) || (fabs(y1 - ye) > 0.01) || (fabs(z1 - ze) > 0.01)) {
    // v1=(x1, y1, z1) first goes along vector1 =(v1x, v1y, v1z) and then along vector2

    if (state1 == 0) {
      x1 -= v1x/diag;
      y1 -= v1y/diag;
      z1 -= v1z/diag;

      if (((v1x) && (fabs(x1 - x0) < 0.01)) ||
          ((v1y) && (fabs(y1 - y0) < 0.01)) ||
          ((v1z) && (fabs(z1 - z0) < 0.01))) {
        state1 = 1;
      }
    } else {

      x1 += v2x/diag;
      y1 += v2y/diag;
      z1 += v2z/diag;
    }

    if (state2 == 0) {
      x2 += v2x/diag;
      y2 += v2y/diag;
      z2 += v2z/diag;

      if (((v2x) && (fabs(x2 - (x0+v2x+v1x)) < 0.01)) ||
          ((v2y) && (fabs(y2 - (y0+v2y+v1y)) < 0.01)) ||
          ((v2z) && (fabs(z2 - (z0+v2z+v1z)) < 0.01))) {
        state2 = 1;
      }
    } else {

      x2 -= v1x/diag;
      y2 -= v1y/diag;
      z2 -= v1z/diag;
    }

    glVertex3f(x1, y1, z1); glVertex3f(x2, y2, z2);
  }

  glEnd();
}

static void drawGridTriangle(double x0, double y0, double z0,
                         double v1x, double v1y, double v1z,
                         double v2x, double v2y, double v2z, int diag) {

  glBegin(GL_LINES);
  glVertex3f(x0, y0, z0); glVertex3f(x0+v1x, y0+v1y, z0+v1z);
  glVertex3f(x0+v1x, y0+v1y, z0+v1z); glVertex3f(x0+v2x, y0+v2y, z0+v2z);
  glVertex3f(x0+v2x, y0+v2y, z0+v2z); glVertex3f(x0, y0, z0);

  float x1 = x0;
  float y1 = y0;
  float z1 = z0;

  float x2 = x0;
  float y2 = y0;
  float z2 = z0;

  v1x /= diag;
  v1y /= diag;
  v1z /= diag;

  v2x /= diag;
  v2y /= diag;
  v2z /= diag;

  for (int i = 0; i < diag; i++)
  {
    x1 += v1x;
    y1 += v1y;
    z1 += v1z;

    x2 += v2x;
    y2 += v2y;
    z2 += v2z;

    glVertex3f(x1, y1, z1); glVertex3f(x2, y2, z2);
  }

  glEnd();
}

// this function finds out if a given square is inside the selected region
// this check includes the symmetric and column edit modes
bool inRegion(int x, int y, int z, int x1, int x2, int y1, int y2, int z1, int z2, int sx, int sy, int sz, int mode) {

  if ((x < 0) || (y < 0) || (z < 0) || (x >= sx) || (y >= sy) || (z >= sz)) return false;

  if (mode == 0)
    return (x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2);
  if (mode == gridEditor_c::TOOL_STACK_Y)
    return (x1 <= x) && (x <= x2) && (z1 <= z) && (z <= z2);
  if (mode == gridEditor_c::TOOL_STACK_X)
    return (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2);
  if (mode == gridEditor_c::TOOL_STACK_Z)
    return (x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2);

  if (mode == gridEditor_c::TOOL_STACK_X + gridEditor_c::TOOL_STACK_Y)
    return (((x1 <= x) && (x <= x2)) || ((y1 <= y) && (y <= y2))) && (z1 <= z) && (z <= z2);
  if (mode == gridEditor_c::TOOL_STACK_X + gridEditor_c::TOOL_STACK_Z)
    return (((x1 <= x) && (x <= x2)) || ((z1 <= z) && (z <= z2))) && (y1 <= y) && (y <= y2);
  if (mode == gridEditor_c::TOOL_STACK_Y + gridEditor_c::TOOL_STACK_Z)
    return (((y1 <= y) && (y <= y2)) || ((z1 <= z) && (z <= z2))) && (x1 <= x) && (x <= x2);

  if (mode == gridEditor_c::TOOL_STACK_X + gridEditor_c::TOOL_STACK_Y + gridEditor_c::TOOL_STACK_Z)
    return (((x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2)) ||
            ((x1 <= x) && (x <= x2) && (z1 <= z) && (z <= z2)) ||
            ((y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2)));

  if (mode & gridEditor_c::TOOL_MIRROR_X)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~gridEditor_c::TOOL_MIRROR_X) ||
      inRegion(sx-x-1, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~gridEditor_c::TOOL_MIRROR_X);

  if (mode & gridEditor_c::TOOL_MIRROR_Y)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~gridEditor_c::TOOL_MIRROR_Y) ||
      inRegion(x, sy-y-1, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~gridEditor_c::TOOL_MIRROR_Y);

  if (mode & gridEditor_c::TOOL_MIRROR_Z)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~gridEditor_c::TOOL_MIRROR_Z) ||
      inRegion(x, y, sz-z-1, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~gridEditor_c::TOOL_MIRROR_Z);

  return false;
}





void voxelFrame_c::drawVoxelSpace() {

  glShadeModel(GL_FLAT);

  glPushName(0);

  for (unsigned int run = 0; run < 2; run++) {
    for (unsigned int piece = 0; piece < shapes.size(); piece++) {

      shapeInfo * shape = &shapes[piece];

      if (shape->a == 0)
        continue;
      if (shape->mode == invisible)
        continue;
      if (!shape->shape && !shape->poly)
        continue;

      glLoadName(piece);

      // in run 0 we only paint opaque objects and in run 1 only transparent ones
      // this lets the transparent objects be always in front of the others
      if ((run == 0) && (shape->a != 1)) continue;
      if ((run == 1) && (shape->a == 1)) continue;

      glPushMatrix();

      float hx = 0, hy = 0, hz = 0;

      if (shape->shape)
      {
        hx = shape->shape->getHx();
        hy = shape->shape->getHy();
        hz = shape->shape->getHz();

        shape->shape->recalcSpaceCoordinates(&hx, &hy, &hz);
      }

      switch(trans) {
      case ScaleRotateTranslate:
        glTranslatef(shape->x, shape->y, shape->z);
        glScalef(shape->scale, shape->scale, shape->scale);
        rotater->addTransform();
        if (shape->shape)
        {
          float cx, cy, cz;
          shape->shape->calculateSize(&cx, &cy, &cz);
          glTranslatef(-0.5*cx, -0.5*cy, -0.5*cz);
        }
        break;
      case TranslateRoateScale:
        rotater->addTransform();
        glTranslatef(shape->x, shape->y, shape->z);
        if (shape->shape)
        {
          float cx, cy, cz;
          shape->shape->calculateSize(&cx, &cy, &cz);
          glTranslatef(-0.5*cx, -0.5*cy, -0.5*cz);
        }
        glScalef(shape->scale, shape->scale, shape->scale);
        break;
      case CenterTranslateRoateScale:
        rotater->addTransform();
        glTranslatef(shape->x - hx, shape->y - hy, shape->z - hz);
        glTranslatef(-centerX, -centerY, -centerZ);
        glScalef(shape->scale, shape->scale, shape->scale);
        break;
      default:
        break;
      }

      if (_showCoordinateSystem && shape->shape) {
        if (_useLightning) glDisable(GL_LIGHTING);
        glDisable(GL_BLEND);
        glBegin(GL_LINES);
        float cx, cy, cz;
        shape->shape->calculateSize(&cx, &cy, &cz);

        if (colors == anaglyphColor || colors == anaglyphColorL) {
          glColor3f(0.3, 0.3, 0.3); glVertex3f(-1, -1, -1); glVertex3f(cx+1, -1, -1);
          glColor3f(0.6, 0.6, 0.6); glVertex3f(-1, -1, -1); glVertex3f(-1, cy+1, -1);
          glColor3f(0.1, 0.1, 0.1); glVertex3f(-1, -1, -1); glVertex3f(-1, -1, cz+1);
        } else {
          glColor3f(1, 0,    0); glVertex3f(-1, -1, -1); glVertex3f(cx+1, -1, -1);
          glColor3f(0, 0.75, 0); glVertex3f(-1, -1, -1); glVertex3f(-1, cy+1, -1);
          glColor3f(0, 0,    1); glVertex3f(-1, -1, -1); glVertex3f(-1, -1, cz+1);
        }
        glEnd();

#if 0    // if you enable this, the hotspot will be shown as a small cross
        float x = shapes[piece].shape->getHx();
        float y = shapes[piece].shape->getHy();
        float z = shapes[piece].shape->getHz();

        recalcSpaceCoordinates(&x, &y, &z);

        glBegin(GL_LINES);
        glColor3f(0, 1, 0);
        glVertex3f(x-0.2, y, z); glVertex3f(x+0.2, y, z);
        glVertex3f(x, y-0.2, z); glVertex3f(x, y+0.2, z);
        glVertex3f(x, y, z-0.2); glVertex3f(x, y, z+0.2);
        glEnd();
#endif

        if (_useLightning) glEnable(GL_LIGHTING);
        glEnable(GL_BLEND);
      }

      if (shape->list) {

        glCallList(shape->list);

      } else {

        if (config.useDisplayLists()) {

          shape->list = glGenLists(1);

          if (shape->list)
            glNewList(shape->list, GL_COMPILE_AND_EXECUTE);

        }

        if (!shape->poly)
          shape->poly = shape->shape->getDrawingMesh();

        if (shape->poly)
        {
          float lr = lightPieceColor(shape->r);
          float lg = lightPieceColor(shape->g);
          float lb = lightPieceColor(shape->b);
          float dr = darkPieceColor(shape->r);
          float dg = darkPieceColor(shape->g);
          float db = darkPieceColor(shape->b);

          if (shape->dim)
          {
            lr = 1 - (1 - lr) * 0.2;
            lg = 1 - (1 - lg) * 0.2;
            lb = 1 - (1 - lb) * 0.2;

            dr = 1 - (1 - dr) * 0.2;
            dg = 1 - (1 - dg) * 0.2;
            db = 1 - (1 - db) * 0.2;
          }

          if (colors == anaglyphColorL || colors == anaglyphColor)
          {
            float tmp;
            tmp = 0.1*lb + 0.3*lr + 0.6*lg;
            tmp = 1-(1-tmp)/3;
            lr = lg = lb = tmp;
            tmp = 0.1*db + 0.3*dr + 0.6*dg;
            tmp = 1-(1-tmp)/3;
            dr = dg = db = tmp;
          }

          for(Polyhedron::const_face_iterator it=shape->poly->fBegin(); it!=shape->poly->fEnd(); it++)
          {
            const Face* f = *it;

            if (f->hole())
              continue;

            if ((f->_flags & FF_INSIDE_FACE) && !insideVisible)
              continue;

            if (shape->mode == gridline && !((f->_flags & FF_WIREFRAME)))
              continue;

            glPushName(f->_fb_index);
            glPushName(f->_fb_face);

            GLfloat alpha = 1;

            if (f->_flags & FF_INSIDE_FACE)
            {
              glNormal3fv((-f->normal()).getData());
              alpha = 1;
              glEnable(GL_DEPTH_TEST);
            }
            else
            {
              glNormal3fv(f->normal().getData());
              if (insideVisible)
              {
                alpha = 0.1;
                glDisable(GL_DEPTH_TEST);
              }
              else
              {
                alpha = shape->a;
                glEnable(GL_DEPTH_TEST);
              }
            }

            glBegin(GL_TRIANGLES);

            if (   colors == paletteColor
                && f->_color > 0 && f->_color <= palette.size()
                && !(f->_flags & FF_VARIABLE_FACE))
                glColor4f(palette[f->_color-1].r, palette[f->_color-1].g, palette[f->_color-1].b, alpha);
            else if (f->_flags & FF_VARIABLE_FACE)
              glColor4f(0, 0, 0, alpha);
            else if (f->_flags & FF_COLOR_LIGHT && shape->useChecker)
              glColor4f(lr, lg, lb, alpha);
            else
              glColor4f(dr, dg, db, alpha);

            Face::const_edge_circulator e = f->begin();
            Face::const_edge_circulator sentinel = e;
            e++;
            Vector3Df start = (*e)->dst()->position();
            e++;

            do {
              glVertex3fv(start.getData());
              glVertex3fv((*e)->dst()->position().getData());
              e++;
              glVertex3fv((*e)->dst()->position().getData());
            } while (e != sentinel);

            if (f->_flags & FF_VARIABLE_MARK)
            {
              // draw the variable face
              // TODO, properly draw quadrilaterals (and possibly even more corners, right now only triangles work
              glColor3f(0, 0, 0);
              Face::const_edge_circulator e2 = f->begin();
              float x1 = (*e2)->dst()->position().x();
              float y1 = (*e2)->dst()->position().y();
              float z1 = (*e2)->dst()->position().z();
              e2++;
              float x2 = (*e2)->dst()->position().x();
              float y2 = (*e2)->dst()->position().y();
              float z2 = (*e2)->dst()->position().z();
              e2++;
              float x3 = (*e2)->dst()->position().x();
              float y3 = (*e2)->dst()->position().y();
              float z3 = (*e2)->dst()->position().z();
              e2++;

              if (e2 == f->begin())
                drawShrinkTriangle(x1, y1, z1, x2, y2, z2, x3, y3, z3);
              else
              {
                float x4 = (*e2)->dst()->position().x();
                float y4 = (*e2)->dst()->position().y();
                float z4 = (*e2)->dst()->position().z();

                drawShrinkQuadrilateral(x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4);
              }
            }

            glEnd();

            glPopName();
            glPopName();
          }
        }

        if (shape->list)
          glEndList();

      }

      // the marker should be only active, when only one shape is there
      // otherwise it's drawn for every shape
      if ((markerType >= 0) && (mX1 <= mX2) && (mY1 <= mY2))
      {
        if (_useLightning) glDisable(GL_LIGHTING);
        glDisable(GL_BLEND);

        glColor4f(0, 0, 0, 1);

        unsigned int sx = shape->shape->getX();
        unsigned int sy = shape->shape->getY();
        unsigned int sz = shape->shape->getZ();

        std::vector<float> face;

        // draw the cursor, this is done by iterating over all
        // voxels and checking for neighbors, when the current
        // voxel is inside the cursor region and the neighbor isn't,
        // we draw the dividing face
        for (unsigned int x = 0; x < sx; x++)
          for (unsigned int y = 0; y < sy; y++)
            for (unsigned int z = 0; z < sz; z++)
              if (shape->shape->validCoordinate(x, y, z) && inRegion(x, y, z, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType))
              {
                int nx, ny, nz;
                int n = 0;

                while (shape->shape->getNeighbor(n, 0, x, y, z, &nx, &ny, &nz))
                {
                  if (!inRegion(nx, ny, nz, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType))
                  {
                    face.clear();
                    shape->shape->getConnectionFace(x, y, z, n, 0, 0, face);

                    if (face.size() == 3*3)
                      drawGridTriangle(face[0], face[1], face[2], face[3]-face[0], face[4]-face[1], face[5]-face[2], face[6]-face[0], face[7]-face[1], face[8]-face[2], 4);
                    else if (face.size() == 4*3)
                      drawGridRect(face[0], face[1], face[2], face[3]-face[0], face[4]-face[1], face[5]-face[2], face[9]-face[0], face[10]-face[1], face[11]-face[2], 4);
                    else
                      printf("oops not implemented face shape for 3D cursor %i\n", face.size());
                  }

                  n++;
                }
              }

        if (_useLightning) glEnable(GL_LIGHTING);
        glEnable(GL_BLEND);
      }

      glPopMatrix();
    }

    glDepthMask(GL_FALSE);
  }

  glPopName();
  glDepthMask(GL_TRUE);
}

unsigned int voxelFrame_c::addSpace(const voxel_c * vx) {
  shapeInfo i;

  i.r = i.g = i.b = 1;
  i.a = 1;
  i.shape = vx;
  i.useChecker = true;

  i.mode = normal;

  i.x = i.y = i.z = 0;
  i.scale = 1;

  i.dim = false;

  i.list = 0;
  i.poly = 0;

  shapes.push_back(i);

  return shapes.size()-1;
}

void voxelFrame_c::clearSpaces(void) {

  for (unsigned int i = 0; i < shapes.size(); i++) {
    if (shapes[i].list) glDeleteLists(shapes[i].list, 1);
    delete shapes[i].shape;
    if (shapes[i].poly)
      delete shapes[i].poly;
    shapes[i].poly = 0;
  }

  shapes.clear();
}

void voxelFrame_c::setSpaceColor(unsigned int nr, float r, float g, float b, float a) {

  shapes[nr].r = r;
  shapes[nr].g = g;
  shapes[nr].b = b;
  shapes[nr].a = a;

  if (shapes[nr].list) {
    glDeleteLists(shapes[nr].list, 1);
    shapes[nr].list = 0;
  }

  redraw();
}

void voxelFrame_c::setSpaceColor(unsigned int nr, float a) {

  if (shapes[nr].a != a) {

    shapes[nr].a = a;

    if (shapes[nr].list) {
      glDeleteLists(shapes[nr].list, 1);
      shapes[nr].list = 0;
    }
  }
}

void voxelFrame_c::setSpacePosition(unsigned int nr, float x, float y, float z, float scale) {

  shapes[nr].shape->recalcSpaceCoordinates(&x, &y, &z);

  shapes[nr].x = x;
  shapes[nr].y = y;
  shapes[nr].z = z;
  shapes[nr].scale = scale;
}

void voxelFrame_c::setDrawingMode(unsigned int nr, drawingMode mode) {

  if (shapes[nr].mode != mode) {
    shapes[nr].mode = mode;

    if (shapes[nr].list) {
      glDeleteLists(shapes[nr].list, 1);
      shapes[nr].list = 0;
    }
  }

  redraw();
}

void voxelFrame_c::setColorMode(colorMode color) {
  colors = color;

  for (unsigned int nr = 0; nr < shapes.size(); nr++) {
    if (shapes[nr].list) {
      glDeleteLists(shapes[nr].list, 1);
      shapes[nr].list = 0;
    }
  }

  redraw();
}

void voxelFrame_c::setMarker(int x1, int y1, int x2, int y2, int z, int mT) {
  markerType = mT;
  mX1 = x1;
  mY1 = y1;
  mX2 = x2;
  mY2 = y2;
  mZ = z;
  redraw();
}

void voxelFrame_c::hideMarker(void) {
  markerType = -1;
  redraw();
}

void voxelFrame_c::showNothing(void) {
  clearSpaces();
}

void voxelFrame_c::setInsideVisible(bool on)
{
  insideVisible = on;
  for (unsigned int nr = 0; nr < shapes.size(); nr++) {
    if (shapes[nr].list) {
      glDeleteLists(shapes[nr].list, 1);
      shapes[nr].list = 0;
    }
  }
  redraw();
}

void voxelFrame_c::showSingleShape(const puzzle_c * puz, unsigned int shapeNum) {

  hideMarker();
  clearSpaces();
  unsigned int num = addSpace(puz->getGridType()->getVoxel(puz->getShape(shapeNum)));

  setSpaceColor(num, pieceColorR(shapeNum), pieceColorG(shapeNum), pieceColorB(shapeNum), 1);

  trans = TranslateRoateScale;
  _showCoordinateSystem = true;

  redraw();
}

void voxelFrame_c::showMesh(Polyhedron * poly)
{
  hideMarker();
  clearSpaces();

  shapeInfo i;

  i.r = i.g = i.b = 0.5;
  i.a = 1;
  i.shape = 0;
  i.poly = poly;
  i.useChecker = false;

  i.mode = normal;

  // calculate the bounding box of the polygon to properly center is for display
  Vector3Df bbox[2];
  bbox[0] = bbox[1] = (*poly->vBegin())->position();
  for (Polyhedron::const_vertex_iterator it=poly->vBegin() ; it!=poly->vEnd() ; ++it)
  {
    const Vector3Df& v = (*it)->position();
    for (int i=0 ; i<3 ; i++)
    {
      if (v[i] < bbox[0][i])
        bbox[0][i] = v[i];
      if (v[i] > bbox[1][i])
        bbox[1][i] = v[i];
    }
  }

  i.x = -0.5*(bbox[0][0]+bbox[1][0]);
  i.y = -0.5*(bbox[0][1]+bbox[1][1]);
  i.z = -0.5*(bbox[0][2]+bbox[1][2]);
  i.scale = 1;

  i.dim = false;

  i.list = 0;

  shapes.push_back(i);

  trans = TranslateRoateScale;
  _showCoordinateSystem = true;

  redraw();
}

void voxelFrame_c::showProblem(const puzzle_c * puz, unsigned int problem, unsigned int selShape) {

  hideMarker();
  clearSpaces();

  if (puz && problem < puz->problemNumber()) {

    const problem_c * pr = puz->getProblem(problem);

    float diagonal = 1;

    // now find a scaling factor, so that all pieces fit into their square

    // find the biggest piece shape
    for (unsigned int p = 0; p < pr->partNumber(); p++)
      if (pr->getShapeShape(p)->getDiagonal() > diagonal)
        diagonal = pr->getShapeShape(p)->getDiagonal();

    // find out how much bigger the result is compared to the shapes
    unsigned int factor;
    if (pr->resultValid()) {

      factor = (int)((sqrt(pr->getResultShape()->getDiagonal()) + 0.5)/sqrt(diagonal));
    } else
      factor = 1;

    if (factor < 1)
      factor = 1;

    diagonal = sqrt(diagonal);

    // first find out how to arrange the pieces:
    unsigned int square = 2*factor+1;
    while (square * (square-2*factor) < pr->partNumber()) square++;

    unsigned int num;

    // now place the result shape
    if (pr->resultValid()) {

      num = addSpace(pr->getGridType()->getVoxel(pr->getResultShape()));
      setSpaceColor(num,
                            pieceColorR(pr->getResultId()),
                            pieceColorG(pr->getResultId()),
                            pieceColorB(pr->getResultId()), 1);
      setSpacePosition(num,
          0.5* (square*diagonal) * (factor*1.0/square - 0.5),
          0.5* (square*diagonal) * (0.5 - factor*1.0/square), -20, 1.0);
    }

    // now place the selected shape
    if (selShape < puz->shapeNumber()) {

      num = addSpace(puz->getGridType()->getVoxel(puz->getShape(selShape)));
      setSpaceColor(num,
                            pieceColorR(selShape),
                            pieceColorG(selShape),
                            pieceColorB(selShape), 1);
      setSpacePosition(num,
          0.5* (square*diagonal) * (0.5 - 0.5/square),
          0.5* (square*diagonal) * (0.5 - 0.5/square), -20, 0.5);
    }

    // and now the shapes
    int unsigned line = 2*factor;
    int unsigned col = 0;
    for (unsigned int p = 0; p < pr->partNumber(); p++) {
      num = addSpace(pr->getGridType()->getVoxel(pr->getShapeShape(p)));

      setSpaceColor(num,
                            pieceColorR(pr->getShape(p)),
                            pieceColorG(pr->getShape(p)),
                            pieceColorB(pr->getShape(p)), 1);

      setSpacePosition(num,
                               0.5* (square*diagonal) * ((col+0.5)/square - 0.5),
                               0.5* (square*diagonal) * (0.5 - (line+0.5)/square),
                               -20, 0.5);

      col++;
      if (col == square) {
        col = 0;
        line++;
      }
    }

    trans = ScaleRotateTranslate;
    _showCoordinateSystem = false;
  }
  redraw();
}

void voxelFrame_c::showColors(const puzzle_c * puz, colorMode mode) {

  if (mode == paletteColor) {

    palette.clear();

    for (unsigned int i = 0; i < puz->colorNumber(); i++) {
      unsigned char r, g, b;
      puz->getColor(i, &r, &g, &b);

      colorInfo ci;

      ci.r = r/255.0;
      ci.g = g/255.0;
      ci.b = b/255.0;

      palette.push_back(ci);
    }
    setColorMode(paletteColor);

  } else
    setColorMode(mode);

  redraw();
}

void voxelFrame_c::showAssembly(const problem_c * puz, unsigned int solNum) {

  bt_assert(puz->resultValid());

  if (curAssembly) {
    delete curAssembly;
    curAssembly = 0;
  }

  hideMarker();
  clearSpaces();

  if (puz &&
      (solNum < puz->solutionNumber())) {

    unsigned int num;

    curAssembly = new assembly_c(puz->getSolution(solNum)->getAssembly());
    const assembly_c * assm = curAssembly;

    unsigned int piece = 0;

    // and now the shapes
    for (unsigned int p = 0; p < puz->partNumber(); p++)
      for (unsigned int q = 0; q < puz->getShapeMax(p); q++) {

        if (assm->isPlaced(piece)) {

          voxel_c * vx = puz->getGridType()->getVoxel(puz->getShapeShape(p));

          bt_assert2(vx->transform(assm->getTransformation(piece)));

          num = addSpace(vx);

          setSpacePosition(num, assm->getX(piece), assm->getY(piece), assm->getZ(piece), 1);

          setSpaceColor(num,
              pieceColorR(puz->getShape(p), q),
              pieceColorG(puz->getShape(p), q),
              pieceColorB(puz->getShape(p), q), 1);

        } else {

          voxel_c * vx = puz->getGridType()->getVoxel(puz->getShapeShape(p));

          num = addSpace(vx);

          setSpacePosition(num, 0, 0, 0, 1);

          setSpaceColor(num, 0);
        }

        piece++;
      }

    /* at the end add an empty voxel space that might be used for intersections */
    num = addSpace(puz->getGridType()->getVoxel(1, 1, 1, voxel_c::VX_EMPTY));
    setSpacePosition(num, 0, 0, 0, 1);
    setSpaceColor(num, 1, 0, 0, 1);   // bright red
    setDrawingMode(num, invisible);

    float cx, cy, cz;
    puz->getResultShape()->calculateSize(&cx, &cy, &cz);
    setCenter(cx*0.5, cy*0.5, cz*0.5);
    trans = CenterTranslateRoateScale;
    _showCoordinateSystem = false;
  }

  redraw();
}

void voxelFrame_c::showAssemblerState(const problem_c * puz, const assembly_c * assm) {

  bt_assert(puz->resultValid());

  hideMarker();
  clearSpaces();

  if (puz) {

    unsigned int num;
    unsigned int piece = 0;

    // and now the shapes
    for (unsigned int p = 0; p < puz->partNumber(); p++)
      for (unsigned int q = 0; q < puz->getShapeMax(p); q++) {

        if (assm->isPlaced(piece)) {

          voxel_c * vx = puz->getGridType()->getVoxel(puz->getShapeShape(p));

          bt_assert2(vx->transform(assm->getTransformation(piece)));

          num = addSpace(vx);

          setSpacePosition(num, assm->getX(piece), assm->getY(piece), assm->getZ(piece), 1);

          setSpaceColor(num,
              pieceColorR(puz->getShape(p), q),
              pieceColorG(puz->getShape(p), q),
              pieceColorB(puz->getShape(p), q), 1);

        }

        piece++;
      }

    float cx, cy, cz;
    puz->getResultShape()->calculateSize(&cx, &cy, &cz);
    setCenter(cx*0.5, cy*0.5, cz*0.5);

    trans = CenterTranslateRoateScale;
    _showCoordinateSystem = false;

    num = addSpace(puz->getGridType()->getVoxel(puz->getResultShape()));
    setSpaceColor(num,
                        pieceColorR(puz->getResultId()),
                        pieceColorG(puz->getResultId()),
                        pieceColorB(puz->getResultId()), 1);
    setDrawingMode(num, gridline);
  }

  redraw();
}

void voxelFrame_c::showPlacement(const problem_c * puz, unsigned int piece, unsigned char t, int x, int y, int z) {

  bt_assert(puz->resultValid());

  clearSpaces();
  hideMarker();
  trans = CenterTranslateRoateScale;
  _showCoordinateSystem = false;

  float hx, hy, hz;
  hx = puz->getResultShape()->getHx();
  hy = puz->getResultShape()->getHy();
  hz = puz->getResultShape()->getHz();

  puz->getResultShape()->recalcSpaceCoordinates(&hx, &hy, &hz);

  float cx, cy, cz;
  puz->getResultShape()->calculateSize(&cx, &cy, &cz);
  setCenter(cx*0.5-hx, cy*0.5-hy, cz*0.5-hz);

  hx = puz->getResultShape()->getHx();
  hy = puz->getResultShape()->getHy();
  hz = puz->getResultShape()->getHz();

  int num;

  if (t < puz->getGridType()->getSymmetries()->getNumTransformationsMirror()) {

    int shape = 0;
    unsigned int p = piece;
    while (p >= puz->getShapeMax(shape)) {
      p -= puz->getShapeMax(shape);
      shape++;
    }

    voxel_c * vx = puz->getGridType()->getVoxel(puz->getShapeShape(shape));
    bt_assert2(vx->transform(t));
    num = addSpace(vx);

    setSpacePosition(num, x-hx, y-hy, z-hz, 1);
    setSpaceColor(num,
                          pieceColorR(puz->getShape(shape), p),
                          pieceColorG(puz->getShape(shape), p),
                          pieceColorB(puz->getShape(shape), p), 1);
    setDrawingMode(num, normal);
  }

  num = addSpace(puz->getGridType()->getVoxel(puz->getResultShape()));
  setSpaceColor(num,
                        pieceColorR(puz->getResultId()),
                        pieceColorG(puz->getResultId()),
                        pieceColorB(puz->getResultId()), 1);
  setDrawingMode(num, gridline);

  redraw();
}


void voxelFrame_c::updatePositions(piecePositions_c *shifting) {

  for (unsigned int p = 0; p < shapes.size()-1; p++) {
    setSpacePosition(p, shifting->getX(p), shifting->getY(p), shifting->getZ(p), 1);
    setSpaceColor(p, shifting->getA(p));
  }

  redraw();
}

void voxelFrame_c::updatePositionsOverlap(piecePositions_c *shifting) {

  /* in this case all the positions must be on the raster, so check that first */
  for (unsigned int p = 0; p < shapes.size()-1; p++) {
    if ( (int)(shifting->getX(p)) != shifting->getX(p) ||
         (int)(shifting->getY(p)) != shifting->getY(p) ||
         (int)(shifting->getZ(p)) != shifting->getZ(p)
      ) {
      bt_assert(0);
    }
  }

  /* first find the most negatively valued coordinates */
  int negx = 0;
  int negy = 0;
  int negz = 0;
  for (unsigned int p = 0; p < shapes.size()-1; p++) {

    if (!shifting->getA(p) ||
        fabs(shifting->getX(p)) > 10000 ||
        fabs(shifting->getY(p)) > 10000 ||
        fabs(shifting->getZ(p)) > 10000) continue;

    if ((int)shifting->getX(p) < negx) negx = (int)shifting->getX(p);
    if ((int)shifting->getY(p) < negy) negy = (int)shifting->getY(p);
    if ((int)shifting->getZ(p) < negz) negz = (int)shifting->getZ(p);
  }

  /* now find all the voxels where something overlaps
   * we do this with the last piece within the piece list
   */
  voxel_c * inter = const_cast<voxel_c*>(shapes.rbegin()->shape);
  inter->setAll(voxel_c::VX_EMPTY);

  bool involved[shapes.size()];
  for (unsigned int p = 0; p < shapes.size(); p++)
    involved[p] = false;

  /* intersect each with everybody */
  for (unsigned int a = 0; a < shapes.size()-2; a++) {

    if (!shifting->getA(a) ||
        fabs(shifting->getX(a)) > 10000 ||
        fabs(shifting->getY(a)) > 10000 ||
        fabs(shifting->getZ(a)) > 10000) continue;

    for (unsigned int b = a+1; b < shapes.size()-1; b++) {

      if (!shifting->getA(b) ||
          fabs(shifting->getX(b)) > 10000 ||
          fabs(shifting->getY(b)) > 10000 ||
          fabs(shifting->getZ(b)) > 10000) continue;

      if (inter->unionintersect(
            shapes[a].shape,
            (int)shifting->getX(a)-negx - shapes[a].shape->getHx(),
            (int)shifting->getY(a)-negy - shapes[a].shape->getHy(),
            (int)shifting->getZ(a)-negz - shapes[a].shape->getHz(),
            shapes[b].shape,
            (int)shifting->getX(b)-negx - shapes[b].shape->getHx(),
            (int)shifting->getY(b)-negy - shapes[b].shape->getHy(),
            (int)shifting->getZ(b)-negz - shapes[b].shape->getHz())) {
        involved[a] = true;
        involved[b] = true;
      }
    }
  }

  /* now there are 2 possibilities */
  if (inter->countState(voxel_c::VX_FILLED) > 0) {

    /* we have some intersection then all involved pieces will be drawn as wireframe
     * not involved pieces will become invisible
     * the intersection piece will be visible
     */

    for (unsigned int p = 0; p < shapes.size()-1; p++) {
      if (involved[p]) {
        setDrawingMode(p, gridline);
      } else {
        setDrawingMode(p, invisible);
      }
    }
    setDrawingMode(shapes.size()-1, normal);
    setSpacePosition(shapes.size()-1, negx, negy, negz, 1);

    if (shapes[shapes.size()-1].list) {
      glDeleteLists(shapes[shapes.size()-1].list, 1);
      shapes[shapes.size()-1].list = 0;
    }

  } else {
    /* no intersection, all pieces are drawn normally
     * interset piece is invisible
     */
    for (unsigned int p = 0; p < shapes.size()-1; p++)
      setDrawingMode(p, normal);

    setDrawingMode(shapes.size()-1, invisible);
  }

  for (unsigned int p = 0; p < shapes.size()-1; p++) {
    setSpacePosition(p, shifting->getX(p), shifting->getY(p), shifting->getZ(p), 1);
    setSpaceColor(p, shifting->getA(p));
  }

  _showCoordinateSystem = false;

  redraw();
}

void voxelFrame_c::dimStaticPieces(piecePositions_c *shifting) {

  for (unsigned int p = 0; p < shapes.size()-1; p++) {
    if (shapes[p].dim != !shifting->moving(p)) {
      shapes[p].dim = !shifting->moving(p);
      if (shapes[p].list) {
        glDeleteLists(shapes[p].list, 1);
        shapes[p].list = 0;
      }
    }
  }

  redraw();
}

void voxelFrame_c::updateVisibility(PieceVisibility * pcvis) {

  /* savety check, it might be possible to click onto the visibility
   * selector even if no solution is displayed, e.g. when there is no
   * solution, if we then have more pieces on the display as there
   * are blocks in the visibility selector we crash, so if the
   * number of blocks inside the visibility selector is smaller
   * than the number of visible voxel spaces, drop out
   */
  if (pcvis->blockNumber() < shapes.size()-1) return;

  for (unsigned int p = 0; p < shapes.size()-1; p++) {

    switch(pcvis->getVisibility(p)) {
    case 0:
      setDrawingMode(p, normal);
      break;
    case 1:
      setDrawingMode(p, gridline);
      break;
    case 2:
      setDrawingMode(p, invisible);
      break;
    }
  }

  redraw();
}

static void gluPerspective(double fovy, double aspect, double zNear, double zFar) {

  double xmin, xmax, ymin, ymax;
  ymax = zNear * tan(fovy * 3.1415927 / 360.0);
  ymin = -ymax;
  xmin = ymin * aspect;
  xmax = ymax * aspect;
  glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

static void gluPickMatrix(double x, double y, double deltax, double deltay, GLint viewport[4]) {

  glTranslatef((viewport[2]-2*(x-viewport[0]))/deltax,
      (viewport[3]-2*(y-viewport[1]))/deltay, 0);
  glScalef(viewport[2]/deltax, viewport[3]/deltay, 1.0);
}

void voxelFrame_c::draw() {

  if (!valid()) {

    GLfloat LightAmbient[]= { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat LightDiffuse[]= { 0.6f, 0.6f, 0.6f, 0.0f };
    GLfloat LightPosition[]= { 700.0f, 200.0f, 700.0f, 1.0f };

    GLfloat AmbientParams[] = {0.1, 0.1, 0.1, 1};
    GLfloat DiffuseParams[] = {0.7, 0.7, 0.7, 0.1};
    GLfloat SpecularParams[] = {0.4, 0.4, 0.4, 0.5};

    glLoadIdentity();
    glViewport(0,0,w(),h());

    glEnable(GL_COLOR_MATERIAL);
    glLightfv(GL_LIGHT1, GL_AMBIENT, LightAmbient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, LightDiffuse);
    glLightfv(GL_LIGHT1, GL_POSITION, LightPosition);
    glEnable(GL_LIGHT1);

    glMaterialf(GL_FRONT, GL_SHININESS, 0.5);
    glMaterialfv(GL_FRONT, GL_AMBIENT, AmbientParams);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, DiffuseParams);
    glMaterialfv(GL_FRONT, GL_SPECULAR, SpecularParams);

    glEnable(GL_RESCALE_NORMAL);

    rotater->setBounds(w(), h());

    unsigned char r, g, b;
    Fl::get_color(color(), r, g, b);
    glClearColor(r/255.0, g/255.0, b/255.0, 0);

    glShadeModel(GL_FLAT);
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glLineWidth(3);

  }

  if (!cb || !cb->PreDraw()) {

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    if (pickx >= 0) {
      GLint viewport[4];

      glGetIntegerv(GL_VIEWPORT, viewport);
      gluPickMatrix(pickx, picky, 3, 3, viewport);
    }

    // this call has to be identical to the one in image_c::prepareOpenGlImagePart
    gluPerspective(15, 1.0*w()/h(), size+1, 3*size+1);
    glMatrixMode(GL_MODELVIEW);

  }

  if (_useLightning)
    glEnable(GL_LIGHTING);
  else
    glDisable(GL_LIGHTING);

  glPushMatrix();
  glTranslatef(0, 0, -size*2);

  if (colors == anaglyphColor) {
    glPushMatrix();
    glTranslatef(-0.04, 0, 0);
    glRotatef(-1, 0, 1, 0);
    glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawVoxelSpace();

    glPopMatrix();
    glTranslatef(0.04, 0, 0);
    glRotatef(1, 0, 1, 0);
    glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  if (colors == anaglyphColorL) {
    glPushMatrix();
    glTranslatef(-0.04, 0, 0);
    glRotatef(-1, 0, 1, 0);
    glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawVoxelSpace();

    glPopMatrix();
    glTranslatef(0.04, 0, 0);
    glRotatef(1, 0, 1, 0);
    glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  drawVoxelSpace();

  glPopMatrix();

  if (colors == anaglyphColor || colors == anaglyphColorL) {
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  if (cb)
    cb->PostDraw();
}

int voxelFrame_c::handle(int event) {

  if (Fl_Gl_Window::handle(event))
    return 1;

  switch(event) {
  case FL_PUSH:

    if (!Fl::event_state(FL_SHIFT | FL_ALT | FL_CTRL))
      rotater->click(Fl::event_x(), Fl::event_y());

    do_callback();

    return 1;

  case FL_DRAG:

    rotater->drag(Fl::event_x(), Fl::event_y());
    redraw();

    do_callback();

    return 1;

  case FL_RELEASE:

    rotater->clack(Fl::event_x(), Fl::event_y());
    redraw();

    return 1;
  }

  return 0;
}

void voxelFrame_c::setSize(double sz) {
  size = sz;
  redraw();
}

bool voxelFrame_c::pickShape(int x, int y, unsigned int *shape, unsigned long *voxel, unsigned int *face) {

  GLuint sbuffer[500];

  glSelectBuffer(500, sbuffer);
  glRenderMode(GL_SELECT);

  glInitNames();

  pickx = x;
  picky = y;

  draw();

  GLint hits = glRenderMode(GL_RENDER);
  pickx = -1;

  if (hits <= 0) {
    return false;
  }

  int frontHit = -1;

  int pos = 0;

  /* find entry with smallest z */
  for (int i = 0; i < hits; i++) {

    if (sbuffer[pos] == 3)
      if ((frontHit < 0) || (sbuffer[pos+1] < sbuffer[frontHit+1]))
        frontHit = pos;

    pos += 3 + sbuffer[pos];
  }

  if (frontHit < 0) return false;

  if (shape) *shape = sbuffer[frontHit+3];
  if (voxel) *voxel = sbuffer[frontHit+4];
  if (face)  *face  = sbuffer[frontHit+5];

  return true;
}

void voxelFrame_c::exportToVector(const char * fname, VectorFiletype vt) {


#if 0
#if (VFT_PS  != GL2PS_PS  || VFT_EPS != GL2PS_EPS || \
     VFT_TEX != GL2PS_TEX || VFT_PDF != GL2PS_PDF || \
     VFT_SVG != GL2PS_SVG || VFT_PGF != GL2PS_PGF)

#error vector file types dont fit to GL2PS file types
#endif
#endif

  FILE * of = fopen(fname, "wb");

  int state = GL2PS_OVERFLOW;
  int bufsize = 0;

  while (state == GL2PS_OVERFLOW) {

    bufsize += 1024*1024;

    gl2psBeginPage("BurrTools", "BurrTools", NULL, vt, GL2PS_BSP_SORT,
        GL2PS_USE_CURRENT_VIEWPORT | GL2PS_OCCLUSION_CULL, GL_RGBA, 0, NULL, 0, 0, 0,
        bufsize, of, fname);

    draw();

    state = gl2psEndPage();
  }

  fclose(of);
}

