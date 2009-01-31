/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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
#include "multilinewindow.h"

#include "WindowWidgets.h"

#include <FL/Fl_Multiline_Input.H>

static void cb_mlWindowClose(Fl_Widget* /*o*/, void * v) { ((multiLineWindow_c*)v)->hide(true); }
static void cb_mlWindowAbort(Fl_Widget* /*o*/, void * v) { ((multiLineWindow_c*)v)->hide(false); }

#define SZ_MLWIN_X 400
#define SZ_MLWIN_Y 200
#define SZ_BUTTON_X2 50
#define SZ_GAP 5
#define SZ_BUTTON 20

multiLineWindow_c::multiLineWindow_c(const char * tit, const char *lab, const char *deflt) : Fl_Double_Window(SZ_MLWIN_X, SZ_MLWIN_Y) {

  label(tit);

  int w, h;
  w = h = 0;
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

const char * multiLineWindow_c::getText(void) { return inp->value(); }

