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
#ifndef __PLACEMENT_BROWSER_H__
#define __PLACEMENT_BROWSER_H__

#include "WindowWidgets.h"

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Value_Slider.H>

class puzzle_c;

class PlacementBrowser : public Fl_Double_Window {

  View3dGroup *view3d;

  Fl_Value_Slider * pieceSelector;
  Fl_Value_Slider * placementSelector;

  puzzle_c * puzzle;
  unsigned int problem;

  unsigned int node;
  unsigned int placement;

public:

  PlacementBrowser(puzzle_c * p, unsigned int prob);

  int handle(int event);

  void cb_piece(Fl_Value_Slider* o);
  void cb_placement(Fl_Value_Slider* o);

};

#endif
