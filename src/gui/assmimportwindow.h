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
#ifndef __ASSMIMPORT_WINDOW_H__
#define __ASSMIMPORT_WINDOW_H__

#include "Layouter.h"

class puzzle_c;
class ProblemSelector;
class Fl_Round_Button;
class Fl_Check_Button;
class Fl_Int_Input;

/* The window that asks the user to which new gridtype the current puzzle should
 * be converted. The list is dynamically created depending on the
 * canConvert function returns
 *
 * okSelected returns true, when the user pressed the continue button, false in all other cases
 * the selected grid can be aquired with getTargetType
 */
class assmImportWindow_c : public LFl_Double_Window {

  private:

    bool _ok;

    ProblemSelector *problemSelectorSrc, *problemSelectorDst;
    Fl_Round_Button *rdDontAdd, *rdAddNew, *rdAddDst;
    Fl_Check_Button *ckDrpDisconnected, *ckDrpMirror, *ckDrpSymm, *ckDrpMillable, *ckDrpNotchable, *ckDrpIdentical;
    Fl_Int_Input *min, *max, *shapeMin, *shapeMax;

  public:

    assmImportWindow_c(const puzzle_c * puzzle);

    bool okSelected(void) { return _ok; }

    void okay_cb(void);

    int getSrcProblem(void);
    int getDstProblem(void);

    enum
    {
      A_DONT_ADD,
      A_ADD_NEW,
      A_ADD_DST
    } ;

    int getAction(void);

    static const int dropDisconnected = 0x01;
    static const int dropMirror       = 0x02;
    static const int dropSymmetric    = 0x04;
    static const int dropNonMillable  = 0x08;
    static const int dropNonNotchable = 0x10;
    static const int dropIdentical    = 0x20;

    unsigned int getFilter(void);

    unsigned int getMin(void);
    unsigned int getMax(void);

    unsigned int getShapeMin(void);
    unsigned int getShapeMax(void);
};

#endif

