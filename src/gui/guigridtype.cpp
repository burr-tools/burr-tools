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
#include "guigridtype.h"

#include "grideditor_0.h"
#include "voxeldrawer_0.h"
#include "gridtypegui.h"

#include "../lib/gridtype.h"

guiGridType_c::guiGridType_c(gridType_c * g) : gt(g) { }

gridEditor_c * guiGridType_c::getGridEditor(int x, int y, int w, int h, puzzle_c * puzzle) const {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return new gridEditor_0_c(x, y, w, h, puzzle);
  }

  return 0;
}

voxelDrawer_c * guiGridType_c::getVoxelDrawer(int x, int y, int w, int h) const {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return new voxelDrawer_0_c(x, y, w, h);
  }

  return 0;
}

/* returns a group to edit the parameters for this grid type
 * is is used in the new puzzle grid selection dialog
 * and also in the later possible grid parameters dialog
 */
gridTypeGui_c * guiGridType_c::getCofigurationDialog(int x, int y, int w, int h) {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return new gridTypeGui_0_c(x, y, w, h, gt);
  }

  return 0;
}

/* return icon and text for the current grid type */
char * guiGridType_c::getIcon(void) const {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return 0;
  }

  return 0;
}

const char * guiGridType_c::getName(void) const {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return "Brick";
  }

  return 0;
}

