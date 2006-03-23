#include "guigridtype.h"

#include "grideditor_0.h"
#include "VoxelDrawer.h"

#include "../lib/gridtype.h"

guiGridType_c::guiGridType_c(gridType_c * g) : gt(g) { }

gridEditor_c * guiGridType_c::getGridEditor(int x, int y, int w, int h, puzzle_c * puzzle) const {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return new gridEditor_0_c(x, y, w, h, puzzle);
  }

  return 0;
}

VoxelDrawer * guiGridType_c::getVoxelDrawer(int x, int y, int w, int h) const {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return new VoxelDrawer(x, y, w, h);
  }

  return 0;
}

/* returns a group to edit the parameters for this grid type
 * is is used in the new puzzle grid selection dialog
 * and also in the later possible grid parameters dialog
 */
LFl_Group * guiGridType_c::getCofigurationDialog(void) const {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return 0;
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

