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
#ifndef __WINDOW_WIDGETS_H__
#define __WINDOW_WIDGETS_H__

#include "grideditor.h"
#include "voxeldrawer.h"
#include "BlockList.h"
#include "Images.h"
#include "Layouter.h"

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
#include <FL/fl_draw.H>

class guiGridType_c;
class piecePositions_c;

// my button, the only change it that the box is automatically set to engraved
class FlatButton : public Fl_Button {

public:

  FlatButton(int x, int y, int w, int h, const char * txt, const char * tt) : Fl_Button(x, y, w, h, txt) {
    tooltip(tt);
    clear_visible_focus();
  }

  FlatButton(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback* cb, void * cb_para) : Fl_Button(x, y, w, h, txt) {
    tooltip(tt);
    callback(cb, cb_para);
    clear_visible_focus();
  }

  FlatButton(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback1* cb, long cb_para) : Fl_Button(x, y, w, h, txt) {
    tooltip(tt);
    callback(cb, cb_para);
    clear_visible_focus();
  }

  FlatButton(int x, int y, int w, int h, Fl_Image * img, Fl_Image * inact, const char * tt, Fl_Callback1* cb, long cb_para) : Fl_Button(x, y, w, h) {
    tooltip(tt);
    callback(cb, cb_para);
    image(img);
    deimage(inact);
    clear_visible_focus();
  }
};

class LFlatButton_c : public FlatButton, public layoutable_c {

public:

  LFlatButton_c(int x, int y, int w, int h, const char * txt, const char * tt) : FlatButton(0, 0, 0, 0, txt, tt), layoutable_c(x, y, w, h) {
  }

  LFlatButton_c(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback* cb, void * cb_para) : FlatButton(0, 0, 0, 0, txt, tt, cb, cb_para), layoutable_c(x, y, w, h) {
  }

  LFlatButton_c(int x, int y, int w, int h, const char * txt, const char * tt, Fl_Callback1* cb, long cb_para) : FlatButton(0, 0, 0, 0, txt, tt, cb, cb_para), layoutable_c(x, y, w, h) {
  }

  LFlatButton_c(int x, int y, int w, int h, Fl_Image * img, Fl_Image * inact, const char * tt, Fl_Callback1* cb, long cb_para) : FlatButton(0, 0, 0, 0, img, inact, tt, cb, cb_para), layoutable_c(x, y, w, h) {
  }

  virtual void getMinSize(int *width, int *height) const {
    *width = 0;
    ((LFlatButton_c*)this)->measure_label(*width, *height);
    *width += 4;
    *height += 4;
  }
};

class LBlockListGroup_c : public Fl_Group, public layoutable_c {

  Fl_Slider * Slider;
  BlockList * List;
  int callbackReason;

  public:

  LBlockListGroup_c(int x, int y, int w, int h, BlockList * l);

  void cb_slider(void) { List->setShift((int)Slider->value()); }
  void cb_list(void);

  int getReason(void) { return callbackReason; }

  virtual void getMinSize(int *width, int *height) const {
    *width = 30;
    *height = 20;
  }
};

class LConstraintsGroup_c : public Fl_Group, public layoutable_c {

  Fl_Slider * Slider;
  ColorConstraintsEdit * List;
  int callbackReason;

  public:

  LConstraintsGroup_c(int x, int y, int w, int h, ColorConstraintsEdit * l);

  void cb_slider(void) { List->setShift((int)Slider->value()); }
  void cb_list(void);

  int getReason(void) { return callbackReason; }

  virtual void getMinSize(int *width, int *height) const {
    *width = 30;
    *height = 20;
  }

};

// a widget to separate 2 groups
class LSeparator_c : public Fl_Group, public layoutable_c  {

public:

  LSeparator_c(int x, int y, int w, int h, const char * label, bool button);

  virtual void getMinSize(int *width, int *height) const {
    *width = 10;
    *height = 10;
  }
};

// a group that can contain only buttons and one button is
// pressed while others are not
class ButtonGroup : public layouter_c {

  unsigned int currentButton;

public:

  ButtonGroup(int x, int y, int w, int h);

  Fl_Button * addButton(void);

  void cb_Push(Fl_Button * btn);

  unsigned int getSelected(void) const { return currentButton; }
  void select(int num);
};

class ResultViewer : public Fl_Box, public layoutable_c {

private:

  puzzle_c * puzzle;
  unsigned int problem;
  Fl_Color bg;

public:

  ResultViewer(int x, int y, int w, int h, puzzle_c * p);
  void setPuzzle(puzzle_c * p, unsigned int prob);
//  void setcontent(void);
  void draw(void);

  virtual void getMinSize(int *width, int *height) const {
    *width = 4;
    *height = 4;
  }

};

// a status line containing text and a button to toggle
// between coloured and normal view
class LStatusLine : public layouter_c {

private:

  ButtonGroup *mode;
  LFl_Box * text;
  pixmapList_c pm;

public:

  LStatusLine(int x, int y, int w, int h);

  void setText(const char * t);
  voxelDrawer_c::colorMode getColorMode(void) const {
    return mode->getSelected()==0
      ?voxelDrawer_c::pieceColor
      :(mode->getSelected()==1
          ?voxelDrawer_c::paletteColor
          :voxelDrawer_c::anaglyphColor);
  }
  void callback(Fl_Callback* fkt, void * dat) { mode->callback(fkt, dat); }

  virtual void getMinSize(int *width, int *height) const {
    *width = 30;
    *height = 25;
  }
};

#endif
