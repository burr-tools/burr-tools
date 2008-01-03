/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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
#include "buttongroup.h"

static void cb_ButtonGroup_stub(Fl_Widget* o, void* v) { ((ButtonGroup_c*)v)->cb_Push((Fl_Button*)o); }

ButtonGroup_c::ButtonGroup_c(int x, int y, int w, int h) : layouter_c(x, y, w, h), currentButton(0) {
  end();
}

LFl_Button * ButtonGroup_c::addButton(void) {

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

void ButtonGroup_c::cb_Push(Fl_Button * btn) {

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

void ButtonGroup_c::select(int num) {
  if (num < children())
    cb_Push((Fl_Button*)array()[num]);
}

