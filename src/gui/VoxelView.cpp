/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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


#include "VoxelView.h"
#include <math.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <stdlib.h>

#include "../lib/puzzle.h"

#include "pieceColor.h"


#define EDGELO 0.05f
#define EDGEHI 0.95f
#define MY 0.005f

VoxelView::VoxelView(int x,int y,int w,int h,const char *l) : Fl_Gl_Window(x,y,w,h,l),
  arcBall(new ArcBall_c(w, h)), size(10), markerType(false), colors(pieceColor), doUpdates(true)
{
};

// draws a wireframe box depending on the neibors
static void drawFrame(const voxel_c * space, int x, int y, int z) {

  glBegin(GL_QUADS);

  if (space->isEmpty2(x, y, z-1)) {
    glNormal3f( 0.0f, 0.0f, -1.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z-1)) { glVertex3f(x+0, y+0, z); glVertex3f(x+EDGELO, y+0, z); glVertex3f(x+EDGELO, y+1, z); glVertex3f(x+0, y+1, z); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z-1)) { glVertex3f(x+1, y+0, z); glVertex3f(x+EDGEHI, y+0, z); glVertex3f(x+EDGEHI, y+1, z); glVertex3f(x+1, y+1, z); }
    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x, y-1, z-1)) { glVertex3f(x+0, y+0, z); glVertex3f(x+0, y+EDGELO, z); glVertex3f(x+1, y+EDGELO, z); glVertex3f(x+1, y+0, z); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x, y+1, z-1)) { glVertex3f(x+0, y+1, z); glVertex3f(x+0, y+EDGEHI, z); glVertex3f(x+1, y+EDGEHI, z); glVertex3f(x+1, y+1, z); }
  }
  if (space->isEmpty2(x-1, y, z)) {
    glNormal3f( -1.0f, 0.0f, 0.0f);

    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x-1, y-1, z)) { glVertex3f(x, y+0, z+0); glVertex3f(x, y+EDGELO, z+0); glVertex3f(x, y+EDGELO, z+1); glVertex3f(x, y+0, z+1); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x-1, y+1, z)) { glVertex3f(x, y+1, z+0); glVertex3f(x, y+1, z+1); glVertex3f(x, y+EDGEHI, z+1); glVertex3f(x, y+EDGEHI, z+0); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x-1, y, z-1)) { glVertex3f(x, y+0, z+EDGELO); glVertex3f(x, y+0, z+0); glVertex3f(x, y+1, z+0); glVertex3f(x, y+1, z+EDGELO); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x-1, y, z+1)) { glVertex3f(x, y+0, z+1); glVertex3f(x, y+0, z+EDGEHI); glVertex3f(x, y+1, z+EDGEHI); glVertex3f(x, y+1, z+1); }
  }
  if (space->isEmpty2(x+1, y, z)) {
    glNormal3f( 1.0f, 0.0f, 0.0f);

    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x+1, y-1, z)) { glVertex3f(x+1-MY, y+0, z+0); glVertex3f(x+1-MY, y+0, z+1); glVertex3f(x+1-MY, y+EDGELO, z+1); glVertex3f(x+1-MY, y+EDGELO, z+0); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x+1, y+1, z)) { glVertex3f(x+1-MY, y+EDGEHI, z+0); glVertex3f(x+1-MY, y+EDGEHI, z+1); glVertex3f(x+1-MY, y+1, z+1); glVertex3f(x+1-MY, y+1, z+0); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x+1, y, z-1)) { glVertex3f(x+1-MY, y+1, z+EDGELO); glVertex3f(x+1-MY, y+1, z+0); glVertex3f(x+1-MY, y+0, z+0); glVertex3f(x+1-MY, y+0, z+EDGELO); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x+1, y, z+1)) { glVertex3f(x+1-MY, y+1, z+1); glVertex3f(x+1-MY, y+1, z+EDGEHI); glVertex3f(x+1-MY, y+0, z+EDGEHI); glVertex3f(x+1-MY, y+0, z+1); }
  }
  if (space->isEmpty2(x, y, z+1)) {
    glNormal3f( 0.0f, 0.0f, 1.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z+1)) { glVertex3f(x+EDGELO, y+0, z+1-MY); glVertex3f(x+EDGELO, y+1, z+1-MY); glVertex3f(x+0, y+1, z+1-MY); glVertex3f(x+0, y+0, z+1-MY); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z+1)) { glVertex3f(x+1, y+0, z+1-MY); glVertex3f(x+1, y+1, z+1-MY); glVertex3f(x+EDGEHI, y+1, z+1-MY); glVertex3f(x+EDGEHI, y+0, z+1-MY); }
    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x, y-1, z+1)) { glVertex3f(x+0, y+EDGELO, z+1-MY); glVertex3f(x+0, y+0, z+1-MY); glVertex3f(x+1, y+0, z+1-MY); glVertex3f(x+1, y+EDGELO, z+1-MY); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x, y+1, z+1)) { glVertex3f(x+0, y+1, z+1-MY); glVertex3f(x+0, y+EDGEHI, z+1-MY); glVertex3f(x+1, y+EDGEHI, z+1-MY); glVertex3f(x+1, y+1, z+1-MY); }
  }
  if (space->isEmpty2(x, y-1, z)) {
    glNormal3f( 0.0f, -1.0f, 0.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y-1, z)) { glVertex3f(x+EDGELO, y, z+0); glVertex3f(x+0, y, z+0); glVertex3f(x+0, y, z+1); glVertex3f(x+EDGELO, y, z+1); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y-1, z)) { glVertex3f(x+1, y, z+0); glVertex3f(x+EDGEHI, y, z+0); glVertex3f(x+EDGEHI, y, z+1); glVertex3f(x+1, y, z+1); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x, y-1, z-1)) { glVertex3f(x+0, y, z+EDGELO); glVertex3f(x+1, y, z+EDGELO); glVertex3f(x+1, y, z+0); glVertex3f(x+0, y, z+0); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x, y-1, z+1)) { glVertex3f(x+0, y, z+1); glVertex3f(x+1, y, z+1); glVertex3f(x+1, y, z+EDGEHI); glVertex3f(x+0, y, z+EDGEHI); }
  }
  if (space->isEmpty2(x, y+1, z)) {
    glNormal3f( 0.0f, 1.0f, 0.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y+1, z)) { glVertex3f(x+EDGELO, y+1-MY, z+0); glVertex3f(x+EDGELO, y+1-MY, z+1); glVertex3f(x+0, y+1-MY, z+1); glVertex3f(x+0, y+1-MY, z+0); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y+1, z)) { glVertex3f(x+1, y+1-MY, z+0); glVertex3f(x+1, y+1-MY, z+1); glVertex3f(x+EDGEHI, y+1-MY, z+1); glVertex3f(x+EDGEHI, y+1-MY, z+0); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x, y+1, z-1)) { glVertex3f(x+0, y+1-MY, z+EDGELO); glVertex3f(x+0, y+1-MY, z+0); glVertex3f(x+1, y+1-MY, z+0); glVertex3f(x+1, y+1-MY, z+EDGELO); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x, y+1, z+1)) { glVertex3f(x+0, y+1-MY, z+1); glVertex3f(x+0, y+1-MY, z+EDGEHI); glVertex3f(x+1, y+1-MY, z+EDGEHI); glVertex3f(x+1, y+1-MY, z+1); }
  }

  glEnd();
}

// draws a box with borders depending on the neibor boxes
static void drawBox(const voxel_c * space, int x, int y, int z, float alpha) {

  GLfloat a0, b0, a1, b1;

  glBegin(GL_QUADS);
  if (space->isEmpty2(x, y, z-1)) {
    glNormal3f( 0.0f, 0.0f, -1.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y, z-1)) a0 = 0; else a0 = EDGELO;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y, z-1)) a1 = 1; else a1 = EDGEHI;
    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x, y-1, z-1)) b0 = 0; else b0 = EDGELO;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x, y+1, z-1)) b1 = 1; else b1 = EDGEHI;

    glVertex3f(x+a0, y+b0, z); glVertex3f(x+a0, y+b1, z); glVertex3f(x+a1, y+b1, z); glVertex3f(x+a1, y+b0, z);
  }
  if (space->isEmpty2(x-1, y, z)) {
    glNormal3f( -1.0f, 0.0f, 0.0f);

    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x-1, y-1, z)) a0 = 0; else a0 = EDGELO;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x-1, y+1, z)) a1 = 1; else a1 = EDGEHI;
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x-1, y, z-1)) b0 = 0; else b0 = EDGELO;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x-1, y, z+1)) b1 = 1; else b1 = EDGEHI;

    glVertex3f(x, y+a0, z+b0); glVertex3f(x, y+a0, z+b1); glVertex3f(x, y+a1, z+b1); glVertex3f(x, y+a1, z+b0);
  }
  if (space->isEmpty2(x+1, y, z)) {
    glNormal3f( 1.0f, 0.0f, 0.0f);

    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x+1, y-1, z)) a0 = 0; else a0 = EDGELO;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x+1, y+1, z)) a1 = 1; else a1 = EDGEHI;
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x+1, y, z-1)) b0 = 0; else b0 = EDGELO;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x+1, y, z+1)) b1 = 1; else b1 = EDGEHI;

    glVertex3f(x+1-MY, y+a1, z+b0); glVertex3f(x+1-MY, y+a1, z+b1); glVertex3f(x+1-MY, y+a0, z+b1); glVertex3f(x+1-MY, y+a0, z+b0);
  }
  if (space->isEmpty2(x, y, z+1)) {
    glNormal3f( 0.0f, 0.0f, 1.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y, z+1)) a0 = 0; else a0 = EDGELO;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y, z+1)) a1 = 1; else a1 = EDGEHI;
    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x, y-1, z+1)) b0 = 0; else b0 = EDGELO;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x, y+1, z+1)) b1 = 1; else b1 = EDGEHI;

    glVertex3f(x+a0, y+b0, z+1-MY); glVertex3f(x+a0, y+b1, z+1-MY); glVertex3f(x+a1, y+b1, z+1-MY); glVertex3f(x+a1, y+b0, z+1-MY);
  }
  if (space->isEmpty2(x, y-1, z)) {
    glNormal3f( 0.0f, -1.0f, 0.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y-1, z)) a0 = 0; else a0 = EDGELO;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y-1, z)) a1 = 1; else a1 = EDGEHI;
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x, y-1, z-1)) b0 = 0; else b0 = EDGELO;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x, y-1, z+1)) b1 = 1; else b1 = EDGEHI;

    glVertex3f(x+a0, y  , z+b0); glVertex3f(x+a1, y  , z+b0); glVertex3f(x+a1, y  , z+b1); glVertex3f(x+a0, y  , z+b1);
  }
  if (space->isEmpty2(x, y+1, z)) {
    glNormal3f( 0.0f, 1.0f, 0.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y+1, z)) a0 = 0; else a0 = EDGELO;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y+1, z)) a1 = 1; else a1 = EDGEHI;
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x, y+1, z-1)) b0 = 0; else b0 = EDGELO;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x, y+1, z+1)) b1 = 1; else b1 = EDGEHI;

    glVertex3f(x+a0, y+1-MY, z+b0); glVertex3f(x+a0, y+1-MY, z+b1); glVertex3f(x+a1, y+1-MY, z+b1); glVertex3f(x+a1, y+1-MY, z+b0);
  }

  glEnd();

  glColor4f(0, 0, 0, alpha);

  drawFrame(space, x, y, z);
}


// draw a bube that is smaller than 1
static void drawCube(const voxel_c * space, int x, int y, int z) {
  glBegin(GL_QUADS);
  glNormal3f( 0.0f, 0.0f, -1.0f);
  glVertex3f(x+0.2, y+0.2, z-0.02); glVertex3f(x+0.2, y+0.8, z-0.02); glVertex3f(x+0.8, y+0.8, z-0.02); glVertex3f(x+0.8, y+0.2, z-0.02);
  glNormal3f( -1.0f, 0.0f, 0.0f);
  glVertex3f(x-0.02, y+0.2, z+0.2); glVertex3f(x-0.02, y+0.2, z+0.8); glVertex3f(x-0.02, y+0.8, z+0.8); glVertex3f(x-0.02, y+0.8, z+0.2);
  glNormal3f( 1.0f, 0.0f, 0.0f);
  glVertex3f(x+1.02, y+0.8, z+0.2); glVertex3f(x+1.02, y+0.8, z+0.8); glVertex3f(x+1.02, y+0.2, z+0.8); glVertex3f(x+1.02, y+0.2, z+0.2);
  glNormal3f( 0.0f, 0.0f, 1.0f);
  glVertex3f(x+0.2, y+0.2, z+1.02); glVertex3f(x+0.2, y+0.8, z+1.02); glVertex3f(x+0.8, y+0.8, z+1.02); glVertex3f(x+0.8, y+0.2, z+1.02);
  glNormal3f( 0.0f, -1.0f, 0.0f);
  glVertex3f(x+0.2, y-0.02, z+0.2); glVertex3f(x+0.8, y-0.02, z+0.2); glVertex3f(x+0.8, y-0.02, z+0.8); glVertex3f(x+0.2, y-0.02, z+0.8);
  glNormal3f( 0.0f, 1.0f, 0.0f);
  glVertex3f(x+0.2, y+1.02, z+0.2); glVertex3f(x+0.2, y+1.02, z+0.8); glVertex3f(x+0.8, y+1.02, z+0.8); glVertex3f(x+0.8, y+1.02, z+0.2);
  glEnd();
}

static void drawRect(int x0, int y0, int z0,
                int v1x, int v1y, int v1z,
                int v2x, int v2y, int v2z, bool type, int diag) {

  assert((v1x >= 0) && (v1y >= 0) && (v1z >= 0));
  assert((v2x >= 0) && (v2y >= 0) && (v2z >= 0));

  glBegin(GL_LINES);
  glVertex3f(x0, y0, z0); glVertex3f(x0+v1x, y0+v1y, z0+v1z);
  glVertex3f(x0+v1x, y0+v1y, z0+v1z); glVertex3f(x0+v1x+v2x, y0+v1y+v2y, z0+v1z+v2z);
  glVertex3f(x0+v1x+v2x, y0+v1y+v2y, z0+v1z+v2z); glVertex3f(x0+v2x, y0+v2y, z0+v2z);
  glVertex3f(x0+v2x, y0+v2y, z0+v2z); glVertex3f(x0, y0, z0);

  int state1 = 0;
  int state2 = 0;

  if (type) {

    float x1 = x0;
    float y1 = y0;
    float z1 = z0;
  
    float x2 = x0;
    float y2 = y0;
    float z2 = z0;
  
    int xe = x0 + v1x + v2x;
    int ye = y0 + v1y + v2y;
    int ze = z0 + v1z + v2z;

    while ((x1 < xe) || (y1 < ye) || (z1 < ze)) {
      // v1=(x1, y1, z1) first goes along vector1 =(v1x, v1y, v1z) and then along vector2
  
      if (state1 == 0) {
        if (v1x) x1 += 1.0/diag;
        if (v1y) y1 += 1.0/diag;
        if (v1z) z1 += 1.0/diag;
  
        if ((v1x) && (x1 >= x0+v1x) || (v1y) && (y1 >= y0+v1y) || (v1z) && (z1 >= z0+v1z))
          state1 = 1;
      } else {
  
        if (v2x) x1 += 1.0/diag;
        if (v2y) y1 += 1.0/diag;
        if (v2z) z1 += 1.0/diag;
      }
  
      if (state2 == 0) {
        if (v2x) x2 += 1.0/diag;
        if (v2y) y2 += 1.0/diag;
        if (v2z) z2 += 1.0/diag;
  
        if ((v2x) && (x2 >= x0+v2x) || (v2y) && (y2 >= y0+v2y) || (v2z) && (z2 >= z0+v2z))
          state2 = 1;
      } else {
  
        if (v1x) x2 += 1.0/diag;
        if (v1y) y2 += 1.0/diag;
        if (v1z) z2 += 1.0/diag;
      }
  
      glVertex3f(x1, y1, z1); glVertex3f(x2, y2, z2);
    }

  } else {

    float x1 = x0 + v1x;
    float y1 = y0 + v1y;
    float z1 = z0 + v1z;
  
    float x2 = x0 + v1x;
    float y2 = y0 + v1y;
    float z2 = z0 + v1z;
  
    int xe = x0 + v2x;
    int ye = y0 + v2y;
    int ze = z0 + v2z;

    while ((x1 < xe) || (y1 < ye) || (z1 < ze)) {
      // v1=(x1, y1, z1) first goes along vector1 =(v1x, v1y, v1z) and then along vector2
  
      if (state1 == 0) {
        if (v1x) x1 -= 1.0/diag;
        if (v1y) y1 -= 1.0/diag;
        if (v1z) z1 -= 1.0/diag;
  
        if ((v1x) && (x1 <= x0) || (v1y) && (y1 <= y0) || (v1z) && (z1 <= z0))
          state1 = 1;
      } else {
  
        if (v2x) x1 += 1.0/diag;
        if (v2y) y1 += 1.0/diag;
        if (v2z) z1 += 1.0/diag;
      }
  
      if (state2 == 0) {
        if (v2x) x2 += 1.0/diag;
        if (v2y) y2 += 1.0/diag;
        if (v2z) z2 += 1.0/diag;
  
        if ((v2x) && (x2 >= x0+v2x+v1x) || (v2y) && (y2 >= y0+v2y+v1y) || (v2z) && (z2 >= z0+v2z+v1z))
          state2 = 1;
      } else {
  
        if (v1x) x2 -= 1.0/diag;
        if (v1y) y2 -= 1.0/diag;
        if (v1z) z2 -= 1.0/diag;
      }
  
      glVertex3f(x1, y1, z1); glVertex3f(x2, y2, z2);
    }
  }

  glEnd();
}


void VoxelView::drawVoxelSpace() {

  glShadeModel(GL_FLAT);

  glScalef(scale, scale, scale);

  for (unsigned int piece = 0; piece < shapes.size(); piece++) {

    if (shapes[piece].a == 0)
      continue;

    glPushMatrix();

    switch(trans) {
    case ScaleRotateTranslate:
      glTranslatef(shapes[piece].x, shapes[piece].y, shapes[piece].z);
      glScalef(shapes[piece].scale, shapes[piece].scale, shapes[piece].scale);
      arcBall->addTransform();
      glTranslatef(shapes[piece].shape->getX()/-2.0, shapes[piece].shape->getY()/-2.0, shapes[piece].shape->getZ()/-2.0);
      break;
    case TranslateRoateScale:
      arcBall->addTransform();
      glTranslatef(shapes[piece].x, shapes[piece].y, shapes[piece].z);
      glTranslatef(shapes[piece].shape->getX()/-2.0, shapes[piece].shape->getY()/-2.0, shapes[piece].shape->getZ()/-2.0);
      glScalef(shapes[piece].scale, shapes[piece].scale, shapes[piece].scale);
      break;
    case CenterTranslateRoateScale:
      arcBall->addTransform();
      glTranslatef(shapes[piece].x, shapes[piece].y, shapes[piece].z);
      glTranslatef(-centerX, -centerY, -centerZ);
      glScalef(shapes[piece].scale, shapes[piece].scale, shapes[piece].scale);
      break;
    default:
      break;
    }

    if (_showCoordinateSystem) {
      if (_useLightning) glDisable(GL_LIGHTING);
      glDisable(GL_BLEND);
      glBegin(GL_LINES);
      glColor3f(1, 0, 0);
      glVertex3f(-1, -1, -1); glVertex3f(shapes[piece].shape->getX()+1, -1, -1);
      glColor3f(0, 1, 0);
      glVertex3f(-1, -1, -1); glVertex3f(-1, shapes[piece].shape->getY()+1, -1);
      glColor3f(0, 0, 1);
      glVertex3f(-1, -1, -1); glVertex3f(-1, -1, shapes[piece].shape->getZ()+1);
      glEnd();
      if (_useLightning) glEnable(GL_LIGHTING);
      glEnable(GL_BLEND);
    }

    for (unsigned int x = 0; x < shapes[piece].shape->getX(); x++)
      for (unsigned int y = 0; y < shapes[piece].shape->getY(); y++)
        for (unsigned int z = 0; z < shapes[piece].shape->getZ(); z++) {
  
          if (shapes[piece].shape->isEmpty(x, y , z))
            continue;

          switch (colors) {
          case pieceColor:
            if ((x+y+z) & 1)
              glColor4f(shapes[piece].r, shapes[piece].g, shapes[piece].b, shapes[piece].a);
            else
              glColor4f(shapes[piece].r*0.9, shapes[piece].g*0.9, shapes[piece].b*0.9, shapes[piece].a);
            break;
          case paletteColor:
            unsigned int color = shapes[piece].shape->getColor(x, y, z);
            if ((color == 0) || (color - 1 >= palette.size())) {
              if ((x+y+z) & 1)
                glColor4f(shapes[piece].r, shapes[piece].g, shapes[piece].b, shapes[piece].a);
              else
                glColor4f(shapes[piece].r*0.9, shapes[piece].g*0.9, shapes[piece].b*0.9, shapes[piece].a);
            } else
              glColor4f(palette[color-1].r, palette[color-1].g, palette[color-1].b, shapes[piece].a);
          }

          switch (shapes[piece].mode) {
          case normal:
            if (shapes[piece].shape->getState(x, y , z) == voxel_c::VX_VARIABLE) {
              drawBox(shapes[piece].shape, x, y, z, shapes[piece].a);
              glColor4f(0, 0, 0, shapes[piece].a);
              drawCube(shapes[piece].shape, x, y, z);
            } else
              drawBox(shapes[piece].shape, x, y, z, shapes[piece].a);
            break;
          case gridline:
            drawFrame(shapes[piece].shape, x, y, z);
            break;
          case invisible:
            break;
          }
        }

    if (markerType) {
      if (_useLightning) glDisable(GL_LIGHTING);
      glDisable(GL_BLEND);

      glColor4f(1, 1, 1, 1);

      drawRect(-1, -1, mZ, shapes[piece].shape->getX()+2, 0, 0, 0, shapes[piece].shape->getY()+2, 0, true, 2);
      drawRect(-1, -1, mZ+1, shapes[piece].shape->getX()+2, 0, 0, 0, shapes[piece].shape->getY()+2, 0, true, 2);
      drawRect(-1, -1, mZ, 0, 0, 1, 0, shapes[piece].shape->getY()+2, 0, true, 2);
      drawRect(-1, -1, mZ, shapes[piece].shape->getX()+2, 0, 0, 0, 0, 1, true, 2);
      drawRect(shapes[piece].shape->getX()+1, -1, mZ, 0, 0, 1, 0, shapes[piece].shape->getY()+2, 0, true, 2);
      drawRect(-1, shapes[piece].shape->getY()+1, mZ, shapes[piece].shape->getX()+2, 0, 0, 0, 0, 1, true, 2);

      glColor4f(0, 0, 0, 1);

      drawRect(mX1, mY1, mZ, mX2-mX1, 0, 0, 0, mY2-mY1, 0, false, 4);
      drawRect(mX1, mY1, mZ+1, mX2-mX1, 0, 0, 0, mY2-mY1, 0, false, 4);
      drawRect(mX1, mY1, mZ, 0, 0, 1, 0, mY2-mY1, 0, false, 4);
      drawRect(mX1, mY1, mZ, mX2-mX1, 0, 0, 0, 0, 1, false, 4);
      drawRect(mX2, mY1, mZ, 0, 0, 1, 0, mY2-mY1, 0, false, 4);
      drawRect(mX1, mY2, mZ, mX2-mX1, 0, 0, 0, 0, 1, false, 4);

      if (_useLightning) glEnable(GL_LIGHTING);
      glEnable(GL_BLEND);
    }

    glPopMatrix();
  }

}

static void gluPerspective(double fovy, double aspect, double zNear, double zFar) {

   double xmin, xmax, ymin, ymax;
   ymax = zNear * tan(fovy * 3.1415927 / 360.0);
   ymin = -ymax;
   xmin = ymin * aspect;
   xmax = ymax * aspect;
   glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

void VoxelView::draw() {

  if (!doUpdates)
    return;

  GLfloat LightAmbient[]= { 0.01f, 0.01f, 0.01f, 1.0f };
  GLfloat LightDiffuse[]= { 1.5f, 1.5f, 1.5f, 1.0f };
  GLfloat LightPosition[]= { 700.0f, 200.0f, -90.0f, 1.0f };

  GLfloat AmbientParams[] = {0.1, 0.1, 0.1, 1};
  GLfloat DiffuseParams[] = {0.7, 0.7, 0.7, 0.1};
  GLfloat SpecularParams[] = {0.4, 0.4, 0.4, 0.5};

  if (!valid()) {
    glLoadIdentity();
    glViewport(0,0,w(),h());

    if (_useLightning)
      glEnable(GL_LIGHTING);
    else
      glDisable(GL_LIGHTING);

    glEnable(GL_COLOR_MATERIAL);
    glLightfv(GL_LIGHT1, GL_AMBIENT, LightAmbient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, LightDiffuse);
    glLightfv(GL_LIGHT1, GL_POSITION, LightPosition);
    glEnable(GL_LIGHT1);

    glShadeModel(GL_FLAT);
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMaterialf(GL_FRONT, GL_SHININESS, 0.5);
    glMaterialfv(GL_FRONT, GL_AMBIENT, AmbientParams);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, DiffuseParams);
    glMaterialfv(GL_FRONT, GL_SPECULAR, SpecularParams);

    arcBall->setBounds(w(), h());
  }

  {
    unsigned char r, g, b;
    Fl::get_color(color(), r, g, b);
    glClearColor(r/255.0, g/255.0, b/255.0, 0);
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glPushMatrix();
  gluPerspective(5+size, 1.0*w()/h(), 10, 1100);
  glTranslatef(0, 0, -50);
  drawVoxelSpace();
  glPopMatrix();
}

int VoxelView::handle(int event) {

  if (Fl_Gl_Window::handle(event))
    return 1;

  switch(event) {
  case FL_PUSH:
    arcBall->click(Fl::event_x(), Fl::event_y());
    return 1;

  case FL_DRAG:
    arcBall->drag(Fl::event_x(), Fl::event_y());
    redraw();
    return 1;

  case FL_RELEASE:
    arcBall->clack(Fl::event_x(), Fl::event_y());
    return 1;
  }

  return 0;
}

unsigned int VoxelView::addSpace(const voxel_c * vx) {
  shapeInfo i;

  i.r = i.g = i.b = 1;
  i.a = 1;
  i.shape = vx;

  i.mode = normal;

  i.x = i.y = i.z = 0;
  i.scale = 1;

  shapes.push_back(i);

  redraw();

  return shapes.size()-1;
}

void VoxelView::clearSpaces(void) {

  for (unsigned int i = 0; i < shapes.size(); i++)
    delete shapes[i].shape;

  shapes.clear();
  redraw();
}

unsigned int VoxelView::spaceNumber(void) {
  return shapes.size();
}

void VoxelView::setSpaceColor(unsigned int nr, float r, float g, float b, float a) {
  shapes[nr].r = r;
  shapes[nr].g = g;
  shapes[nr].b = b;
  shapes[nr].a = a;

  redraw();
}

void VoxelView::setSpaceColor(unsigned int nr, float a) {
  shapes[nr].a = a;

  redraw();
}

void VoxelView::setSpacePosition(unsigned int nr, float x, float y, float z, float scale) {
  shapes[nr].x = x;
  shapes[nr].y = y;
  shapes[nr].z = z;
  shapes[nr].scale = scale;
  redraw();
}

void VoxelView::setDrawingMode(unsigned int nr, drawingMode mode) {
  shapes[nr].mode = mode;
  redraw();
}

void VoxelView::setColorMode(colorMode color) {
  colors = color;
  redraw();
}

void VoxelView::setScaling(float factor) {
  scale = factor;
  redraw();
}

void VoxelView::setTransformationType(transformationType type) {
  trans = type;
  redraw();
}

void VoxelView::update(bool doIt) {
  doUpdates = doIt;
  if (doIt)
    redraw();
}

void VoxelView::addPaletteEntry(float r, float g, float b) {

  colorInfo ci;

  ci.r = r;
  ci.g = g;
  ci.b = b;

  palette.push_back(ci);
}

void VoxelView::setMarker(int x1, int y1, int x2, int y2, int z) {
  markerType = true;
  mX1 = x1;
  mY1 = y1;
  mX2 = x2+1;
  mY2 = y2+1;
  mZ = z;
}

void VoxelView::hideMarker(void) {
  markerType = false;
}

