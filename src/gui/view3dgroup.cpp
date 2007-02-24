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
#include "view3dgroup.h"

#include "guigridtype.h"

#include "../lib/puzzle.h"
#include "../lib/disasmtomoves.h"

#include <FL/Fl_Pixmap.H>
#include <FL/Fl_Multiline_Output.H>

#include <math.h>

// some tool widgets, that may be swapped out later into another file

static void cb_View3dGroupSlider_stub(Fl_Widget* o, void* v) { ((LView3dGroup*)(o->parent()))->cb_slider(); }

void LView3dGroup::cb_slider(void) {
  View3D->setSize(exp(6-slider->value()));
}


LView3dGroup::LView3dGroup(int x, int y, int w, int h, const guiGridType_c * ggt) : Fl_Group(0, 0, 50, 50), layoutable_c(x, y, w, h) {

  x = y = 0;
  w = h = 50;

  box(FL_DOWN_BOX);

  View3D = ggt->getVoxelDrawer(x, y, w-15, h);
  View3D->tooltip(" Rotate the puzzle by dragging with the mouse ");
  View3D->box(FL_NO_BOX);

  slider = new Fl_Slider(x+w-15, y, 15, h);
  slider->tooltip("Zoom view.");
  slider->maximum(6);
  slider->minimum(0);
  slider->step(0.01);
  slider->value(2);
  slider->callback(cb_View3dGroupSlider_stub);
  slider->clear_visible_focus();

  cb_slider();

  resizable(View3D);
  end();
}

void LView3dGroup::newGridType(const guiGridType_c * ggt) {

  View3D->hide();

  voxelDrawer_c * nv;

  nv = ggt->getVoxelDrawer(View3D->x(), View3D->y(), View3D->w(), View3D->h());
  nv->tooltip(" Rotate the puzzle by dragging with the mouse ");
  nv->box(FL_NO_BOX);

  resizable(nv);

  remove(View3D);
  delete View3D;

  View3D = nv;
  add(View3D);

  cb_slider();

  View3D->show();
}

void LView3dGroup::showSingleShape(const puzzle_c * puz, unsigned int shapeNum) {
  View3D->update(false);
  View3D->showSingleShape(puz, shapeNum);
  View3D->update(true);
}

void LView3dGroup::showProblem(const puzzle_c * puz, unsigned int probNum, unsigned int selShape) {

  View3D->update(false);
  View3D->showProblem(puz, probNum, selShape);
  View3D->update(true);
}

void LView3dGroup::showColors(const puzzle_c * puz, voxelDrawer_c::colorMode mode) {
  View3D->update(false);
  View3D->showColors(puz, mode);
  View3D->update(true);
}

void LView3dGroup::showAssembly(const puzzle_c * puz, unsigned int probNum, unsigned int solNum) {
  View3D->update(false);
  View3D->showAssembly(puz, probNum, solNum);
  View3D->update(true);
}

void LView3dGroup::showPlacement(const puzzle_c * puz, unsigned int probNum, unsigned int piece, unsigned char trans, int x, int y, int z) {

  View3D->update(false);
  View3D->showPlacement(puz, probNum, piece, trans, x, y, z);
  View3D->update(true);
}

void LView3dGroup::updatePositions(piecePositions_c *shifting) {
  View3D->update(false);
  View3D->updatePositions(shifting);
  View3D->update(true);
}

void LView3dGroup::updateVisibility(PieceVisibility * pcvis) {
  View3D->update(false);
  View3D->updateVisibility(pcvis);
  View3D->update(true);
}

