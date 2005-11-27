/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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
      *w = 150;
      *h = 150;
    }
};

class ImageExportWindow : public LFl_Double_Window {

  private:

    puzzle_c * puzzle;
    LView3dGroup *view3D;

  public:

    ImageExportWindow(puzzle_c * p);

    void cb_Abort(void);
    void cb_Export(void);

};
