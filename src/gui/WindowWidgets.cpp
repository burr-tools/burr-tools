#include "WindowWidgets.h"

// some tool widgets, that may be swapped out later into another file


static void cb_VoxelEditGroupZselect_stub(Fl_Widget* o, void* v) { ((VoxelEditGroup*)v)->cb_Zselect((Fl_Slider*)o); }
static void cb_VoxelEditGroupSqedit_stub(Fl_Widget* o, void* v) { ((VoxelEditGroup*)v)->cb_Sqedit((SquareEditor*)o); }

VoxelEditGroup::VoxelEditGroup(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {

  zselect = new Fl_Slider(x, y, 15, h);
  zselect->tooltip("Select Z Plane");
  zselect->box(FL_THIN_DOWN_BOX);
  zselect->color((Fl_Color)237);
  zselect->step(1);
  zselect->callback(cb_VoxelEditGroupZselect_stub, this);

  {
    Fl_Box* o = new Fl_Box(x+20, y, 5, h-5);
    o->box(FL_FLAT_BOX);
    o->color((Fl_Color)2);
  }
  {
    Fl_Box* o = new Fl_Box(x+25, y+h-5, w-25, 5);
    o->box(FL_FLAT_BOX);
    o->color((Fl_Color)1);
  }

  sqedit = new SquareEditor(x+30, y, w-30, h-10, "The Square Editor");
  sqedit->tooltip("Fill and empty cubes");
  sqedit->box(FL_NO_BOX);
  sqedit->callback(cb_VoxelEditGroupSqedit_stub, this);

  resizable(sqedit);
}


static void cb_TransformButtons_stub(Fl_Widget* o, long v) { ((TransformButtons*)(o->parent()))->cb_Press(v); }

TransformButtons::TransformButtons(int x, int y, int w, int h) : Fl_Group(x, y, w, h, "Transform") {

  new FlatButton(  5+x,  5+y, 40, 25, "S+X", "Shift up along X",                  cb_TransformButtons_stub,  0,   1);
  new FlatButton( 45+x,  5+y, 40, 25, "S-X", "Shift down along X",                cb_TransformButtons_stub,  1,   1);
  new FlatButton(  5+x, 30+y, 40, 25, "S+Y", "Shift up along Y",                  cb_TransformButtons_stub,  2,   2);
  new FlatButton( 45+x, 30+y, 40, 25, "S-Y", "Shift down along Y",                cb_TransformButtons_stub,  3,   2);
  new FlatButton(  5+x, 55+y, 40, 25, "S+Z", "Shift up along Z",                  cb_TransformButtons_stub,  4, 237);
  new FlatButton( 45+x, 55+y, 40, 25, "S-Z", "Shift down along Z",                cb_TransformButtons_stub,  5, 237);
  new FlatButton( 90+x,  5+y, 40, 25, "R+X", "Rotate clockwise along X-Axis",     cb_TransformButtons_stub,  6,   1);
  new FlatButton(130+x,  5+y, 40, 25, "R-X", "Rotate anticlockwise along X-Axis", cb_TransformButtons_stub,  7,   1);
  new FlatButton( 90+x, 30+y, 40, 25, "R+Y", "Rotate clockwise along Y-Axis",     cb_TransformButtons_stub,  8,   2);
  new FlatButton(130+x, 30+y, 40, 25, "R-Y", "Rotate anticlockwise along Y-Axis", cb_TransformButtons_stub,  9,   2);
  new FlatButton( 90+x, 55+y, 40, 25, "R+Z", "Rotate clockwise along Z-Axis",     cb_TransformButtons_stub, 10, 237);
  new FlatButton(130+x, 55+y, 40, 25, "R-Z", "Rotate anticlockwise along Z-Axis", cb_TransformButtons_stub, 11, 237);
  new FlatButton( 32+x, 85+y, 30, 25, "F X", "Flip along Y-Z Plane",              cb_TransformButtons_stub, 12,   1);
  new FlatButton( 67+x, 85+y, 30, 25, "F Y", "Flip along X-Z Plane",              cb_TransformButtons_stub, 13,   2);
  new FlatButton(102+x, 85+y, 30, 25, "F Z", "Flip along X-Y Plane",              cb_TransformButtons_stub, 14, 237);
}


static void cb_ChangeSize_stub(Fl_Widget* o, long v) { ((ChangeSize*)(o->parent()))->cb_roll(v); }

ChangeSize::ChangeSize(int x, int y, int w, int h) : Fl_Group(x, y, w, h, "Size") {

  tooltip("Change size of space");

  SizeX = new Fl_Roller(70+x, 15+y, 90, 15);
  SizeX->type(1);
  SizeX->box(FL_THIN_DOWN_BOX);
  SizeX->minimum(1);
  SizeX->maximum(1000);
  SizeX->step(0.25);
  SizeX->callback(cb_ChangeSize_stub, 0l);

  SizeY = new Fl_Roller(70+x, 50+y, 90, 15);
  SizeY->type(1);
  SizeY->box(FL_THIN_DOWN_BOX);
  SizeY->minimum(1);
  SizeY->maximum(1000);
  SizeY->step(0.25);
  SizeY->callback(cb_ChangeSize_stub, 1l);

  SizeZ = new Fl_Roller(70+x, 85+y, 90, 15);
  SizeZ->type(1);
  SizeZ->box(FL_THIN_DOWN_BOX);
  SizeZ->minimum(1);
  SizeZ->maximum(1000);
  SizeZ->step(0.25);
  SizeZ->callback(cb_ChangeSize_stub, 2l);

  SizeOutX = new Fl_Value_Output(20+x, 10+y, 40, 20, "X");
  SizeOutX->box(FL_THIN_DOWN_BOX);
  SizeOutX->minimum(1);
  SizeOutX->maximum(1000);
  SizeOutX->color((Fl_Color)1);

  SizeOutY = new Fl_Value_Output(20+x, 45+y, 40, 20, "Y");
  SizeOutY->box(FL_THIN_DOWN_BOX);
  SizeOutY->minimum(1);
  SizeOutY->maximum(1000);
  SizeOutY->color((Fl_Color)2);

  SizeOutZ = new Fl_Value_Output(20+x, 80+y, 40, 20, "Z");
  SizeOutZ->box(FL_THIN_DOWN_BOX);
  SizeOutZ->minimum(1);
  SizeOutZ->maximum(1000);
  SizeOutZ->color((Fl_Color)237);
}


static void cb_ToolTabSize_stub(Fl_Widget* o, long v) { ((ToolTab*)(o->parent()))->cb_size(); }
static void cb_ToolTabTransform_stub(Fl_Widget* o, long v) { ((ToolTab*)(o->parent()))->cb_transform(v); }

ToolTab::ToolTab(int x, int y, int w, int h, int type) : Fl_Tabs(x, y, w, h) {

  {
    Fl_Group* o = changeSize = new ChangeSize(x, y+20, w, h-20);
    o->callback(cb_ToolTabSize_stub);
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
    new FlatButton(x+5, y+25, w-10, 20, "Minimize", "Minimize the size", cb_ToolTabTransform_stub, 15);
    if (type == 1)
      new FlatButton(x+5, y+50, w-10, 20, "Make inside Variable", "Make the inside of the puzzle variable, so that it can contain holes", cb_ToolTabTransform_stub, 16);

    o->end();
  }
}


static void cb_SelectorGroupSlider_stub(Fl_Widget* o, void* v) { ((SelectorGroup*)(o->parent()))->cb_slider(); }
static void cb_SelectorGroupPcSel_stub(Fl_Widget* o, void* v) { ((SelectorGroup*)(o->parent()))->cb_pcsel(); }

SelectorGroup::SelectorGroup(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {

  PcSelSlider = new Fl_Slider(x+w-15, y, 15, h);
  PcSelSlider->box(FL_THIN_DOWN_BOX);
  PcSelSlider->maximum(20);
  PcSelSlider->callback(cb_SelectorGroupSlider_stub);

  PcSel = new PieceSelector(x, y, w-20, h, "Piece Selector");
  PcSel->tooltip("Select piece. Change number of instances for one piece by dragging left and right.");
  PcSel->box(FL_NO_BOX);
  PcSel->callback(cb_SelectorGroupPcSel_stub);
  Fl_Group::current()->resizable(PcSel);
}


static void cb_View3dGroupSlider_stub(Fl_Widget* o, void* v) { ((View3dGroup*)(o->parent()))->cb_slider(); }

View3dGroup::View3dGroup(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
  box(FL_DOWN_BOX);

  View3D = new VoxelView(x, y, w-15, h, "3d View");
  View3D->tooltip("Rotate the puzzle by dragging with the mouse.");
  View3D->box(FL_NO_BOX);
  Fl_Group::current()->resizable(View3D);

  slider = new Fl_Slider(x+w-15, y, 15, h);
  slider->tooltip("Zoom view.");
  slider->box(FL_THIN_DOWN_BOX);
  slider->maximum(50);
  slider->step(0.01);
  slider->value(10);
  slider->callback(cb_View3dGroupSlider_stub);
}

