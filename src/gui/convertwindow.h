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
#ifndef __CONVERTWINDOW_H__
#define __CONVERTWINDOW_H__

#include "Layouter.h"

#include "../lib/gridtype.h"

#include <vector>

/* The window that asks the user to which new gridtype the current puzzle should
 * be converted. The list is dynamically created depending on the
 * canConvert function returns
 *
 * okSelected returns true, when the user pressed the continue button, false in all other cases
 * the selected grid can be aquired with getTargetType
 */
class convertWindow_c : public LFl_Double_Window {

  private:

    bool _ok;
    unsigned int current;
    std::vector<gridType_c::gridType> gridTypes;
    std::vector<LFl_Radio_Button*> gti;

  public:

    convertWindow_c(gridType_c::gridType srcType);

    gridType_c::gridType getTargetType(void);

    bool okSelected(void) { return _ok; }

    void select_cb(void);
    void okay_cb(void);

};

#endif
