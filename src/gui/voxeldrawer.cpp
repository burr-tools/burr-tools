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
#include "voxeldrawer.h"
#include "arcball.h"

#include "piececolor.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"
#include "../lib/assembly.h"
#include "../lib/disasmtomoves.h"

#include <math.h>
#include <stdlib.h>

#include <FL/Fl.H>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#elif WIN32
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GL/gl.h>
#endif


voxelFrame_c::voxelFrame_c(int x,int y,int w,int h) :
  Fl_Gl_Window(x,y,w,h),
  drawer(0),
  curAssembly(0),
  markerType(-1),
  arcBall(new arcBall_c(w, h)),
  doUpdates(true), size(10), cb(0),
  colors(pieceColor),
  _useLightning(true),
  _gtChanged(true),
  pickx(-1)
{
};

voxelFrame_c::~voxelFrame_c(void) {
  clearSpaces();
  if (curAssembly) {
    delete curAssembly;
    curAssembly = 0;
  }
  delete arcBall;
}

void voxelFrame_c::setTransformationMatrix(GLfloat m[16]) {
  for (unsigned int i = 0; i < 16; i++)
    transformMatrix[i] = m[i];
}

void voxelFrame_c::gridTypeChanged(void) {
  GLfloat m[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1};

  setTransformationMatrix(m);

  _gtChanged = false;
  redraw();
}

void voxelFrame_c::setDrawer(voxelDrawer_c * dr) {
  if (drawer) delete drawer;
  drawer = dr;
  redraw();
}

void voxelFrame_c::drawVoxelSpace() {

  glShadeModel(GL_FLAT);

  glPushName(0);

  for (unsigned int run = 0; run < 2; run++) {
    for (unsigned int piece = 0; piece < shapes.size(); piece++) {

      if (shapes[piece].a == 0)
        continue;

      glLoadName(piece);

      // in run 0 we only paint opaque objects and in run 1 only transparent ones
      // this lets the transparent objects be always in front of the others
      if ((run == 0) && (shapes[piece].a != 1)) continue;
      if ((run == 1) && (shapes[piece].a == 1)) continue;

      glPushMatrix();

      float hx, hy, hz;
      hx = shapes[piece].shape->getHx();
      hy = shapes[piece].shape->getHy();
      hz = shapes[piece].shape->getHz();
      drawer->recalcSpaceCoordinates(&hx, &hy, &hz);

      switch(trans) {
      case ScaleRotateTranslate:
        glTranslatef(shapes[piece].x,
                     shapes[piece].y,
                     shapes[piece].z);
        glScalef(shapes[piece].scale, shapes[piece].scale, shapes[piece].scale);
        addRotationTransformation();
        glMultMatrixf(transformMatrix);
        {
          float cx, cy, cz;
          drawer->calculateSize(shapes[piece].shape, &cx, &cy, &cz);
          glTranslatef(-0.5*cx, -0.5*cy, -0.5*cz);
        }
        break;
      case TranslateRoateScale:
        addRotationTransformation();
        glTranslatef(shapes[piece].x,
                     shapes[piece].y,
                     shapes[piece].z);
        glMultMatrixf(transformMatrix);
        {
          float cx, cy, cz;
          drawer->calculateSize(shapes[piece].shape, &cx, &cy, &cz);
          glTranslatef(-0.5*cx, -0.5*cy, -0.5*cz);
        }
        glScalef(shapes[piece].scale, shapes[piece].scale, shapes[piece].scale);
        break;
      case CenterTranslateRoateScale:
        addRotationTransformation();
        glMultMatrixf(transformMatrix);
        glTranslatef(shapes[piece].x - hx,
                     shapes[piece].y - hy,
                     shapes[piece].z - hz);
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
        float cx, cy, cz;
        drawer->calculateSize(shapes[piece].shape, &cx, &cy, &cz);

        if (colors == anaglyphColor) {
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

      const shapeInfo * shape = &shapes[piece];

      glPushName(0);

      for (unsigned int x = 0; x < shape->shape->getX(); x++)
        for (unsigned int y = 0; y < shape->shape->getY(); y++)
          for (unsigned int z = 0; z < shape->shape->getZ(); z++) {

            if (shape->shape->isEmpty(x, y , z))
              continue;

            glLoadName(shape->shape->getIndex(x, y, z));

            float cr, cg, cb, ca;
            cr = cg = cb = 0;
            ca = 1;

            switch (colors) {
              case pieceColor:
              case anaglyphColor:
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
                {
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
                break;
            }

            if (colors == anaglyphColor) {
              double gr = 0.1*cb + 0.3*cr + 0.6*cg;
              gr = 1 - (1-gr)/3;
              cr = cg = cb = gr;
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
                  drawer->drawNormalVoxel(shape->shape, x, y, z, shape->a, shape->dim ? 0 : 0.05);
                  glColor4f(0, 0, 0, shape->a);
                  drawer->drawVariableMarkers(shape->shape, x, y, z);
                } else
                  drawer->drawNormalVoxel(shape->shape, x, y, z, shape->a, shape->dim ? 0 : 0.05);
                break;
              case gridline:
                drawer->drawFrame(shape->shape, x, y, z, 0.05);
                break;
              case invisible:
                break;
            }
          }

      glPopName();

      // the marker should be only active, when only one shape is there
      // otherwise it's drawn for every shape
      if ((markerType >= 0) && (mX1 <= mX2) && (mY1 <= mY2)) {

        if (_useLightning) glDisable(GL_LIGHTING);
        glDisable(GL_BLEND);

        glColor4f(0, 0, 0, 1);

        drawer->drawCursor(shapes[piece].shape, shapes[piece].shape->getX(), shapes[piece].shape->getY(), shapes[piece].shape->getZ());

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

void voxelFrame_c::drawData(void) {

  if (!drawer) return;

  if (_gtChanged)
    gridTypeChanged();

  glShadeModel(GL_FLAT);
  glEnable(GL_DEPTH_TEST);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glLineWidth(3);

  if (_useLightning)
    glEnable(GL_LIGHTING);
  else
    glDisable(GL_LIGHTING);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();

  drawVoxelSpace();
  glPopMatrix();
}

unsigned int voxelFrame_c::addSpace(const voxel_c * vx) {
  shapeInfo i;

  i.r = i.g = i.b = 1;
  i.a = 1;
  i.shape = vx;

  i.mode = normal;

  i.x = i.y = i.z = 0;
  i.scale = 1;

  i.dim = false;

  shapes.push_back(i);

  updateRequired();

  return shapes.size()-1;
}

void voxelFrame_c::clearSpaces(void) {

  for (unsigned int i = 0; i < shapes.size(); i++)
    delete shapes[i].shape;

  shapes.clear();
  updateRequired();
}

unsigned int voxelFrame_c::spaceNumber(void) {
  return shapes.size();
}

void voxelFrame_c::setSpaceColor(unsigned int nr, float r, float g, float b, float a) {
  shapes[nr].r = r;
  shapes[nr].g = g;
  shapes[nr].b = b;
  shapes[nr].a = a;

  updateRequired();
}

void voxelFrame_c::setSpaceColor(unsigned int nr, float a) {
  shapes[nr].a = a;

  updateRequired();
}

void voxelDrawer_c::recalcSpaceCoordinates(float * /*x*/, float * /*y*/, float * /*z*/) {}

void voxelFrame_c::setSpacePosition(unsigned int nr, float x, float y, float z, float scale) {

  drawer->recalcSpaceCoordinates(&x, &y, &z);

  shapes[nr].x = x;
  shapes[nr].y = y;
  shapes[nr].z = z;
  shapes[nr].scale = scale;

  updateRequired();
}

void voxelFrame_c::setSpaceDim(unsigned int nr, bool dim) {

  shapes[nr].dim = dim;

  updateRequired();
}


void voxelFrame_c::setDrawingMode(unsigned int nr, drawingMode mode) {
  shapes[nr].mode = mode;

  updateRequired();
}

void voxelFrame_c::setColorMode(colorMode color) {
  colors = color;

  updateRequired();
}

void voxelFrame_c::setTransformationType(transformationType type) {
  trans = type;

  updateRequired();
}

void voxelFrame_c::addPaletteEntry(float r, float g, float b) {

  colorInfo ci;

  ci.r = r;
  ci.g = g;
  ci.b = b;

  palette.push_back(ci);
}

void voxelFrame_c::setMarker(int x1, int y1, int x2, int y2, int z, int mT) {
  markerType = mT;
  mX1 = x1;
  mY1 = y1;
  mX2 = x2;
  mY2 = y2;
  mZ = z;
}

void voxelFrame_c::hideMarker(void) {
  markerType = -1;
}

void voxelFrame_c::showSingleShape(const puzzle_c * puz, unsigned int shapeNum) {

  hideMarker();
  clearSpaces();
  unsigned int num = addSpace(puz->getGridType()->getVoxel(puz->getShape(shapeNum)));

  setSpaceColor(num, pieceColorR(shapeNum), pieceColorG(shapeNum), pieceColorB(shapeNum), 1);

  setTransformationType(TranslateRoateScale);
  showCoordinateSystem(true);
}

void voxelFrame_c::showProblem(const puzzle_c * puz, unsigned int probNum, unsigned int selShape) {

  hideMarker();
  clearSpaces();

  if (probNum < puz->problemNumber()) {

    float diagonal = 0;

    // now find a scaling factor, so that all pieces fit into their square

    // find the biggest piece shape
    for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++)
      if (puz->probGetShapeShape(probNum, p)->getDiagonal() > diagonal)
        diagonal = puz->probGetShapeShape(probNum, p)->getDiagonal();

    // find out how much bigger the result is compared to the shapes
    unsigned int factor;
    if (puz->probGetResult(probNum) < puz->shapeNumber()) {

      factor = (int)((sqrt(puz->probGetResultShape(probNum)->getDiagonal()) + 0.5)/sqrt(diagonal));
    } else
      factor = 1;

    if (factor < 1)
      factor = 1;

    diagonal = sqrt(diagonal);

    // first find out how to arrange the pieces:
    unsigned int square = 2*factor+1;
    while (square * (square-2*factor) < puz->probShapeNumber(probNum)) square++;

    unsigned int num;

    // now place the result shape
    if (puz->probGetResult(probNum) < puz->shapeNumber()) {

      num = addSpace(puz->getGridType()->getVoxel(puz->probGetResultShape(probNum)));
      setSpaceColor(num,
                            pieceColorR(puz->probGetResult(probNum)),
                            pieceColorG(puz->probGetResult(probNum)),
                            pieceColorB(puz->probGetResult(probNum)), 1);
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
    for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++) {
      num = addSpace(puz->getGridType()->getVoxel(puz->probGetShapeShape(probNum, p)));

      setSpaceColor(num,
                            pieceColorR(puz->probGetShape(probNum, p)),
                            pieceColorG(puz->probGetShape(probNum, p)),
                            pieceColorB(puz->probGetShape(probNum, p)), 1);

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

    setTransformationType(ScaleRotateTranslate);
    showCoordinateSystem(false);
  }
}

void voxelFrame_c::showColors(const puzzle_c * puz, colorMode mode) {

  if (mode == paletteColor) {

    clearPalette();
    for (unsigned int i = 0; i < puz->colorNumber(); i++) {
      unsigned char r, g, b;
      puz->getColor(i, &r, &g, &b);
      addPaletteEntry(r/255.0, g/255.0, b/255.0);
    }
    setColorMode(paletteColor);

  } else
    setColorMode(mode);

}

void voxelFrame_c::showAssembly(const puzzle_c * puz, unsigned int probNum, unsigned int solNum) {

  if (curAssembly) {
    delete curAssembly;
    curAssembly = 0;
  }

  hideMarker();
  clearSpaces();

  if ((probNum < puz->problemNumber()) &&
      (solNum < puz->probSolutionNumber(probNum))) {

    unsigned int num;

    curAssembly = new assembly_c(puz->probGetAssembly(probNum, solNum));
    const assembly_c * assm = curAssembly;

    unsigned int piece = 0;

    // and now the shapes
    for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++)
      for (unsigned int q = 0; q < puz->probGetShapeMax(probNum, p); q++) {

        if (assm->isPlaced(piece)) {

          voxel_c * vx = puz->getGridType()->getVoxel(puz->probGetShapeShape(probNum, p));

          bt_assert(vx->transform(assm->getTransformation(piece)));

          num = addSpace(vx);

          setSpacePosition(num, assm->getX(piece), assm->getY(piece), assm->getZ(piece), 1);

          setSpaceColor(num,
              pieceColorR(puz->probGetShape(probNum, p), q),
              pieceColorG(puz->probGetShape(probNum, p), q),
              pieceColorB(puz->probGetShape(probNum, p), q), 1);

        } else {

          voxel_c * vx = puz->getGridType()->getVoxel(puz->probGetShapeShape(probNum, p));

          num = addSpace(vx);

          setSpacePosition(num, 0, 0, 0, 1);

          setSpaceColor(num, 0);
        }

        piece++;
      }

    float cx, cy, cz;
    drawer->calculateSize(puz->probGetResultShape(probNum), &cx, &cy, &cz);
    setCenter(cx*0.5, cy*0.5, cz*0.5);
    setTransformationType(CenterTranslateRoateScale);
    showCoordinateSystem(false);
  }
}

void voxelFrame_c::showAssemblerState(const puzzle_c * puz, unsigned int probNum, const assembly_c * assm) {

  hideMarker();
  clearSpaces();

  if (probNum < puz->problemNumber()) {

    unsigned int num;
    unsigned int piece = 0;

    // and now the shapes
    for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++)
      for (unsigned int q = 0; q < puz->probGetShapeMax(probNum, p); q++) {

        if (assm->isPlaced(piece)) {

          voxel_c * vx = puz->getGridType()->getVoxel(puz->probGetShapeShape(probNum, p));

          bt_assert(vx->transform(assm->getTransformation(piece)));

          num = addSpace(vx);

          setSpacePosition(num, assm->getX(piece), assm->getY(piece), assm->getZ(piece), 1);

          setSpaceColor(num,
              pieceColorR(puz->probGetShape(probNum, p), q),
              pieceColorG(puz->probGetShape(probNum, p), q),
              pieceColorB(puz->probGetShape(probNum, p), q), 1);

        }

        piece++;
      }

    float cx, cy, cz;
    drawer->calculateSize(puz->probGetResultShape(probNum), &cx, &cy, &cz);
    setCenter(cx*0.5, cy*0.5, cz*0.5);

    setTransformationType(CenterTranslateRoateScale);
    showCoordinateSystem(false);

    num = addSpace(puz->getGridType()->getVoxel(puz->probGetResultShape(probNum)));
    setSpaceColor(num,
                        pieceColorR(puz->probGetResult(probNum)),
                        pieceColorG(puz->probGetResult(probNum)),
                        pieceColorB(puz->probGetResult(probNum)), 1);
    setDrawingMode(num, gridline);
  }
}

void voxelFrame_c::showPlacement(const puzzle_c * puz, unsigned int probNum, unsigned int piece, unsigned char trans, int x, int y, int z) {

  clearSpaces();
  hideMarker();
  setTransformationType(CenterTranslateRoateScale);
  showCoordinateSystem(false);

  float hx, hy, hz;
  hx = puz->probGetResultShape(probNum)->getHx();
  hy = puz->probGetResultShape(probNum)->getHy();
  hz = puz->probGetResultShape(probNum)->getHz();
  drawer->recalcSpaceCoordinates(&hx, &hy, &hz);

  float cx, cy, cz;
  drawer->calculateSize(puz->probGetResultShape(probNum), &cx, &cy, &cz);
  setCenter(cx*0.5-hx, cy*0.5-hy, cz*0.5-hz);

  hx = puz->probGetResultShape(probNum)->getHx();
  hy = puz->probGetResultShape(probNum)->getHy();
  hz = puz->probGetResultShape(probNum)->getHz();

  int num;

  if (trans < puz->getGridType()->getSymmetries()->getNumTransformationsMirror()) {

    int shape = 0;
    unsigned int p = piece;
    while (p >= puz->probGetShapeMax(probNum, shape)) {
      p -= puz->probGetShapeMax(probNum, shape);
      shape++;
    }

    voxel_c * vx = puz->getGridType()->getVoxel(puz->probGetShapeShape(probNum, shape));
    bt_assert(vx->transform(trans));
    num = addSpace(vx);

    setSpacePosition(num, x-hx, y-hy, z-hz, 1);
    setSpaceColor(num,
                          pieceColorR(puz->probGetShape(probNum, shape), p),
                          pieceColorG(puz->probGetShape(probNum, shape), p),
                          pieceColorB(puz->probGetShape(probNum, shape), p), 1);
    setDrawingMode(num, normal);
  }

  num = addSpace(puz->getGridType()->getVoxel(puz->probGetResultShape(probNum)));
  setSpaceColor(num,
                        pieceColorR(puz->probGetResult(probNum)),
                        pieceColorG(puz->probGetResult(probNum)),
                        pieceColorB(puz->probGetResult(probNum)), 1);
  setDrawingMode(num, gridline);
}


void voxelFrame_c::updatePositions(piecePositions_c *shifting) {

  for (unsigned int p = 0; p < spaceNumber(); p++) {
    setSpacePosition(p, shifting->getX(p), shifting->getY(p), shifting->getZ(p), 1);
    setSpaceColor(p, shifting->getA(p));
  }
}

void voxelFrame_c::dimStaticPieces(piecePositions_c *shifting) {

  for (unsigned int p = 0; p < spaceNumber(); p++) {
    setSpaceDim(p, !shifting->moving(p));
  }
}

void voxelFrame_c::updateVisibility(PieceVisibility * pcvis) {

  /* savety check, it might be possible to click onto the visibility
   * selector even if no solution is displayed, e.g. when there is no
   * solution, if we then have more pieces on the display as there
   * are blocks in the visibility selector we crash, so if the
   * number of blocks inside the visibility selector is smaller
   * than the number of visible voxel spaces, drop out
   */
  if (pcvis->blockNumber() < spaceNumber()) return;

  for (unsigned int p = 0; p < spaceNumber(); p++) {

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
}

void voxelDrawer_c::drawGridRect(double x0, double y0, double z0,
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

      if ((v1x) && (fabs(x1 - x0) < 0.01) ||
          (v1y) && (fabs(y1 - y0) < 0.01) ||
          (v1z) && (fabs(z1 - z0) < 0.01)) {
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

      if ((v2x) && (fabs(x2 - (x0+v2x+v1x)) < 0.01) ||
          (v2y) && (fabs(y2 - (y0+v2y+v1y)) < 0.01) ||
          (v2z) && (fabs(z2 - (z0+v2z+v1z)) < 0.01)) {
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


void voxelDrawer_c::drawGridTriangle(double x0, double y0, double z0,
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

  float xe = x0 + v1x;
  float ye = y0 + v1y;
  float ze = z0 + v1z;

  while ((fabs(x1 - xe) > 0.01) || (fabs(y1 - ye) > 0.01) || (fabs(z1 - ze) > 0.01)) {

    x1 += v1x/diag;
    y1 += v1y/diag;
    z1 += v1z/diag;

    x2 += v2x/diag;
    y2 += v2y/diag;
    z2 += v2z/diag;

    glVertex3f(x1, y1, z1); glVertex3f(x2, y2, z2);
  }

  glEnd();
}

// this function finds out if a given square is inside the selected region
// this check includes the symmetric and column edit modes
bool voxelDrawer_c::inRegion(int x, int y, int z, int x1, int x2, int y1, int y2, int z1, int z2, int sx, int sy, int sz, int mode) {

  if ((x < 0) || (y < 0) || (z < 0) || (x >= sx) || (y >= sy) || (z >= sz)) return false;

  if (mode == 0)
    return (x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2);
  if (mode == voxelFrame_c::TOOL_STACK_Y)
    return (x1 <= x) && (x <= x2) && (z1 <= z) && (z <= z2);
  if (mode == voxelFrame_c::TOOL_STACK_X)
    return (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2);
  if (mode == voxelFrame_c::TOOL_STACK_Z)
    return (x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2);

  if (mode == voxelFrame_c::TOOL_STACK_X + voxelFrame_c::TOOL_STACK_Y)
    return ((x1 <= x) && (x <= x2) || (y1 <= y) && (y <= y2)) && ((z1 <= z) && (z <= z2));
  if (mode == voxelFrame_c::TOOL_STACK_X + voxelFrame_c::TOOL_STACK_Z)
    return ((x1 <= x) && (x <= x2) || (z1 <= z) && (z <= z2)) && ((y1 <= y) && (y <= y2));
  if (mode == voxelFrame_c::TOOL_STACK_Y + voxelFrame_c::TOOL_STACK_Z)
    return ((y1 <= y) && (y <= y2) || (y1 <= y) && (y <= y2)) && ((x1 <= x) && (x <= x2));

  if (mode == voxelFrame_c::TOOL_STACK_X + voxelFrame_c::TOOL_STACK_Y + voxelFrame_c::TOOL_STACK_Z)
    return ((x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2) ||
        (x1 <= x) && (x <= x2) && (z1 <= z) && (z <= z2) ||
        (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2));

  if (mode & voxelFrame_c::TOOL_MIRROR_X)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~voxelFrame_c::TOOL_MIRROR_X) ||
      inRegion(sx-x-1, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~voxelFrame_c::TOOL_MIRROR_X);

  if (mode & voxelFrame_c::TOOL_MIRROR_Y)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~voxelFrame_c::TOOL_MIRROR_Y) ||
      inRegion(x, sy-y-1, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~voxelFrame_c::TOOL_MIRROR_Y);

  if (mode & voxelFrame_c::TOOL_MIRROR_Z)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~voxelFrame_c::TOOL_MIRROR_Z) ||
      inRegion(x, y, sz-z-1, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~voxelFrame_c::TOOL_MIRROR_Z);

  return false;
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

  if (!doUpdates)
    return;

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

    arcBall->setBounds(w(), h());

    unsigned char r, g, b;
    Fl::get_color(color(), r, g, b);
    glClearColor(r/255.0, g/255.0, b/255.0, 0);
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

  glPushMatrix();
  glTranslatef(0, 0, -size*2);

  if (colors == anaglyphColor) {
    glPushMatrix();
    glTranslatef(-0.04, 0, 0);
    glRotatef(-1, 0, 1, 0);
    glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
    drawData();
    glPopMatrix();
    glTranslatef(0.04, 0, 0);
    glRotatef(1, 0, 1, 0);
    glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  drawData();
  glPopMatrix();

  if (colors == anaglyphColor) {
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  if (cb)
    cb->PostDraw();
}

void voxelFrame_c::update(bool doIt) {
  doUpdates = doIt;
  if (doIt)
    redraw();
}

int voxelFrame_c::handle(int event) {

  if (Fl_Gl_Window::handle(event))
    return 1;

  switch(event) {
  case FL_PUSH:

    if (!Fl::event_state(FL_SHIFT | FL_ALT | FL_CTRL))
      arcBall->click(Fl::event_x(), Fl::event_y());

    do_callback();

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

void voxelFrame_c::addRotationTransformation(void) {
  arcBall->addTransform();
}

void voxelFrame_c::updateRequired(void) {
  redraw();
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

