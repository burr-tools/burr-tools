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


#ifndef __WINDOW_WIDGETS_H__
#define __WINDOW_WIDGETS_H__

#include "SquareEditor.h"
#include "VoxelView.h"
#include "BlockList.h"

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Light_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Roller.H>
#include <FL/Fl_Value_Output.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Output.H>
#include <FL/fl_draw.h>

// my button, the only change it that the box is automatically set to engraved
class FlatButton : public Fl_Button {

public:
  FlatButton(int x, int y, int w, int h, const char * txt, const char * tt) : Fl_Button(x, y, w, h, txt) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
  }

  FlatButton(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback* cb) : Fl_Button(x, y, w, h, txt) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
    callback(cb);
  }

  FlatButton(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback1* cb, long cb_para) : Fl_Button(x, y, w, h, txt) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
    callback(cb, cb_para);
  }

  FlatButton(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback* cb, void * cb_para) : Fl_Button(x, y, w, h, txt) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
    callback(cb, cb_para);
  }

  FlatButton(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback1* cb, long cb_para, int col) : Fl_Button(x, y, w, h, txt) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
    callback(cb, cb_para);
    color((Fl_Color)col);
  }
};

class FlatLightButton : public Fl_Light_Button {

public:
  FlatLightButton(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback1* cb, long cb_para) : Fl_Light_Button(x, y, w, h, txt) {
    box(FL_THIN_UP_BOX);
    tooltip(tt);
    callback(cb, cb_para);
  }
};


// the group for the square editor including the colord marker and the slider for the z axis
class VoxelEditGroup : public Fl_Group {

  SquareEditor * sqedit;
  Fl_Slider * zselect;

public:

  VoxelEditGroup(int x, int y, int w, int h, puzzle_c * puzzle);

  void cb_Zselect(Fl_Slider* o) {
    sqedit->setZ(int(o->value()));
  }

  void setZ(unsigned int val);

  void cb_Sqedit(SquareEditor* o) { do_callback(); }

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

  void lock(bool lock) {
    sqedit->lock(lock);
  }

  void editAllLayers(bool edit) {
    sqedit->editAllLayers(edit);
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

  Fl_Value_Output* SizeOutX;
  Fl_Value_Output* SizeOutY;
  Fl_Value_Output* SizeOutZ;

public:

  ChangeSize(int w, int y, int w, int h);

  void cb_roll(long dir) {
    switch (dir) {
    case 0: SizeOutX->value(int(SizeX->value())); break;
    case 1: SizeOutY->value(int(SizeY->value())); break;
    case 2: SizeOutZ->value(int(SizeZ->value())); break;
    }

    do_callback();
  }

  int getX(void) { return (int)SizeX->value(); }
  int getY(void) { return (int)SizeY->value(); }
  int getZ(void) { return (int)SizeZ->value(); }

  void setXYZ(long x, long y, long z) {
    SizeX->value(x);
    SizeY->value(y);
    SizeZ->value(z);
    SizeOutX->value(x);
    SizeOutY->value(y);
    SizeOutZ->value(z);
  }
};

// the class that contains the tool tab
class ToolTab : public Fl_Tabs {

  ChangeSize * changeSize;
  pieceVoxel_c * space;
  bool _editLayersChanged;
  Fl_Light_Button *editLayersButton;

public:

  ToolTab(int x, int y, int w, int h);

  void setVoxelSpace(pieceVoxel_c * sp) {
    space = sp;
    if (space)
      changeSize->setXYZ(sp->getX(), sp->getY(), sp->getZ());
    else
      changeSize->setXYZ(0, 0, 0);
  }

  void cb_size(void) {
    if (space) {
      space->resize(changeSize->getX(), changeSize->getY(), changeSize->getZ(), 0);
      do_callback();
    }
  }

  void cb_transform(long task) {
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
      case 17: _editLayersChanged = true; break;
      }

      do_callback();
    }
  }

  bool editLayersChanged(void) { return _editLayersChanged; };
  bool editLayersOn(void) { return editLayersButton->value() == 1; }
};


class BlockListGroup : public Fl_Group {

  Fl_Slider * Slider;
  BlockList * List;

public:

  BlockListGroup(int x, int y, int w, int h, BlockList * l);

  void cb_slider(void) { List->setShift((int)Slider->value()); }
  void cb_list(void);
};

class ConstraintsGroup : public Fl_Group {

  Fl_Slider * Slider;
  ColorConstraintsEdit * List;

public:

  ConstraintsGroup(int x, int y, int w, int h, ColorConstraintsEdit * l);

  void cb_slider(void) { List->setShift((int)Slider->value()); }
  void cb_list(void);
};


// the groups with the 3d view and the zoom slider
class View3dGroup : public Fl_Group {

  VoxelView * View3D;
  Fl_Slider * slider;

public:

  View3dGroup(int x, int y, int w, int h);

  void cb_slider(void);

  void showNothing(void) { View3D->clearSpaces(); }
  void showSingleShape(const puzzle_c * puz, unsigned int shapeNum, bool showColors);
  void showProblem(const puzzle_c * puz, unsigned int probNum, unsigned int selShape, bool showColors);
  void showAssembly(const puzzle_c * puz, unsigned int probNum, unsigned int solNum, bool showColors);
  void updatePositions(PiecePositions *shifting);
  void updateVisibility(PieceVisibility * pcvis);
  void showColors(const puzzle_c * puz, bool show);

  void setMarker(int x1, int y1, int x2, int y2, int z) { View3D->setMarker(x1, y1, x2, y2, z); }
  void hideMarker(void) { View3D->hideMarker(); }
  void useLightning(bool val) { View3D->useLightning(val); }

  void redraw(void) { View3D->redraw(); }
};

// a widget to separate 2 groups

class Separator : public Fl_Group {

public:

  Separator(int x, int y, int w, int h, const char * label, bool button);
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
  Fl_Output * text;

public:

  StatusLine(int x, int y, int w, int h);

  void setText(const char * t) { text->value(t); }
  bool useColors(void) { return colors->value() != 0; }
  void callback(Fl_Callback* fkt) { colors->callback(fkt); }
};

#endif
