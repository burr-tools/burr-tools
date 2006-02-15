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
#include "ImageExport.h"
#include "pieceColor.h"
#include "VoxelDrawer.h"
#include "Image.h"

#include <FL/Fl.h>

#define IMAGESIZE 512

class MyVoxelDrawer : public VoxelDrawer {

  private:

    VoxelView *vv;

  public:

    MyVoxelDrawer(VoxelView * v) : vv(v) {}

    virtual void addRotationTransformation(void) {
      vv->getArcBall()->addTransform();
    }
};

static void cb_ImageExportAbort_stub(Fl_Widget* o, void* v) { ((ImageExportWindow*)(v))->cb_Abort(); }
void ImageExportWindow::cb_Abort(void) {
  hide();
}

static void cb_ImageExportExport_stub(Fl_Widget* o, void* v) { ((ImageExportWindow*)(v))->cb_Export(); }
void ImageExportWindow::cb_Export(void) {

  status->label("Prepare for image");

  glDrawBuffer(GL_BACK);
  glReadBuffer(GL_BACK);

  MyVoxelDrawer dr(view3D->getView());

  // calculate antialiasing factor
  int aa = 1;
  if (AA2->value()) aa = 2;
  if (AA3->value()) aa = 3;
  if (AA4->value()) aa = 4;
  if (AA5->value()) aa = 5;

  status->label("Create image");
  Image i(atoi(SizePixelX->value())*aa, atoi(SizePixelY->value())*aa, &dr, tr);

  if (BgWhite->value()) {
    status->label("Set background");
    i.deTransparentize(255, 255, 255);
  }

  if (aa > 1) {
    status->label("Apply antialiasing");
    i.scaleDown(aa);
  }

  {
    char name[1000];

    if (Pname->value() && Pname->value()[0] && Pname->value()[strlen(Pname->value())-1] != '/')
      snprintf(name, 1000, "%s/%s", Pname->value(), Fname->value());
    else
      snprintf(name, 1000, "%s%s", Pname->value(), Fname->value());


    status->label("Saving");
    i.saveToPNG(name);
  }

  glDrawBuffer(GL_FRONT);
  glReadBuffer(GL_FRONT);

  status->label(0);
  hide();
}

ImageExportWindow::ImageExportWindow(puzzle_c * p) : puzzle(p) {

  label("Export Images");

  LFl_Frame *fr;

  fr = new LFl_Frame(0, 0, 1, 2);
  BgWhite = new LFl_Radio_Button("White Background", 0, 0);
  BgTransp = new LFl_Radio_Button("Transparent Background", 0, 1);
  BgWhite->value(1);
  (new LFl_Box(0, 2))->weight(0, 1);
  fr->end();

  fr = new LFl_Frame(0, 2);

  AA1 = new LFl_Radio_Button("No Antialiasing", 0, 0);
  AA2 = new LFl_Radio_Button("2x2 Supersampling", 0, 1);
  AA3 = new LFl_Radio_Button("3x3 Supersampling", 0, 2);
  AA3->value(1);
  AA4 = new LFl_Radio_Button("4x4 Supersampling", 0, 3);
  AA5 = new LFl_Radio_Button("5x5 Supersampling", 0, 4);

  new LFl_Round_Button("Use piece colors", 0, 5);
  new LFl_Round_Button("Use color constraint colors", 0, 6);
  new LFl_Check_Button("Dim static pieces", 0, 7);
  (new LFl_Box(0, 8))->weight(0, 1);
  fr->end();

  // user defined size input
  fr = new LFl_Frame(1, 1, 1, 2);

  {
    int y = 0;

    new LFl_Radio_Button("A4 Portrait", 0, y++, 5, 1);
    new LFl_Radio_Button("A4 Landscape", 0, y++, 5, 1);
    new LFl_Radio_Button("Legal Portrait", 0, y++, 5, 1);
    new LFl_Radio_Button("Legal Landscape", 0, y++, 5, 1);
    (new LFl_Radio_Button("manual", 0, y++, 5, 1))->value(1);

    (new LFl_Box("Size X", 0, y))->stretchRight();
    new LFl_Input(2, y);
    (new LFl_Box("mm", 4, y))->stretchLeft();
    (new LFl_Box(3, y))->setMinimumSize(5, 0);
    (new LFl_Box(1, y++))->setMinimumSize(5, 0);

    (new LFl_Box("Size Y", 0, y))->stretchRight();
    new LFl_Input(2, y);
    (new LFl_Box("mm", 4, y++))->stretchLeft();

    (new LFl_Box("DPI", 0, y))->stretchRight();
    new LFl_Input(2, y++);

    (new LFl_Box("Pixel X", 0, y))->stretchRight();
    SizePixelX = new LFl_Int_Input(2, y++);
    SizePixelX->value("300");
    SizePixelX->setMinimumSize(50, 0);

    (new LFl_Box("Pixel Y", 0, y))->stretchRight();
    SizePixelY = new LFl_Int_Input(2, y++);
    SizePixelY->value("300");

    (new LFl_Box(0, y))->weight(0, 1);

    fr->end();
  }

  fr = new LFl_Frame(0, 3, 2, 1);

  (new LFl_Box("File name", 0, 0))->stretchLeft();
  (new LFl_Box("Path", 0, 1))->stretchLeft();
  (new LFl_Box("Number of files", 0, 2, 3, 1))->stretchLeft();
  (new LFl_Box("Number of images", 0, 3, 3, 1))->stretchLeft();

  (new LFl_Box(1, 0))->setMinimumSize(5, 0);
  (new LFl_Box(3, 0))->setMinimumSize(5, 0);

  Fname = new LFl_Input(2, 0, 3, 1);
  Fname->value("test.png");
  Fname->weight(1, 0);
  Pname = new LFl_Input(2, 1, 3, 1);
  new LFl_Int_Input(4, 2);
  new LFl_Int_Input(4, 3);

  fr->end();

  fr = new LFl_Frame(0, 4, 2, 1);

  new LFl_Radio_Button("Export Shape", 0, 0);
  new LFl_Radio_Button("Export Problem", 0, 1);
  new LFl_Radio_Button("Export Assembly", 0, 2);
  new LFl_Radio_Button("Export Solution", 0, 3);

  (new LFl_Box(0, 4))->weight(0, 1);

  fr->end();

  new LBlockListGroup(0, 5, 2, 1, new PieceSelector(0, 0, 20, 20, puzzle));

  {
    layouter_c * l = new layouter_c(0, 6, 3, 1);

    LFl_Button *b;

    status = new LFl_Box();
    status->weight(1, 0);
    status->label("Test");
    status->pitch(7);
    status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    b = new LFl_Button("Export Image(s)", 1, 0);
    b->pitch(7);
    b->callback(cb_ImageExportExport_stub, this);
    b->box(FL_THIN_UP_BOX);

    b = new LFl_Button("Abort", 2, 0);
    b->pitch(7);
    b->callback(cb_ImageExportAbort_stub, this);
    b->box(FL_THIN_UP_BOX);

    l->end();
  }

  view3D = new LView3dGroup(2, 0, 1, 6);
//  view3D->showSingleShape(p, 0, false);
  view3D->showAssembly(p, 0, 0, false);

}

