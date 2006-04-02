#include "voxeldrawer_1.h"

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

// the height of one of the prisms
#define HEIGHT 0.8660254     // sqrt(3)/2
#define EDGEMULT sin(30*M_PI/180)
#define EDGEMULT2 cos(30*M_PI/180)

#define C1E1X EDGEMULT
#define C1E1Y 1
#define C1E2X EDGEMULT2
#define C1E2Y 0

#define C2EX EDGEMULT
#define C2EY 1


// draws a wireframe box depending on the neibors
void voxelDrawer_1_c::drawFrame(const voxel_c * space, int x, int y, int z, float edge) {

  if (fabs(edge) < 0.00001) return;

  glBegin(GL_QUADS);

  // bottom triangle
  if (space->isEmpty2(x, y, z-1)) {
    glNormal3f( 0.0f, 0.0f, -1.0f);

    if ((x+y) & 1) {

      float x1 = x*0.5;   float y1 = (y+1)*HEIGHT;
      float x2 = x*0.5+1; float y2 = y1;
      float x3 = x1+0.5;  float y3 = y1 - HEIGHT;

      if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x, y+1, z-1)) {
        glVertex3f(x1, y1, z); glVertex3f(x2, y2, z);
        glVertex3f(x2-edge*C1E1X, y2-edge*C1E1Y, z); glVertex3f(x1+edge*C1E1X, y1-edge*C1E1Y, z);
      }
      if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z-1)) {
        glVertex3f(x1, y1, z); glVertex3f(x3, y3, z);
        glVertex3f(x3+C2EX*edge, y3+C2EY*edge, z); glVertex3f(x1+edge*C1E2X, y1-edge*C1E2Y, z);
      }
      if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z-1)) {
        glVertex3f(x2, y2, z); glVertex3f(x3, y3, z);
        glVertex3f(x3-C2EX*edge, y3+C2EY*edge, z); glVertex3f(x2-edge*C1E2X, y2-edge*C1E2Y, z);
      }

    } else {

      float x1 = x*0.5;   float y1 = y*HEIGHT;
      float x2 = x*0.5+1; float y2 = y1;
      float x3 = x1+0.5;  float y3 = y1 + HEIGHT;

      if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x, y-1, z-1)) {
        glVertex3f(x1, y1, z); glVertex3f(x2, y2, z);
        glVertex3f(x2-edge*C1E1X, y2+edge*C1E1Y, z); glVertex3f(x1+edge*C1E1X, y1+edge*C1E1Y, z);
      }
      if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z-1)) {
        glVertex3f(x1, y1, z); glVertex3f(x3, y3, z);
        glVertex3f(x3+C2EX*edge, y3-C2EY*edge, z); glVertex3f(x1+edge*C1E2X, y1+edge*C1E2Y, z);
      }
      if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z-1)) {
        glVertex3f(x2, y2, z); glVertex3f(x3, y3, z);
        glVertex3f(x3-C2EX*edge, y3-C2EY*edge, z); glVertex3f(x2-edge*C1E2X, y2+edge*C1E2Y, z);
      }

    }
  }

  // top triangle
  if (space->isEmpty2(x, y, z+1)) {
    glNormal3f( 0.0f, 0.0f, 1.0f);

    if ((x+y) & 1) {

      float x1 = x*0.5;   float y1 = (y+1)*HEIGHT;
      float x2 = x*0.5+1; float y2 = y1;
      float x3 = x1+0.5;  float y3 = y1 - HEIGHT;

      if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x, y+1, z+1)) {
        glVertex3f(x1, y1, z+1); glVertex3f(x2, y2, z+1);
        glVertex3f(x2-edge*C1E1X, y2-edge*C1E1Y, z+1); glVertex3f(x1+edge*C1E1X, y1-edge*C1E1Y, z+1);
      }
      if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z+1)) {
        glVertex3f(x1, y1, z+1); glVertex3f(x3, y3, z+1);
        glVertex3f(x3+C2EX*edge, y3+C2EY*edge, z+1); glVertex3f(x1+edge*C1E2X, y1-edge*C1E2Y, z+1);
      }
      if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z+1)) {
        glVertex3f(x2, y2, z+1); glVertex3f(x3, y3, z+1);
        glVertex3f(x3-C2EX*edge, y3+C2EY*edge, z+1); glVertex3f(x2-edge*C1E2X, y2-edge*C1E2Y, z+1);
      }

    } else {

      float x1 = x*0.5;   float y1 = y*HEIGHT;
      float x2 = x*0.5+1; float y2 = y1;
      float x3 = x1+0.5;  float y3 = y1 + HEIGHT;

      if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x, y-1, z+1)) {
        glVertex3f(x1, y1, z+1); glVertex3f(x2, y2, z+1);
        glVertex3f(x2-edge*C1E1X, y2+edge*C1E1Y, z+1); glVertex3f(x1+edge*C1E1X, y1+edge*C1E1Y, z+1);
      }
      if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z+1)) {
        glVertex3f(x1, y1, z+1); glVertex3f(x3, y3, z+1);
        glVertex3f(x3+C2EX*edge, y3-C2EY*edge, z+1); glVertex3f(x1+edge*C1E2X, y1+edge*C1E2Y, z+1);
      }
      if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z+1)) {
        glVertex3f(x2, y2, z+1); glVertex3f(x3, y3, z+1);
        glVertex3f(x3-C2EX*edge, y3-C2EY*edge, z+1); glVertex3f(x2-edge*C1E2X, y2+edge*C1E2Y, z+1);
      }

    }
  }

  // the left square
  if (space->isEmpty2(x-1, y, z)) {

    float x1, y1, x3, y3;

    if ((x+y) & 1) {

      glNormal3f( -EDGEMULT2, -EDGEMULT, 0.0f);

      x1 = x*0.5;   y1 = (y+1)*HEIGHT;
      x3 = x1+0.5;  y3 = y1 - HEIGHT;

      if (!space->isEmpty2(x-1, y-1, z) ||
          !space->isEmpty2(x, y-1, z) ||
          space->isEmpty2(x+1, y-1, z)) {
        glVertex3f(x3, y3, z); glVertex3f(x3, y3, z+1);
        glVertex3f(x3-C2EX*edge, y3+C2EY*edge, z+1); glVertex3f(x3-C2EX*edge, y3+C2EY*edge, z);
      }

      if (!space->isEmpty2(x-2, y, z) ||
          !space->isEmpty2(x-2, y+1, z) ||
          space->isEmpty2(x-1, y+1, z)) {
        glVertex3f(x1, y1, z); glVertex3f(x1, y1, z+1);
        glVertex3f(x1+C2EX*edge, y1-C2EY*edge, z+1); glVertex3f(x1+C2EX*edge, y1-C2EY*edge, z);
      }

    } else {

      glNormal3f( -EDGEMULT2, EDGEMULT, 0.0f);

      x1 = x*0.5;     y1 = y*HEIGHT;
      x3 = x1 + 0.5;  y3 = y1 + HEIGHT;

      if (!space->isEmpty2(x-1, y+1, z) ||
          !space->isEmpty2(x, y+1, z) ||
          space->isEmpty2(x+1, y+1, z)) {
        glVertex3f(x3, y3, z); glVertex3f(x3, y3, z+1);
        glVertex3f(x3-C2EX*edge, y3-C2EY*edge, z+1); glVertex3f(x3-C2EX*edge, y3-C2EY*edge, z);
      }

      if (!space->isEmpty2(x-2, y, z) ||
          !space->isEmpty2(x-2, y-1, z) ||
          space->isEmpty2(x-1, y-1, z)) {
        glVertex3f(x1, y1, z); glVertex3f(x1, y1, z+1);
        glVertex3f(x1+C2EX*edge, y1+C2EY*edge, z+1); glVertex3f(x1+C2EX*edge, y1+C2EY*edge, z);
      }
    }

    if (!space->isEmpty2(x-1, y, z+1) ||
        space->isEmpty2(x, y, z+1)) {
      glVertex3f(x1, y1, z+1); glVertex3f(x3, y3, z+1);
      glVertex3f(x3, y3, z+1-edge); glVertex3f(x1, y1, z+1-edge);
    }

    if (!space->isEmpty2(x-1, y, z-1) ||
        space->isEmpty2(x, y, z-1)) {
      glVertex3f(x1, y1, z); glVertex3f(x3, y3, z);
      glVertex3f(x3, y3, z+edge); glVertex3f(x1, y1, z+edge);
    }
  }

  // the right square
  if (space->isEmpty2(x+1, y, z)) {

    float x1, y1, x3, y3;

    if ((x+y) & 1) {

      glNormal3f( EDGEMULT2, -EDGEMULT, 0.0f);

      x1 = x*0.5+1;   y1 = (y+1)*HEIGHT;
      x3 = x1-0.5;  y3 = y1 - HEIGHT;

      if (!space->isEmpty2(x+1, y-1, z) ||
          !space->isEmpty2(x, y-1, z) ||
          space->isEmpty2(x-1, y-1, z)) {
        glVertex3f(x3, y3, z); glVertex3f(x3, y3, z+1);
        glVertex3f(x3+C2EX*edge, y3+C2EY*edge, z+1); glVertex3f(x3+C2EX*edge, y3+C2EY*edge, z);
      }

      if (!space->isEmpty2(x+2, y, z) ||
          !space->isEmpty2(x+2, y+1, z) ||
          space->isEmpty2(x+1, y+1, z)) {
        glVertex3f(x1, y1, z); glVertex3f(x1, y1, z+1);
        glVertex3f(x1-C2EX*edge, y1-C2EY*edge, z+1); glVertex3f(x1-C2EX*edge, y1-C2EY*edge, z);
      }

    } else {

      glNormal3f( EDGEMULT2, EDGEMULT, 0.0f);

      x1 = x*0.5+1;     y1 = y*HEIGHT;
      x3 = x1 - 0.5;  y3 = y1 + HEIGHT;

      if (!space->isEmpty2(x+1, y+1, z) ||
          !space->isEmpty2(x, y+1, z) ||
          space->isEmpty2(x-1, y+1, z)) {
        glVertex3f(x3, y3, z); glVertex3f(x3, y3, z+1);
        glVertex3f(x3+C2EX*edge, y3-C2EY*edge, z+1); glVertex3f(x3+C2EX*edge, y3-C2EY*edge, z);
      }

      if (!space->isEmpty2(x+2, y, z) ||
          !space->isEmpty2(x+2, y-1, z) ||
          space->isEmpty2(x+1, y-1, z)) {
        glVertex3f(x1, y1, z); glVertex3f(x1, y1, z+1);
        glVertex3f(x1-C2EX*edge, y1+C2EY*edge, z+1); glVertex3f(x1-C2EX*edge, y1+C2EY*edge, z);
      }
    }

    if (!space->isEmpty2(x+1, y, z+1) ||
        space->isEmpty2(x, y, z+1)) {
      glVertex3f(x1, y1, z+1); glVertex3f(x3, y3, z+1);
      glVertex3f(x3, y3, z+1-edge); glVertex3f(x1, y1, z+1-edge);
    }

    if (!space->isEmpty2(x+1, y, z-1) ||
        space->isEmpty2(x, y, z-1)) {
      glVertex3f(x1, y1, z); glVertex3f(x3, y3, z);
      glVertex3f(x3, y3, z+edge); glVertex3f(x1, y1, z+edge);
    }
  }

  // the square above or below (depending on the triangle type)
  if ((x+y) & 1) {

    if (space->isEmpty2(x, y+1, z)) {

      glNormal3f(0.0f, 1.0f, 0.0f);

      float x1 = x*0.5;  float y1 = (y+1)*HEIGHT;
      float x2 = x1 + 1; float y2 = y1;

      if (!space->isEmpty2(x-2, y+1, z) ||
          !space->isEmpty2(x-1, y+1, z) ||
          space->isEmpty2(x-2, y, z)) {
        glVertex3f(x1, y1, z); glVertex3f(x1, y1, z+1);
        glVertex3f(x1+edge, y1, z+1); glVertex3f(x1+edge, y1, z);
      }

      if (!space->isEmpty2(x+2, y+1, z) ||
          !space->isEmpty2(x+1, y+1, z) ||
          space->isEmpty2(x+2, y, z)) {
        glVertex3f(x2, y2, z); glVertex3f(x2, y2, z+1);
        glVertex3f(x2-edge, y1, z+1); glVertex3f(x2-edge, y1, z);
      }

      if (!space->isEmpty2(x, y+1, z+1) ||
          space->isEmpty2(x, y, z+1)) {
        glVertex3f(x1, y1, z+1); glVertex3f(x2, y2, z+1);
        glVertex3f(x2, y2, z+1-edge); glVertex3f(x1, y1, z+1-edge);
      }

      if (!space->isEmpty2(x, y+1, z-1) ||
          space->isEmpty2(x, y, z-1)) {
        glVertex3f(x1, y1, z); glVertex3f(x2, y2, z);
        glVertex3f(x2, y2, z+edge); glVertex3f(x1, y1, z+edge);
      }
    }

  } else {

    if (space->isEmpty2(x, y-1, z)) {
      glNormal3f(0.0f, -1.0f, 0.0f);

      float x1 = x*0.5;  float y1 = y*HEIGHT;
      float x2 = x1 + 1; float y2 = y1;

      if (!space->isEmpty2(x-2, y-1, z) ||
          !space->isEmpty2(x-1, y-1, z) ||
          space->isEmpty2(x-2, y, z)) {
        glVertex3f(x1, y1, z); glVertex3f(x1, y1, z+1);
        glVertex3f(x1+edge, y1, z+1); glVertex3f(x1+edge, y1, z);
      }

      if (!space->isEmpty2(x+2, y-1, z) ||
          !space->isEmpty2(x+1, y-1, z) ||
          space->isEmpty2(x+2, y, z)) {
        glVertex3f(x2, y2, z); glVertex3f(x2, y2, z+1);
        glVertex3f(x2-edge, y1, z+1); glVertex3f(x2-edge, y1, z);
      }

      if (!space->isEmpty2(x, y-1, z+1) ||
          space->isEmpty2(x, y, z+1)) {
        glVertex3f(x1, y1, z+1); glVertex3f(x2, y2, z+1);
        glVertex3f(x2, y2, z+1-edge); glVertex3f(x1, y1, z+1-edge);
      }

      if (!space->isEmpty2(x, y-1, z-1) ||
          space->isEmpty2(x, y, z-1)) {
        glVertex3f(x1, y1, z); glVertex3f(x2, y2, z);
        glVertex3f(x2, y2, z+edge); glVertex3f(x1, y1, z+edge);
      }
    }
  }

  glEnd();
}

// draws a box with borders depending on the neibor boxes
void voxelDrawer_1_c::drawNormalVoxel(const voxel_c * space, int x, int y, int z, float alpha, float edge) {


  GLfloat x1, y1, x2, y2, x3, y3, n1x, n1y, n2x, n2y, n3x, n3y;

  if ((x+y) & 1) {
    // triangle with base at the top

    x1 = x/2.0;
    y1 = HEIGHT+y*HEIGHT;
    x2 = x1+1;
    y2 = y1;
    x3 = x1+0.5;
    y3 = y1-HEIGHT;

    n1x = 0; n1y = 1;
    n2x = -cos(M_PI/180*30); n2y = -sin(M_PI/180*30);
    n3x = cos(M_PI/180*30); n3y = -sin(M_PI/180*30);

  } else {

    x1 = x/2.0;
    y1 = y*HEIGHT;
    x2 = x1+1;
    y2 = y1;
    x3 = x1+0.5;
    y3 = y1+HEIGHT;

    n1x = 0; n1y = -1;
    n2x = -cos(M_PI/180*30); n2y = sin(M_PI/180*30);
    n3x = cos(M_PI/180*30); n3y = sin(M_PI/180*30);
  }

  glBegin(GL_QUADS);

  // top or bottom square
  if (((x+y) & 1) && space->isEmpty2(x, y+1, z) ||
      !((x+y) & 1) && space->isEmpty2(x, y-1, z)) {

    float x1p = x1;
    float y1p = y1;
    float z1p = z;
    float x2p = x2;
    float y2p = y2;
    float z2p = z+1;

    int ydelta;

    if ((x+y) & 1)
      ydelta = 1;
    else
      ydelta = -1;

    if (!space->isEmpty2(x-2, y+ydelta, z) ||
        !space->isEmpty2(x-1, y+ydelta, z) ||
        space->isEmpty2(x-2, y, z)) {
      x1p += edge;
    }

    if (!space->isEmpty2(x+2, y+ydelta, z) ||
        !space->isEmpty2(x+1, y+ydelta, z) ||
        space->isEmpty2(x+2, y, z)) {
      x2p -= edge;
    }

    if (!space->isEmpty2(x, y+ydelta, z+1) ||
        space->isEmpty2(x, y, z+1)) {
      z2p -= edge;
    }

    if (!space->isEmpty2(x, y+ydelta, z-1) ||
        space->isEmpty2(x, y, z-1)) {
      z1p += edge;
    }

    glNormal3f(n1x, n1y, 0.0f);
    glVertex3f(x1p, y1p, z1p); glVertex3f(x2p, y2p, z1p); glVertex3f(x2p, y2p, z2p); glVertex3f(x1p, y1p, z2p);
  }

  if (space->isEmpty2(x-1, y, z)) {

    float x1p = x1;
    float y1p = y1;
    float z1p = z;
    float x3p = x3;
    float y3p = y3;
    float z3p = z+1;

    if ((x+y) & 1) {

      if (!space->isEmpty2(x-1, y-1, z) ||
          !space->isEmpty2(x, y-1, z) ||
          space->isEmpty2(x+1, y-1, z)) {
        x3p -= C2EX*edge; y3p += C2EY*edge;
      }

      if (!space->isEmpty2(x-2, y, z) ||
          !space->isEmpty2(x-2, y+1, z) ||
          space->isEmpty2(x-1, y+1, z)) {
        x1p += C2EX*edge; y1p -= C2EY*edge;
      }

    } else {

      if (!space->isEmpty2(x-1, y+1, z) ||
          !space->isEmpty2(x, y+1, z) ||
          space->isEmpty2(x+1, y+1, z)) {
        x3p -= C2EX*edge; y3p -= C2EY*edge;
      }

      if (!space->isEmpty2(x-2, y, z) ||
          !space->isEmpty2(x-2, y-1, z) ||
          space->isEmpty2(x-1, y-1, z)) {
        x1p += C2EX*edge; y1p += C2EY*edge;
      }
    }

    if (!space->isEmpty2(x-1, y, z+1) ||
        space->isEmpty2(x, y, z+1)) {
      z3p -= edge;
    }

    if (!space->isEmpty2(x-1, y, z-1) ||
        space->isEmpty2(x, y, z-1)) {
      z1p += edge;
    }

    glNormal3f(n2x, n2y, 0.0f);
    glVertex3f(x1p, y1p, z1p); glVertex3f(x3p, y3p, z1p); glVertex3f(x3p, y3p, z3p); glVertex3f(x1p, y1p, z3p);
  }

  // right triangle
  if (space->isEmpty2(x+1, y, z)) {

    float x2p = x2;
    float y2p = y2;
    float z2p = z;
    float x3p = x3;
    float y3p = y3;
    float z3p = z+1;

    if ((x+y) & 1) {

      if (!space->isEmpty2(x+1, y-1, z) ||
          !space->isEmpty2(x, y-1, z) ||
          space->isEmpty2(x-1, y-1, z)) {
        x3p += C2EX*edge; y3p += C2EY*edge;
      }

      if (!space->isEmpty2(x+2, y, z) ||
          !space->isEmpty2(x+2, y+1, z) ||
          space->isEmpty2(x+1, y+1, z)) {
        x2p -= C2EX*edge; y2p -= C2EY*edge;
      }

    } else {

      if (!space->isEmpty2(x+1, y+1, z) ||
          !space->isEmpty2(x, y+1, z) ||
          space->isEmpty2(x-1, y+1, z)) {
        x3p += C2EX*edge; y3p -= C2EY*edge;
      }

      if (!space->isEmpty2(x+2, y, z) ||
          !space->isEmpty2(x+2, y-1, z) ||
          space->isEmpty2(x+1, y-1, z)) {
        x2p -= C2EX*edge; y2p += C2EY*edge;
      }
    }

    if (!space->isEmpty2(x+1, y, z+1) ||
        space->isEmpty2(x, y, z+1)) {
      z3p -= edge;
    }

    if (!space->isEmpty2(x+1, y, z-1) ||
        space->isEmpty2(x, y, z-1)) {
      z2p += edge;
    }

    glNormal3f( n3x, n3y, 0.0f);
    glVertex3f(x2p, y2p, z2p); glVertex3f(x3p, y3p, z2p); glVertex3f(x3p, y3p, z3p); glVertex3f(x2p, y2p, z3p);
  }

  glEnd();

  glBegin(GL_TRIANGLES);

  // bottom triangle
  if (space->isEmpty2(x, y, z-1)) {

    float x1p = x1;
    float y1p = y1;
    float x2p = x2;
    float y2p = y2;
    float x3p = x3;
    float y3p = y3;

    if ((x+y) & 1) {

      if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x, y+1, z-1)) {
        x1p += edge*C1E1X; x2p -= edge*C1E1X; y1p -= edge*C1E1Y; y2p -= edge*C1E1Y;
      }
      if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z-1)) {
        x3p += C2EX*edge; y3p += C2EY*edge; x1p += edge*C1E2X; y1p -= edge*C1E2Y;
      }
      if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z-1)) {
        x3p -= C2EX*edge; y3p += C2EY*edge; x2p -= edge*C1E2X; y2p -= edge*C1E2Y;
      }

    } else {

      if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x, y-1, z-1)) {
        x2p -= edge*C1E1X; y2p += edge*C1E1Y; x1p += edge*C1E1X; y1p += edge*C1E1Y;
      }
      if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z-1)) {
        x3p += C2EX*edge; y3p -= C2EY*edge; x1p += edge*C1E2X; y1p += edge*C1E2Y;
      }
      if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z-1)) {
        x3p -= C2EX*edge; y3p -= C2EY*edge; x2p -= edge*C1E2X; y2p += edge*C1E2Y;
      }

    }
    glNormal3f( 0.0f, 0.0f, -1.0f);
    glVertex3f(x1p, y1p, z); glVertex3f(x2p, y2p, z); glVertex3f(x3p, y3p, z);
  }

  // top triangle
  if (space->isEmpty2(x, y, z+1)) {

    float x1p = x1;
    float y1p = y1;
    float x2p = x2;
    float y2p = y2;
    float x3p = x3;
    float y3p = y3;

    if ((x+y) & 1) {

      if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x, y+1, z+1)) {
        x2p -= edge*C1E1X; y2p -= edge*C1E1Y; x1p += edge*C1E1X; y1p -= edge*C1E1Y;
      }
      if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z+1)) {
        x3p += C2EX*edge; y3p += C2EY*edge; x1p += edge*C1E2X; y1p -= edge*C1E2Y;
      }
      if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z+1)) {
        x3p -= C2EX*edge; y3p += C2EY*edge; x2p -= edge*C1E2X; y2p -= edge*C1E2Y;
      }

    } else {

      if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x, y-1, z+1)) {
        x2p -= edge*C1E1X; y2p += edge*C1E1Y; x1p += edge*C1E1X; y1p += edge*C1E1Y;
      }
      if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z+1)) {
        x3p += C2EX*edge; y3p -= C2EY*edge; x1p += edge*C1E2X; y1p += edge*C1E2Y;
      }
      if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z+1)) {
        x3p -= C2EX*edge; y3p -= C2EY*edge; x2p -= edge*C1E2X; y2p += edge*C1E2Y;
      }
    }

    glNormal3f( 0.0f, 0.0f, 1.0f);
    glVertex3f(x1p, y1p, z+1); glVertex3f(x2p, y2p, z+1); glVertex3f(x3p, y3p, z+1);
  }

  glEnd();

  glColor4f(0, 0, 0, alpha);
  drawFrame(space, x, y, z, edge);
}

void voxelDrawer_1_c::drawVariableMarkers(const voxel_c * space, int x, int y, int z) {

  GLfloat x1, y1, x2, y2, x3, y3, n1x, n1y, n2x, n2y, n3x, n3y;

  if ((x+y) & 1) {
    // triangle with base at the top

    x1 = x/2.0;
    y1 = HEIGHT+y*HEIGHT;
    x2 = x1+1;
    y2 = y1;
    x3 = x1+0.5;
    y3 = y1-HEIGHT;

    n1x = 0; n1y = 1;
    n2x = -cos(M_PI/180*30); n2y = -sin(M_PI/180*30);
    n3x = cos(M_PI/180*30); n3y = -sin(M_PI/180*30);

  } else {

    x1 = x/2.0;
    y1 = y*HEIGHT;
    x2 = x1+1;
    y2 = y1;
    x3 = x1+0.5;
    y3 = y1+HEIGHT;

    n1x = 0; n1y = -1;
    n2x = -cos(M_PI/180*30); n2y = sin(M_PI/180*30);
    n3x = cos(M_PI/180*30); n3y = sin(M_PI/180*30);
  }

  glBegin(GL_QUADS);

  if (((x+y) & 1) && space->isEmpty2(x, y+1, z) ||
      !((x+y) & 1) && space->isEmpty2(x, y-1, z)) {
    glNormal3f(n1x, n1y, 0.0f);
    glVertex3f(x1+(x2-x1)*0.2+MY*n1x, y1+(y2-y1)*0.2+MY*n1y, z+0.2);
    glVertex3f(x2+(x1-x2)*0.2+MY*n1x, y2+(y1-y2)*0.2+MY*n1y, z+0.2);
    glVertex3f(x2+(x1-x2)*0.2+MY*n1x, y2+(y1-y2)*0.2+MY*n1y, z+0.8);
    glVertex3f(x1+(x2-x1)*0.2+MY*n1x, y1+(y2-y1)*0.2+MY*n1y, z+0.8);
  }

  if (space->isEmpty2(x-1, y, z)) {
    glNormal3f(n2x, n2y, 0.0f);
    glVertex3f(x1+(x3-x1)*0.2+MY*n2x, y1+(y3-y1)*0.2+MY*n2y, z+0.2);
    glVertex3f(x3+(x1-x3)*0.2+MY*n2x, y3+(y1-y3)*0.2+MY*n2y, z+0.2);
    glVertex3f(x3+(x1-x3)*0.2+MY*n2x, y3+(y1-y3)*0.2+MY*n2y, z+0.8);
    glVertex3f(x1+(x3-x1)*0.2+MY*n2x, y1+(y3-y1)*0.2+MY*n2y, z+0.8);
  }

  if (space->isEmpty2(x+1, y, z)) {
    glNormal3f(n3x, n3y, 0.0f);
    glVertex3f(x2+(x3-x2)*0.2+MY*n3x, y2+(y3-y2)*0.2+MY*n3y, z+0.2);
    glVertex3f(x3+(x2-x3)*0.2+MY*n3x, y3+(y2-y3)*0.2+MY*n3y, z+0.2);
    glVertex3f(x3+(x2-x3)*0.2+MY*n3x, y3+(y2-y3)*0.2+MY*n3y, z+0.8);
    glVertex3f(x2+(x3-x2)*0.2+MY*n3x, y2+(y3-y2)*0.2+MY*n3y, z+0.8);
  }

  glEnd();

  glBegin(GL_TRIANGLES);

  if (space->isEmpty2(x, y, z-1)) {
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(x1+sqrt(0.1)*n3x, y1+sqrt(0.1)*n3y, z-MY);
    glVertex3f(x2+sqrt(0.1)*n2x, y2+sqrt(0.1)*n2y, z-MY);
    glVertex3f(x3+sqrt(0.1)*n1x, y3+sqrt(0.1)*n1y, z-MY);
  }

  if (space->isEmpty2(x, y, z+1)) {
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(x1+sqrt(0.1)*n3x, y1+sqrt(0.1)*n3y, z+1+MY);
    glVertex3f(x2+sqrt(0.1)*n2x, y2+sqrt(0.1)*n2y, z+1+MY);
    glVertex3f(x3+sqrt(0.1)*n1x, y3+sqrt(0.1)*n1y, z+1+MY);
  }

  glEnd();

}

void voxelDrawer_1_c::drawCursor(unsigned int sx, unsigned int sy, unsigned int sz) {
  // draw the cursor, this is done by iterating over all
  // cubes and checking for the 3 directions (in one direction only as the other
  // direction is done with the next cube), if there is a border in the cursor
  // between these 2 cubes, if so draw the cursor grid
  for (unsigned int x = 0; x <= sx; x++)
    for (unsigned int y = 0; y <= sy; y++)
      for (unsigned int z = 0; z <= sz; z++) {
        bool ins = inRegion(x, y, z, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType);

        if (ins ^ inRegion(x-1, y, z, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType)) {
          if ((x+y) & 1)
            drawGridRect(0.5+x*0.5, y*HEIGHT, z, -0.5, HEIGHT, 0, 0, 0, 1, 4);
          else
            drawGridRect(x*0.5, y*HEIGHT, z, 0.5, HEIGHT, 0, 0, 0, 1, 4);
        }

        if ((((x+y) & 1) == 0) && (ins ^ inRegion(x, y-1, z, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType))) {
          drawGridRect(x*0.5, y*HEIGHT, z, 1, 0, 0, 0, 0, 1, 4);
        }

        if (ins ^ inRegion(x, y, z-1, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType)) {
          if ((x+y) & 1)
            drawGridTriangle(0.5+x*0.5, y*HEIGHT, z, -0.5, HEIGHT, 0, 0.5, HEIGHT, 0, 4);
          else
            drawGridTriangle(0.5+x*0.5, (y+1)*HEIGHT, z, -0.5, -HEIGHT, 0, 0.5, -HEIGHT, 0, 4);
        }
      }
}

void voxelDrawer_1_c::gridTypeChanged(void) {

  GLfloat sx, sy, sz;
  sx = sy = sz = 1;

  GLfloat m[16] = {
    sx, 0, 0, 0,
    0, sy, 0, 0,
    0, 0, sz, 0,
    0, 0, 0,  1 };

  setTransformationMatrix(m);
}

void voxelDrawer_1_c::calculateSize(const voxel_c * shape, float * x, float * y, float * z) {
  *x = (1 + (shape->getX()-1) * 0.5);
  *y = shape->getY() * HEIGHT;
  *z = shape->getZ();
}
