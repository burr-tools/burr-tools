#include "VoxelView.h"
#include <math.h>

#include "GL/gl.h"

#include <stdlib.h>

#include "../lib/puzzle.h"

#include "pieceColor.h"


VoxelView::VoxelView(int x,int y,int w,int h,const char *l)
#ifndef DEBUG
  : Fl_Gl_Window(x,y,w,h,l),
#else
  : Fl_Widget(x,y,w,h,l),
#endif
  size(10), space(0), mode(singleMode), markerType(false), arcBall(new ArcBall_c(w, h))
{
};

void VoxelView::setColor(voxel_type piece, int p, float alpha) {

  if (mode == multiMode) {

    piece <<= 1;

    // simple method for the beginning
    if (p & 1)
      glColor4f(darkPieceColor(pieceColorR(colArray[piece], colArray[piece + 1])),
                darkPieceColor(pieceColorG(colArray[piece], colArray[piece + 1])),
                darkPieceColor(pieceColorB(colArray[piece], colArray[piece + 1])), alpha);
    else
      glColor4f(lightPieceColor(pieceColorR(colArray[piece], colArray[piece + 1])),
                lightPieceColor(pieceColorG(colArray[piece], colArray[piece + 1])),
                lightPieceColor(pieceColorB(colArray[piece], colArray[piece + 1])), alpha);
  } else

    if (p & 1)
      glColor4f(pieceColorR(piece) * 0.9,
                pieceColorG(piece) * 0.9,
                pieceColorB(piece) * 0.9, alpha);
    else
      glColor4f(1-((1-pieceColorR(piece)) * 0.9),
                1-((1-pieceColorG(piece)) * 0.9),
                1-((1-pieceColorB(piece)) * 0.9), alpha);
}

static void drawBox(const voxel_c * space, int x, int y, int z) {

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

  glColor4f(0, 0, 0, 1);

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

void VoxelView::drawVoxelSpace() {

#ifndef DEBUG

/* Draw a colored cube */
  glShadeModel(GL_FLAT);

  if (!space) return;

  glTranslatef(-space->getX()/2.0, -space->getY()/2.0, -space->getZ()/2.0);

  glDisable(GL_LIGHTING);
  glDisable(GL_BLEND);
  glBegin(GL_LINES);
  glColor3f(1, 0, 0);
  glVertex3f(-1, -1, -1); glVertex3f(space->getX()+1, -1, -1);
  glColor3f(0, 1, 0);
  glVertex3f(-1, -1, -1); glVertex3f(-1, space->getY()+1, -1);
  glColor3f(0, 0, 1);
  glVertex3f(-1, -1, -1); glVertex3f(-1, -1, space->getZ()+1);
  glEnd();
  glEnable(GL_LIGHTING);
  glEnable(GL_BLEND);

  for (int x = 0; x < space->getX(); x++)
    for (int y = 0; y < space->getY(); y++)
      for (int z = 0; z < space->getZ(); z++) {
        voxel_type p = space->get(x, y, z);
        enum {
          box,
          grid,
          variable,
          nothing
        } toDraw;

        if (p != VX_EMPTY) {
          if (mode == multiMode) {
            setColor(p-1, x+y+z, shiftArray[4*p-1]);
            glPushMatrix();
            if (space->get(x, y, z) <= arraySize)
              glTranslatef(shiftArray[4*p-4], shiftArray[4*p-3], shiftArray[4*p-2]);

            switch(visArray[p-1]) {
            case 0: toDraw = box; break;
            case 1: toDraw = grid; break;
            case 2: toDraw = nothing; break;
            }
            if (shiftArray[4*p-1] == 0)
              toDraw = nothing;
          } else {
            setColor(pieceNumber, x+y+z, 1);
            if (p == VX_FILLED)
              toDraw = box;
            else
              toDraw = variable;
          }

          switch (toDraw) {
          case box:
            drawBox(space, x, y, z);
            break;
          case grid:
            glDisable(GL_LIGHTING);
            glBegin(GL_LINES);

            if ((space->get2(x, y-1, z) != p) && (space->get2(x, y, z-1) != p)) { glVertex3f(x  , y  , z  ); glVertex3f(x+1, y  , z  ); }
            if (((space->get2(x, y-1, z) == p) ^ (space->get2(x, y, z-1) == p)) && (space->get2(x, y-1, z-1) == p)) { glVertex3f(x  , y  , z  ); glVertex3f(x+1, y  , z  ); }

            if ((space->get2(x, y-1, z) != p) && (space->get2(x, y, z+1) != p)) { glVertex3f(x  , y  , z+1); glVertex3f(x+1, y  , z+1); }
            if (((space->get2(x, y-1, z) == p) ^ (space->get2(x, y, z+1) == p)) && (space->get2(x, y-1, z+1) == p)){ glVertex3f(x  , y  , z+1); glVertex3f(x+1, y  , z+1); }

            if ((space->get2(x, y+1, z) != p) && (space->get2(x, y, z+1) != p)) { glVertex3f(x  , y+1, z+1); glVertex3f(x+1, y+1, z+1); }
            if (((space->get2(x, y+1, z) == p) ^ (space->get2(x, y, z+1) == p)) && (space->get2(x, y+1, z+1) == p)) { glVertex3f(x  , y+1, z+1); glVertex3f(x+1, y+1, z+1); }

            if ((space->get2(x, y+1, z) != p) && (space->get2(x, y, z-1) != p)) { glVertex3f(x  , y+1, z  ); glVertex3f(x+1, y+1, z  ); }
            if (((space->get2(x, y+1, z) == p) ^ (space->get2(x, y, z-1) == p)) && (space->get2(x, y+1, z-1) == p)) { glVertex3f(x  , y+1, z  ); glVertex3f(x+1, y+1, z  ); }

            if ((space->get2(x-1, y, z) != p) && (space->get2(x, y, z-1) != p)) { glVertex3f(x  , y  , z  ); glVertex3f(x  , y+1, z  ); }
            if (((space->get2(x-1, y, z) == p) ^ (space->get2(x, y, z-1) == p)) && (space->get2(x-1, y, z-1) == p)) { glVertex3f(x  , y  , z  ); glVertex3f(x  , y+1, z  ); }

            if ((space->get2(x-1, y, z) != p) && (space->get2(x, y, z+1) != p)) { glVertex3f(x  , y  , z+1); glVertex3f(x  , y+1, z+1); }
            if (((space->get2(x-1, y, z) == p) ^ (space->get2(x, y, z+1) == p)) && (space->get2(x-1, y, z+1) == p)) { glVertex3f(x  , y  , z+1); glVertex3f(x  , y+1, z+1); }

            if ((space->get2(x+1, y, z) != p) && (space->get2(x, y, z+1) != p)) { glVertex3f(x+1, y  , z+1); glVertex3f(x+1, y+1, z+1); }
            if (((space->get2(x+1, y, z) == p) ^ (space->get2(x, y, z+1) == p)) && (space->get2(x+1, y, z+1) == p)) { glVertex3f(x+1, y  , z+1); glVertex3f(x+1, y+1, z+1); }

            if ((space->get2(x+1, y, z) != p) && (space->get2(x, y, z-1) != p)) { glVertex3f(x+1, y  , z  ); glVertex3f(x+1, y+1, z  ); }
            if (((space->get2(x+1, y, z) == p) ^ (space->get2(x, y, z-1) == p)) && (space->get2(x+1, y, z-1) == p)) { glVertex3f(x+1, y  , z  ); glVertex3f(x+1, y+1, z  ); }

            if ((space->get2(x-1, y, z) != p) && (space->get2(x, y-1, z) != p)) { glVertex3f(x  , y  , z  ); glVertex3f(x  , y  , z+1); }
            if (((space->get2(x-1, y, z) == p) ^ (space->get2(x, y-1, z) == p)) && (space->get2(x-1, y-1, z) == p)) { glVertex3f(x  , y  , z  ); glVertex3f(x  , y  , z+1); }

            if ((space->get2(x-1, y, z) != p) && (space->get2(x, y+1, z) != p)) { glVertex3f(x  , y+1, z  ); glVertex3f(x  , y+1, z+1); }
            if (((space->get2(x-1, y, z) == p) ^ (space->get2(x, y+1, z) == p)) && (space->get2(x-1, y+1, z) == p)) { glVertex3f(x  , y+1, z  ); glVertex3f(x  , y+1, z+1); }

            if ((space->get2(x+1, y, z) != p) && (space->get2(x, y+1, z) != p)) { glVertex3f(x+1, y+1, z  ); glVertex3f(x+1, y+1, z+1); }
            if (((space->get2(x+1, y, z) == p) ^ (space->get2(x, y+1, z) == p)) && (space->get2(x+1, y+1, z) == p)) { glVertex3f(x+1, y+1, z  ); glVertex3f(x+1, y+1, z+1); }

            if ((space->get2(x+1, y, z) != p) && (space->get2(x, y-1, z) != p)) { glVertex3f(x+1, y  , z  ); glVertex3f(x+1, y  , z+1); }
            if (((space->get2(x+1, y, z) == p) ^ (space->get2(x, y-1, z) == p)) && (space->get2(x+1, y-1, z) == p)) { glVertex3f(x+1, y  , z  ); glVertex3f(x+1, y  , z+1); }

            glEnd();
            glEnable(GL_LIGHTING);
            break;
          case variable:
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
            break;
          }

          if (mode == multiMode) {
            glPopMatrix();
          }
        }
      }

  if ((mode == singleMode) && (markerType)) {
    glColor4f(0.5, 0.5, 0.5, 0.5);
    glDisable(GL_LIGHTING);
    glBegin(GL_QUADS);

    glNormal3f( 0.0f, 0.0f, 1.0f);
    glVertex3f(-1             , -1             , mZ+0.5);
    glVertex3f(-1             , space->getY()+1, mZ+0.5);
    glVertex3f(space->getX()+1, space->getY()+1, mZ+0.5);
    glVertex3f(space->getX()+1, -1             , mZ+0.5);
    glEnd();
    glEnable(GL_LIGHTING);
  }

#endif

};



static void gluPerspective(double fovy, double aspect, double zNear, double zFar) {

#ifndef DEBUG
   double xmin, xmax, ymin, ymax;
   ymax = zNear * tan(fovy * 3.1415927 / 360.0);
   ymin = -ymax;
   xmin = ymin * aspect;
   xmax = ymax * aspect;
   glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
#endif
}

void VoxelView::draw() {

#ifndef DEBUG
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
    //0.94, 0.92, 0.94, 0);
  }
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glPushMatrix();
  gluPerspective(5+size, 1.0*w()/h(), 10, 1100);
  glTranslatef(0, 0, -50);
  arcBall->addTransform();
  drawVoxelSpace();
  glPopMatrix();
#endif
};

int VoxelView::handle(int event) {

#ifndef DEBUG
  Fl_Gl_Window::handle(event);
#else
  if (Fl_Widget::handle(event))
    return 1;
#endif

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

void VoxelView::setVoxelSpace(const voxel_c *sp, int pn) {
  space = sp;
  pieceNumber = pn;
  mode = singleMode;
  redraw();
}

void VoxelView::setVoxelSpace(const voxel_c *sp, float * shArray, char * vArray, int numPieces, int * colors) {
  mode = multiMode;

  shiftArray = shArray;
  visArray = vArray;
  arraySize = numPieces;
  colArray = colors;

  space = sp;

  redraw();
}

