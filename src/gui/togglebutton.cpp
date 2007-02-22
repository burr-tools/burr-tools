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

#include "togglebutton.h"

static void cb_LToggleButton_stub(Fl_Widget* o, void* v) { ((LToggleButton_c*)o)->toggle(); }

LToggleButton_c::LToggleButton_c(int x, int y, int w, int h, Fl_Callback* cb, void * cb_para, long p) : Fl_Button(0, 0, 10, 10), layoutable_c(x, y, w, h) {
  callback = cb;
  callback_para = cb_para;
  para = p;
  Fl_Button::callback(cb_LToggleButton_stub);
  selection_color(fl_lighter(color()));
  clear_visible_focus();
}

