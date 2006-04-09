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
#include "WindowWidgets.h"
#include "pieceColor.h"
#include "Images.h"
#include "guigridtype.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

#include <FL/Fl_Pixmap.H>
#include <FL/Fl_Multiline_Output.H>

#include <math.h>

// some tool widgets, that may be swapped out later into another file

static void cb_ToggleButton_stub(Fl_Widget* o, void* v) { ((ToggleButton*)o)->toggle(); }

ToggleButton::ToggleButton(int x, int y, int w, int h, Fl_Callback* cb, void * cb_para, long p) : Fl_Button(x, y, w, h) {
  box(FL_THIN_UP_BOX);
  callback = cb;
  callback_para = cb_para;
  para = p;
  Fl_Button::callback(cb_ToggleButton_stub);
  selection_color(fl_lighter(color()));
  clear_visible_focus();
}

// draws an definable evenly spaced number of lines in one direction
class LineSpacer : Fl_Widget {

  int lines;
  bool vertical;
  int gap;

  public:

    LineSpacer(int x, int y, int w, int h, int borderSpace) : Fl_Widget(x, y, w, h), lines(2), vertical(true), gap(borderSpace) {}

    void draw(void) {

      fl_color(color());
      fl_rectf(x(), y(), w(), h());

      if (lines <= 1) return;

      fl_color(0);

      if (vertical) {

        for (int i = 0; i < lines; i++) {
          int ypos = y()+ gap + (h()-2*gap-1)*i/(lines-1);
          fl_line(x(), ypos, x()+w()-1, ypos);
        }

      } else {

        for (int i = 0; i < lines; i++) {
          int xpos = x()+ gap + (w()-2*gap-1)*i/(lines-1);
          fl_line(y(), xpos, y()+w()-1, xpos);
        }
      }

    }

    void setLines(int l, int vert) {
      lines = l;
      vertical = vert;
      redraw();
    }

};

static void cb_VoxelEditGroupZselect_stub(Fl_Widget* o, void* v) { ((VoxelEditGroup*)v)->cb_Zselect((Fl_Slider*)o); }
static void cb_VoxelEditGroupSqedit_stub(Fl_Widget* o, void* v) { ((VoxelEditGroup*)v)->cb_Sqedit((gridEditor_c*)o); }

VoxelEditGroup::VoxelEditGroup(int x, int y, int w, int h, puzzle_c * puzzle, const guiGridType_c * ggt) : Fl_Group(x, y, w, h) {

  zselect = new Fl_Slider(x, y, 15, h);
  zselect->tooltip(" Select Z Plane ");
  zselect->box(FL_THIN_DOWN_BOX);
  zselect->color((Fl_Color)237);
  zselect->step(1);
  zselect->callback(cb_VoxelEditGroupZselect_stub, this);
  zselect->clear_visible_focus();

  space = new LineSpacer(x+15, y, 5, h, 4);

  {
    Fl_Box* o = new Fl_Box(x+25, y, 5, h-5);
    o->box(FL_FLAT_BOX);
    o->color(fl_rgb_color(0, 192, 0));
  }
  {
    Fl_Box* o = new Fl_Box(x+25, y+h-5, w-25, 5);
    o->box(FL_FLAT_BOX);
    o->color((Fl_Color)1);
  }

  sqedit = ggt->getGridEditor(x+35, y, w-35, h-10, puzzle);
  sqedit->tooltip(" Fill and empty cubes ");
  sqedit->box(FL_NO_BOX);
  sqedit->callback(cb_VoxelEditGroupSqedit_stub, this);
  sqedit->clear_visible_focus();

  resizable(sqedit);
}

void VoxelEditGroup::newGridType(const guiGridType_c * ggt, puzzle_c * puzzle) {

  gridEditor_c * nsq;

  nsq = ggt->getGridEditor(sqedit->x(), sqedit->y(), sqedit->w(), sqedit->h(), puzzle);
  nsq->tooltip(" Fill and empty cubes ");
  nsq->box(FL_NO_BOX);
  nsq->callback(cb_VoxelEditGroupSqedit_stub, this);
  nsq->clear_visible_focus();

  resizable(nsq);

  remove(sqedit);
  delete sqedit;

  sqedit = nsq;
  add(sqedit);
}


void VoxelEditGroup::setZ(unsigned int val) {
  if (val > zselect->maximum()) val = (unsigned int)zselect->maximum();
  zselect->value(int(zselect->maximum()-val));
  sqedit->setZ(val);
}

void VoxelEditGroup::setPuzzle(puzzle_c * puzzle, unsigned int num) {
  sqedit->setPuzzle(puzzle, num);
  if (puzzle && (num < puzzle->shapeNumber())) {
    voxel_c * v = puzzle->getShape(num);
    if (v) {
      zselect->bounds(0, v->getZ()-1);
      zselect->value(int(zselect->maximum()-sqedit->getZ()));
      space->setLines(v->getZ(), true);
    }
  }
}

void VoxelEditGroup::draw() {
  fl_push_clip(x(), y(), w(), h());
  Fl_Group::draw();
  fl_pop_clip();
}

#define SZ_BUTTON_Y 20
#define SZ_BUTTON2_Y 25
#define LABEL_FONT_SIZE 12

static void cb_TransformButtons_stub(Fl_Widget* o, long v) { ((TransformButtons*)(o->parent()))->cb_Press(v); }

TransformButtons::TransformButtons(int x, int y, int w, int h) : Fl_Group(x, y, w, h, "Transform") {

  (new Fl_Box( 80+x, y+5, 70, SZ_BUTTON_Y, "Nudge"))->labelsize(LABEL_FONT_SIZE);
  (new Fl_Box(155+x, y+5, 70, SZ_BUTTON_Y, "Rotate"))->labelsize(LABEL_FONT_SIZE);
  (new Fl_Box(  5+x, y+5, 70, SZ_BUTTON_Y, "Flip"))->labelsize(LABEL_FONT_SIZE);

  y += SZ_BUTTON_Y;

  new FlatButton( 80+x,  5+y, 35, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Nudge_X_Left_xpm)  , new Fl_Pixmap(Transform_Disabled_Nudge_X_Left_xpm)  ,
      " Shift down along X ",                cb_TransformButtons_stub,  1);
  new FlatButton(115+x,  5+y, 35, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Nudge_X_Right_xpm) , new Fl_Pixmap(Transform_Disabled_Nudge_X_Right_xpm) ,
      " Shift up along X ",                  cb_TransformButtons_stub,  0);
  new FlatButton( 80+x, 35+y, 35, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Nudge_Y_Left_xpm)  , new Fl_Pixmap(Transform_Disabled_Nudge_Y_Left_xpm)  ,
      " Shift down along Y ",                cb_TransformButtons_stub,  3);
  new FlatButton(115+x, 35+y, 35, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Nudge_Y_Right_xpm) , new Fl_Pixmap(Transform_Disabled_Nudge_Y_Right_xpm) ,
      " Shift up along Y ",                  cb_TransformButtons_stub,  2);
  new FlatButton( 80+x, 65+y, 35, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Nudge_Z_Left_xpm)  , new Fl_Pixmap(Transform_Disabled_Nudge_Z_Left_xpm)  ,
      " Shift down along Z ",                cb_TransformButtons_stub,  5);
  new FlatButton(115+x, 65+y, 35, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Nudge_Z_Right_xpm) , new Fl_Pixmap(Transform_Disabled_Nudge_Z_Right_xpm) ,
      " Shift up along Z ",                  cb_TransformButtons_stub,  4);
  new FlatButton(155+x,  5+y, 35, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Rotate_X_Left_xpm) , new Fl_Pixmap(Transform_Disabled_Rotate_X_Left_xpm) ,
      " Rotate clockwise along X-Axis ",     cb_TransformButtons_stub,  6);
  new FlatButton(190+x,  5+y, 35, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Rotate_X_Right_xpm), new Fl_Pixmap(Transform_Disabled_Rotate_X_Right_xpm),
      " Rotate anticlockwise along X-Axis ", cb_TransformButtons_stub,  7);
  new FlatButton(155+x, 35+y, 35, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Rotate_Y_Left_xpm) , new Fl_Pixmap(Transform_Disabled_Rotate_Y_Left_xpm) ,
      " Rotate clockwise along Y-Axis ",     cb_TransformButtons_stub,  9);
  new FlatButton(190+x, 35+y, 35, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Rotate_Y_Right_xpm), new Fl_Pixmap(Transform_Disabled_Rotate_Y_Right_xpm),
      " Rotate anticlockwise along Y-Axis ", cb_TransformButtons_stub,  8);
  new FlatButton(155+x, 65+y, 35, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Rotate_Z_Left_xpm) , new Fl_Pixmap(Transform_Disabled_Rotate_Z_Left_xpm) ,
      " Rotate clockwise along Z-Axis ",     cb_TransformButtons_stub, 10);
  new FlatButton(190+x, 65+y, 35, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Rotate_Z_Right_xpm), new Fl_Pixmap(Transform_Disabled_Rotate_Z_Right_xpm),
      " Rotate anticlockwise along Z-Axis ", cb_TransformButtons_stub, 11);
  new FlatButton(  5+x,  5+y, 70, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Flip_X_xpm)        , new Fl_Pixmap(Transform_Disabled_Flip_X_xpm)        ,
      " Flip along Y-Z Plane ",              cb_TransformButtons_stub, 12);
  new FlatButton(  5+x, 35+y, 70, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Flip_Y_xpm)        , new Fl_Pixmap(Transform_Disabled_Flip_Y_xpm)        ,
      " Flip along X-Z Plane ",              cb_TransformButtons_stub, 13);
  new FlatButton(  5+x, 65+y, 70, SZ_BUTTON2_Y,
      new Fl_Pixmap(Transform_Color_Flip_Z_xpm)        , new Fl_Pixmap(Transform_Disabled_Flip_Z_xpm)        ,
      " Flip along X-Y Plane ",              cb_TransformButtons_stub, 14);
}

static void cb_ChangeSize_stub(Fl_Widget* o, long v) { ((ChangeSize*)(o->parent()))->cb_roll(v); }
static void cb_InputSize_stub(Fl_Widget* o, long v) { ((ChangeSize*)(o->parent()))->cb_input(v); }

void ChangeSize::cb_roll(long dir) {

  int ox, oy, oz, nx, ny, nz;

  ox = atoi(SizeOutX->value());
  nx = (int)SizeX->value();
  oy = atoi(SizeOutY->value());
  ny = (int)SizeY->value();
  oz = atoi(SizeOutZ->value());
  nz = (int)SizeZ->value();

  calcNewSizes(ox, oy, oz, &nx, &ny, &nz);

  char num[20];

  snprintf(num, 20, "%i", nx); SizeOutX->value(num);
  snprintf(num, 20, "%i", ny); SizeOutY->value(num);
  snprintf(num, 20, "%i", nz); SizeOutZ->value(num);
  SizeX->value(nx);
  SizeY->value(ny);
  SizeZ->value(nz);

  do_callback();
}

void ChangeSize::cb_input(long dir) {
  int ox, oy, oz, nx, ny, nz;

  ox = (int)SizeX->value();
  nx = atoi(SizeOutX->value());
  oy = (int)SizeY->value();
  ny = atoi(SizeOutY->value());
  oz = (int)SizeZ->value();
  nz = atoi(SizeOutZ->value());

  calcNewSizes(ox, oy, oz, &nx, &ny, &nz);

  char num[20];

  snprintf(num, 20, "%i", nx); SizeOutX->value(num);
  snprintf(num, 20, "%i", ny); SizeOutY->value(num);
  snprintf(num, 20, "%i", nz); SizeOutZ->value(num);
  SizeX->value(nx);
  SizeY->value(ny);
  SizeZ->value(nz);

  // seems like the Rollers don't callback, when value is set
  do_callback();
}

void ChangeSize::calcNewSizes(int ox, int oy, int oz, int *nx, int *ny, int *nz) {
  int dx = *nx - ox;
  int dy = *ny - oy;
  int dz = *nz - oz;

  if (dx != 0 && ConnectX->value()) {
    if (ConnectY->value()) dy = dx;
    if (ConnectZ->value()) dz = dx;
  }

  if (dy != 0 && ConnectY->value()) {
    if (ConnectX->value()) dx = dy;
    if (ConnectZ->value()) dz = dy;
  }

  if (dz != 0 && ConnectZ->value()) {
    if (ConnectX->value()) dx = dz;
    if (ConnectY->value()) dy = dz;
  }

  *nx = ox + dx;
  *ny = oy + dy;
  *nz = oz + dz;

  if (*nx < 1) *nx = 1;
  if (*ny < 1) *ny = 1;
  if (*nz < 1) *nz = 1;

  if (*nx > 1000) *nx = 1000;
  if (*ny > 1000) *ny = 1000;
  if (*nz > 1000) *nz = 1000;
}

ChangeSize::ChangeSize(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {

  tooltip(" Change size of space ");

  SizeX = new Fl_Roller(70+x, 10+y, w-90, 20);
  SizeX->type(1);
  SizeX->box(FL_THIN_DOWN_BOX);
  SizeX->minimum(1);
  SizeX->maximum(1000);
  SizeX->step(0.25);
  SizeX->callback(cb_ChangeSize_stub, 0l);
  SizeX->clear_visible_focus();

  SizeY = new Fl_Roller(70+x, 35+y, w-90, 20);
  SizeY->type(1);
  SizeY->box(FL_THIN_DOWN_BOX);
  SizeY->minimum(1);
  SizeY->maximum(1000);
  SizeY->step(0.25);
  SizeY->callback(cb_ChangeSize_stub, 1l);
  SizeY->clear_visible_focus();

  SizeZ = new Fl_Roller(70+x, 60+y, w-90, 20);
  SizeZ->type(1);
  SizeZ->box(FL_THIN_DOWN_BOX);
  SizeZ->minimum(1);
  SizeZ->maximum(1000);
  SizeZ->step(0.25);
  SizeZ->callback(cb_ChangeSize_stub, 2l);
  SizeZ->clear_visible_focus();

  SizeOutX = new Fl_Int_Input(20+x, 10+y, 45, 20, "X");
  SizeOutX->box(FL_THIN_DOWN_BOX);
  SizeOutX->callback(cb_InputSize_stub, 0l);
  SizeOutX->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);

  SizeOutY = new Fl_Int_Input(20+x, 35+y, 45, 20, "Y");
  SizeOutY->box(FL_THIN_DOWN_BOX);
  SizeOutY->callback(cb_InputSize_stub, 1l);
  SizeOutY->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);

  SizeOutZ = new Fl_Int_Input(20+x, 60+y, 45, 20, "Z");
  SizeOutZ->box(FL_THIN_DOWN_BOX);
  SizeOutZ->callback(cb_InputSize_stub, 2l);
  SizeOutZ->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);

  ConnectX = new Fl_Check_Button(x+w-20, 10+y, 20, 20);
  ConnectY = new Fl_Check_Button(x+w-20, 35+y, 20, 20);
  ConnectZ = new Fl_Check_Button(x+w-20, 60+y, 20, 20);

  ConnectX->tooltip(" Link the sizes together so that a change will be done to the other sizes as well ");
  ConnectY->tooltip(" Link the sizes together so that a change will be done to the other sizes as well ");
  ConnectZ->tooltip(" Link the sizes together so that a change will be done to the other sizes as well ");

  ConnectX->clear_visible_focus();
  ConnectY->clear_visible_focus();
  ConnectZ->clear_visible_focus();

  end();
}

void ChangeSize::setXYZ(long x, long y, long z) {
  SizeX->value(x);
  SizeY->value(y);
  SizeZ->value(z);

  char num[20];

  snprintf(num, 20, "%li", x); SizeOutX->value(num);
  snprintf(num, 20, "%li", y); SizeOutY->value(num);
  snprintf(num, 20, "%li", z); SizeOutZ->value(num);
}

void ToolTab::setVoxelSpace(puzzle_c * puz, unsigned int sh) {
  puzzle = puz;
  shape = sh;
  if (puzzle && shape < puzzle->shapeNumber())
    changeSize->setXYZ(puzzle->getShape(shape)->getX(),
        puzzle->getShape(shape)->getY(),
        puzzle->getShape(shape)->getZ());
  else
    changeSize->setXYZ(0, 0, 0);
}

static void cb_ToolTabSize_stub(Fl_Widget* o, long v) { ((ToolTab*)(o->parent()->parent()))->cb_size(); }
static void cb_ToolTabTransform_stub(Fl_Widget* o, long v) { ((ToolTab*)(o->parent()))->cb_transform(v); }
static void cb_ToolTabTransform2_stub(Fl_Widget* o, long v) { ((ToolTab*)(o->parent()->parent()))->cb_transform(v); }

ToolTab::ToolTab(int x, int y, int w, int h) : Fl_Tabs(x, y, w, h) {

  {
    Fl_Group * o = new Fl_Group(x, y+20, w, h-20, "Size");

    changeSize = new ChangeSize(x, y+20+20, w-90, h-40);
    changeSize->callback(cb_ToolTabSize_stub);

    (new Fl_Box(x+w-80, y+25, 35, SZ_BUTTON_Y, "Grid"))->labelsize(LABEL_FONT_SIZE);

    new FlatButton(x+w-80, y+ 45, 35, 25, new Fl_Pixmap(Grid_Color_Minimize_xpm), new Fl_Pixmap(Grid_Disabled_Minimize_xpm), "Minimize size of grid", cb_ToolTabTransform2_stub, 15);
    new FlatButton(x+w-80, y+ 75, 35, 25, new Fl_Pixmap(Grid_Color_Center_xpm), new Fl_Pixmap(Grid_Disabled_Center_xpm), "Center shape inside the grid", cb_ToolTabTransform2_stub, 25);
    new FlatButton(x+w-80, y+105, 35, 25, new Fl_Pixmap(Grid_Color_Origin_xpm), new Fl_Pixmap(Grid_Disabled_Origin_xpm), "Move shape to origin of grid", cb_ToolTabTransform2_stub, 24);

    (new Fl_Box(x+w-40, y+25, 35, SZ_BUTTON_Y, "Shape"))->labelsize(LABEL_FONT_SIZE);

    new FlatButton(x+w-40, y+ 45, 35, 25, new Fl_Pixmap(Rescale_Color_X1_xpm), new Fl_Pixmap(Rescale_Disabled_X1_xpm), "Try to minimize size of shape", cb_ToolTabTransform2_stub, 26);
    new FlatButton(x+w-40, y+ 75, 35, 25, new Fl_Pixmap(Rescale_Color_X2_xpm), new Fl_Pixmap(Rescale_Disabled_X2_xpm), "Double size of shape", cb_ToolTabTransform2_stub, 22);
    new FlatButton(x+w-40, y+105, 35, 25, new Fl_Pixmap(Rescale_Color_X3_xpm), new Fl_Pixmap(Rescale_Disabled_X3_xpm), "Triple size of shape", cb_ToolTabTransform2_stub, 23);

    toAll = new Fl_Check_Button(x+15, y+25, w-85, 20, "Apply to all Shapes");
    toAll->tooltip(" If this is active, all operations (including transformations and constrains are done to all shapes ");
    toAll->clear_visible_focus();

    o->end();
  }
  {
    Fl_Group* o = new TransformButtons(x, y+20, w, h-20);
    o->callback(cb_ToolTabTransform_stub);
    o->hide();
    o->end();
  }
  {
    Fl_Group* o = new Fl_Group(x, y+20, w, h-20, "Tools");
    o->hide();

    (new Fl_Box(x+5, y+25, 70, SZ_BUTTON_Y, "Constrain"))->labelsize(LABEL_FONT_SIZE);


    new FlatButton(x+5, y+ 45, 35, 25, new Fl_Pixmap(InOut_Color_Fixed_In_xpm), new Fl_Pixmap(InOut_Disabled_Fixed_In_xpm), "Make inside fixed", cb_ToolTabTransform2_stub, 16);
    new FlatButton(x+5, y+ 75, 35, 25, new Fl_Pixmap(InOut_Color_Variable_In_xpm), new Fl_Pixmap(InOut_Disabled_Variable_In_xpm), "Make inside variable", cb_ToolTabTransform2_stub, 18);
    new FlatButton(x+5, y+105, 35, 25, new Fl_Pixmap(InOut_Color_RemoveColor_In_xpm), new Fl_Pixmap(InOut_Disabled_RemoveColor_In_xpm), "Remove Colours from inside cubes", cb_ToolTabTransform2_stub, 20);


    new FlatButton(x+40, y+ 45, 35, 25, new Fl_Pixmap(InOut_Color_Fixed_Out_xpm), new Fl_Pixmap(InOut_Disabled_Fixed_Out_xpm), "Make outside fixed", cb_ToolTabTransform2_stub, 17);
    new FlatButton(x+40, y+ 75, 35, 25, new Fl_Pixmap(InOut_Color_Variable_Out_xpm), new Fl_Pixmap(InOut_Disabled_Variable_Out_xpm), "Make outside variable", cb_ToolTabTransform2_stub, 19);
    new FlatButton(x+40, y+105, 35, 25, new Fl_Pixmap(InOut_Color_RemoveColor_Out_xpm), new Fl_Pixmap(InOut_Disabled_RemoveColor_Out_xpm), "RemoveColours from outside cubes", cb_ToolTabTransform2_stub, 21);
    o->end();
  }
}

void ToolTab::cb_size(void) {
  if (puzzle && shape < puzzle->shapeNumber()) {

    if (toAll->value()) {

      int dx, dy, dz;

      dx = changeSize->getX() - puzzle->getShape(shape)->getX();
      dy = changeSize->getY() - puzzle->getShape(shape)->getY();
      dz = changeSize->getZ() - puzzle->getShape(shape)->getZ();

      if (dx < 0) dx = 0;
      if (dy < 0) dy = 0;
      if (dz < 0) dz = 0;

      for (unsigned int s = 0; s < puzzle->shapeNumber(); s++) {
        int nx = puzzle->getShape(s)->getX()+dx;
        int ny = puzzle->getShape(s)->getY()+dy;
        int nz = puzzle->getShape(s)->getZ()+dz;

        if (nx < 1) nx = 1;
        if (ny < 1) ny = 1;
        if (nz < 1) nz = 1;

        puzzle->getShape(s)->resize(nx, ny, nz, 0);
      }

    }

    // we always do this, is may be that the shape is not changed in the loop above because
    // that loop and only inclrease the size, so smaller sizes for the selected shape must be done
    // here
    puzzle->getShape(shape)->resize(changeSize->getX(), changeSize->getY(), changeSize->getZ(), 0);

    do_callback();
  }
}

void ToolTab::cb_transform(long task) {
  if (puzzle && shape < puzzle->shapeNumber()) {

    int ss, se;

    if (toAll->value() && ((task == 15) || ((task >= 22) && (task <= 26)))) {
      ss = 0;
      se = puzzle->shapeNumber();
    } else {
      ss = shape;
      se = shape+1;
    }

    if (task == 26) {

      unsigned char primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 0};

      // special case for minimisation

      int prime = 0;

      while (primes[prime]) {

        bool canScale = true;

        for (int s = ss; s < se; s++)
          if (!puzzle->getShape(s)->scaleDown(primes[prime], false)) {
            canScale = false;
            break;
          }

        if (canScale) {
          for (int s = ss; s < se; s++)
            puzzle->getShape(s)->scaleDown(primes[prime], true);
        } else
          prime++;
      }

      for (int s = ss; s < se; s++)
        puzzle->getShape(s)->setHotspot(0, 0, 0);

      do_callback(this, user_data());

      return;
    }

    for (int s = ss; s < se; s++) {
      voxel_c * space = puzzle->getShape(s);

      switch(task) {
        case  0: space->translate( 1, 0, 0, 0); break;
        case  1: space->translate(-1, 0, 0, 0); break;
        case  2: space->translate( 0, 1, 0, 0); break;
        case  3: space->translate( 0,-1, 0, 0); break;
        case  4: space->translate( 0, 0, 1, 0); break;
        case  5: space->translate( 0, 0,-1, 0); break;
        case  7: space->rotatex(); space->rotatex(); // fallthrough
        case  6: space->rotatex(); break;
        case  9: space->rotatey(); space->rotatey(); // fallthrough
        case  8: space->rotatey(); break;
        case 11: space->rotatez(); space->rotatez(); // fallthrough
        case 10: space->rotatez(); break;
        case 12: space->mirrorX(); break;
        case 13: space->mirrorY(); break;
        case 14: space->mirrorZ(); break;
        case 15: space->minimizePiece(); break;
        case 16: space->actionOnSpace(voxel_c::ACT_FIXED, true); break;
        case 17: space->actionOnSpace(voxel_c::ACT_FIXED, false); break;
        case 18: space->actionOnSpace(voxel_c::ACT_VARIABLE, true); break;
        case 19: space->actionOnSpace(voxel_c::ACT_VARIABLE, false); break;
        case 20: space->actionOnSpace(voxel_c::ACT_DECOLOR, true); break;
        case 21: space->actionOnSpace(voxel_c::ACT_DECOLOR, false); break;
        case 22: space->scale(2); break;
        case 23: space->scale(3); break;
        case 24: space->translate(- space->boundX1(), - space->boundY1(), - space->boundZ1(), 0); break;
        case 25:
                 {
		   // if the space is empty, don't do anything
		   if (space->boundX2() < space->boundX1())
		     break;

                   int fx = space->getX() - (space->boundX2()-space->boundX1()+1);
                   int fy = space->getY() - (space->boundY2()-space->boundY1()+1);
                   int fz = space->getZ() - (space->boundZ2()-space->boundZ1()+1);

                   if ((fx & 1) || (fy & 1) || (fz & 1)) {
                     space->resize(space->getX()+(fx&1), space->getY()+(fy&1), space->getZ()+(fz&1), 0);
                     fx += fx&1;
                     fy += fy&1;
                     fz += fz&1;
                   }
                   space->translate(fx/2 - space->boundX1(), fy/2 - space->boundY1(), fz/2 - space->boundZ1(), 0);
                 }
                 break;
      }
      space->setHotspot(0, 0, 0);
    }

    do_callback(this, user_data());
  }
}

static void cb_BlockListGroupSlider_stub(Fl_Widget* o, void* v) { ((BlockListGroup*)(o->parent()))->cb_slider(); }
void BlockListGroup::cb_list(void) {

  if (List->getReason() == PieceSelector::RS_CHANGEDHIGHT) {

    Slider->range(0, List->calcHeight());
    if (Slider->value() > List->calcHeight())
      Slider->value(List->calcHeight());

    List->setShift((int)Slider->value());

  } else {
    callbackReason = List->getReason();
    do_callback(this, user_data());
  }
}

static void cb_BlockListGroupList_stub(Fl_Widget* o, void* v) { ((BlockListGroup*)(o->parent()))->cb_list(); }

BlockListGroup::BlockListGroup(int x, int y, int w, int h, BlockList * l) : Fl_Group(x, y, w, h), List(l) {

  box(FL_THIN_DOWN_FRAME);
  x++; y++; w-=2; h-=2;

  // important the list must be added first to be drawn first
  // this is necessary to find out the size of the list and update the
  // slider accordingly
  add(List);

  Slider = new Fl_Slider(x+w-15-1, y-1, 15+2, h+2);
  Slider->box(FL_THIN_DOWN_BOX);
  Slider->maximum(0);
  Slider->callback(cb_BlockListGroupSlider_stub);
  Slider->clear_visible_focus();

  w-=15;

  Fl_Box * frame = new Fl_Box(x, y, w, h);
  frame->box(FL_THIN_UP_FRAME);
  x++; y++; w-=2; h-=2;

  List->resize(x, y, w, h);
  List->callback(cb_BlockListGroupList_stub);

  resizable(List);
  end();
}

static void cb_ConstraintsGroupSlider_stub(Fl_Widget* o, void* v) { ((ConstraintsGroup*)(o->parent()))->cb_slider(); }
static void cb_ConstraintsGroupList_stub(Fl_Widget* o, void* v) { ((ConstraintsGroup*)(o->parent()))->cb_list(); }
void ConstraintsGroup::cb_list(void) {

  if (List->getReason() == ColorConstraintsEdit::RS_CHANGEDHIGHT) {

    Slider->range(0, List->calcHeight());
    if (Slider->value() > List->calcHeight())
      Slider->value(List->calcHeight());

    List->setShift((int)Slider->value());

  } else {
    callbackReason = List->getReason();
    do_callback(this, user_data());
  }
}

ConstraintsGroup::ConstraintsGroup(int x, int y, int w, int h, ColorConstraintsEdit * l) : Fl_Group(x, y, w, h), List(l) {

  box(FL_THIN_DOWN_FRAME);
  x++; y++; w-=2; h-=2;

  add(List);

  Slider = new Fl_Slider(x+w-15-1, y-1, 15+2, h+2);
  Slider->box(FL_THIN_DOWN_BOX);
  Slider->maximum(0);
  Slider->callback(cb_ConstraintsGroupSlider_stub);
  Slider->clear_visible_focus();

  w-=15;

  Fl_Box * frame = new Fl_Box(x, y, w, h);
  frame->box(FL_THIN_UP_FRAME);
  x++; y++; w-=2; h-=2;

  List->resize(x, y, w, h);
  List->callback(cb_ConstraintsGroupList_stub);

  resizable(List);
  end();
}

static void cb_View3dGroupSlider_stub(Fl_Widget* o, void* v) { ((View3dGroup*)(o->parent()))->cb_slider(); }

void View3dGroup::cb_slider(void) {
  View3D->setSize(exp(6-slider->value()));
}

View3dGroup::View3dGroup(int x, int y, int w, int h, const guiGridType_c * ggt) : Fl_Group(x, y, w, h) {
  box(FL_DOWN_BOX);

  View3D = ggt->getVoxelDrawer(x, y, w-15, h);
  View3D->tooltip(" Rotate the puzzle by dragging with the mouse ");
  View3D->box(FL_NO_BOX);

  slider = new Fl_Slider(x+w-15, y, 15, h);
  slider->tooltip("Zoom view.");
  slider->box(FL_THIN_DOWN_BOX);
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

void View3dGroup::newGridType(const guiGridType_c * ggt) {

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

Separator::Separator(int x, int y, int w, int h, const char * label, bool button) : Fl_Group(x, y, w, h) {

  if (label) {
    int lw, lh;

    fl_font(labelfont(), labelsize()-4);

    lw = lh = 0;
    fl_measure(label, lw, lh);
    (new Fl_Box(FL_FLAT_BOX, x, y, lw+4, h, label))->labelsize(labelsize()-4);

    x += lw + 6;
    w -= lw + 6;
  }

  if (button) {
    new Fl_Box(FL_THIN_UP_BOX, x+w-8, y+h/2-4, 8, 8, 0);
    w -= 8;
  }

  resizable(new Fl_Box(FL_THIN_DOWN_BOX, x, y+h/2-1, w, 2, 0));

  end();
}

ResultViewer::ResultViewer(int x, int y, int w, int h, puzzle_c * p) : Fl_Box(x, y, w, h), puzzle(p), problem(0) {
  bt_assert(p);
  bg = color();
//  setcontent();
  box(FL_BORDER_BOX);
  clear_visible_focus();
}

void ResultViewer::setPuzzle(puzzle_c * p, unsigned int prob) {
  puzzle = p;
  problem = prob;
  redraw();
//  setcontent();
}

void ResultViewer::draw(void) {
  if (problem >= puzzle->problemNumber() ||
      (puzzle->probGetResult(problem) > puzzle->shapeNumber())) {
    label("No Result");
    color(bg);
    labelcolor(fl_rgb_color(255, 0, 0));
  } else {
    static char txt[120];

    unsigned int result = puzzle->probGetResult(problem);

    if (puzzle->probGetResultShape(problem)->getName().length())
      snprintf(txt, 120, "Result: S%i - %s", result+1, puzzle->probGetResultShape(problem)->getName().c_str());
    else
      snprintf(txt, 19, "Result: S%i", result + 1);

    unsigned char r, g, b;

    r = pieceColorRi(result);
    g = pieceColorGi(result);
    b = pieceColorBi(result);

    color(fl_rgb_color(r, g, b));

    if ((int)3*r + 6*g + 1*b > 1275)
      labelcolor(fl_rgb_color(0, 0, 0));
    else
      labelcolor(fl_rgb_color(255, 255, 255));

    label(txt);
  }
  Fl_Box::draw();
}

void View3dGroup::showSingleShape(const puzzle_c * puz, unsigned int shapeNum) {
  View3D->update(false);
  View3D->showSingleShape(puz, shapeNum);
  View3D->update(true);
}

void View3dGroup::showProblem(const puzzle_c * puz, unsigned int probNum, unsigned int selShape) {

  View3D->update(false);
  View3D->showProblem(puz, probNum, selShape);
  View3D->update(true);
}

void View3dGroup::showColors(const puzzle_c * puz, bool show) {
  View3D->update(false);
  View3D->showColors(puz, show);
  View3D->update(true);
}

void View3dGroup::showAssembly(const puzzle_c * puz, unsigned int probNum, unsigned int solNum) {
  View3D->update(false);
  View3D->showAssembly(puz, probNum, solNum);
  View3D->update(true);
}

void View3dGroup::showPlacement(const puzzle_c * puz, unsigned int probNum, unsigned int piece, unsigned char trans, int x, int y, int z) {

  View3D->update(false);
  View3D->showPlacement(puz, probNum, piece, trans, x, y, z);
  View3D->update(true);
}

void View3dGroup::updatePositions(PiecePositions *shifting) {
  View3D->update(false);
  View3D->updatePositions(shifting);
  View3D->update(true);
}

void View3dGroup::updateVisibility(PieceVisibility * pcvis) {
  View3D->update(false);
  View3D->updateVisibility(pcvis);
  View3D->update(true);
}

static void cb_ButtonGroup_stub(Fl_Widget* o, void* v) { ((ButtonGroup*)v)->cb_Push((Fl_Button*)o); }

ButtonGroup::ButtonGroup(int x, int y, int w, int h) : Fl_Group(x, y, w, h), currentButton(0) {
  end();
}

Fl_Button * ButtonGroup::addButton(int x, int y, int w, int h) {

  int c = children();

  Fl_Button * b = new Fl_Button(x, y, w, h);
  b->box(FL_THIN_UP_BOX);
  b->selection_color(fl_lighter(color()));
  b->clear_visible_focus();

  b->callback(cb_ButtonGroup_stub, this);

  if (c == 0)
    b->set();
  else
    b->clear();

  add(b);

  return b;
}

void ButtonGroup::cb_Push(Fl_Button * btn) {

  Fl_Button ** a = (Fl_Button**) array();

  for (int i = 0; i < children(); i++)
    if (a[i] != btn) {
      a[i]->clear();
    } else {
      a[i]->set();
      currentButton = i;
    }

  do_callback();
}

void ButtonGroup::select(int num) {
  if (num < children())
    cb_Push((Fl_Button*)array()[num]);
}

StatusLine::StatusLine(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {

  text = new Fl_Box(x, y, w - 130, h);
  text->box(FL_THIN_UP_BOX);
  text->color(FL_BACKGROUND_COLOR);
  text->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

  colors = new Fl_Check_Button(x+w-130, y, 130, h, "Colour 3D View");
  colors->box(FL_THIN_UP_BOX);
  colors->tooltip(" Toggle between piece colours and the colours of the colour constraints (neutral units will have piece colour) ");

  resizable(text);

  clear_visible_focus();

  end();
}

void StatusLine::setText(const char * t) {

  text->copy_label(t);
}

#define ASSERT_WINDOW_X 500
#define ASSERT_WINDOW_Y 400

#define ASSERT_TXT1 40
#define ASSERT_TXT2 60

#define ASSERT_BTN_X 100

static void cb_assertClose_stub(Fl_Widget* o, void* v) { ((Fl_Double_Window*)v)->hide(); }

assertWindow::assertWindow(const char * text, assert_exception * a) : Fl_Double_Window(0, 0, ASSERT_WINDOW_X, ASSERT_WINDOW_Y) {

  char txt[1000];

  // first the text given
  (new Fl_Box(5, 5, ASSERT_WINDOW_X-10, ASSERT_TXT1, text))->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_LEFT);

  // now some standard text
  (new Fl_Box(5, 10+ASSERT_TXT1, ASSERT_WINDOW_X-10, ASSERT_TXT2,
             "Please send the text in the following box to roever@@users.sf.net\n"
             "if possible include a file of the puzzle that failed and a description\n"
             "of what you did until this error occured,   thank you."
            ))->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_LEFT);

  // finally the text box
  int pos = snprintf(txt, 1000,
                     "Assert failed in\n"
                     "file: %s\n"
                     "function: %s\n"
                     "line: %i\n"
                     "condition: %s\n",
                     a->file, a->function, a->line, a->expr);

  // append the log
  if (assert_log->lines()) {

    pos += snprintf(txt+pos, 1000-pos, "log:\n");

    for (unsigned int l = 0; l < assert_log->lines(); l++)
      pos += snprintf(txt+pos, 1000-pos, "%s\n", assert_log->line(l));
  }

  (new Fl_Multiline_Output(5, 15+ASSERT_TXT1+ASSERT_TXT2, ASSERT_WINDOW_X-10, ASSERT_WINDOW_Y - 25 - 20 - ASSERT_TXT1 - ASSERT_TXT2))->value(txt);

  (new Fl_Button((ASSERT_WINDOW_X-ASSERT_BTN_X)/2, ASSERT_WINDOW_Y-25, ASSERT_BTN_X, 20, "Close"))->callback(cb_assertClose_stub, this);

  label("Error");
}

static void cb_mlWindowClose(Fl_Widget* o, void * v) { ((multiLineWindow*)v)->hide(true); }
static void cb_mlWindowAbort(Fl_Widget* o, void * v) { ((multiLineWindow*)v)->hide(false); }

#define SZ_MLWIN_X 400
#define SZ_MLWIN_Y 200
#define SZ_BUTTON_X2 50
#define SZ_GAP 5
#define SZ_BUTTON 20

multiLineWindow::multiLineWindow(const char * tit, const char *lab, const char *deflt) : Fl_Double_Window(SZ_MLWIN_X, SZ_MLWIN_Y) {

  label(tit);

  int w, h;
  fl_measure(lab, w, h);

  new Fl_Box(0, SZ_GAP, SZ_MLWIN_X, h, lab);

  inp = new Fl_Multiline_Input(0, h+2*SZ_GAP, SZ_MLWIN_X, SZ_MLWIN_Y-h-4*SZ_GAP-SZ_BUTTON);
  inp->value(deflt);

  new FlatButton( (SZ_MLWIN_X/2)-SZ_BUTTON_X2-SZ_BUTTON_X2-2, SZ_MLWIN_Y-SZ_BUTTON-SZ_GAP, 2*SZ_BUTTON_X2, SZ_BUTTON,
      "Finished", "Close and save changes", cb_mlWindowClose, this);

  new FlatButton( (SZ_MLWIN_X/2)-SZ_BUTTON_X2+SZ_BUTTON_X2+3, SZ_MLWIN_Y-SZ_BUTTON-SZ_GAP, 2*SZ_BUTTON_X2, SZ_BUTTON,
      "Abort", "Close and drop changes", cb_mlWindowAbort, this);

  resizable(inp);

  _saveChanges = false;
}

void ProgressBar::draw(void) {
  int	progress;	// Size of progress bar...
  int	bx, by, bw, bh;	// Box areas...
  int	tx, tw;		// Temporary X + width


  // Get the box borders...
  bx = Fl::box_dx(box());
  by = Fl::box_dy(box());
  bw = Fl::box_dw(box());
  bh = Fl::box_dh(box());

  tx = x() + bx;
  tw = w() - bw;

  // Draw the progress bar...
  if (maximum() > minimum())
    progress = (int)(tw * (value() - minimum()) / (maximum() - minimum()) + 0.5f);
  else
    progress = 0;

  // Draw the box...

  if (progress > 0)
  {
    fl_clip(x(), y(), progress + bx, h());
    draw_box(box(), x(), y(), w(), h(), active_r() ? color2() : fl_inactive(color2()));
    // Finally, the label...
    labelcolor(color());
    draw_label(tx, y() + by, tw, h() - bh);
    fl_pop_clip();

    fl_clip(tx + progress, y(), w() - progress, h());
    draw_box(box(), x(), y(), w(), h(), active_r() ? color() : fl_inactive(color()));
    // Finally, the label...
    labelcolor(color2());
    draw_label(tx, y() + by, tw, h() - bh);
    fl_pop_clip();
  }
  else {
    draw_box(box(), x(), y(), w(), h(), color());
    labelcolor(color2());
    draw_label(tx, y() + by, tw, h() - bh);
  }
}
