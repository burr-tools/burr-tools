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
#ifndef __CONSTRAINTS_GROUP_H__
#define __CONSTRAINTS_GROUP_H__

#include "Layouter.h"

#include <FL/Fl_Group.H>

class ColorConstraintsEdit;

class LConstraintsGroup_c : public Fl_Group, public layoutable_c {

  Fl_Slider * Slider;
  ColorConstraintsEdit * List;
  int callbackReason;

  public:

  LConstraintsGroup_c(int x, int y, int w, int h, ColorConstraintsEdit * l);

  void cb_slider(void);
  void cb_list(void);

  int getReason(void) { return callbackReason; }

  virtual void getMinSize(int *width, int *height) const {
    *width = 30;
    *height = 20;
  }
};

#endif
