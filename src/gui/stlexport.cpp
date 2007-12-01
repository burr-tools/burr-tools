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
#include "stlexport.h"

#include "BlockList.h"
#include "view3dgroup.h"
#include "blocklistgroup.h"

#include "../lib/puzzle.h"
#include "../lib/stl.h"
#include "../lib/gridtype.h"
#include "../lib/bt_assert.h"
#include "../lib/voxel.h"

#include <FL/Fl.H>
#include <FL/fl_ask.H>

static void cb_stlExportAbort_stub(Fl_Widget* /*o*/, void* v) { ((stlExport_c*)(v))->cb_Abort(); }

void stlExport_c::cb_Abort(void) {
  hide();
}

static void cb_stlExportExport_stub(Fl_Widget* /*o*/, void* v) { ((stlExport_c*)(v))->cb_Export(); }

void stlExport_c::cb_Export(void) {

  exportSTL(ShapeSelect->getSelection());

}

static void cb_stlExport3DUpdate_stub(Fl_Widget* /*o*/, void* v) { ((stlExport_c*)(v))->cb_Update3DView(); }
void stlExport_c::cb_Update3DView(void) {

  view3D->showSingleShape(puzzle, ShapeSelect->getSelection());
}

stlExport_c::stlExport_c(puzzle_c * p, const guiGridType_c * ggt) : LFl_Double_Window(false), puzzle(p) {

  label("Export STL");

  stl = p->getGridType()->getStlExporter();
  bt_assert(stl);

  LFl_Frame *fr;

  {
    fr = new LFl_Frame(0, 0, 1, 1);

    (new LFl_Box("File name", 0, 0))->stretchLeft();
    (new LFl_Box("Path", 0, 1))->stretchLeft();

    (new LFl_Box(1, 0))->setMinimumSize(5, 0);
    (new LFl_Box(3, 0))->setMinimumSize(5, 0);

    Fname = new LFl_Input(2, 0, 3, 1);
    Fname->value("test");
    Fname->weight(1, 0);
    Fname->setMinimumSize(50, 0);
    Pname = new LFl_Input(2, 1, 3, 1);
    Pname->value("./");
    Pname->weight(1, 0);
    Pname->setMinimumSize(50, 0);

    Binary = new LFl_Check_Button("Binary STL", 0, 2, 3, 1);
    if (stl->getBinaryMode())
      Binary->value(1);
    else
      Binary->value(0);

    fr->end();
  }

  {
    fr = new LFl_Frame(0, 1, 1, 1);

    for (unsigned int i = 0; i < stl->numParameters(); i++) {
      (new LFl_Box(stl->getParameterName(i), 0, i))->stretchRight();
      (new LFl_Box(1, i))->setMinimumSize(5, 0);
      params.push_back(new LFl_Float_Input(2, i, 1, 1));
      char val[10];
      snprintf(val, 10, "%2.2f", stl->getParameter(i));
      (*params.rbegin())->value(val);
      (*params.rbegin())->weight(1, 0);
    }

    fr->end();
  }

  {
    ShapeSelect = new PieceSelector(0, 0, 20, 20, puzzle);

    ShapeSelect->setSelection(0);

    LBlockListGroup_c * gr = new LBlockListGroup_c(0, 2, 1, 1, ShapeSelect);
    gr->callback(cb_stlExport3DUpdate_stub, this);
    gr->setMinimumSize(200, 100);
    gr->stretch();
  }

  {
    layouter_c * l = new layouter_c(0, 3, 2, 1);

    status = new LFl_Box();
    status->weight(1, 0);
    status->pitch(7);
    status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    BtnStart = new LFl_Button("Export STL", 1, 0);
    BtnStart->pitch(7);
    BtnStart->callback(cb_stlExportExport_stub, this);

    BtnAbbort = new LFl_Button("Abort", 2, 0);
    BtnAbbort->pitch(7);
    BtnAbbort->callback(cb_stlExportAbort_stub, this);

    l->end();
  }

  view3D = new LView3dGroup(1, 0, 1, 3, ggt);
  view3D->setMinimumSize(400, 400);
  cb_Update3DView();

  set_modal();
}

void stlExport_c::exportSTL(int shape)
{
  char name[1000];

  voxel_c *v = puzzle->getShape(shape);

  for (unsigned int i = 0; i < stl->numParameters(); i++)
    stl->setParameter(i, atof(params[i]->value()));

  stl->setBinaryMode(Binary->value() != 0);

  int idx = 0;

  if (Pname->value() && Pname->value()[0] && Pname->value()[strlen(Pname->value())-1] != '/') {
      idx = snprintf(name, 1000, "%s/%s", Pname->value(), Fname->value());
  } else {
      idx = snprintf(name, 1000, "%s%s", Pname->value(), Fname->value());
  }

  // append name
  if (v->getName().length())
    idx += snprintf(name+idx, 1000-idx, "_%s", v->getName().c_str());
  else
    idx += snprintf(name+idx, 1000-idx, "_S%d", shape+1);

  try {
    stl->write(name,  v);
  }

  catch (stlException_c * e) {
    fl_message(e->comment);
  }
}
