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

#ifndef __GRID_TYPE_GUI_H__
#define __GRID_TYPE_GUI_H__

#include "Layouter.h"

class gridType_c;
class guiGridType_c;

/* this is a fltk group that contains all elements to change the
 * parameters that a certain gridtype can have
 *
 * in fact this is only the base class, for each gridtype there will be
 * an inherited class that contains the elements
 */
class gridTypeGui_c : public layouter_c {
};

/* the required elements for the brick grid type */
class gridTypeGui_0_c : public gridTypeGui_c {

  public:

    gridTypeGui_0_c(int x, int y, int w, int h, gridType_c * gt);
};

class gridTypeGui_1_c : public gridTypeGui_c {

  public:

    gridTypeGui_1_c(int x, int y, int w, int h, gridType_c * gt);
};

class gridTypeGui_2_c : public gridTypeGui_c {

  public:

    gridTypeGui_2_c(int x, int y, int w, int h, gridType_c * gt);
};

/* this window allows you to edit the parameters of one
 * grid type
 */
class gridTypeParameterWindow_c : public LFl_Double_Window {

  public:

    /* creates the parameter window for the given gui grid type */
    gridTypeParameterWindow_c(guiGridType_c * ggt);
};

/* this class is used in the window below to hold all data necessary
 * for one grid type
 */
class gridTypeInfos_c;

/* this window allows you to select the grid type and
 * the parameters for the selected type
 */

class gridTypeSelectorWindow_c : public LFl_Double_Window {

  private:

    std::vector<gridTypeInfos_c*> gti;

    /* currently selected grid type from the vector above */
    unsigned int current;

  public:

    gridTypeSelectorWindow_c(void);
    ~gridTypeSelectorWindow_c(void);

    /* after the window has been close you can get the created grid type with this function
     */
    gridType_c * getGridType(void);


    void select_cb(void);
};

#endif
