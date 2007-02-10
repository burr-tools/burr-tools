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

/* often requested here it is: export solutions, just the assembly or the disassembly animation
 * into a (set of) image files (png files)
 */

/* following parameters can be chosen:
 * pixel size of the images
 * for animations: how many files are to be saved
 * the rotation for each step
 * file name, number are appended for multiple images
 */

/* fixed parameters (for now):
 * transparent background
 * 3x3 antialiasing
 * block justification of the images, except for the last line
 */

/* the images are measured and then sized so, that they do fit onto the
 * requested number of pages
 * then they are taken at the requires size times 3
 * smoothly downscaled and put onto the page
 * add a line of text explaining what is currently done
 */

#include "Layouter.h"
#include "WindowWidgets.h"
#include "voxeldrawer.h"

#include <vector>

class LView3dGroup;
class LBlockListGroup;
class ImageInfo;
class image_c;
class puzzle_c;
class guiGridType_c;

class imageExport_c : public LFl_Double_Window, public VoxelViewCallbacks {

  private:

    /* the puzzle that is going to be exported */
    puzzle_c * puzzle;

    /* The different window elements */
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
    LFl_Button *BtnStart, *BtnAbbort;
    PieceSelector * ShapeSelect;
    ProblemSelector * ProblemSelect;

    /* true, when there is an export in execution */
    bool working;

    /* this vector is set up at the beginning of an export with
     * all the images that need to be in the target image
     */
    std::vector<ImageInfo*> images;

    /* some internal variables for the image export */
    unsigned int state;        /* what is currently done, 0: preview, 1: export */
    image_c *i;                  /* current page that is worked on */
    unsigned int curWidth;     /* how much of the current line is filled */
    unsigned int curLine;      /* current line number */
    unsigned int curPage;      /* number of the current page */
    unsigned int im;           /* number of the image int images that is worked on */
    unsigned int linesPerPage; /* how many lines must be on a page to fit everything on it */

    void nextImage(bool finish);

  public:

    imageExport_c(puzzle_c * p, const guiGridType_c * ggt);

    /* returns true, when there is currently a image export in progress */
    bool isWorking(void) { return working; }

    /* this must be called cyclically, this updates the buttons activation
     * status and also it sets up a new redraw cycle, and thus getting the next tile
     * so when isWorking returns true, call as fast as possible, otherwise call
     * from time to time
     */
    void update(void);

    void cb_Abort(void);
    void cb_Export(void);
    void cb_Update3DView(void);
    void cb_SzUpdate(void);

    /* the 2 functions that do the export are stored inside the callback of the voxelView */
    virtual bool PreDraw(void);
    virtual void PostDraw(void);
};
