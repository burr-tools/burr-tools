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
#include "WindowWidgets.h"
#include "piececolor.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <FL/Fl_Pixmap.H>

// some tool widgets, that may be swapped out later into another file

static void cb_BlockListGroupSlider_stub(Fl_Widget* o, void* v) { ((LBlockListGroup_c*)(o->parent()))->cb_slider(); }
void LBlockListGroup_c::cb_list(void) {

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

static void cb_BlockListGroupList_stub(Fl_Widget* o, void* v) { ((LBlockListGroup_c*)(o->parent()))->cb_list(); }

LBlockListGroup_c::LBlockListGroup_c(int x, int y, int w, int h, BlockList * l) : Fl_Group(0, 0, 100, 100), layoutable_c(x, y, w, h), List(l) {

  box(FL_DOWN_FRAME);

  x = y = 0;
  w = h = 100;
  x++; y++; w-=2; h-=2;

  // important the list must be added first to be drawn first
  // this is necessary to find out the size of the list and update the
  // slider accordingly
  add(List);

  Slider = new Fl_Slider(x+w-15-1, y-1, 15+2, h+2);
  Slider->maximum(0);
  Slider->callback(cb_BlockListGroupSlider_stub);
  Slider->clear_visible_focus();

  w-=15;

  new Fl_Box(FL_UP_FRAME, x, y, w, h, 0);
  x++; y++; w-=2; h-=2;

  List->resize(x, y, w, h);
  List->callback(cb_BlockListGroupList_stub);

  resizable(List);
  end();
}

static void cb_ConstraintsGroupSlider_stub(Fl_Widget* o, void* v) { ((LConstraintsGroup_c*)(o->parent()))->cb_slider(); }
static void cb_ConstraintsGroupList_stub(Fl_Widget* o, void* v) { ((LConstraintsGroup_c*)(o->parent()))->cb_list(); }
void LConstraintsGroup_c::cb_list(void) {

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

LConstraintsGroup_c::LConstraintsGroup_c(int x, int y, int w, int h, ColorConstraintsEdit * l) : Fl_Group(0, 0, 100, 100), layoutable_c(x, y, w, h), List(l) {

  box(FL_DOWN_FRAME);

  x = y = 0;
  w = h = 100;
  x++; y++; w-=2; h-=2;

  add(List);

  Slider = new Fl_Slider(x+w-15-1, y-1, 15+2, h+2);
  Slider->maximum(0);
  Slider->callback(cb_ConstraintsGroupSlider_stub);
  Slider->clear_visible_focus();

  w-=15;

  new Fl_Box(FL_UP_FRAME, x, y, w, h, 0);
  x++; y++; w-=2; h-=2;

  List->resize(x, y, w, h);
  List->callback(cb_ConstraintsGroupList_stub);

  resizable(List);
  end();
}

static void cb_ButtonGroup_stub(Fl_Widget* o, void* v) { ((ButtonGroup*)v)->cb_Push((Fl_Button*)o); }

ButtonGroup::ButtonGroup(int x, int y, int w, int h) : layouter_c(x, y, w, h), currentButton(0) {
  end();
}

Fl_Button * ButtonGroup::addButton(void) {

  int c = children();

  LFl_Button * b = new LFl_Button(0, c, 0, 1, 1);
  b->selection_color(fl_lighter(color()));
  b->clear_visible_focus();
  b->setPadding(2, 0);

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

