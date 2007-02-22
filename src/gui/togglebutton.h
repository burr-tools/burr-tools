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
#ifndef __TOGGLE_BUTTON_H__
#define __TOGGLE_BUTTON_H__

#include "Layouter.h"

#include <FL/Fl_Button.H>

/* this is a button that toggles between a pressed and a released
 * state by clicking with the mouse onto it
 */
class LToggleButton_c : public Fl_Button, public layoutable_c {

  Fl_Callback *callback;
  void * callback_para;
  long para;

  public:
    LToggleButton_c(int x, int y, int w, int h, Fl_Callback *cb, void * cb_para, long para);

    void toggle(void) {
      value(1-value());
      if (callback)
        callback(this, callback_para);
    }

    long ButtonVal(void) { return para; }

    virtual void getMinSize(int *width, int *height) const {
      *width = 0;
      const_cast<LToggleButton_c*>(this)->measure_label(*width, *height);
      *width += 4;
      *height += 4;
    }
};

#endif
