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

/* stlexport outputs the puzzle shape in stl format
 * for 3d-printing the puzzle
 */

/* following parameters can be chosen:
 * cube size in mm
 * bevel (amount of mm to round the edges of cubes)
 * offset (in mm - tolerance of the 3d printer,
 *         shrinks all solids by that amount)
 * file name
 */
#ifndef __STL_EXPORT_H__
#define __STL_EXPORT_H__

#include "Layouter.h"

class LView3dGroup;
class LBlockListGroup;
class puzzle_c;
class guiGridType_c;
class PieceSelector;

class stlExport_c : public LFl_Double_Window {

  private:

    /* the puzzle that is going to be exported */
    puzzle_c * puzzle;

    /* The different window elements */
    LView3dGroup *view3D;

    LFl_Float_Input *Bevel, *Offset, *CubeSize;
    LFl_Input *Fname, *Pname;
    LFl_Box *status;
    LFl_Button *BtnStart, *BtnAbbort;
    PieceSelector * ShapeSelect;
    LFl_Radio_Button *ExpShape;

  public:

    stlExport_c(puzzle_c * p, const guiGridType_c * ggt);

    void cb_Export(void);
    void cb_Abort(void);
    void cb_Update3DView(void);
    void exportSTL(int shape);
};

#endif
