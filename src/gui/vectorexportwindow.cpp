/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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
#include "vectorexportwindow.h"

#include "../flu/Flu_File_Chooser.h"

static const char * extensions[] = {
  "ps", "eps", "tex", "pdf", "svg", "pgf"
};

static void cb_Button1_stub(Fl_Widget* /*o*/, void* v) {
  ((vectorExportWindow_c*)v)->cancelled = false;
  ((vectorExportWindow_c*)v)->hide();
}
static void cb_Button2_stub(Fl_Widget* /*o*/, void* v) { ((vectorExportWindow_c*)v)->hide(); }

static void cb_FileChoose_stub(Fl_Widget* /*o*/, void* v) { ((vectorExportWindow_c*)v)->cb_FileChoose(); }
void vectorExportWindow_c::cb_FileChoose(void) {

  const char * newFile = flu_file_chooser("File to save image to", "", inp->value());

  if (newFile)
    inp->value(newFile);
}

vectorExportWindow_c::vectorExportWindow_c(void) : LFl_Double_Window(false) {

  label("Parameters for Vector Export");

  layouter_c * o = new layouter_c(0, 0, 1, 1);

  (new LFl_Box("Filename:", 0, 0, 3, 1))->stretchLeft();;

  inp = new LFl_Input(0, 1, 1, 1);
  inp->weight(1, 0);
  inp->value("./out");
  (new LFl_Box(1, 1, 1, 1))->setMinimumSize(5, 0);
  LFl_Button * btn = new LFl_Button("...", 2, 1, 1, 1);
  btn->callback(cb_FileChoose_stub, this);

  o->end();

  (new LFl_Box(0, 1, 1, 1))->setMinimumSize(0, 5);

  (new LFl_Box("File type:", 0, 2, 1, 1))->stretchLeft();

  o = new layouter_c(0, 3, 1, 1);

  /* only put LFl_RadioButtons in this layouter and keep the
   * oder the same as in voxelFrame enum
   */
  new LFl_Radio_Button("Postscript", 0, 0, 1, 1);
  new LFl_Radio_Button("Encapsulated Postscript", 0, 1, 1, 1);
  new LFl_Radio_Button("TeX", 1, 0, 1, 1);
  new LFl_Radio_Button("PDF", 1, 1, 1, 1);
  (new LFl_Radio_Button("SVG", 2, 0, 1, 1))->value(1);
  new LFl_Radio_Button("PGF", 2, 1, 1, 1);

  o->end();
  radGroup = o;

  (new LFl_Box(0, 4, 1, 1))->setMinimumSize(0, 5);

  o = new layouter_c(0, 5, 1, 1);

  btn = new LFl_Button("Export", 0, 0, 1, 1);
  btn->callback(cb_Button1_stub, this);
  btn->weight(1, 0);
  btn = new LFl_Button("Cancel", 2, 0, 1, 1);
  btn->callback(cb_Button2_stub, this);
  btn->weight(1, 0);

  (new LFl_Box(1, 0, 1, 1))->setMinimumSize(5, 0);

  o->end();

  cancelled = true;
}

const char * vectorExportWindow_c::getFileName(void) {

  return inp->value();
}

voxelFrame_c::VectorFiletype vectorExportWindow_c::getVectorType(void) {

  for (int i = 0; i < radGroup->children(); i++)
    if (((LFl_Radio_Button*)radGroup->child(i))->value() > 0)
      return (voxelFrame_c::VectorFiletype)i;

  return voxelFrame_c::VFT_SVG;
}

