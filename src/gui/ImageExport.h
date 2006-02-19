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



/* often requeste here it is: export solutions, just the assembly or the disassembly animation
 * into a (set of) image files (png files)
 */

/* folowing parameters can be chosen:
 * pixel size of the images
 * for animations: how many files are to be saved
 * the rotation for each step
 * filename, number are appended for multiple images
 */

/* fixed parameters (for now):
 * transparent background
 * 3x3 antialiasing
 * block justification of the images, except for the last line
 */

/* the images are measured and then sizes so, that thes do fit onto the
 * requested number of pages
 * then they are taken at the requires size times 3
 * smoothly downscaled and put onto the page
 * add a line of text explaining what is currently done
 */

#include "Layouter.h"
#include "WindowWidgets.h"

#include "../lib/puzzle.h"

class LView3dGroup : public View3dGroup, public layoutable_c {

  public:

    LView3dGroup(int x, int y, int w, int h) : View3dGroup(0, 0, 50, 50), layoutable_c(x, y, w, h) {}

    virtual void getMinSize(int * w, int *h) const {
      *w = 400;
      *h = 400;
    }
};

class LBlockListGroup : public BlockListGroup, public layoutable_c {
  public:
    LBlockListGroup(int x, int y, int w, int h, BlockList * l) : BlockListGroup(0, 0, 50, 50, l), layoutable_c(x, y, w, h) {}

    virtual void getMinSize(int *w, int *h) const {
      *w = 100;
      *h = 60;
    }
};

class ImageExportWindow : public LFl_Double_Window {

  private:

    puzzle_c * puzzle;
    LView3dGroup *view3D;

    LFl_Int_Input *SizePixelX, *SizePixelY;
    LFl_Radio_Button *AA1, *AA2, *AA3, *AA4, *AA5;
    LFl_Radio_Button *BgWhite, *BgTransp;
    LFl_Radio_Button *ColPiece, *ColConst;
    LFl_Radio_Button *SzA4Port, *SzA4Land, *SzLetterPort, *SzLetterLand, *SzManual;
    LFl_Input *SzDPI, *SzX, *SzY;
    LFl_Input *Fname, *Pname;
    LFl_Int_Input *NumPages;
    LFl_Box *status;
    LFl_Radio_Button *ExpShape, *ExpProblem, *ExpAssembly, *ExpSolution;
    LFl_Check_Button *DimStatic;
    PieceSelector * ShapeSelect;
    ProblemSelector * ProblemSelect;

  public:

    ImageExportWindow(puzzle_c * p);

    void cb_Abort(void);
    void cb_Export(void);
    void cb_Update3DView(void);
    void cb_SzUpdate(void);

};
