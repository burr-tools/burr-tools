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

#include <FL/Fl_Multiline_Output.H>

#include <math.h>

// some tool widgets, that may be swapped out later into another file


static void cb_VoxelEditGroupZselect_stub(Fl_Widget* o, void* v) { ((VoxelEditGroup*)v)->cb_Zselect((Fl_Slider*)o); }
static void cb_VoxelEditGroupSqedit_stub(Fl_Widget* o, void* v) { ((VoxelEditGroup*)v)->cb_Sqedit((SquareEditor*)o); }

VoxelEditGroup::VoxelEditGroup(int x, int y, int w, int h, puzzle_c * puzzle) : Fl_Group(x, y, w, h) {

  zselect = new Fl_Slider(x, y, 15, h);
  zselect->tooltip(" Select Z Plane ");
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
  sqedit->tooltip(" Fill and empty cubes ");
  sqedit->box(FL_NO_BOX);
  sqedit->callback(cb_VoxelEditGroupSqedit_stub, this);

  resizable(sqedit);
}

void VoxelEditGroup::setZ(unsigned int val) {
  if (val > zselect->maximum()) val = (unsigned int)zselect->maximum();
  zselect->value(val);
  sqedit->setZ(val);
}

void VoxelEditGroup::setPuzzle(puzzle_c * puzzle, unsigned int num) {
  sqedit->setPuzzle(puzzle, num);
  if (puzzle && (num < puzzle->shapeNumber())) {
    voxel_c * v = puzzle->getShape(num);
    if (v) {
      zselect->bounds(0, v->getZ()-1);
      zselect->value(sqedit->getZ());
    }
  }
}



#define SZ_BUTTON_Y 20

static void cb_TransformButtons_stub(Fl_Widget* o, long v) { ((TransformButtons*)(o->parent()))->cb_Press(v); }

TransformButtons::TransformButtons(int x, int y, int w, int h) : Fl_Group(x, y, w, h, "Transform") {

  new FlatButton(  5+x,  5+y, 40, SZ_BUTTON_Y, "S+X", " Shift up along X ",                  cb_TransformButtons_stub,  0,   1);
  new FlatButton( 45+x,  5+y, 40, SZ_BUTTON_Y, "S-X", " Shift down along X ",                cb_TransformButtons_stub,  1,   1);
  new FlatButton(  5+x, 25+y, 40, SZ_BUTTON_Y, "S+Y", " Shift up along Y ",                  cb_TransformButtons_stub,  2,   2);
  new FlatButton( 45+x, 25+y, 40, SZ_BUTTON_Y, "S-Y", " Shift down along Y ",                cb_TransformButtons_stub,  3,   2);
  new FlatButton(  5+x, 45+y, 40, SZ_BUTTON_Y, "S+Z", " Shift up along Z ",                  cb_TransformButtons_stub,  4, 237);
  new FlatButton( 45+x, 45+y, 40, SZ_BUTTON_Y, "S-Z", " Shift down along Z ",                cb_TransformButtons_stub,  5, 237);
  new FlatButton( 90+x,  5+y, 40, SZ_BUTTON_Y, "R+X", " Rotate clockwise along X-Axis ",     cb_TransformButtons_stub,  6,   1);
  new FlatButton(130+x,  5+y, 40, SZ_BUTTON_Y, "R-X", " Rotate anticlockwise along X-Axis ", cb_TransformButtons_stub,  7,   1);
  new FlatButton( 90+x, 25+y, 40, SZ_BUTTON_Y, "R+Y", " Rotate clockwise along Y-Axis ",     cb_TransformButtons_stub,  8,   2);
  new FlatButton(130+x, 25+y, 40, SZ_BUTTON_Y, "R-Y", " Rotate anticlockwise along Y-Axis ", cb_TransformButtons_stub,  9,   2);
  new FlatButton( 90+x, 45+y, 40, SZ_BUTTON_Y, "R+Z", " Rotate clockwise along Z-Axis ",     cb_TransformButtons_stub, 10, 237);
  new FlatButton(130+x, 45+y, 40, SZ_BUTTON_Y, "R-Z", " Rotate anticlockwise along Z-Axis ", cb_TransformButtons_stub, 11, 237);
  new FlatButton( 32+x, 70+y, 30, SZ_BUTTON_Y, "F X", " Flip along Y-Z Plane ",              cb_TransformButtons_stub, 12,   1);
  new FlatButton( 67+x, 70+y, 30, SZ_BUTTON_Y, "F Y", " Flip along X-Z Plane ",              cb_TransformButtons_stub, 13,   2);
  new FlatButton(102+x, 70+y, 30, SZ_BUTTON_Y, "F Z", " Flip along X-Y Plane ",              cb_TransformButtons_stub, 14, 237);
}


static void cb_ChangeSize_stub(Fl_Widget* o, long v) { ((ChangeSize*)(o->parent()))->cb_roll(v); }

ChangeSize::ChangeSize(int x, int y, int w, int h) : Fl_Group(x, y, w, h, "Size") {

  tooltip(" Change size of space ");

  SizeX = new Fl_Roller(70+x, 15+y, 90, 15);
  SizeX->type(1);
  SizeX->box(FL_THIN_DOWN_BOX);
  SizeX->minimum(1);
  SizeX->maximum(1000);
  SizeX->step(0.25);
  SizeX->callback(cb_ChangeSize_stub, 0l);

  SizeY = new Fl_Roller(70+x, 40+y, 90, 15);
  SizeY->type(1);
  SizeY->box(FL_THIN_DOWN_BOX);
  SizeY->minimum(1);
  SizeY->maximum(1000);
  SizeY->step(0.25);
  SizeY->callback(cb_ChangeSize_stub, 1l);

  SizeZ = new Fl_Roller(70+x, 65+y, 90, 15);
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

  SizeOutY = new Fl_Value_Output(20+x, 35+y, 40, 20, "Y");
  SizeOutY->box(FL_THIN_DOWN_BOX);
  SizeOutY->minimum(1);
  SizeOutY->maximum(1000);
  SizeOutY->color((Fl_Color)2);

  SizeOutZ = new Fl_Value_Output(20+x, 60+y, 40, 20, "Z");
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
    new FlatButton(x+5, y+25, w-10, 20, "Minimize", " Minimize the size ", cb_ToolTabTransform2_stub, 15);
    new FlatButton(x+5, y+50, w-10, 20, "Make inside Variable", " Make the inside of the puzzle variable, so that it can contain holes ", cb_ToolTabTransform2_stub, 16);
    o->end();
  }
}

void ToolTab::cb_transform(long task) {
  if (space) {

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
    case 16: space->makeInsideHoly(); break;
    }
    space->setHotspot(0, 0, 0);

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
  View3D->setSize(exp(slider->value()));
}


View3dGroup::View3dGroup(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
  box(FL_DOWN_BOX);

  View3D = new VoxelView(x, y, w-15, h, "3d View");
  View3D->tooltip(" Rotate the puzzle by dragging with the mouse ");
  View3D->box(FL_NO_BOX);

  slider = new Fl_Slider(x+w-15, y, 15, h);
  slider->tooltip("Zoom view.");
  slider->box(FL_THIN_DOWN_BOX);
  slider->maximum(5);
  slider->minimum(-3);
  slider->step(0.01);
  slider->value(2);
  slider->callback(cb_View3dGroupSlider_stub);

  cb_slider();

  resizable(View3D);
  end();
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


void View3dGroup::showSingleShape(const puzzle_c * puz, unsigned int shapeNum, bool showColors) {

  View3D->update(false);

  View3D->hideMarker();
  View3D->clearSpaces();
  unsigned int num = View3D->addSpace(new voxel_c(puz->getShape(shapeNum)));

  View3D->setSpaceColor(num, pieceColorR(shapeNum), pieceColorG(shapeNum), pieceColorB(shapeNum), 255);

  View3D->setTransformationType(VoxelView::TranslateRoateScale);
  View3D->setScaling(1);
  View3D->showCoordinateSystem(true);

  View3D->update(true);
}

void View3dGroup::showProblem(const puzzle_c * puz, unsigned int probNum, unsigned int selShape, bool showColors) {

  View3D->update(false);

  View3D->hideMarker();
  View3D->clearSpaces();

  if (probNum < puz->problemNumber()) {

    // first find out how to arrange the pieces:
    unsigned int square = 3;
    while (square * (square-2) < puz->probShapeNumber(probNum)) square++;

    unsigned int num;

    float diagonal = 0;

    // now find a scaling factor, so that all pieces fit into their square
    if (puz->probGetResult(probNum) < puz->shapeNumber()) {

      if (puz->probGetResultShape(probNum)->getDiagonal() > diagonal)
        diagonal = puz->probGetResultShape(probNum)->getDiagonal();
    }

    // check the selected shape
    if (selShape < puz->shapeNumber()) {

      if (puz->getShape(selShape)->getDiagonal() > diagonal)
        diagonal = puz->getShape(selShape)->getDiagonal();
    }

    for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++)
      if (puz->probGetShapeShape(probNum, p)->getDiagonal() > diagonal)
        diagonal = puz->probGetShapeShape(probNum, p)->getDiagonal();

    diagonal = sqrt(diagonal)/1.5;

    // now place the result shape
    if (puz->probGetResult(probNum) < puz->shapeNumber()) {

      num = View3D->addSpace(new voxel_c(puz->probGetResultShape(probNum)));
      View3D->setSpaceColor(num,
                            pieceColorR(puz->probGetResult(probNum)),
                            pieceColorG(puz->probGetResult(probNum)),
                            pieceColorB(puz->probGetResult(probNum)), 255);
      View3D->setSpacePosition(num,
                               0.5* (square*diagonal) * (1.0/square - 0.5),
                               0.5* (square*diagonal) * (0.5 - 1.0/square), -20, 1.0);
    }

    // now place the selected shape
    if (selShape < puz->shapeNumber()) {

      num = View3D->addSpace(new voxel_c(puz->getShape(selShape)));
      View3D->setSpaceColor(num,
                            pieceColorR(selShape),
                            pieceColorG(selShape),
                            pieceColorB(selShape), 255);
      View3D->setSpacePosition(num,
                               0.5* (square*diagonal) * (0.5 - 0.5/square),
                               0.5* (square*diagonal) * (0.5 - 0.5/square), -20, 0.5);
    }

    // and now the shapes
    int unsigned line = 2;
    int unsigned col = 0;
    for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++) {
      num = View3D->addSpace(new voxel_c(puz->probGetShapeShape(probNum, p)));

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
    View3D->showCoordinateSystem(false);
  }

  View3D->update(true);
}

void View3dGroup::showColors(const puzzle_c * puz, bool show) {
  View3D->update(false);

  if (show) {

    View3D->clearPalette();
    for (unsigned int i = 0; i < puz->colorNumber(); i++) {
      unsigned char r, g, b;
      puz->getColor(i, &r, &g, &b);
      View3D->addPaletteEntry(r/255.0, g/255.0, b/255.0);
    }
    View3D->setColorMode(VoxelView::paletteColor);

  } else
    View3D->setColorMode(VoxelView::pieceColor);

  View3D->update(true);
}


void View3dGroup::showAssembly(const puzzle_c * puz, unsigned int probNum, unsigned int solNum, bool showColors) {
  View3D->update(false);

  View3D->hideMarker();
  View3D->clearSpaces();

  if ((probNum < puz->problemNumber()) &&
      (solNum < puz->probSolutionNumber(probNum))) {

    unsigned int num;

    const assembly_c * assm = puz->probGetAssembly(probNum, solNum);

    unsigned int piece = 0;

    // and now the shapes
    for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++)
      for (unsigned int q = 0; q < puz->probGetShapeCount(probNum, p); q++) {

        num = View3D->addSpace(new voxel_c(puz->probGetShapeShape(probNum, p), assm->getTransformation(piece)));

        View3D->setSpacePosition(num, assm->getX(piece), assm->getY(piece), assm->getZ(piece), 1);

        View3D->setSpaceColor(num,
                              pieceColorR(puz->probGetShape(probNum, p), q),
                              pieceColorG(puz->probGetShape(probNum, p), q),
                              pieceColorB(puz->probGetShape(probNum, p), q), 255);

        piece++;
      }

    View3D->setScaling(1);
    View3D->setCenter(0.5*puz->probGetResultShape(probNum)->getX(),
                      0.5*puz->probGetResultShape(probNum)->getY(),
                      0.5*puz->probGetResultShape(probNum)->getZ()
                     );
    View3D->setTransformationType(VoxelView::CenterTranslateRoateScale);
    View3D->showCoordinateSystem(false);
  }

  View3D->update(true);
}

void View3dGroup::showPlacement(const puzzle_c * puz, unsigned int probNum, unsigned int piece, unsigned char trans, int x, int y, int z) {

  View3D->update(false);
  View3D->clearSpaces();
  View3D->hideMarker();
  View3D->setScaling(1);
  View3D->setTransformationType(VoxelView::CenterTranslateRoateScale);
  View3D->showCoordinateSystem(false);
  View3D->setCenter(0.5*puz->probGetResultShape(probNum)->getX(),
                    0.5*puz->probGetResultShape(probNum)->getY(),
                    0.5*puz->probGetResultShape(probNum)->getZ()
                   );

  int num;

  if (trans < NUM_TRANSFORMATIONS_MIRROR) {

    int shape = 0;
    unsigned int p = piece;
    while (p >= puz->probGetShapeCount(probNum, shape)) {
      p -= puz->probGetShapeCount(probNum, shape);
      shape++;
    }

    num = View3D->addSpace(new voxel_c(puz->probGetShapeShape(probNum, shape), trans));
    View3D->setSpacePosition(num, x, y, z, 1);
    View3D->setSpaceColor(num,
                          pieceColorR(puz->probGetShape(probNum, shape), p),
                          pieceColorG(puz->probGetShape(probNum, shape), p),
                          pieceColorB(puz->probGetShape(probNum, shape), p), 255);
    View3D->setDrawingMode(num, VoxelView::normal);
  }

  num = View3D->addSpace(new voxel_c(puz->probGetResultShape(probNum)));
  View3D->setSpaceColor(num,
                        pieceColorR(puz->probGetResult(probNum)),
                        pieceColorG(puz->probGetResult(probNum)),
                        pieceColorB(puz->probGetResult(probNum)), 255);
  View3D->setDrawingMode(num, VoxelView::gridline);

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

void View3dGroup::updateVisibility(PieceVisibility * pcvis) {
  View3D->update(false);

  for (unsigned int p = 0; p < View3D->spaceNumber(); p++) {

    switch(pcvis->getVisibility(p)) {
    case 0:
      View3D->setDrawingMode(p, VoxelView::normal);
      break;
    case 1:
      View3D->setDrawingMode(p, VoxelView::gridline);
      break;
    case 2:
      View3D->setDrawingMode(p, VoxelView::invisible);
      break;
    }
  }

  View3D->update(true);
}


static void cb_ButtonGroup_stub(Fl_Widget* o, void* v) { ((ButtonGroup*)v)->cb_Push((Fl_Button*)o); }

ButtonGroup::ButtonGroup(int x, int y, int w, int h) : Fl_Group(x, y, w, h), currentButton(0) {
  end();
}

Fl_Button * ButtonGroup::addButton(int x, int y, int w, int h) {

  int c = children();

  Fl_Button * b = new Fl_Button(x, y, w, h);
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

  colors = new Fl_Check_Button(x+w-130, y, 130, h, "Color 3D View");
  colors->box(FL_THIN_UP_BOX);
  colors->tooltip(" Toggle between piece colors and the colors of the color constraints (neutral units will have piece color) ");

  resizable(text);

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
