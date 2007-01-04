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

gridTypeGui_0_c::gridTypeGui_0_c(int x, int y, int w, int h, gridType_c * g) {

  new LFl_Box("In a distant future there might be parameters\n"
      "to stretch and shear the cube into a parallelepiped\n"
      "but not right now!!", 0, 0);

  end();
}

gridTypeGui_1_c::gridTypeGui_1_c(int x, int y, int w, int h, gridType_c * gt) {
  new LFl_Box("There are no parameters for this space grid!\n"
      "This space grid also has no disassembler (yet)", 0, 0);

  end();
}

gridTypeGui_2_c::gridTypeGui_2_c(int x, int y, int w, int h, gridType_c * gt) {

  new LFl_Box("There are no parameters for this space grid!\n"
      "This space grid also has no assembler and disassembler (yet)", 0, 0);

  end();
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
  gti.push_back(new gridTypeInfos_c(new gridType_c(gridType_c::GT_SPHERES)));

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

