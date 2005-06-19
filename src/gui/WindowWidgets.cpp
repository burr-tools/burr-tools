/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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

// some tool widgets, that may be swapped out later into another file


static void cb_VoxelEditGroupZselect_stub(Fl_Widget* o, void* v) { ((VoxelEditGroup*)v)->cb_Zselect((Fl_Slider*)o); }
static void cb_VoxelEditGroupSqedit_stub(Fl_Widget* o, void* v) { ((VoxelEditGroup*)v)->cb_Sqedit((SquareEditor*)o); }

VoxelEditGroup::VoxelEditGroup(int x, int y, int w, int h, puzzle_c * puzzle) : Fl_Group(x, y, w, h) {

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

  sqedit = new SquareEditor(x+30, y, w-30, h-10, puzzle);
  sqedit->tooltip("Fill and empty cubes");
  sqedit->box(FL_NO_BOX);
  sqedit->callback(cb_VoxelEditGroupSqedit_stub, this);

  resizable(sqedit);
}

#define SZ_BUTTON_Y 20

static void cb_TransformButtons_stub(Fl_Widget* o, long v) { ((TransformButtons*)(o->parent()))->cb_Press(v); }

TransformButtons::TransformButtons(int x, int y, int w, int h) : Fl_Group(x, y, w, h, "Transform") {

  new FlatButton(  5+x,  5+y, 40, SZ_BUTTON_Y, "S+X", "Shift up along X",                  cb_TransformButtons_stub,  0,   1);
  new FlatButton( 45+x,  5+y, 40, SZ_BUTTON_Y, "S-X", "Shift down along X",                cb_TransformButtons_stub,  1,   1);
  new FlatButton(  5+x, 30+y, 40, SZ_BUTTON_Y, "S+Y", "Shift up along Y",                  cb_TransformButtons_stub,  2,   2);
  new FlatButton( 45+x, 30+y, 40, SZ_BUTTON_Y, "S-Y", "Shift down along Y",                cb_TransformButtons_stub,  3,   2);
  new FlatButton(  5+x, 55+y, 40, SZ_BUTTON_Y, "S+Z", "Shift up along Z",                  cb_TransformButtons_stub,  4, 237);
  new FlatButton( 45+x, 55+y, 40, SZ_BUTTON_Y, "S-Z", "Shift down along Z",                cb_TransformButtons_stub,  5, 237);
  new FlatButton( 90+x,  5+y, 40, SZ_BUTTON_Y, "R+X", "Rotate clockwise along X-Axis",     cb_TransformButtons_stub,  6,   1);
  new FlatButton(130+x,  5+y, 40, SZ_BUTTON_Y, "R-X", "Rotate anticlockwise along X-Axis", cb_TransformButtons_stub,  7,   1);
  new FlatButton( 90+x, 30+y, 40, SZ_BUTTON_Y, "R+Y", "Rotate clockwise along Y-Axis",     cb_TransformButtons_stub,  8,   2);
  new FlatButton(130+x, 30+y, 40, SZ_BUTTON_Y, "R-Y", "Rotate anticlockwise along Y-Axis", cb_TransformButtons_stub,  9,   2);
  new FlatButton( 90+x, 55+y, 40, SZ_BUTTON_Y, "R+Z", "Rotate clockwise along Z-Axis",     cb_TransformButtons_stub, 10, 237);
  new FlatButton(130+x, 55+y, 40, SZ_BUTTON_Y, "R-Z", "Rotate anticlockwise along Z-Axis", cb_TransformButtons_stub, 11, 237);
  new FlatButton( 32+x, 85+y, 30, SZ_BUTTON_Y, "F X", "Flip along Y-Z Plane",              cb_TransformButtons_stub, 12,   1);
  new FlatButton( 67+x, 85+y, 30, SZ_BUTTON_Y, "F Y", "Flip along X-Z Plane",              cb_TransformButtons_stub, 13,   2);
  new FlatButton(102+x, 85+y, 30, SZ_BUTTON_Y, "F Z", "Flip along X-Y Plane",              cb_TransformButtons_stub, 14, 237);
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
static void cb_ToolTabTransform2_stub(Fl_Widget* o, long v) { ((ToolTab*)(o->parent()->parent()))->cb_transform(v); }

ToolTab::ToolTab(int x, int y, int w, int h) : Fl_Tabs(x, y, w, h) {

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
    new FlatButton(x+5, y+25, w-10, 20, "Minimize", "Minimize the size", cb_ToolTabTransform2_stub, 15);
    new FlatButton(x+5, y+50, w-10, 20, "Make inside Variable", "Make the inside of the puzzle variable, so that it can contain holes", cb_ToolTabTransform2_stub, 16);
    o->end();
  }
}

static void cb_BlockListGroupSlider_stub(Fl_Widget* o, void* v) { ((BlockListGroup*)(o->parent()))->cb_slider(); }
static void cb_BlockListGroupList_stub(Fl_Widget* o, void* v) { ((BlockListGroup*)(o->parent()))->cb_list(); }

BlockListGroup::BlockListGroup(int x, int y, int w, int h, BlockList * l) : Fl_Group(x, y, w, h), List(l) {

  box(FL_THIN_DOWN_FRAME);
  x++; y++; w-=2; h-=2;

  Slider = new Fl_Slider(x+w-15-1, y-1, 15+2, h+2);
  Slider->box(FL_THIN_DOWN_BOX);
  Slider->maximum(0);
  Slider->callback(cb_BlockListGroupSlider_stub);

  w-=15;

  Fl_Box * frame = new Fl_Box(x, y, w, h);
  frame->box(FL_THIN_UP_FRAME);
  x++; y++; w-=2; h-=2;

  add(List);
  List->resize(x, y, w, h);
  List->callback(cb_BlockListGroupList_stub);

  resizable(List);
  end();
}

static void cb_ConstraintsGroupSlider_stub(Fl_Widget* o, void* v) { ((ConstraintsGroup*)(o->parent()))->cb_slider(); }
static void cb_ConstraintsGroupList_stub(Fl_Widget* o, void* v) { ((ConstraintsGroup*)(o->parent()))->cb_list(); }

ConstraintsGroup::ConstraintsGroup(int x, int y, int w, int h, ColorConstraintsEdit * l) : Fl_Group(x, y, w, h), List(l) {

  box(FL_THIN_DOWN_FRAME);
  x++; y++; w-=2; h-=2;

  Slider = new Fl_Slider(x+w-15-1, y-1, 15+2, h+2);
  Slider->box(FL_THIN_DOWN_BOX);
  Slider->maximum(0);
  Slider->callback(cb_ConstraintsGroupSlider_stub);

  w-=15;

  Fl_Box * frame = new Fl_Box(x, y, w, h);
  frame->box(FL_THIN_UP_FRAME);
  x++; y++; w-=2; h-=2;

  add(List);
  List->resize(x, y, w, h);
  List->callback(cb_ConstraintsGroupList_stub);

  resizable(List);
  end();
}


static void cb_View3dGroupSlider_stub(Fl_Widget* o, void* v) { ((View3dGroup*)(o->parent()))->cb_slider(); }

void View3dGroup::cb_slider(void) {
  View3D->setSize(exp(slider->value()));
}


View3dGroup::View3dGroup(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
  box(FL_DOWN_BOX);

  View3D = new VoxelView(x, y, w-15, h, "3d View");
  View3D->tooltip("Rotate the puzzle by dragging with the mouse.");
  View3D->box(FL_NO_BOX);

  slider = new Fl_Slider(x+w-15, y, 15, h);
  slider->tooltip("Zoom view.");
  slider->box(FL_THIN_DOWN_BOX);
  slider->maximum(5);
  slider->minimum(-3);
  slider->step(0.01);
  slider->value(2);
  slider->callback(cb_View3dGroupSlider_stub);

  resizable(View3D);
  end();
}


Separator::Separator(int x, int y, int w, int h, const char * label, bool button) : Fl_Group(x, y, w, h) {

  if (label) {
    int lw, lh;

    fl_font(labelfont(), labelsize()-4);
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
  assert(p);
  bg = color();
//  setcontent();
  box(FL_BORDER_BOX);
}

void ResultViewer::setPuzzle(puzzle_c * p, unsigned int prob) {
  puzzle = p;
  problem = prob;
  redraw();
//  setcontent();
}

void ResultViewer::draw(void) {
  if (problem >= puzzle->problemNumber() ||
      (puzzle->probGetResult(problem) < 0) || (puzzle->probGetResult(problem) > puzzle->shapeNumber())) {
    label("No Result");
    color(bg);
    labelcolor(fl_rgb_color(255, 0, 0));
  } else {
    static char txt[20];

    unsigned int result = puzzle->probGetResult(problem);

    snprintf(txt, 19, "Result: %i", result);

    unsigned char r, g, b;

    r = (int)(255*pieceColorR(result));
    g = (int)(255*pieceColorG(result));
    b = (int)(255*pieceColorB(result));

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

  View3D->clearSpaces();
  unsigned int num = View3D->addSpace(new pieceVoxel_c(puz->getShape(shapeNum)));

  View3D->setSpaceColor(num, pieceColorR(shapeNum), pieceColorG(shapeNum), pieceColorB(shapeNum), 255);

  View3D->setTransformationType(VoxelView::TranslateRoateScale);
  View3D->setScaling(1);
  View3D->update(true);
}

void View3dGroup::showProblem(const puzzle_c * puz, unsigned int probNum) {

  View3D->update(false);

  View3D->clearSpaces();

  // first find out how to arrange the pieces:
  unsigned int square = 3;
  while (square * (square-2) < puz->probShapeNumber(probNum)) square++;

  unsigned int num;

  float diagonal = 0;

  // now find a scaling factor, so that all pieces fit into their square
  if (puz->probGetResultShape(probNum)) {

    if (puz->probGetResultShape(probNum)->getDiagonal() > diagonal)
      diagonal = puz->probGetResultShape(probNum)->getDiagonal();
  }

  for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++)
    if (puz->probGetShapeShape(probNum, p)->getDiagonal() > diagonal)
      diagonal = puz->probGetShapeShape(probNum, p)->getDiagonal();

  diagonal = sqrt(diagonal)/1.5;

  // now place the result shape
  if (puz->probGetResultShape(probNum)) {

    num = View3D->addSpace(new pieceVoxel_c(puz->probGetResultShape(probNum)));
    View3D->setSpaceColor(num,
                          pieceColorR(puz->probGetResult(probNum)),
                          pieceColorG(puz->probGetResult(probNum)),
                          pieceColorB(puz->probGetResult(probNum)), 255);
    View3D->setSpacePosition(num,
                             0.5* (square*diagonal) * (1.0/square - 0.5),
                             0.5* (square*diagonal) * (0.5 - 1.0/square), -20, 1.0);
  }

  // and now the shapes
  int unsigned line = 2;
  int unsigned col = 0;
  for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++) {
    num = View3D->addSpace(new pieceVoxel_c(puz->probGetShapeShape(probNum, p)));

    View3D->setSpaceColor(num,
                          pieceColorR(puz->probGetShape(probNum, p)),
                          pieceColorG(puz->probGetShape(probNum, p)),
                          pieceColorB(puz->probGetShape(probNum, p)), 255);

    View3D->setSpacePosition(num,
                             0.5* (square*diagonal) * ((col+0.5)/square - 0.5),
                             0.5* (square*diagonal) * (0.5 - (line+0.5)/square),
                             -20, 0.5);

    col++;
    if (col == square) {
      col = 0;
      line++;
    }
  }

  View3D->setScaling(5);
  View3D->setTransformationType(VoxelView::ScaleRotateTranslate);
  View3D->update(true);
}

void View3dGroup::showAssembly(const puzzle_c * puz, unsigned int probNum, unsigned int solNum) {
  View3D->update(false);

  View3D->clearSpaces();

  unsigned int num;

  const assembly_c * assm = puz->probGetAssembly(probNum, solNum);

  unsigned int piece = 0;

  // and now the shapes
  for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++)
    for (unsigned int q = 0; q < puz->probGetShapeCount(probNum, p); q++) {

      num = View3D->addSpace(new pieceVoxel_c(puz->probGetShapeShape(probNum, p), assm->getTransformation(piece)));

      View3D->setSpacePosition(num, assm->getX(piece), assm->getY(piece), assm->getZ(piece), 1);
  
      View3D->setSpaceColor(num,
                            pieceColorR(puz->probGetShape(probNum, p), q),
                            pieceColorG(puz->probGetShape(probNum, p), q),
                            pieceColorB(puz->probGetShape(probNum, p), q), 255);

      piece++;
    }

  View3D->setScaling(1);
  View3D->setCenter(puz->probGetResultShape(probNum)->getX()/2,
                    puz->probGetResultShape(probNum)->getY()/2,
                    puz->probGetResultShape(probNum)->getZ()/2
                   );
  View3D->setTransformationType(VoxelView::CenterTranslateRoateScale);
  View3D->update(true);
}

void View3dGroup::updatePositions(PiecePositions *shifting) {

  View3D->update(false);

  for (unsigned int p = 0; p < View3D->spaceNumber(); p++) {
    View3D->setSpacePosition(p, shifting->getX(p), shifting->getY(p), shifting->getZ(p), 1);
    View3D->setSpaceColor(p, shifting->getA(p));
  }

  View3D->update(true);
}
