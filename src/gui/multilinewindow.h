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
#ifndef __MULTI_LINE_WINDOW_H__
#define __MULTI_LINE_WINDOW_H__

#include <FL/Fl_Double_Window.H>

class Fl_Multiline_Input;

// a simple window containing a multi line input
class multiLineWindow_c : public Fl_Double_Window {

    Fl_Multiline_Input * inp;
    bool _saveChanges;

  public:
    multiLineWindow_c(const char * title, const char *label, const char *deflt = 0);

    const char * getText(void);

    void hide(bool save) {
      _saveChanges = save;
      Fl_Double_Window::hide();
    }

    bool saveChanges(void) { return _saveChanges; }
};


#endif
