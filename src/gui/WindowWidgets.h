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
#ifndef __WINDOW_WIDGETS_H__
#define __WINDOW_WIDGETS_H__

#include "SquareEditor.h"
#include "VoxelDrawer.h"
#include "BlockList.h"
#include "DisasmToMoves.h"

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Light_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Roller.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Multiline_Input.H>
#include <FL/Fl_Progress.H>
#include <FL/fl_draw.h>

// my button, the only change it that the box is automatically set to engraved
class FlatButton : public Fl_Button {

public:
  FlatButton(int x, int y, int w, int h, const char * txt, const char * tt) : Fl_Button(x, y, w, h, txt) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
    clear_visible_focus();
  }

  FlatButton(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback* cb) : Fl_Button(x, y, w, h, txt) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
    callback(cb);
    clear_visible_focus();
  }

  FlatButton(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback1* cb, long cb_para) : Fl_Button(x, y, w, h, txt) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
    callback(cb, cb_para);
    clear_visible_focus();
  }

  FlatButton(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback* cb, void * cb_para) : Fl_Button(x, y, w, h, txt) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
    callback(cb, cb_para);
    clear_visible_focus();
  }

  FlatButton(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback1* cb, long cb_para, int col) : Fl_Button(x, y, w, h, txt) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
    callback(cb, cb_para);
    color((Fl_Color)col);
    clear_visible_focus();
  }

  FlatButton(int x, int y, int w, int h, Fl_Image * img, Fl_Image * inact, const char * tt, Fl_Callback1* cb, long cb_para) : Fl_Button(x, y, w, h) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
    callback(cb, cb_para);
    image(img);
    deimage(inact);
    clear_visible_focus();
  }
};

class FlatLightButton : public Fl_Light_Button {

public:
  FlatLightButton(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback1* cb, long cb_para) : Fl_Light_Button(x, y, w, h, txt) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
    callback(cb, cb_para);
    clear_visible_focus();
  }

  FlatLightButton(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback* cb, void * cb_para) : Fl_Light_Button(x, y, w, h, txt) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
    callback(cb, cb_para);
    clear_visible_focus();
  }
};

class ToggleButton : public Fl_Button {

  Fl_Callback *callback;
  void * callback_para;
  long para;


  public:
    ToggleButton(int x, int y, int w, int h, Fl_Callback *cb, void * cb_para, long para);

    void toggle(void) {
      value(1-value());
      if (callback)
        callback(this, callback_para);
    }

    long ButtonVal(void) { return para; }
};

class LineSpacer;

// the group for the square editor including the colord marker and the slider for the z axis
class VoxelEditGroup : public Fl_Group {

  SquareEditor * sqedit;
  Fl_Slider * zselect;
  LineSpacer * space;

public:

  VoxelEditGroup(int x, int y, int w, int h, puzzle_c * puzzle);

  void draw();

  void cb_Zselect(Fl_Slider* o) {
    sqedit->setZ(int(zselect->maximum() - o->value()));
  }

  void setZ(unsigned int val);
  int getZ(void) { return sqedit->getZ(); }

  void cb_Sqedit(SquareEditor* o) { do_callback(this, user_data()); }

  int getReason(void) { return sqedit->getReason(); }

  bool getMouse(void) { return sqedit->getMouse(); }

  int getMouseX1(void) { return sqedit->getMouseX1(); }
  int getMouseY1(void) { return sqedit->getMouseY1(); }
  int getMouseX2(void) { return sqedit->getMouseX2(); }
  int getMouseY2(void) { return sqedit->getMouseY2(); }
  int getMouseZ(void) { return sqedit->getMouseZ(); }

  void setPuzzle(puzzle_c * puzzle, unsigned int num);

  void clearPuzzle(void) {
    sqedit->clearPuzzle();
  }

  void setColor(unsigned int num) {
    sqedit->setColor(num);
  }

  void deactivate(void) {
    sqedit->deactivate();
  }

  void activate(void) {
    sqedit->activate();
  }

  void editSymmetries(int syms) {
    sqedit->setTool(syms);
  }

  void editChoice(SquareEditor::enTask c) {
    sqedit->setTask(c);
  }

  void editType(int type) {
    sqedit->setEditType(type);
  }
};

// the transform group
class TransformButtons : public Fl_Group {

public:

  TransformButtons(int x, int y, int w, int h);

  void cb_Press(long button) { do_callback(this, button); }
};

// the change size group
class ChangeSize : public Fl_Group {

  Fl_Roller* SizeX;
  Fl_Roller* SizeY;
  Fl_Roller* SizeZ;

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

  int getX(void) { return (int)SizeX->value(); }
  int getY(void) { return (int)SizeY->value(); }
  int getZ(void) { return (int)SizeZ->value(); }

  void setXYZ(long x, long y, long z);
};

// the class that contains the tool tab
class ToolTab : public Fl_Tabs {

  ChangeSize * changeSize;
  Fl_Check_Button * toAll;
  puzzle_c * puzzle;
  unsigned int shape;

public:

  ToolTab(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);

  bool operationToAll(void) { return toAll->value() != 0; }
};

class BlockListGroup : public Fl_Group {

  Fl_Slider * Slider;
  BlockList * List;
  int callbackReason;

public:

  BlockListGroup(int x, int y, int w, int h, BlockList * l);

  void cb_slider(void) { List->setShift((int)Slider->value()); }
  void cb_list(void);

  int getReason(void) { return callbackReason; }
};

class ConstraintsGroup : public Fl_Group {

  Fl_Slider * Slider;
  ColorConstraintsEdit * List;
  int callbackReason;

public:

  ConstraintsGroup(int x, int y, int w, int h, ColorConstraintsEdit * l);

  void cb_slider(void) { List->setShift((int)Slider->value()); }
  void cb_list(void);

  int getReason(void) { return callbackReason; }
};

// the groups with the 3d view and the zoom slider
class View3dGroup : public Fl_Group {

  VoxelDrawer * View3D;
  Fl_Slider * slider;

public:

  View3dGroup(int x, int y, int w, int h);

  void cb_slider(void);

  void showNothing(void) { View3D->clearSpaces(); }
  void showSingleShape(const puzzle_c * puz, unsigned int shapeNum);
  void showProblem(const puzzle_c * puz, unsigned int probNum, unsigned int selShape);
  void showAssembly(const puzzle_c * puz, unsigned int probNum, unsigned int solNum);
  void showPlacement(const puzzle_c * puz, unsigned int probNum, unsigned int piece, unsigned char trans, int x, int y, int z);
  void showAssemblerState(const puzzle_c * puz, unsigned int probNum, const assembly_c * assm) {
    View3D->showAssemblerState(puz, probNum, assm);
  }


  void updatePositions(PiecePositions *shifting);
  void updateVisibility(PieceVisibility * pcvis);
  void showColors(const puzzle_c * puz, bool show);

  void setMarker(int x1, int y1, int x2, int y2, int z, int type) { View3D->setMarker(x1, y1, x2, y2, z, type); }
  void hideMarker(void) { View3D->hideMarker(); }
  void useLightning(bool val) { View3D->useLightning(val); }

  double getZoom(void) { return slider->value(); }
  void setZoom(double v) { slider->value(v); cb_slider(); }

  void redraw(void) { View3D->redraw(); }

  VoxelView * getView(void) { return View3D; }
};

// a widget to separate 2 groups

class Separator : public Fl_Group {

public:

  Separator(int x, int y, int w, int h, const char * label, bool button);
};

// a group that can contain only buttons and one button is
// pressed while others are not
class ButtonGroup : public Fl_Group {

  unsigned int currentButton;

public:

  ButtonGroup(int x, int y, int w, int h);

  Fl_Button * addButton(int x, int y, int w, int h);

  void cb_Push(Fl_Button * btn);

  unsigned int getSelected(void) { return currentButton; }
  void select(int num);
};

class ResultViewer : public Fl_Box {

private:

  puzzle_c * puzzle;
  unsigned int problem;
  Fl_Color bg;

public:

  ResultViewer(int x, int y, int w, int h, puzzle_c * p);
  void setPuzzle(puzzle_c * p, unsigned int prob);
//  void setcontent(void);
  void draw(void);

};

// a status line containing text and a button to toggle
// between colored and normal view
class StatusLine : public Fl_Group {

private:

  Fl_Check_Button * colors;
  Fl_Box * text;

public:

  StatusLine(int x, int y, int w, int h);

  void setText(const char * t);
  bool useColors(void) { return colors->value() != 0; }
  void callback(Fl_Callback* fkt, void * dat) { colors->callback(fkt, dat); }
};

// this window is used to display assert messages
class assertWindow : public Fl_Double_Window {

public:

  assertWindow(const char * text, assert_exception * a);

};

// a simple window containing a multi line input
class multiLineWindow : public Fl_Double_Window {

    Fl_Multiline_Input * inp;
    bool _saveChanges;

  public:
    multiLineWindow(const char * title, const char *label, const char *deflt = 0);

    const char * getText(void) { return inp->value(); }

    void hide(bool save) {
      _saveChanges = save;
      Fl_Double_Window::hide();
    }

    bool saveChanges(void) { return _saveChanges; }
};

class ProgressBar : public Fl_Progress {
  public:

    ProgressBar(int x, int y, int w, int h) : Fl_Progress(x, y, w, h) {}

    void draw(void);
};

#endif
