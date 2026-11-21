/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
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
#include "grideditor_2.h"
#include "grideditor_3.h"
#include "grideditor_4.h"
#include "grideditor_layers.h"
#include "gridtypegui.h"
#include "tooltabs.h"

#include "../lib/gridtype.h"

guiGridType_c::guiGridType_c(gridType_c * g) : gt(g) { }

gridEditor_c * guiGridType_c::getGridEditor(int x, int y, int w, int h, puzzle_c * puzzle) const {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return new gridEditor_0_c(x, y, w, h, puzzle);
    case gridType_c::GT_SPHERES: return new gridEditor_2_c(x, y, w, h, puzzle);
    case gridType_c::GT_RHOMBIC: return new gridEditor_3_c(x, y, w, h, puzzle);
    case gridType_c::GT_TETRA_OCTA: return new gridEditor_4_c(x, y, w, h, puzzle);
    default:
      // the described grids (NG's): one editor for all, from their layers
      if (gt->isDescribed()) return new layerGridEditor_c(x, y, w, h, puzzle);
      return 0;
  }
}

/* returns a group to edit the parameters for this grid type
 * is is used in the new puzzle grid selection dialogue
 * and also in the later possible grid parameters dialogue
 */
gridTypeGui_c * guiGridType_c::getConfigurationDialog(int x, int y, int w, int h) {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return new gridTypeGui_0_c(x, y, w, h, gt);
    case gridType_c::GT_SPHERES: return new gridTypeGui_2_c(x, y, w, h, gt);
    case gridType_c::GT_RHOMBIC: return new gridTypeGui_0_c(x, y, w, h, gt);
    case gridType_c::GT_TETRA_OCTA: return new gridTypeGui_0_c(x, y, w, h, gt);
    default:
      if (gt->isDescribed()) return new gridTypeGui_lattice_c(x, y, w, h, gt);
      return 0;
  }
}

/* return icon and text for the current grid type */
char * guiGridType_c::getIcon(void) const {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return 0;
    case gridType_c::GT_SPHERES: return 0;
    case gridType_c::GT_RHOMBIC: return 0;
    case gridType_c::GT_TETRA_OCTA: return 0;
    default: return 0;
  }
}

const char * guiGridType_c::getName(void) const {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return "Brick";
    case gridType_c::GT_TRIANGULAR_PRISM: return "Triangular Prism";
    case gridType_c::GT_SPHERES: return "Spheres";
    case gridType_c::GT_RHOMBIC: return "Rhombic Tetrahedra";
    case gridType_c::GT_TETRA_OCTA: return "Tetrahedra-Octahera";
    case gridType_c::GT_HEXAGONAL_PRISM: return "Hexagonal Prism";
    case gridType_c::GT_CAIRO_PRISM: return "Cairo Prism";
    case gridType_c::GT_RHOMBIC_TETRAHEDRA: return "Rhombic Tetrahedra (NG)";
    case gridType_c::GT_OCTAHEDRAL_TETRAHEDRA: return "Octahedral Tetrahedra";
    case gridType_c::GT_DESCRIBED_BRICKS: return "Brick (described)";
    case gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM: return "Triangular Prism";
    default: return 0;
  }
}

ToolTab * guiGridType_c::getToolTab(int x, int y, int w, int h) const {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return new ToolTab_0(x, y, w, h);
    case gridType_c::GT_SPHERES: return new ToolTab_2(x, y, w, h);
    case gridType_c::GT_RHOMBIC: return new ToolTab_3(x, y, w, h);
    case gridType_c::GT_TETRA_OCTA: return new ToolTab_4(x, y, w, h);
    default:
      // the described grids (NG's): one tab for all, from the grid's data
      if (gt->isDescribed()) return new ToolTab_lattice(x, y, w, h);
      return 0;
  }
}

unsigned int guiGridType_c::defaultSize(void) const {
  switch(gt->getType()) {
    case gridType_c::GT_RHOMBIC: return 5;
    case gridType_c::GT_SPHERES: return 3;
    default:                     return 6;
  }
}
