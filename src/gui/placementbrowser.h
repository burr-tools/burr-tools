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
#ifndef __PLACEMENT_BROWSER_H__
#define __PLACEMENT_BROWSER_H__

#include "Layouter.h"

class problem_c;

class LView3dGroup;

class placementBrowser_c : public LFl_Double_Window {

  LView3dGroup *view3d;

  LFl_Value_Slider * pieceSelector;
  LFl_Value_Slider * placementSelector;

  problem_c * puzzle;

  unsigned int node;
  unsigned int placement;

public:

  placementBrowser_c(problem_c * p);

  int handle(int event);

  void cb_piece(void);
  void cb_placement(Fl_Value_Slider* o);

};

#endif
