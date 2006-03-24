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
static void drawFrame(const voxel_c * space, int x, int y, int z, float edge) {

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
static void drawBox(const voxel_c * space, int x, int y, int z, float alpha, float edge) {

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
static void drawCube(const voxel_c * space, int x, int y, int z) {
  glBegin(GL_QUADS);
  glNormal3f( 0.0f, 0.0f, -1.0f);
  glVertex3f(x+0.2, y+0.2, z-MY); glVertex3f(x+0.2, y+0.8, z-MY); glVertex3f(x+0.8, y+0.8, z-MY); glVertex3f(x+0.8, y+0.2, z-MY);
  glNormal3f( -1.0f, 0.0f, 0.0f);
  glVertex3f(x-MY, y+0.2, z+0.2); glVertex3f(x-MY, y+0.2, z+0.8); glVertex3f(x-MY, y+0.8, z+0.8); glVertex3f(x-MY, y+0.8, z+0.2);
  glNormal3f( 1.0f, 0.0f, 0.0f);
  glVertex3f(x+1+MY, y+0.8, z+0.2); glVertex3f(x+1+MY, y+0.8, z+0.8); glVertex3f(x+1+MY, y+0.2, z+0.8); glVertex3f(x+1+MY, y+0.2, z+0.2);
  glNormal3f( 0.0f, 0.0f, 1.0f);
  glVertex3f(x+0.2, y+0.2, z+1+MY); glVertex3f(x+0.2, y+0.8, z+1+MY); glVertex3f(x+0.8, y+0.8, z+1+MY); glVertex3f(x+0.8, y+0.2, z+1.02);
  glNormal3f( 0.0f, -1.0f, 0.0f);
  glVertex3f(x+0.2, y-MY, z+0.2); glVertex3f(x+0.8, y-MY, z+0.2); glVertex3f(x+0.8, y-MY, z+0.8); glVertex3f(x+0.2, y-MY, z+0.8);
  glNormal3f( 0.0f, 1.0f, 0.0f);
  glVertex3f(x+0.2, y+1+MY, z+0.2); glVertex3f(x+0.2, y+1+MY, z+0.8); glVertex3f(x+0.8, y+1+MY, z+0.8); glVertex3f(x+0.8, y+1+MY, z+0.2);
  glEnd();
}

static void drawRect(int x0, int y0, int z0,
                     int v1x, int v1y, int v1z,
                     int v2x, int v2y, int v2z, bool type, int diag) {

  bt_assert((v1x >= 0) && (v1y >= 0) && (v1z >= 0));
  bt_assert((v2x >= 0) && (v2y >= 0) && (v2z >= 0));

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

// this function finds out if a given square is inside the selected region
// this check includes the symmetric and comulmn edit modes
static bool inRegion(int x, int y, int z, int x1, int x2, int y1, int y2, int z1, int z2, int sx, int sy, int sz, int mode) {

  if ((x < 0) || (y < 0) || (z < 0) || (x >= sx) || (y >= sy) || (z >= sz)) return false;

  if (mode == 0)
    return (x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2);
  if (mode == voxelDrawer_c::TOOL_STACK_Y)
    return (x1 <= x) && (x <= x2) && (z1 <= z) && (z <= z2);
  if (mode == voxelDrawer_c::TOOL_STACK_X)
    return (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2);
  if (mode == voxelDrawer_c::TOOL_STACK_Z)
    return (x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2);

  if (mode == voxelDrawer_c::TOOL_STACK_X + voxelDrawer_c::TOOL_STACK_Y)
    return ((x1 <= x) && (x <= x2) || (y1 <= y) && (y <= y2)) && ((z1 <= z) && (z <= z2));
  if (mode == voxelDrawer_c::TOOL_STACK_X + voxelDrawer_c::TOOL_STACK_Z)
    return ((x1 <= x) && (x <= x2) || (z1 <= z) && (z <= z2)) && ((y1 <= y) && (y <= y2));
  if (mode == voxelDrawer_c::TOOL_STACK_Y + voxelDrawer_c::TOOL_STACK_Z)
    return ((y1 <= y) && (y <= y2) || (y1 <= y) && (y <= y2)) && ((x1 <= x) && (x <= x2));

  if (mode == voxelDrawer_c::TOOL_STACK_X + voxelDrawer_c::TOOL_STACK_Y + voxelDrawer_c::TOOL_STACK_Z)
    return ((x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2) ||
        (x1 <= x) && (x <= x2) && (z1 <= z) && (z <= z2) ||
        (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2));

  if (mode & voxelDrawer_c::TOOL_MIRROR_X)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~voxelDrawer_c::TOOL_MIRROR_X) ||
      inRegion(sx-x-1, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~voxelDrawer_c::TOOL_MIRROR_X);

  if (mode & voxelDrawer_c::TOOL_MIRROR_Y)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~voxelDrawer_c::TOOL_MIRROR_Y) ||
      inRegion(x, sy-y-1, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~voxelDrawer_c::TOOL_MIRROR_Y);

  if (mode & voxelDrawer_c::TOOL_MIRROR_Z)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~voxelDrawer_c::TOOL_MIRROR_Z) ||
      inRegion(x, y, sz-z-1, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~voxelDrawer_c::TOOL_MIRROR_Z);

  return false;
}


void voxelDrawer_0_c::drawShape(const shapeInfo * shape, colorMode colors) {

  for (unsigned int x = 0; x < shape->shape->getX(); x++)
    for (unsigned int y = 0; y < shape->shape->getY(); y++)
      for (unsigned int z = 0; z < shape->shape->getZ(); z++) {

        if (shape->shape->isEmpty(x, y , z))
          continue;

        float cr, cg, cb, ca;
        cr = cg = cb = 0;
        ca = 1;

        switch (colors) {
          case pieceColor:
            if ((x+y+z) & 1) {
              cr = lightPieceColor(shape->r);
              cg = lightPieceColor(shape->g);
              cb = lightPieceColor(shape->b);
              ca = shape->a;
            } else {
              cr = darkPieceColor(shape->r);
              cg = darkPieceColor(shape->g);
              cb = darkPieceColor(shape->b);
              ca = shape->a;
            }
            break;
          case paletteColor:
            unsigned int color = shape->shape->getColor(x, y, z);
            if ((color == 0) || (color - 1 >= palette.size())) {
              if ((x+y+z) & 1) {
                cr = lightPieceColor(shape->r);
                cg = lightPieceColor(shape->g);
                cb = lightPieceColor(shape->b);
                ca = shape->a;
              } else {
                cr = darkPieceColor(shape->r);
                cg = darkPieceColor(shape->g);
                cb = darkPieceColor(shape->b);
                ca = shape->a;
              }
            } else {
              cr = palette[color-1].r;
              cg = palette[color-1].g;
              cb = palette[color-1].b;
              ca = shape->a;
            }
        }

        if (shape->dim) {
          cr = 1 - (1 - cr) * 0.2;
          cg = 1 - (1 - cg) * 0.2;
          cb = 1 - (1 - cb) * 0.2;
        }

        glColor4f(cr, cg, cb, ca);

        switch (shape->mode) {
          case normal:
            if (shape->shape->getState(x, y , z) == voxel_c::VX_VARIABLE) {
              drawBox(shape->shape, x, y, z, shape->a, shape->dim ? 0 : 0.05);
              glColor4f(0, 0, 0, shape->a);
              drawCube(shape->shape, x, y, z);
            } else
              drawBox(shape->shape, x, y, z, shape->a, shape->dim ? 0 : 0.05);
            break;
          case gridline:
            drawFrame(shape->shape, x, y, z, 0.05);
            break;
          case invisible:
            break;
        }
      }
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
          drawRect(x, y, z, 0, 1, 0, 0, 0, 1, false, 4);
        }

        if (ins ^ inRegion(x, y-1, z, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType)) {
          drawRect(x, y, z, 1, 0, 0, 0, 0, 1, false, 4);
        }

        if (ins ^ inRegion(x, y, z-1, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType)) {
          drawRect(x, y, z, 1, 0, 0, 0, 1, 0, false, 4);
        }
      }
}
