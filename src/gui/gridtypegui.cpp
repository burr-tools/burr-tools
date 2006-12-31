/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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

#include "gridtypegui.h"

#include "guigridtype.h"
#include "Layouter.h"

#include "../lib/gridtype.h"

#include <FL/Fl_Browser_.H>
#include <FL/Fl_Image.H>

static void cb_gridTypeGui0Update(Fl_Widget *o, void *v) { ((gridTypeGui_0_c*)(v))->updateValues(); }
void gridTypeGui_0_c::updateValues(void) {

  unsigned char bb = 0;
  if (arxy->value()) bb |= 1;
  if (arxz->value()) bb |= 2;
  if (aryz->value()) bb |= 4;

  switch (bb) {
    case 0:
      axyeaxz->activate();
      axyeayz->activate();
      axzeayz->activate();
      break;
    case 1:
      axyeaxz->deactivate(); axyeaxz->set();
      axyeayz->deactivate(); axyeayz->set();
      axzeayz->activate();
      break;
    case 2:
      axyeaxz->deactivate(); axyeaxz->set();
      axyeayz->activate();
      axzeayz->deactivate(); axzeayz->set();
      break;
    case 3:
      axyeaxz->deactivate(); axyeaxz->clear();
      axyeayz->deactivate(); axyeayz->set();
      axzeayz->deactivate(); axzeayz->clear();
      break;
    case 4:
      axyeaxz->activate();
      axyeayz->deactivate(); axyeayz->set();
      axzeayz->deactivate(); axzeayz->set();
      break;
    case 5:
      axyeaxz->deactivate(); axyeaxz->set();
      axyeayz->deactivate(); axyeayz->clear();
      axzeayz->deactivate(); axzeayz->clear();
      break;
    case 6:
      axyeaxz->deactivate(); axyeaxz->clear();
      axyeayz->deactivate(); axyeayz->clear();
      axzeayz->deactivate(); axzeayz->set();
      break;
    case 7:
      axyeaxz->deactivate(); axyeaxz->clear();
      axyeayz->deactivate(); axyeayz->clear();
      axzeayz->deactivate(); axzeayz->clear();
      break;
  }

  gt->setBrickXneY(lxy->value());
  gt->setBrickXneZ(lxz->value());
  gt->setBrickYneZ(lyz->value());
  gt->setBrickAngleOrthoXY(arxy->value());
  gt->setBrickAngleOrthoXZ(arxz->value());
  gt->setBrickAngleOrthoYZ(aryz->value());
  gt->setBrickAngleXYneXZ(axyeaxz->value());
  gt->setBrickAngleXYneYZ(axyeayz->value());
  gt->setBrickAngleXZneYZ(axzeayz->value());
}

gridTypeGui_0_c::gridTypeGui_0_c(int x, int y, int w, int h, gridType_c * g) : gt(g) {

  LFl_Frame *fr;

  {
    fr = new LFl_Frame(0, 0);

    lxy = new LFl_Check_Button("Length X different from Length Y", 0, 0);
    lxz = new LFl_Check_Button("Length X different from Length Z", 0, 1);
    lyz = new LFl_Check_Button("Length Y different from Length Z", 0, 2);

    if (gt->getBrickXneY()) lxy->set();
    if (gt->getBrickXneZ()) lxz->set();
    if (gt->getBrickYneZ()) lyz->set();

    fr->end();
  }
  {
    fr = new LFl_Frame(0, 1);

    arxy = new LFl_Check_Button("Angle between X and Y Axis is 90", 0, 0);
    arxz = new LFl_Check_Button("Angle between X and Z Axis is 90", 0, 1);
    aryz = new LFl_Check_Button("Angle between Y and Z Axis is 90", 0, 2);

    if (gt->getBrickAngleOrthoXY()) arxy->set();
    if (gt->getBrickAngleOrthoXZ()) arxz->set();
    if (gt->getBrickAngleOrthoYZ()) aryz->set();

    fr->end();
  }
  {
    fr = new LFl_Frame(0, 2);

    axyeaxz = new LFl_Check_Button("X-Y Axis and X-Z Axis Angles are different", 0, 0);
    axyeayz = new LFl_Check_Button("X-Y Axis and Y-Z Axis Angles are different", 0, 1);
    axzeayz = new LFl_Check_Button("X-Z Axis and Y-Z Axis Angles are different", 0, 2);

    if (gt->getBrickAngleXYneXZ()) axyeaxz->set();
    if (gt->getBrickAngleXYneYZ()) axyeayz->set();
    if (gt->getBrickAngleXZneYZ()) axzeayz->set();

    fr->end();
  }


  lxy->callback(cb_gridTypeGui0Update, this);
  lxz->callback(cb_gridTypeGui0Update, this);
  lyz->callback(cb_gridTypeGui0Update, this);
  arxy->callback(cb_gridTypeGui0Update, this);
  arxz->callback(cb_gridTypeGui0Update, this);
  aryz->callback(cb_gridTypeGui0Update, this);
  axyeaxz->callback(cb_gridTypeGui0Update, this);
  axyeayz->callback(cb_gridTypeGui0Update, this);
  axzeayz->callback(cb_gridTypeGui0Update, this);

  end();

  updateValues();
}

gridTypeGui_1_c::gridTypeGui_1_c(int x, int y, int w, int h, gridType_c * gt) {
  new LFl_Box("There are no parameters for this space grid!\n"
      "This space grid also has not assembler and disassembler (yet)", 0, 0);
}


class gridTypeInfos_c {
  public:

    gridType_c * gt;
    guiGridType_c * ggt;
    LFl_Radio_Button * btn;
    gridTypeGui_c * gui;

    gridTypeInfos_c(gridType_c * g) : gt(g), ggt(new guiGridType_c(g)) {}
};

static void cb_WindowButton_stub(Fl_Widget *o, void *v) { ((Fl_Double_Window*)(v))->hide(); }

gridTypeParameterWindow_c::gridTypeParameterWindow_c(guiGridType_c * ggt) : LFl_Double_Window(false) {
  label("Set parameters for grid type");

  ggt->getConfigurationDialog(0, 0, 1, 1);

  LFl_Button * b = new LFl_Button("Close", 0, 1);
  b->pitch(7);
  b->callback(cb_WindowButton_stub, this);
  b->box(FL_THIN_UP_BOX);
}

static void cb_gridTypeSelectorSelect_stub(Fl_Widget *o, void *v) { ((gridTypeSelectorWindow_c*)(v))->select_cb(); }
void gridTypeSelectorWindow_c::select_cb(void) {
  for (unsigned int i = 0; i < gti.size(); i++) {
    if (gti[i]->btn->value()) {

      gti[current]->gui->hide();
      current = i;
      gti[current]->gui->show();

    }
  }
}

gridTypeSelectorWindow_c::gridTypeSelectorWindow_c(void) : LFl_Double_Window(false) {

  /* for each grid type available we need to create an instance
   * here and put it into a gridtypeinfo class and into the
   * vector. This vector will be later on the one
   * with all required information
   */
  gti.push_back(new gridTypeInfos_c(new gridType_c()));
  gti.push_back(new gridTypeInfos_c(new gridType_c(gridType_c::GT_TRIANGULAR_PRISM)));

  /* from here on the code should not need changes when new grid types are added */

  label("Select space grid");

  LFl_Frame *fr;

  /* first the selector for all grid types */
  {
    fr = new LFl_Frame(0, 0, 1, 2);

    for (unsigned int i = 0; i < gti.size(); i++) {
      gti[i]->btn = new LFl_Radio_Button(gti[i]->ggt->getName(), 0, i);
      gti[i]->btn->callback(cb_gridTypeSelectorSelect_stub, this);
      if (i == 0)
        gti[i]->btn->set();
    }

    (new LFl_Box(0, gti.size()))->weight(0, 100);

    fr->end();
  }

  (new LFl_Box("Attention: Concrete values within the\n"
      "parameters are never evaluated, the solver only\n"
      "cares for equal and not equal. So if you want something\n"
      "that is twice as high as wide, assemble is out of\n"
      "several unit pieces.", 1, 0))->pitch(7);

  /* now all the parameter boxes, only the first one is visible, the others are hidden for now */
  {
    fr = new LFl_Frame(1, 1);

    for (unsigned int i = 0; i < gti.size(); i++) {
      gti[i]->gui = gti[i]->ggt->getConfigurationDialog(0, 0, 1, 1);

      if (i != 0)
        gti[i]->gui->hide();
    }

    fr->end();
  }

  /* finally the 3D view that contains exactly one voxel */


  /* now the buttons */
  {
    layouter_c * l = new layouter_c(0, 2, 3, 1);

    LFl_Button * b = new LFl_Button("OK", 0, 0);
    b->pitch(7);
    b->callback(cb_WindowButton_stub, this);
    b->box(FL_THIN_UP_BOX);

    l->end();
  }

  current = 0;
}

gridTypeSelectorWindow_c::~gridTypeSelectorWindow_c(void) {
  for (unsigned int i = 0; i < gti.size(); i++) {
    if (gti[i]->gt)
      delete gti[i]->gt;
    delete gti[i]->ggt;
    delete gti[i];
  }
}


gridType_c * gridTypeSelectorWindow_c::getGridType(void) {
  gridType_c * tmp = gti[current]->gt;
  gti[current]->gt = 0;
  return tmp;
}

