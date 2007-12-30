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
#ifndef __GUI_GRID_TYPE__
#define __GUI_GRID_TYPE__

#include "../lib/gridtype.h"

class gridEditor_c;
class voxelDrawer_c;
class gridTypeGui_c;
class ToolTab;

class guiGridType_c {

  private:

    gridType_c * gt;

  public:

    guiGridType_c(gridType_c * gt);

    gridEditor_c * getGridEditor(int x, int y, int w, int h, puzzle_c * puzzle) const;
    voxelDrawer_c * getVoxelDrawer(void) const;

    /* returns a group to edit the parameters for this grid type
     * is is used in the new puzzle grid selection dialogue
     * and also in the later possible grid parameters dialogue
     */
    gridTypeGui_c * getConfigurationDialog(int x, int y, int w, int h);

    /* return icon and text for the current grid type */
    char * getIcon(void) const;
    const char * getName(void) const;

    const gridType_c * getGridType(void) const { return gt; }

    /* return the tool tab for the current grid type */
    ToolTab * getToolTab(int x, int y, int w, int h) const;

    /* return the default size a new shape should have when adding it to puzzle */
    unsigned int defaultSize(void) const;
};

#endif
