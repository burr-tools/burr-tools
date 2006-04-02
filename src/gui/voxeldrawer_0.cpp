#include "voxeldrawer_0.h"

#include "pieceColor.h"

#include "../lib/voxel.h"

#include <math.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

// this is used to shift one side of the cubes so that they slightly differ
// from the side of the next cube, so that (in case of frames) the sides
// are clearly separated and dont't interlock when drawing
#define MY 0.005f

// draws a wireframe box depending on the neibors
void voxelDrawer_0_c::drawFrame(const voxel_c * space, int x, int y, int z, float edge) {

  if (fabs(edge) < 0.00001) return;

  glBegin(GL_QUADS);

  if (space->isEmpty2(x, y, z-1)) {
    glNormal3f( 0.0f, 0.0f, -1.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z-1)) { glVertex3f(x+0, y+0, z); glVertex3f(x+edge, y+0, z); glVertex3f(x+edge, y+1, z); glVertex3f(x+0, y+1, z); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z-1)) { glVertex3f(x+1, y+0, z); glVertex3f(x+(1-edge), y+0, z); glVertex3f(x+(1-edge), y+1, z); glVertex3f(x+1, y+1, z); }
    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x, y-1, z-1)) { glVertex3f(x+0, y+0, z); glVertex3f(x+0, y+edge, z); glVertex3f(x+1, y+edge, z); glVertex3f(x+1, y+0, z); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x, y+1, z-1)) { glVertex3f(x+0, y+1, z); glVertex3f(x+0, y+(1-edge), z); glVertex3f(x+1, y+(1-edge), z); glVertex3f(x+1, y+1, z); }
  }
  if (space->isEmpty2(x-1, y, z)) {
    glNormal3f( -1.0f, 0.0f, 0.0f);

    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x-1, y-1, z)) { glVertex3f(x, y+0, z+0); glVertex3f(x, y+edge, z+0); glVertex3f(x, y+edge, z+1); glVertex3f(x, y+0, z+1); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x-1, y+1, z)) { glVertex3f(x, y+1, z+0); glVertex3f(x, y+1, z+1); glVertex3f(x, y+(1-edge), z+1); glVertex3f(x, y+(1-edge), z+0); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x-1, y, z-1)) { glVertex3f(x, y+0, z+edge); glVertex3f(x, y+0, z+0); glVertex3f(x, y+1, z+0); glVertex3f(x, y+1, z+edge); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x-1, y, z+1)) { glVertex3f(x, y+0, z+1); glVertex3f(x, y+0, z+(1-edge)); glVertex3f(x, y+1, z+(1-edge)); glVertex3f(x, y+1, z+1); }
  }
  if (space->isEmpty2(x+1, y, z)) {
    glNormal3f( 1.0f, 0.0f, 0.0f);

    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x+1, y-1, z)) { glVertex3f(x+1-MY, y+0, z+0); glVertex3f(x+1-MY, y+0, z+1); glVertex3f(x+1-MY, y+edge, z+1); glVertex3f(x+1-MY, y+edge, z+0); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x+1, y+1, z)) { glVertex3f(x+1-MY, y+(1-edge), z+0); glVertex3f(x+1-MY, y+(1-edge), z+1); glVertex3f(x+1-MY, y+1, z+1); glVertex3f(x+1-MY, y+1, z+0); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x+1, y, z-1)) { glVertex3f(x+1-MY, y+1, z+edge); glVertex3f(x+1-MY, y+1, z+0); glVertex3f(x+1-MY, y+0, z+0); glVertex3f(x+1-MY, y+0, z+edge); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x+1, y, z+1)) { glVertex3f(x+1-MY, y+1, z+1); glVertex3f(x+1-MY, y+1, z+(1-edge)); glVertex3f(x+1-MY, y+0, z+(1-edge)); glVertex3f(x+1-MY, y+0, z+1); }
  }
  if (space->isEmpty2(x, y, z+1)) {
    glNormal3f( 0.0f, 0.0f, 1.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z+1)) { glVertex3f(x+edge, y+0, z+1-MY); glVertex3f(x+edge, y+1, z+1-MY); glVertex3f(x+0, y+1, z+1-MY); glVertex3f(x+0, y+0, z+1-MY); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z+1)) { glVertex3f(x+1, y+0, z+1-MY); glVertex3f(x+1, y+1, z+1-MY); glVertex3f(x+(1-edge), y+1, z+1-MY); glVertex3f(x+(1-edge), y+0, z+1-MY); }
    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x, y-1, z+1)) { glVertex3f(x+0, y+edge, z+1-MY); glVertex3f(x+0, y+0, z+1-MY); glVertex3f(x+1, y+0, z+1-MY); glVertex3f(x+1, y+edge, z+1-MY); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x, y+1, z+1)) { glVertex3f(x+0, y+1, z+1-MY); glVertex3f(x+0, y+(1-edge), z+1-MY); glVertex3f(x+1, y+(1-edge), z+1-MY); glVertex3f(x+1, y+1, z+1-MY); }
  }
  if (space->isEmpty2(x, y-1, z)) {
    glNormal3f( 0.0f, -1.0f, 0.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y-1, z)) { glVertex3f(x+edge, y, z+0); glVertex3f(x+0, y, z+0); glVertex3f(x+0, y, z+1); glVertex3f(x+edge, y, z+1); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y-1, z)) { glVertex3f(x+1, y, z+0); glVertex3f(x+(1-edge), y, z+0); glVertex3f(x+(1-edge), y, z+1); glVertex3f(x+1, y, z+1); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x, y-1, z-1)) { glVertex3f(x+0, y, z+edge); glVertex3f(x+1, y, z+edge); glVertex3f(x+1, y, z+0); glVertex3f(x+0, y, z+0); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x, y-1, z+1)) { glVertex3f(x+0, y, z+1); glVertex3f(x+1, y, z+1); glVertex3f(x+1, y, z+(1-edge)); glVertex3f(x+0, y, z+(1-edge)); }
  }
  if (space->isEmpty2(x, y+1, z)) {
    glNormal3f( 0.0f, 1.0f, 0.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y+1, z)) { glVertex3f(x+edge, y+1-MY, z+0); glVertex3f(x+edge, y+1-MY, z+1); glVertex3f(x+0, y+1-MY, z+1); glVertex3f(x+0, y+1-MY, z+0); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y+1, z)) { glVertex3f(x+1, y+1-MY, z+0); glVertex3f(x+1, y+1-MY, z+1); glVertex3f(x+(1-edge), y+1-MY, z+1); glVertex3f(x+(1-edge), y+1-MY, z+0); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x, y+1, z-1)) { glVertex3f(x+0, y+1-MY, z+edge); glVertex3f(x+0, y+1-MY, z+0); glVertex3f(x+1, y+1-MY, z+0); glVertex3f(x+1, y+1-MY, z+edge); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x, y+1, z+1)) { glVertex3f(x+0, y+1-MY, z+1); glVertex3f(x+0, y+1-MY, z+(1-edge)); glVertex3f(x+1, y+1-MY, z+(1-edge)); glVertex3f(x+1, y+1-MY, z+1); }
  }

  glEnd();
}

// draws a box with borders depending on the neibor boxes
void voxelDrawer_0_c::drawNormalVoxel(const voxel_c * space, int x, int y, int z, float alpha, float edge) {

  GLfloat a0, b0, a1, b1;

  glBegin(GL_QUADS);
  if (space->isEmpty2(x, y, z-1)) {
    glNormal3f( 0.0f, 0.0f, -1.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y, z-1)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y, z-1)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x, y-1, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x, y+1, z-1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+a0, y+b0, z); glVertex3f(x+a0, y+b1, z); glVertex3f(x+a1, y+b1, z); glVertex3f(x+a1, y+b0, z);
  }
  if (space->isEmpty2(x-1, y, z)) {
    glNormal3f( -1.0f, 0.0f, 0.0f);

    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x-1, y-1, z)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x-1, y+1, z)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x-1, y, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x-1, y, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x, y+a0, z+b0); glVertex3f(x, y+a0, z+b1); glVertex3f(x, y+a1, z+b1); glVertex3f(x, y+a1, z+b0);
  }
  if (space->isEmpty2(x+1, y, z)) {
    glNormal3f( 1.0f, 0.0f, 0.0f);

    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x+1, y-1, z)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x+1, y+1, z)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x+1, y, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x+1, y, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+1-MY, y+a1, z+b0); glVertex3f(x+1-MY, y+a1, z+b1); glVertex3f(x+1-MY, y+a0, z+b1); glVertex3f(x+1-MY, y+a0, z+b0);
  }
  if (space->isEmpty2(x, y, z+1)) {
    glNormal3f( 0.0f, 0.0f, 1.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y, z+1)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y, z+1)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x, y-1, z+1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x, y+1, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+a0, y+b0, z+1-MY); glVertex3f(x+a0, y+b1, z+1-MY); glVertex3f(x+a1, y+b1, z+1-MY); glVertex3f(x+a1, y+b0, z+1-MY);
  }
  if (space->isEmpty2(x, y-1, z)) {
    glNormal3f( 0.0f, -1.0f, 0.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y-1, z)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y-1, z)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x, y-1, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x, y-1, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+a0, y  , z+b0); glVertex3f(x+a1, y  , z+b0); glVertex3f(x+a1, y  , z+b1); glVertex3f(x+a0, y  , z+b1);
  }
  if (space->isEmpty2(x, y+1, z)) {
    glNormal3f( 0.0f, 1.0f, 0.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y+1, z)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y+1, z)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x, y+1, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x, y+1, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+a0, y+1-MY, z+b0); glVertex3f(x+a0, y+1-MY, z+b1); glVertex3f(x+a1, y+1-MY, z+b1); glVertex3f(x+a1, y+1-MY, z+b0);
  }

  glEnd();

  glColor4f(0, 0, 0, alpha);

  drawFrame(space, x, y, z, edge);
}

// draw a bube that is smaller than 1
void voxelDrawer_0_c::drawVariableMarkers(const voxel_c * space, int x, int y, int z) {
  glBegin(GL_QUADS);
  if (space->isEmpty2(x, y, z-1)) {
    glNormal3f( 0.0f, 0.0f, -1.0f);
    glVertex3f(x+0.2, y+0.2, z-MY); glVertex3f(x+0.2, y+0.8, z-MY); glVertex3f(x+0.8, y+0.8, z-MY); glVertex3f(x+0.8, y+0.2, z-MY);
  }
  if (space->isEmpty2(x-1, y, z)) {
    glNormal3f( -1.0f, 0.0f, 0.0f);
    glVertex3f(x-MY, y+0.2, z+0.2); glVertex3f(x-MY, y+0.2, z+0.8); glVertex3f(x-MY, y+0.8, z+0.8); glVertex3f(x-MY, y+0.8, z+0.2);
  }
  if (space->isEmpty2(x+1, y, z)) {
    glNormal3f( 1.0f, 0.0f, 0.0f);
    glVertex3f(x+1+MY, y+0.8, z+0.2); glVertex3f(x+1+MY, y+0.8, z+0.8); glVertex3f(x+1+MY, y+0.2, z+0.8); glVertex3f(x+1+MY, y+0.2, z+0.2);
  }
  if (space->isEmpty2(x, y, z+1)) {
    glNormal3f( 0.0f, 0.0f, 1.0f);
    glVertex3f(x+0.2, y+0.2, z+1+MY); glVertex3f(x+0.2, y+0.8, z+1+MY); glVertex3f(x+0.8, y+0.8, z+1+MY); glVertex3f(x+0.8, y+0.2, z+1.02);
  }
  if (space->isEmpty2(x, y-1, z)) {
    glNormal3f( 0.0f, -1.0f, 0.0f);
    glVertex3f(x+0.2, y-MY, z+0.2); glVertex3f(x+0.8, y-MY, z+0.2); glVertex3f(x+0.8, y-MY, z+0.8); glVertex3f(x+0.2, y-MY, z+0.8);
  }
  if (space->isEmpty2(x, y+1, z)) {
    glNormal3f( 0.0f, 1.0f, 0.0f);
    glVertex3f(x+0.2, y+1+MY, z+0.2); glVertex3f(x+0.2, y+1+MY, z+0.8); glVertex3f(x+0.8, y+1+MY, z+0.8); glVertex3f(x+0.8, y+1+MY, z+0.2);
  }
  glEnd();
}

void voxelDrawer_0_c::drawCursor(unsigned int sx, unsigned int sy, unsigned int sz) {
  // draw the cursor, this is done by iterating over all
  // cubes and checking for the 3 directions (in one direction only as the other
  // direction is done with the next cube), if there is a border in the cursor
  // between these 2 cubes, if so draw the cursor grid
  for (unsigned int x = 0; x <= sx; x++)
    for (unsigned int y = 0; y <= sy; y++)
      for (unsigned int z = 0; z <= sz; z++) {
        bool ins = inRegion(x, y, z, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType);

        if (ins ^ inRegion(x-1, y, z, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType)) {
          drawGridRect(x, y, z, 0, 1, 0, 0, 0, 1, 4);
        }

        if (ins ^ inRegion(x, y-1, z, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType)) {
          drawGridRect(x, y, z, 1, 0, 0, 0, 0, 1, 4);
        }

        if (ins ^ inRegion(x, y, z-1, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType)) {
          drawGridRect(x, y, z, 1, 0, 0, 0, 1, 0, 4);
        }
      }
}


void voxelDrawer_0_c::gridTypeChanged(void) {

  GLfloat sx, sy, sz;
  sx = sy = sz = 1;

  unsigned char bb = 0;

  if (gt->getBrickXneY()) bb |= 1;
  if (gt->getBrickXneZ()) bb |= 2;
  if (gt->getBrickYneZ()) bb |= 4;

  switch (bb) {
    case 0:
      // all equal
      break;
    case 1:
    case 2:
    case 4:
      // impossible
      break;
    case 3:
      sx = 0.7;
      break;
    case 5:
      sy = 0.7;
      break;
    case 6:
      sz = 0.7;
      break;
    case 7:
      sy = 0.7;
      sz = 1.3;
      break;
  }

  GLfloat m[16] = {
    sx, 0, 0, 0,
    0, sy, 0, 0,
    0, 0, sz, 0,
    0, 0, 0,  1 };

  setTransformationMatrix(m);
}

void voxelDrawer_0_c::calculateSize(const voxel_c * shape, float * x, float * y, float * z) {
  *x = shape->getX();
  *y = shape->getY();
  *z = shape->getZ();
}

