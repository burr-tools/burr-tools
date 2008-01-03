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
#ifndef __STATUS_WINDOW__
#define __STATUS_WINDOW__

#include "Layouter.h"

#include <vector>

class puzzle_c;

class statusWindow_c : public LFl_Double_Window {

  private:

    puzzle_c * puz;

    std::vector<LFl_Check_Button*> selection;
    bool again;

  public:

    statusWindow_c(puzzle_c * p);

    void cb_removeSelected(void);

    bool getAgain(void) { return again; }

};

#endif
