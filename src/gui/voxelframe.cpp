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
#include "voxeldrawer.h"

#include "piececolor.h"
#include "configuration.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"
#include "../lib/problem.h"
#include "../lib/assembly.h"
#include "../lib/disasmtomoves.h"

#include <math.h>

#include <FL/Fl.H>

#ifdef WIN32
#include <GL/glext.h>
#endif

#include "gl2ps.h"

voxelFrame_c::voxelFrame_c(int x,int y,int w,int h) :
  Fl_Gl_Window(x,y,w,h),
  drawer(0),
  curAssembly(0),
  markerType(-1),
  arcBall(new arcBall_c(w, h)),
  size(10), cb(0),
  colors(pieceColor),
  _useLightning(true),
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
  if (drawer) delete drawer;
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
        arcBall->addTransform();
        {
          float cx, cy, cz;
          drawer->calculateSize(shapes[piece].shape, &cx, &cy, &cz);
          glTranslatef(-0.5*cx, -0.5*cy, -0.5*cz);
        }
        break;
      case TranslateRoateScale:
        arcBall->addTransform();
        glTranslatef(shapes[piece].x,
                     shapes[piece].y,
                     shapes[piece].z);
        {
          float cx, cy, cz;
          drawer->calculateSize(shapes[piece].shape, &cx, &cy, &cz);
          glTranslatef(-0.5*cx, -0.5*cy, -0.5*cz);
        }
        glScalef(shapes[piece].scale, shapes[piece].scale, shapes[piece].scale);
        break;
      case CenterTranslateRoateScale:
        arcBall->addTransform();
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

      if (shapes[piece].list) {

        glCallList(shapes[piece].list);

      } else {

        if (config.useDisplayLists()) {

          shapes[piece].list = glGenLists(1);

          if (shapes[piece].list)
            glNewList(shapes[piece].list, GL_COMPILE_AND_EXECUTE);

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
                case anaglyphColorL:
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

              if (colors == anaglyphColor || colors == anaglyphColorL) {
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

        if (shapes[piece].list)
          glEndList();

      }

      // the marker should be only active, when only one shape is there
      // otherwise it's drawn for every shape
      if ((markerType >= 0) && (mX1 <= mX2) && (mY1 <= mY2)) {

        if (_useLightning) glDisable(GL_LIGHTING);
        glDisable(GL_BLEND);

        glColor4f(0, 0, 0, 1);

        drawer->drawCursor(shapes[piece].shape, mX1, mX2, mY1, mY2, mZ, markerType);

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

  i.mode = normal;

  i.x = i.y = i.z = 0;
  i.scale = 1;

  i.dim = false;

  i.list = 0;

  shapes.push_back(i);

  return shapes.size()-1;
}

void voxelFrame_c::clearSpaces(void) {

  for (unsigned int i = 0; i < shapes.size(); i++) {
    if (shapes[i].list) glDeleteLists(shapes[i].list, 1);
    delete shapes[i].shape;
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

  drawer->recalcSpaceCoordinates(&x, &y, &z);

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

void voxelFrame_c::showSingleShape(const puzzle_c * puz, unsigned int shapeNum) {

  hideMarker();
  clearSpaces();
  unsigned int num = addSpace(puz->getGridType()->getVoxel(puz->getShape(shapeNum)));

  setSpaceColor(num, pieceColorR(shapeNum), pieceColorG(shapeNum), pieceColorB(shapeNum), 1);

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
    for (unsigned int p = 0; p < pr->shapeNumber(); p++)
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
    while (square * (square-2*factor) < pr->shapeNumber()) square++;

    unsigned int num;

    // now place the result shape
    if (pr->resultValid()) {

      num = addSpace(pr->getGridType()->getVoxel(pr->getResultShape()));
      setSpaceColor(num,
                            pieceColorR(pr->getResult()),
                            pieceColorG(pr->getResult()),
                            pieceColorB(pr->getResult()), 1);
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
    for (unsigned int p = 0; p < pr->shapeNumber(); p++) {
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

    curAssembly = new assembly_c(puz->getAssembly(solNum));
    const assembly_c * assm = curAssembly;

    unsigned int piece = 0;

    // and now the shapes
    for (unsigned int p = 0; p < puz->shapeNumber(); p++)
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
    drawer->calculateSize(puz->getResultShape(), &cx, &cy, &cz);
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
    for (unsigned int p = 0; p < puz->shapeNumber(); p++)
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
    drawer->calculateSize(puz->getResultShape(), &cx, &cy, &cz);
    setCenter(cx*0.5, cy*0.5, cz*0.5);

    trans = CenterTranslateRoateScale;
    _showCoordinateSystem = false;

    num = addSpace(puz->getGridType()->getVoxel(puz->getResultShape()));
    setSpaceColor(num,
                        pieceColorR(puz->getResult()),
                        pieceColorG(puz->getResult()),
                        pieceColorB(puz->getResult()), 1);
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
  drawer->recalcSpaceCoordinates(&hx, &hy, &hz);

  float cx, cy, cz;
  drawer->calculateSize(puz->getResultShape(), &cx, &cy, &cz);
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
                        pieceColorR(puz->getResult()),
                        pieceColorG(puz->getResult()),
                        pieceColorB(puz->getResult()), 1);
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

  if (!drawer) return;

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
      arcBall->click(Fl::event_x(), Fl::event_y());

    do_callback();

    return 1;

  case FL_DRAG:
    arcBall->drag(Fl::event_x(), Fl::event_y());
    redraw();

    do_callback();

    return 1;

  case FL_RELEASE:

    arcBall->clack(Fl::event_x(), Fl::event_y());

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

#error vector file types don't fit to GL2PS file types
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

