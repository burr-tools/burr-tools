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

#include "GL/gl.h"

#include <stdlib.h>

#include "../lib/puzzle.h"

#include "pieceColor.h"


VoxelView::VoxelView(int x,int y,int w,int h,const char *l) : Fl_Gl_Window(x,y,w,h,l),
  arcBall(new ArcBall_c(w, h)), size(10), markerType(false), doUpdates(true)
{
};

// draws a box with borders depending on the neibor boxes
static void drawBox(const voxel_c * space, int x, int y, int z, float alpha) {

#define EDGELO 0.05f
#define EDGEHI 0.95f

  GLfloat a0, b0, a1, b1;

  voxel_type p = space->get(x, y, z);

  glBegin(GL_QUADS);
  if (space->get2(x, y, z-1) != p) { 
    glNormal3f( 0.0f, 0.0f, -1.0f);

    if (space->get2(x-1, y, z) == p) a0 = 0; else a0 = EDGELO;
    if (space->get2(x+1, y, z) == p) a1 = 1; else a1 = EDGEHI;
    if (space->get2(x, y-1, z) == p) b0 = 0; else b0 = EDGELO;
    if (space->get2(x, y+1, z) == p) b1 = 1; else b1 = EDGEHI;

    glVertex3f(x+a0, y+b0, z); glVertex3f(x+a0, y+b1, z); glVertex3f(x+a1, y+b1, z); glVertex3f(x+a1, y+b0, z);
  }
  if (space->get2(x-1, y, z) != p) {
    glNormal3f( -1.0f, 0.0f, 0.0f);

    if (space->get2(x, y-1, z) == p) a0 = 0; else a0 = EDGELO;
    if (space->get2(x, y+1, z) == p) a1 = 1; else a1 = EDGEHI;
    if (space->get2(x, y, z-1) == p) b0 = 0; else b0 = EDGELO;
    if (space->get2(x, y, z+1) == p) b1 = 1; else b1 = EDGEHI;

    glVertex3f(x, y+a0, z+b0); glVertex3f(x, y+a0, z+b1); glVertex3f(x, y+a1, z+b1); glVertex3f(x, y+a1, z+b0);
  }
  if (space->get2(x+1, y, z) != p) {
    glNormal3f( 1.0f, 0.0f, 0.0f);

    if (space->get2(x, y-1, z) == p) a0 = 0; else a0 = EDGELO;
    if (space->get2(x, y+1, z) == p) a1 = 1; else a1 = EDGEHI;
    if (space->get2(x, y, z-1) == p) b0 = 0; else b0 = EDGELO;
    if (space->get2(x, y, z+1) == p) b1 = 1; else b1 = EDGEHI;

    glVertex3f(x+1, y+a1, z+b0); glVertex3f(x+1, y+a1, z+b1); glVertex3f(x+1, y+a0, z+b1); glVertex3f(x+1, y+a0, z+b0);
  }
  if (space->get2(x, y, z+1) != p) {
    glNormal3f( 0.0f, 0.0f, 1.0f);

    if (space->get2(x-1, y, z) == p) a0 = 0; else a0 = EDGELO;
    if (space->get2(x+1, y, z) == p) a1 = 1; else a1 = EDGEHI;
    if (space->get2(x, y-1, z) == p) b0 = 0; else b0 = EDGELO;
    if (space->get2(x, y+1, z) == p) b1 = 1; else b1 = EDGEHI;

    glVertex3f(x+a0, y+b0, z+1); glVertex3f(x+a0, y+b1, z+1); glVertex3f(x+a1, y+b1, z+1); glVertex3f(x+a1, y+b0, z+1);
  }
  if (space->get2(x, y-1, z) != p) {
    glNormal3f( 0.0f, -1.0f, 0.0f);

    if (space->get2(x-1, y, z) == p) a0 = 0; else a0 = EDGELO;
    if (space->get2(x+1, y, z) == p) a1 = 1; else a1 = EDGEHI;
    if (space->get2(x, y, z-1) == p) b0 = 0; else b0 = EDGELO;
    if (space->get2(x, y, z+1) == p) b1 = 1; else b1 = EDGEHI;

    glVertex3f(x+a0, y  , z+b0); glVertex3f(x+a1, y  , z+b0); glVertex3f(x+a1, y  , z+b1); glVertex3f(x+a0, y  , z+b1);
  }
  if (space->get2(x, y+1, z) != p) {
    glNormal3f( 0.0f, 1.0f, 0.0f);

    if (space->get2(x-1, y, z) == p) a0 = 0; else a0 = EDGELO;
    if (space->get2(x+1, y, z) == p) a1 = 1; else a1 = EDGEHI;
    if (space->get2(x, y, z-1) == p) b0 = 0; else b0 = EDGELO;
    if (space->get2(x, y, z+1) == p) b1 = 1; else b1 = EDGEHI;

    glVertex3f(x+a0, y+1, z+b0); glVertex3f(x+a0, y+1, z+b1); glVertex3f(x+a1, y+1, z+b1); glVertex3f(x+a1, y+1, z+b0);
  }

  glColor4f(0, 0, 0, alpha);

  if (space->get2(x, y, z-1) != p) {
    glNormal3f( 0.0f, 0.0f, -1.0f);

    if (space->get2(x-1, y, z) != p) { glVertex3f(x+0, y+0, z); glVertex3f(x+EDGELO, y+0, z); glVertex3f(x+EDGELO, y+1, z); glVertex3f(x+0, y+1, z); }
    if (space->get2(x+1, y, z) != p) { glVertex3f(x+1, y+0, z); glVertex3f(x+EDGEHI, y+0, z); glVertex3f(x+EDGEHI, y+1, z); glVertex3f(x+1, y+1, z); }
    if (space->get2(x, y-1, z) != p) { glVertex3f(x+0, y+0, z); glVertex3f(x+0, y+EDGELO, z); glVertex3f(x+1, y+EDGELO, z); glVertex3f(x+1, y+0, z); }
    if (space->get2(x, y+1, z) != p) { glVertex3f(x+0, y+1, z); glVertex3f(x+0, y+EDGEHI, z); glVertex3f(x+1, y+EDGEHI, z); glVertex3f(x+1, y+1, z); }
  }
  if (space->get2(x-1, y, z) != p) {
    glNormal3f( -1.0f, 0.0f, 0.0f);

    if (space->get2(x, y-1, z) != p) { glVertex3f(x, y+0, z+0); glVertex3f(x, y+EDGELO, z+0); glVertex3f(x, y+EDGELO, z+1); glVertex3f(x, y+0, z+1); }
    if (space->get2(x, y+1, z) != p) { glVertex3f(x, y+1, z+0); glVertex3f(x, y+1, z+1); glVertex3f(x, y+EDGEHI, z+1); glVertex3f(x, y+EDGEHI, z+0); }
    if (space->get2(x, y, z-1) != p) { glVertex3f(x, y+0, z+EDGELO); glVertex3f(x, y+0, z+0); glVertex3f(x, y+1, z+0); glVertex3f(x, y+1, z+EDGELO); }
    if (space->get2(x, y, z+1) != p) { glVertex3f(x, y+0, z+1); glVertex3f(x, y+0, z+EDGEHI); glVertex3f(x, y+1, z+EDGEHI); glVertex3f(x, y+1, z+1); }
  }
  if (space->get2(x+1, y, z) != p) {
    glNormal3f( 1.0f, 0.0f, 0.0f);

    if (space->get2(x, y-1, z) != p) { glVertex3f(x+1, y+0, z+0); glVertex3f(x+1, y+0, z+1); glVertex3f(x+1, y+EDGELO, z+1); glVertex3f(x+1, y+EDGELO, z+0); }
    if (space->get2(x, y+1, z) != p) { glVertex3f(x+1, y+EDGEHI, z+0); glVertex3f(x+1, y+EDGEHI, z+1); glVertex3f(x+1, y+1, z+1); glVertex3f(x+1, y+1, z+0); }
    if (space->get2(x, y, z-1) != p) { glVertex3f(x+1, y+1, z+EDGELO); glVertex3f(x+1, y+1, z+0); glVertex3f(x+1, y+0, z+0); glVertex3f(x+1, y+0, z+EDGELO); }
    if (space->get2(x, y, z+1) != p) { glVertex3f(x+1, y+1, z+1); glVertex3f(x+1, y+1, z+EDGEHI); glVertex3f(x+1, y+0, z+EDGEHI); glVertex3f(x+1, y+0, z+1); }
  }
  if (space->get2(x, y, z+1) != p) {
    glNormal3f( 0.0f, 0.0f, 1.0f);

    if (space->get2(x-1, y, z) != p) { glVertex3f(x+EDGELO, y+0, z+1); glVertex3f(x+EDGELO, y+1, z+1); glVertex3f(x+0, y+1, z+1); glVertex3f(x+0, y+0, z+1); }
    if (space->get2(x+1, y, z) != p) { glVertex3f(x+1, y+0, z+1); glVertex3f(x+1, y+1, z+1); glVertex3f(x+EDGEHI, y+1, z+1); glVertex3f(x+EDGEHI, y+0, z+1); }
    if (space->get2(x, y-1, z) != p) { glVertex3f(x+0, y+EDGELO, z+1); glVertex3f(x+0, y+0, z+1); glVertex3f(x+1, y+0, z+1); glVertex3f(x+1, y+EDGELO, z+1); }
    if (space->get2(x, y+1, z) != p) { glVertex3f(x+0, y+1, z+1); glVertex3f(x+0, y+EDGEHI, z+1); glVertex3f(x+1, y+EDGEHI, z+1); glVertex3f(x+1, y+1, z+1); }
  }
  if (space->get2(x, y-1, z) != p) {
    glNormal3f( 0.0f, -1.0f, 0.0f);

    if (space->get2(x-1, y, z) != p) { glVertex3f(x+EDGELO, y, z+0); glVertex3f(x+0, y, z+0); glVertex3f(x+0, y, z+1); glVertex3f(x+EDGELO, y, z+1); }
    if (space->get2(x+1, y, z) != p) { glVertex3f(x+1, y, z+0); glVertex3f(x+EDGEHI, y, z+0); glVertex3f(x+EDGEHI, y, z+1); glVertex3f(x+1, y, z+1); }
    if (space->get2(x, y, z-1) != p) { glVertex3f(x+0, y, z+EDGELO); glVertex3f(x+1, y, z+EDGELO); glVertex3f(x+1, y, z+0); glVertex3f(x+0, y, z+0); }
    if (space->get2(x, y, z+1) != p) { glVertex3f(x+0, y, z+1); glVertex3f(x+1, y, z+1); glVertex3f(x+1, y, z+EDGEHI); glVertex3f(x+0, y, z+EDGEHI); }
  }
  if (space->get2(x, y+1, z) != p) {
    glNormal3f( 0.0f, 1.0f, 0.0f);

    if (space->get2(x-1, y, z) != p) { glVertex3f(x+EDGELO, y+1, z+0); glVertex3f(x+EDGELO, y+1, z+1); glVertex3f(x+0, y+1, z+1); glVertex3f(x+0, y+1, z+0); }
    if (space->get2(x+1, y, z) != p) { glVertex3f(x+1, y+1, z+0); glVertex3f(x+1, y+1, z+1); glVertex3f(x+EDGEHI, y+1, z+1); glVertex3f(x+EDGEHI, y+1, z+0); }
    if (space->get2(x, y, z-1) != p) { glVertex3f(x+0, y+1, z+EDGELO); glVertex3f(x+0, y+1, z+0); glVertex3f(x+1, y+1, z+0); glVertex3f(x+1, y+1, z+EDGELO); }
    if (space->get2(x, y, z+1) != p) { glVertex3f(x+0, y+1, z+1); glVertex3f(x+0, y+1, z+EDGEHI); glVertex3f(x+1, y+1, z+EDGEHI); glVertex3f(x+1, y+1, z+1); }
  }
  glEnd();
}

// draws a wireframe box depending on the neibors
static void drawFrame(const voxel_c * space, int x, int y, int z) {


  voxel_type p = space->get(x, y, z);

  glBegin(GL_QUADS);

  if (space->get2(x, y, z-1) != p) {
    glNormal3f( 0.0f, 0.0f, -1.0f);

    if (space->get2(x-1, y, z) != p) { glVertex3f(x+0, y+0, z); glVertex3f(x+EDGELO, y+0, z); glVertex3f(x+EDGELO, y+1, z); glVertex3f(x+0, y+1, z); }
    if (space->get2(x+1, y, z) != p) { glVertex3f(x+1, y+0, z); glVertex3f(x+EDGEHI, y+0, z); glVertex3f(x+EDGEHI, y+1, z); glVertex3f(x+1, y+1, z); }
    if (space->get2(x, y-1, z) != p) { glVertex3f(x+0, y+0, z); glVertex3f(x+0, y+EDGELO, z); glVertex3f(x+1, y+EDGELO, z); glVertex3f(x+1, y+0, z); }
    if (space->get2(x, y+1, z) != p) { glVertex3f(x+0, y+1, z); glVertex3f(x+0, y+EDGEHI, z); glVertex3f(x+1, y+EDGEHI, z); glVertex3f(x+1, y+1, z); }
  }
  if (space->get2(x-1, y, z) != p) {
    glNormal3f( -1.0f, 0.0f, 0.0f);

    if (space->get2(x, y-1, z) != p) { glVertex3f(x, y+0, z+0); glVertex3f(x, y+EDGELO, z+0); glVertex3f(x, y+EDGELO, z+1); glVertex3f(x, y+0, z+1); }
    if (space->get2(x, y+1, z) != p) { glVertex3f(x, y+1, z+0); glVertex3f(x, y+1, z+1); glVertex3f(x, y+EDGEHI, z+1); glVertex3f(x, y+EDGEHI, z+0); }
    if (space->get2(x, y, z-1) != p) { glVertex3f(x, y+0, z+EDGELO); glVertex3f(x, y+0, z+0); glVertex3f(x, y+1, z+0); glVertex3f(x, y+1, z+EDGELO); }
    if (space->get2(x, y, z+1) != p) { glVertex3f(x, y+0, z+1); glVertex3f(x, y+0, z+EDGEHI); glVertex3f(x, y+1, z+EDGEHI); glVertex3f(x, y+1, z+1); }
  }
  if (space->get2(x+1, y, z) != p) {
    glNormal3f( 1.0f, 0.0f, 0.0f);

    if (space->get2(x, y-1, z) != p) { glVertex3f(x+1, y+0, z+0); glVertex3f(x+1, y+0, z+1); glVertex3f(x+1, y+EDGELO, z+1); glVertex3f(x+1, y+EDGELO, z+0); }
    if (space->get2(x, y+1, z) != p) { glVertex3f(x+1, y+EDGEHI, z+0); glVertex3f(x+1, y+EDGEHI, z+1); glVertex3f(x+1, y+1, z+1); glVertex3f(x+1, y+1, z+0); }
    if (space->get2(x, y, z-1) != p) { glVertex3f(x+1, y+1, z+EDGELO); glVertex3f(x+1, y+1, z+0); glVertex3f(x+1, y+0, z+0); glVertex3f(x+1, y+0, z+EDGELO); }
    if (space->get2(x, y, z+1) != p) { glVertex3f(x+1, y+1, z+1); glVertex3f(x+1, y+1, z+EDGEHI); glVertex3f(x+1, y+0, z+EDGEHI); glVertex3f(x+1, y+0, z+1); }
  }
  if (space->get2(x, y, z+1) != p) {
    glNormal3f( 0.0f, 0.0f, 1.0f);

    if (space->get2(x-1, y, z) != p) { glVertex3f(x+EDGELO, y+0, z+1); glVertex3f(x+EDGELO, y+1, z+1); glVertex3f(x+0, y+1, z+1); glVertex3f(x+0, y+0, z+1); }
    if (space->get2(x+1, y, z) != p) { glVertex3f(x+1, y+0, z+1); glVertex3f(x+1, y+1, z+1); glVertex3f(x+EDGEHI, y+1, z+1); glVertex3f(x+EDGEHI, y+0, z+1); }
    if (space->get2(x, y-1, z) != p) { glVertex3f(x+0, y+EDGELO, z+1); glVertex3f(x+0, y+0, z+1); glVertex3f(x+1, y+0, z+1); glVertex3f(x+1, y+EDGELO, z+1); }
    if (space->get2(x, y+1, z) != p) { glVertex3f(x+0, y+1, z+1); glVertex3f(x+0, y+EDGEHI, z+1); glVertex3f(x+1, y+EDGEHI, z+1); glVertex3f(x+1, y+1, z+1); }
  }
  if (space->get2(x, y-1, z) != p) {
    glNormal3f( 0.0f, -1.0f, 0.0f);

    if (space->get2(x-1, y, z) != p) { glVertex3f(x+EDGELO, y, z+0); glVertex3f(x+0, y, z+0); glVertex3f(x+0, y, z+1); glVertex3f(x+EDGELO, y, z+1); }
    if (space->get2(x+1, y, z) != p) { glVertex3f(x+1, y, z+0); glVertex3f(x+EDGEHI, y, z+0); glVertex3f(x+EDGEHI, y, z+1); glVertex3f(x+1, y, z+1); }
    if (space->get2(x, y, z-1) != p) { glVertex3f(x+0, y, z+EDGELO); glVertex3f(x+1, y, z+EDGELO); glVertex3f(x+1, y, z+0); glVertex3f(x+0, y, z+0); }
    if (space->get2(x, y, z+1) != p) { glVertex3f(x+0, y, z+1); glVertex3f(x+1, y, z+1); glVertex3f(x+1, y, z+EDGEHI); glVertex3f(x+0, y, z+EDGEHI); }
  }
  if (space->get2(x, y+1, z) != p) {
    glNormal3f( 0.0f, 1.0f, 0.0f);

    if (space->get2(x-1, y, z) != p) { glVertex3f(x+EDGELO, y+1, z+0); glVertex3f(x+EDGELO, y+1, z+1); glVertex3f(x+0, y+1, z+1); glVertex3f(x+0, y+1, z+0); }
    if (space->get2(x+1, y, z) != p) { glVertex3f(x+1, y+1, z+0); glVertex3f(x+1, y+1, z+1); glVertex3f(x+EDGEHI, y+1, z+1); glVertex3f(x+EDGEHI, y+1, z+0); }
    if (space->get2(x, y, z-1) != p) { glVertex3f(x+0, y+1, z+EDGELO); glVertex3f(x+0, y+1, z+0); glVertex3f(x+1, y+1, z+0); glVertex3f(x+1, y+1, z+EDGELO); }
    if (space->get2(x, y, z+1) != p) { glVertex3f(x+0, y+1, z+1); glVertex3f(x+0, y+1, z+EDGEHI); glVertex3f(x+1, y+1, z+EDGEHI); glVertex3f(x+1, y+1, z+1); }
  }

  glEnd();
}

// draw a bube that is smaller than 1
static void drawCube(const voxel_c * space, int x, int y, int z) {
  glBegin(GL_QUADS);
  glNormal3f( 0.0f, 0.0f, -1.0f);
  glVertex3f(x+0.2, y+0.2, z+0.2); glVertex3f(x+0.2, y+0.8, z+0.2); glVertex3f(x+0.8, y+0.8, z+0.2); glVertex3f(x+0.8, y+0.2, z+0.2);
  glNormal3f( -1.0f, 0.0f, 0.0f);
  glVertex3f(x+0.2, y+0.2, z+0.2); glVertex3f(x+0.2, y+0.2, z+0.8); glVertex3f(x+0.2, y+0.8, z+0.8); glVertex3f(x+0.2, y+0.8, z+0.2);
  glNormal3f( 1.0f, 0.0f, 0.0f);
  glVertex3f(x+0.8, y+0.8, z+0.2); glVertex3f(x+0.8, y+0.8, z+0.8); glVertex3f(x+0.8, y+0.2, z+0.8); glVertex3f(x+0.8, y+0.2, z+0.2);
  glNormal3f( 0.0f, 0.0f, 1.0f);
  glVertex3f(x+0.2, y+0.2, z+0.8); glVertex3f(x+0.2, y+0.8, z+0.8); glVertex3f(x+0.8, y+0.8, z+0.8); glVertex3f(x+0.8, y+0.2, z+0.8);
  glNormal3f( 0.0f, -1.0f, 0.0f);
  glVertex3f(x+0.2, y+0.2, z+0.2); glVertex3f(x+0.8, y+0.2, z+0.2); glVertex3f(x+0.8, y+0.2, z+0.8); glVertex3f(x+0.2, y+0.2, z+0.8);
  glNormal3f( 0.0f, 1.0f, 0.0f);
  glVertex3f(x+0.2, y+0.8, z+0.2); glVertex3f(x+0.2, y+0.8, z+0.8); glVertex3f(x+0.8, y+0.8, z+0.8); glVertex3f(x+0.8, y+0.8, z+0.2);
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
      glDisable(GL_LIGHTING);
      glDisable(GL_BLEND);
      glBegin(GL_LINES);
      glColor3f(1, 0, 0);
      glVertex3f(-1, -1, -1); glVertex3f(shapes[piece].shape->getX()+1, -1, -1);
      glColor3f(0, 1, 0);
      glVertex3f(-1, -1, -1); glVertex3f(-1, shapes[piece].shape->getY()+1, -1);
      glColor3f(0, 0, 1);
      glVertex3f(-1, -1, -1); glVertex3f(-1, -1, shapes[piece].shape->getZ()+1);
      glEnd();
      glEnable(GL_LIGHTING);
      glEnable(GL_BLEND);
    }

    for (unsigned int x = 0; x < shapes[piece].shape->getX(); x++)
      for (unsigned int y = 0; y < shapes[piece].shape->getY(); y++)
        for (unsigned int z = 0; z < shapes[piece].shape->getZ(); z++) {
  
          if (shapes[piece].shape->getState(x, y , z) == pieceVoxel_c::VX_EMPTY)
            continue;

          if ((x+y+z) & 1)
            glColor4f(shapes[piece].r, shapes[piece].g, shapes[piece].b, shapes[piece].a);
          else
            glColor4f(shapes[piece].r*0.9, shapes[piece].g*0.9, shapes[piece].b*0.9, shapes[piece].a);

          switch (shapes[piece].mode) {
          case normal:
            if (shapes[piece].shape->getState(x, y , z) == pieceVoxel_c::VX_VARIABLE)
              drawCube(shapes[piece].shape, x, y, z);
            else
              drawBox(shapes[piece].shape, x, y, z, shapes[piece].a);
            break;
          case gridline:
            drawFrame(shapes[piece].shape, x, y, z);
            break;
          case invisible:
            break;
          }
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

    glEnable(GL_LIGHTING);
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

  Fl_Gl_Window::handle(event);

  switch(event) {
  case FL_PUSH:
    arcBall->click(Fl::event_x(), Fl::event_y());
    break;

  case FL_DRAG:
    arcBall->drag(Fl::event_x(), Fl::event_y());
    redraw();
    break;

  case FL_RELEASE:
    arcBall->clack();
    break;
  }

  return 1;
}

unsigned int VoxelView::addSpace(const pieceVoxel_c * vx) {
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

