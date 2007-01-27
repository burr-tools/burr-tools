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

#include "tooltabs.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"
#include "../lib/voxel_0.h"
#include "WindowWidgets.h"
#include "guigridtype.h"


// the transform group
class TransformButtons : public layouter_c {

  pixmapList_c pm;

public:

  /* type 0 = bricks
   * type 1 = triangles
   * type 2 = spheres
   */
  TransformButtons(int x, int y, int w, int h, int type);

  void cb_Press(long button) { do_callback(this, button); }
};

class ToolsButtons : public layouter_c {

  pixmapList_c pm;

public:

  ToolsButtons(int x, int y, int w, int h);

  void cb_Press(long button) { do_callback(this, button); }
};

class SizeButtons : public layouter_c {

  pixmapList_c pm;

public:

  SizeButtons(int x, int y, int w, int h, bool addScale);

  void cb_Press(long button) { do_callback(this, button); }
};

// the change size group
class ChangeSize : public layouter_c {

  LFl_Roller* SizeX;
  LFl_Roller* SizeY;
  LFl_Roller* SizeZ;

  Fl_Int_Input* SizeOutX;
  Fl_Int_Input* SizeOutY;
  Fl_Int_Input* SizeOutZ;

  Fl_Check_Button * ConnectX;
  Fl_Check_Button * ConnectY;
  Fl_Check_Button * ConnectZ;

  void calcNewSizes(int ox, int oy, int oz, int *nx, int *ny, int *nz);

public:

  ChangeSize(int w, int y, int w, int h);

  void cb_roll(long dir);
  void cb_input(long dir);

  int getX(void) const { return (int)SizeX->value(); }
  int getY(void) const { return (int)SizeY->value(); }
  int getZ(void) const { return (int)SizeZ->value(); }

  void setXYZ(long x, long y, long z);
};


#define SZ_BUTTON_Y 20
#define SZ_BUTTON2_Y 25
#define LABEL_FONT_SIZE 12

static void cb_TransformButtons_stub(Fl_Widget* o, long v) { ((TransformButtons*)(o->parent()->parent()))->cb_Press(v); }

TransformButtons::TransformButtons(int x, int y, int w, int h, int type) : layouter_c(x, y, w, h) {

  label("Transform");

  (new LFl_Box(0, 0, 1, 1))->weight(1, 1);
  (new LFl_Box(0, 8, 1, 1))->weight(1, 1);
  (new LFl_Box(8, 0, 1, 1))->weight(1, 1);
  (new LFl_Box(8, 8, 1, 1))->weight(1, 1);

  (new LFl_Box("Flip",   1, 1, 1, 1))->labelsize(LABEL_FONT_SIZE);
  (new LFl_Box("Nudge",  3, 1, 2, 1))->labelsize(LABEL_FONT_SIZE);
  (new LFl_Box("Rotate", 6, 1, 2, 1))->labelsize(LABEL_FONT_SIZE);

  layouter_c * o = new layouter_c(1, 3, 1, 1);

  new LFlatButton_c(0, 0, 1, 1, pm.get(Transform_Color_Flip_X_xpm)        , pm.get(Transform_Disabled_Flip_X_xpm)        ,
      " Flip along Y-Z Plane ",              cb_TransformButtons_stub, 12);
  new LFlatButton_c(0, 1, 1, 1, pm.get(Transform_Color_Flip_Y_xpm)        , pm.get(Transform_Disabled_Flip_Y_xpm)        ,
      " Flip along X-Z Plane ",              cb_TransformButtons_stub, 13);
  new LFlatButton_c(0, 2, 1, 1, pm.get(Transform_Color_Flip_Z_xpm)        , pm.get(Transform_Disabled_Flip_Z_xpm)        ,
      " Flip along X-Y Plane ",              cb_TransformButtons_stub, 14);

  o->end();

  o = new layouter_c(3, 3, 1, 1);

  if (type == 0) {
    new LFlatButton_c(0, 0, 1, 1, pm.get(Transform_Color_Nudge_X_Left_xpm)  , pm.get(Transform_Disabled_Nudge_X_Left_xpm)  ,
        " Shift down along X ",                cb_TransformButtons_stub,  1);
    new LFlatButton_c(1, 0, 1, 1, pm.get(Transform_Color_Nudge_X_Right_xpm) , pm.get(Transform_Disabled_Nudge_X_Right_xpm) ,
        " Shift up along X ",                  cb_TransformButtons_stub,  0);
    new LFlatButton_c(0, 1, 1, 1, pm.get(Transform_Color_Nudge_Y_Left_xpm)  , pm.get(Transform_Disabled_Nudge_Y_Left_xpm)  ,
        " Shift down along Y ",                cb_TransformButtons_stub,  3);
    new LFlatButton_c(1, 1, 1, 1, pm.get(Transform_Color_Nudge_Y_Right_xpm) , pm.get(Transform_Disabled_Nudge_Y_Right_xpm) ,
        " Shift up along Y ",                  cb_TransformButtons_stub,  2);
    new LFlatButton_c(0, 2, 1, 1, pm.get(Transform_Color_Nudge_Z_Left_xpm)  , pm.get(Transform_Disabled_Nudge_Z_Left_xpm)  ,
        " Shift down along Z ",                cb_TransformButtons_stub,  5);
    new LFlatButton_c(1, 2, 1, 1, pm.get(Transform_Color_Nudge_Z_Right_xpm) , pm.get(Transform_Disabled_Nudge_Z_Right_xpm) ,
        " Shift up along Z ",                  cb_TransformButtons_stub,  4);

  } else if (type == 1) {

    new LFlatButton_c(0, 0, 1, 1, "up left",
        " Shift up left along XY plane ",  cb_TransformButtons_stub,  1);
    new LFlatButton_c(1, 0, 1, 1, "up right",
        " Shift up right along XY plane ", cb_TransformButtons_stub,  0);
    new LFlatButton_c(0, 1, 1, 1, "left",
        " Shift left along X ",            cb_TransformButtons_stub,  3);
    new LFlatButton_c(1, 1, 1, 1, "right",
        " Shift right along X ",           cb_TransformButtons_stub,  2);
    new LFlatButton_c(0, 2, 1, 1, "down left",
        " Shift down left XY plane ",      cb_TransformButtons_stub,  28);
    new LFlatButton_c(1, 2, 1, 1, "down right",
        " Shift down right XY plane ",     cb_TransformButtons_stub,  27);

    new LFlatButton_c(0, 3, 1, 1, pm.get(Transform_Color_Nudge_Z_Left_xpm)  , pm.get(Transform_Disabled_Nudge_Z_Left_xpm)  ,
        " Shift down along Z ",            cb_TransformButtons_stub,  5);
    new LFlatButton_c(1, 3, 1, 1, pm.get(Transform_Color_Nudge_Z_Right_xpm) , pm.get(Transform_Disabled_Nudge_Z_Right_xpm) ,
        " Shift up along Z ",              cb_TransformButtons_stub,  4);

  } else if (type == 2) {

    new LFlatButton_c(0, 0, 1, 1, "u @6->",
        " Shift up along Z and right along X ",cb_TransformButtons_stub,  0);
    new LFlatButton_c(1, 0, 1, 1, "u @8->",
        " Shift up along Z and up along Y ",   cb_TransformButtons_stub,  1);
    new LFlatButton_c(2, 0, 1, 1, "u @4->",
        " Shift up along Z and left along X ", cb_TransformButtons_stub,  2);
    new LFlatButton_c(3, 0, 1, 1, "u @2->",
        " Shift up along Z and down along Y ", cb_TransformButtons_stub,  3);

    new LFlatButton_c(0, 1, 1, 1, "@9->",
        " Shift up right along XY plane ",     cb_TransformButtons_stub,  4);
    new LFlatButton_c(1, 1, 1, 1, "@7->",
        " Shift up left along XY plane ",      cb_TransformButtons_stub,  5);
    new LFlatButton_c(2, 1, 1, 1, "@1->",
        " Shift down left along XY plane ",    cb_TransformButtons_stub,  27);
    new LFlatButton_c(3, 1, 1, 1, "@3->",
        " Shift down right along XY plane ",   cb_TransformButtons_stub,  28);


    new LFlatButton_c(0, 2, 1, 1, "d @6->",
        " Shift down along Z and right along X ", cb_TransformButtons_stub, 29);
    new LFlatButton_c(1, 2, 1, 1, "d @8->",
        " Shift down along Z and up along Y ",    cb_TransformButtons_stub, 30);
    new LFlatButton_c(2, 2, 1, 1, "d @4->",
        " Shift down along Z and left along X ",  cb_TransformButtons_stub, 31);
    new LFlatButton_c(3, 2, 1, 1, "d @2->",
        " Shift down along Z and down along Y ",  cb_TransformButtons_stub, 32);

  }

  o->end();

  o = new layouter_c(6, 3, 1, 1);

  new LFlatButton_c(6, 3, 1, 1, pm.get(Transform_Color_Rotate_X_Left_xpm) , pm.get(Transform_Disabled_Rotate_X_Left_xpm) ,
      " Rotate clockwise along X-Axis ",     cb_TransformButtons_stub,  6);
  new LFlatButton_c(7, 3, 1, 1, pm.get(Transform_Color_Rotate_X_Right_xpm), pm.get(Transform_Disabled_Rotate_X_Right_xpm),
      " Rotate anticlockwise along X-Axis ", cb_TransformButtons_stub,  7);
  new LFlatButton_c(6, 5, 1, 1, pm.get(Transform_Color_Rotate_Y_Left_xpm) , pm.get(Transform_Disabled_Rotate_Y_Left_xpm) ,
      " Rotate clockwise along Y-Axis ",     cb_TransformButtons_stub,  9);
  new LFlatButton_c(7, 5, 1, 1, pm.get(Transform_Color_Rotate_Y_Right_xpm), pm.get(Transform_Disabled_Rotate_Y_Right_xpm),
      " Rotate anticlockwise along Y-Axis ", cb_TransformButtons_stub,  8);
  new LFlatButton_c(6, 7, 1, 1, pm.get(Transform_Color_Rotate_Z_Left_xpm) , pm.get(Transform_Disabled_Rotate_Z_Left_xpm) ,
      " Rotate clockwise along Z-Axis ",     cb_TransformButtons_stub, 10);
  new LFlatButton_c(7, 7, 1, 1, pm.get(Transform_Color_Rotate_Z_Right_xpm), pm.get(Transform_Disabled_Rotate_Z_Right_xpm),
      " Rotate anticlockwise along Z-Axis ", cb_TransformButtons_stub, 11);

  o->end();

  (new LFl_Box(0, 2, 1, 1))->setMinimumSize(0, 5);

  (new LFl_Box(2, 0, 1, 1))->setMinimumSize(5, 0);
  (new LFl_Box(5, 0, 1, 1))->setMinimumSize(5, 0);

  end();
}


static void cb_ToolsButtons_stub(Fl_Widget* o, long v) { ((ToolsButtons*)(o->parent()))->cb_Press(v); }

ToolsButtons::ToolsButtons(int x, int y, int w, int h) : layouter_c(x, y, w, h) {

  label("Tools");

  (new LFl_Box(0, 0, 1, 1))->weight(1, 1);
  (new LFl_Box(0, 8, 1, 1))->weight(1, 1);
  (new LFl_Box(8, 0, 1, 1))->weight(1, 1);
  (new LFl_Box(8, 8, 1, 1))->weight(1, 1);

  (new LFl_Box("Constrain", 3, 1, 2, 1))->labelsize(LABEL_FONT_SIZE);

  new LFlatButton_c(3, 3, 1, 1, pm.get(InOut_Color_Fixed_In_xpm), pm.get(InOut_Disabled_Fixed_In_xpm),
      " Make inside fixed ", cb_ToolsButtons_stub, 16);
  new LFlatButton_c(3, 5, 1, 1, pm.get(InOut_Color_Variable_In_xpm), pm.get(InOut_Disabled_Variable_In_xpm),
      " Make inside variable ", cb_ToolsButtons_stub, 18);
  new LFlatButton_c(3, 7, 1, 1, pm.get(InOut_Color_RemoveColor_In_xpm), pm.get(InOut_Disabled_RemoveColor_In_xpm),
      " Remove Colours from inside cubes ", cb_ToolsButtons_stub, 20);

  new LFlatButton_c(4, 3, 1, 1, pm.get(InOut_Color_Fixed_Out_xpm), pm.get(InOut_Disabled_Fixed_Out_xpm),
      " Make outside fixed ", cb_ToolsButtons_stub, 17);
  new LFlatButton_c(4, 5, 1, 1, pm.get(InOut_Color_Variable_Out_xpm), pm.get(InOut_Disabled_Variable_Out_xpm),
      " Make outside variable ", cb_ToolsButtons_stub, 19);
  new LFlatButton_c(4, 7, 1, 1, pm.get(InOut_Color_RemoveColor_Out_xpm), pm.get(InOut_Disabled_RemoveColor_Out_xpm),
      " Remove Colours from outside cubes ", cb_ToolsButtons_stub, 21);

  (new LFl_Box(0, 2, 1, 1))->setMinimumSize(0, 5);
  (new LFl_Box(0, 4, 1, 1))->setMinimumSize(0, 5);
  (new LFl_Box(0, 6, 1, 1))->setMinimumSize(0, 5);

  end();
}


static void cb_SizeButtons_stub(Fl_Widget* o, long v) { ((SizeButtons*)(o->parent()))->cb_Press(v); }

SizeButtons::SizeButtons(int x, int y, int w, int h, bool addScale) : layouter_c(x, y, w, h) {

  (new LFl_Box(0, 0, 1, 1))->weight(1, 1);
  (new LFl_Box(0, 8, 1, 1))->weight(1, 1);

  (new LFl_Box("Grid", 0, 1, 1, 1))->labelsize(LABEL_FONT_SIZE);

  new LFlatButton_c(0, 3, 1, 1, pm.get(Grid_Color_Minimize_xpm), pm.get(Grid_Disabled_Minimize_xpm),
      " Minimize size of grid ", cb_SizeButtons_stub, 15);
  new LFlatButton_c(0, 5, 1, 1, pm.get(Grid_Color_Center_xpm), pm.get(Grid_Disabled_Center_xpm),
      " Centre shape inside the grid ", cb_SizeButtons_stub, 25);
  new LFlatButton_c(0, 7, 1, 1, pm.get(Grid_Color_Origin_xpm), pm.get(Grid_Disabled_Origin_xpm),
      " Move shape to origin of grid ", cb_SizeButtons_stub, 24);


  if (addScale) {
    (new LFl_Box("Shape", 2, 1, 1, 1))->labelsize(LABEL_FONT_SIZE);

    new LFlatButton_c(2, 3, 1, 1, pm.get(Rescale_Color_X1_xpm), pm.get(Rescale_Disabled_X1_xpm),
        " Try to minimize size of shape ", cb_SizeButtons_stub, 26);
    new LFlatButton_c(2, 5, 1, 1, pm.get(Rescale_Color_X2_xpm), pm.get(Rescale_Disabled_X2_xpm),
        " Double size of shape ", cb_SizeButtons_stub, 22);
    new LFlatButton_c(2, 7, 1, 1, pm.get(Rescale_Color_X3_xpm), pm.get(Rescale_Disabled_X3_xpm),
        " Triple size of shape ", cb_SizeButtons_stub, 23);
  }

  (new LFl_Box(0, 4, 1, 1))->setMinimumSize(0, 5);
  (new LFl_Box(0, 6, 1, 1))->setMinimumSize(0, 5);

  end();
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

ChangeSize::ChangeSize(int x, int y, int w, int h) : layouter_c(x, y, w, h) {

  tooltip(" Change size of space ");

  (new LFl_Box(0, 2, 1, 1))->setMinimumSize(0, 5);
  (new LFl_Box(0, 4, 1, 1))->setMinimumSize(0, 5);
  (new LFl_Box(4, 1, 1, 1))->setMinimumSize(5, 0);
  (new LFl_Box(2, 1, 1, 1))->setMinimumSize(5, 0);

  (new LFl_Box(2, 0, 1, 1))->weight(0, 1);
  (new LFl_Box(2, 6, 1, 1))->weight(0, 1);

  SizeX = new LFl_Roller(5, 1, 1, 1);
  SizeX->type(1);
  SizeX->box(FL_THIN_DOWN_BOX);
  SizeX->minimum(1);
  SizeX->maximum(1000);
  SizeX->step(0.25);
  SizeX->callback(cb_ChangeSize_stub, 0l);
  SizeX->clear_visible_focus();
  SizeX->weight(1, 0);

  SizeY = new LFl_Roller(5, 3, 1, 1);
  SizeY->type(1);
  SizeY->box(FL_THIN_DOWN_BOX);
  SizeY->minimum(1);
  SizeY->maximum(1000);
  SizeY->step(0.25);
  SizeY->callback(cb_ChangeSize_stub, 1l);
  SizeY->clear_visible_focus();

  SizeZ = new LFl_Roller(5, 5, 1, 1);
  SizeZ->type(1);
  SizeZ->box(FL_THIN_DOWN_BOX);
  SizeZ->minimum(1);
  SizeZ->maximum(1000);
  SizeZ->step(0.25);
  SizeZ->callback(cb_ChangeSize_stub, 2l);
  SizeZ->clear_visible_focus();

  SizeOutX = new LFl_Int_Input(3, 1, 1, 1);
  SizeOutX->box(FL_THIN_DOWN_BOX);
  SizeOutX->callback(cb_InputSize_stub, 0l);
  SizeOutX->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);

  SizeOutY = new LFl_Int_Input(3, 3, 1, 1);
  SizeOutY->box(FL_THIN_DOWN_BOX);
  SizeOutY->callback(cb_InputSize_stub, 1l);
  SizeOutY->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);

  SizeOutZ = new LFl_Int_Input(3, 5, 1, 1);
  SizeOutZ->box(FL_THIN_DOWN_BOX);
  SizeOutZ->callback(cb_InputSize_stub, 2l);
  SizeOutZ->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);

  new LFl_Box("X", 1, 1, 1, 1);
  new LFl_Box("Y", 1, 3, 1, 1);
  new LFl_Box("Z", 1, 5, 1, 1);

  ConnectX = new LFl_Check_Button("", 6, 1, 1, 1);
  ConnectY = new LFl_Check_Button("", 6, 3, 1, 1);
  ConnectZ = new LFl_Check_Button("", 6, 5, 1, 1);

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


static void resizeSpace(bool toAll, const ChangeSize *changeSize, puzzle_c * puzzle, unsigned int shape) {

  if (toAll) {

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
  // that loop and only increase the size, so smaller sizes for the selected shape must be done
  // here
  puzzle->getShape(shape)->resize(changeSize->getX(), changeSize->getY(), changeSize->getZ(), 0);
}






void ToolTab_0::setVoxelSpace(puzzle_c * puz, unsigned int sh) {
  puzzle = puz;
  shape = sh;

  bt_assert(!puzzle || (puzzle->getGridType()->getType() == gridType_c::GT_BRICKS));

  if (puzzle && shape < puzzle->shapeNumber())
    changeSize->setXYZ(puzzle->getShape(shape)->getX(),
        puzzle->getShape(shape)->getY(),
        puzzle->getShape(shape)->getZ());
  else
    changeSize->setXYZ(0, 0, 0);
}

static void cb_ToolTab0Size_stub(Fl_Widget* o, long v) { ((ToolTab_0*)(o->parent()->parent()->parent()))->cb_size(); }
static void cb_ToolTab0Transform_stub(Fl_Widget* o, long v) { ((ToolTab_0*)(o->parent()))->cb_transform(v); }
static void cb_ToolTab0Transform2_stub(Fl_Widget* o, long v) { ((ToolTab_0*)(o->parent()->parent()))->cb_transform(v); }

ToolTab_0::ToolTab_0(int x, int y, int w, int h) : ToolTab(x, y, w, h) {

  {
    layouter_c * o = new layouter_c(0, 1, 1, 1);
    o->label("Size");
    o->pitch(5);

    layouter_c *o2 = new layouter_c(0, 0, 1, 1);

    changeSize = new ChangeSize(0, 1, 1, 1);
    changeSize->callback(cb_ToolTab0Size_stub);

    toAll = new LFl_Check_Button("Apply to All Shapes (Bricks)", 0, 0, 1, 1);
    toAll->tooltip(" If this is active, all operations (including transformations and constrains are done to all shapes ");
    toAll->clear_visible_focus();
    toAll->stretchHCenter();

    o2->end();
    o2->weight(1, 0);

    (new LFl_Box(1, 0, 1, 1))->setMinimumSize(5, 0);

    o2 = new SizeButtons(2, 0, 1, 1, true);
    o2->callback(cb_ToolTab0Transform2_stub);

    o->end();
  }
  {
    Fl_Group* o = new TransformButtons(0, 1, 1, 1, 0);
    o->callback(cb_ToolTab0Transform_stub);
    o->hide();
  }
  {
    Fl_Group* o = new ToolsButtons(0, 1, 1, 1);
    o->callback(cb_ToolTab0Transform_stub);
    o->hide();
  }

  end();
}

void ToolTab_0::cb_size(void) {
  if (puzzle && shape < puzzle->shapeNumber()) {

    resizeSpace(toAll->value(), changeSize, puzzle, shape);
    do_callback();
  }
}

void ToolTab_0::cb_transform(long task) {
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
      voxel_0_c * space = dynamic_cast<voxel_0_c*>(puzzle->getShape(s));

      switch(task) {
        case  0: space->translate( 1, 0, 0, 0); break;
        case  1: space->translate(-1, 0, 0, 0); break;
        case  2: space->translate( 0, 1, 0, 0); break;
        case  3: space->translate( 0,-1, 0, 0); break;
        case  4: space->translate( 0, 0, 1, 0); break;
        case  5: space->translate( 0, 0,-1, 0); break;
        case  7: space->rotatex(); space->rotatex(); // fall through
        case  6: space->rotatex(); break;
        case  9: space->rotatey(); space->rotatey(); // fall through
        case  8: space->rotatey(); break;
        case 11: space->rotatez(); space->rotatez(); // fall through
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





void ToolTab_1::setVoxelSpace(puzzle_c * puz, unsigned int sh) {
  puzzle = puz;
  shape = sh;

  bt_assert(!puzzle || (puzzle->getGridType()->getType() == gridType_c::GT_TRIANGULAR_PRISM));

  if (puzzle && shape < puzzle->shapeNumber())
    changeSize->setXYZ(puzzle->getShape(shape)->getX(),
        puzzle->getShape(shape)->getY(),
        puzzle->getShape(shape)->getZ());
  else
    changeSize->setXYZ(0, 0, 0);
}

static void cb_ToolTab1Size_stub(Fl_Widget* o, long v) { ((ToolTab_1*)(o->parent()->parent()->parent()))->cb_size(); }
static void cb_ToolTab1Transform_stub(Fl_Widget* o, long v) { ((ToolTab_1*)(o->parent()))->cb_transform(v); }
static void cb_ToolTab1Transform2_stub(Fl_Widget* o, long v) { ((ToolTab_1*)(o->parent()->parent()))->cb_transform(v); }

ToolTab_1::ToolTab_1(int x, int y, int w, int h) : ToolTab(x, y, w, h) {

  {
    layouter_c * o = new layouter_c(0, 1, 1, 1);
    o->label("Size");
    o->pitch(5);

    layouter_c *o2 = new layouter_c(0, 0, 1, 1);

    changeSize = new ChangeSize(0, 1, 1, 1);
    changeSize->callback(cb_ToolTab1Size_stub);

    toAll = new LFl_Check_Button("Apply to All Shapes (Triangles)", 0, 0, 1, 1);
    toAll->tooltip(" If this is active, all operations (including transformations and constrains are done to all shapes ");
    toAll->clear_visible_focus();
    toAll->stretchHCenter();

    o2->end();
    o2->weight(1, 0);

    (new LFl_Box(1, 0, 1, 1))->setMinimumSize(5, 0);

    o2 = new SizeButtons(2, 0, 1, 1, true);
    o2->callback(cb_ToolTab1Transform2_stub);

    o->end();
  }
  {
    Fl_Group* o = new TransformButtons(0, 1, 1, 1, 1);
    o->callback(cb_ToolTab1Transform_stub);
    o->hide();
  }
  {
    Fl_Group* o = new ToolsButtons(0, 1, 1, 1);
    o->callback(cb_ToolTab1Transform_stub);
    o->hide();
  }

  end();
}

void ToolTab_1::cb_size(void) {
  if (puzzle && shape < puzzle->shapeNumber()) {

    resizeSpace(toAll->value(), changeSize, puzzle, shape);
    do_callback();
  }
}

void ToolTab_1::cb_transform(long task) {
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
        case  0: space->translate( 1, 1, 0, 0); break;
        case  1: space->translate(-1, 1, 0, 0); break;
        case  2: space->translate( 2, 0, 0, 0); break;
        case  3: space->translate(-2, 0, 0, 0); break;
        case  4: space->translate( 0, 0, 1, 0); break;
        case  5: space->translate( 0, 0,-1, 0); break;
        case 28: space->translate(-1,-1, 0, 0); break;
        case 27: space->translate( 1,-1, 0, 0); break;
        case  7: space->rotatex(); space->rotatex(); // fall through
        case  6: space->rotatex(); break;
        case  9: space->rotatey(); space->rotatey(); // fall through
        case  8: space->rotatey(); break;
        case 11: space->rotatez(); space->rotatez(); // fall through
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


void ToolTab_2::setVoxelSpace(puzzle_c * puz, unsigned int sh) {
  puzzle = puz;
  shape = sh;

  bt_assert(!puzzle || (puzzle->getGridType()->getType() == gridType_c::GT_SPHERES));

  if (puzzle && shape < puzzle->shapeNumber())
    changeSize->setXYZ(puzzle->getShape(shape)->getX(),
        puzzle->getShape(shape)->getY(),
        puzzle->getShape(shape)->getZ());
  else
    changeSize->setXYZ(0, 0, 0);
}

static void cb_ToolTab2Size_stub(Fl_Widget* o, long v) { ((ToolTab_2*)(o->parent()->parent()->parent()))->cb_size(); }
static void cb_ToolTab2Transform_stub(Fl_Widget* o, long v) { ((ToolTab_2*)(o->parent()))->cb_transform(v); }
static void cb_ToolTab2Transform2_stub(Fl_Widget* o, long v) { ((ToolTab_2*)(o->parent()->parent()))->cb_transform(v); }

ToolTab_2::ToolTab_2(int x, int y, int w, int h) : ToolTab(x, y, w, h) {

  {
    layouter_c * o = new layouter_c(0, 1, 1, 1);
    o->label("Size");
    o->pitch(5);

    layouter_c *o2 = new layouter_c(0, 0, 1, 1);

    changeSize = new ChangeSize(0, 1, 1, 1);
    changeSize->callback(cb_ToolTab2Size_stub);

    toAll = new LFl_Check_Button("Apply to All Shapes (Spheres)", 0, 0, 1, 1);
    toAll->tooltip(" If this is active, all operations (including transformations and constrains are done to all shapes ");
    toAll->clear_visible_focus();
    toAll->stretchHCenter();

    o2->end();
    o2->weight(1, 0);

    (new LFl_Box(1, 0, 1, 1))->setMinimumSize(5, 0);

    o2 = new SizeButtons(2, 0, 1, 1, false);
    o2->callback(cb_ToolTab2Transform2_stub);

    o->end();
  }
  {
    Fl_Group* o = new TransformButtons(0, 1, 1, 1, 2);
    o->callback(cb_ToolTab2Transform_stub);
    o->hide();
  }
  {
    Fl_Group* o = new ToolsButtons(0, 1, 1, 1);
    o->callback(cb_ToolTab2Transform_stub);
    o->hide();
  }

  end();
}

void ToolTab_2::cb_size(void) {
  if (puzzle && shape < puzzle->shapeNumber()) {

    resizeSpace(toAll->value(), changeSize, puzzle, shape);
    do_callback();
  }
}

void ToolTab_2::cb_transform(long task) {
  if (puzzle && shape < puzzle->shapeNumber()) {

    int ss, se;

    if (toAll->value() && ((task == 15) || (task == 24) || (task == 25))) {
      ss = 0;
      se = puzzle->shapeNumber();
    } else {
      ss = shape;
      se = shape+1;
    }

    for (int s = ss; s < se; s++) {
      voxel_c * space = puzzle->getShape(s);

      switch(task) {
        case  0: space->translate( 1, 0, 1, 0); break;
        case  1: space->translate( 0, 1, 1, 0); break;
        case  2: space->translate(-1, 0, 1, 0); break;
        case  3: space->translate( 0,-1, 1, 0); break;

        case  4: space->translate( 1, 1, 0, 0); break;
        case  5: space->translate(-1, 1, 0, 0); break;
        case 27: space->translate(-1,-1, 0, 0); break;
        case 28: space->translate( 1,-1, 0, 0); break;

        case 29: space->translate( 1, 0,-1, 0); break;
        case 30: space->translate( 0, 1,-1, 0); break;
        case 31: space->translate(-1, 0,-1, 0); break;
        case 32: space->translate( 0,-1,-1, 0); break;

        case  7: space->transform(9); break;
        case  6: space->transform(14); break;
        case  9: space->transform(5); break;
        case  8: space->transform(16); break;
        case 11: space->transform(2); break;
        case 10: space->transform(6); break;
        case 12: space->transform(120); break;
        case 13: space->transform(124); break;
        case 14: space->transform(141); break;
        case 15: space->minimizePiece(); break;
        case 16: space->actionOnSpace(voxel_c::ACT_FIXED, true); break;
        case 17: space->actionOnSpace(voxel_c::ACT_FIXED, false); break;
        case 18: space->actionOnSpace(voxel_c::ACT_VARIABLE, true); break;
        case 19: space->actionOnSpace(voxel_c::ACT_VARIABLE, false); break;
        case 20: space->actionOnSpace(voxel_c::ACT_DECOLOR, true); break;
        case 21: space->actionOnSpace(voxel_c::ACT_DECOLOR, false); break;
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








static void cb_ToolTabContainer_stub(Fl_Widget* o, void*v) {
  ToolTabContainer *vv = (ToolTabContainer*)v;
  vv->do_callback(vv, vv->user_data());
}

ToolTabContainer::ToolTabContainer(int x, int y, int w, int h, const guiGridType_c * ggt) : layouter_c(x, y, w, h) {
  tt = ggt->getToolTab(0, 0, 1, 1);
  tt->callback(cb_ToolTabContainer_stub, this);
  end();
}

void ToolTabContainer::newGridType(const guiGridType_c * ggt) {

  remove(tt);
  delete tt;
  tt = ggt->getToolTab(0, 0, 1, 1);
  tt->callback(cb_ToolTabContainer_stub, this);
  add(tt);
  resize(x(), y(), w(), h());
}

