#include "guigridtype.h"

#include "SquareEditor.h"
#include "VoxelDrawer.h"

#include "../lib/gridtype.h"

guiGridType_c::guiGridType_c(gridType_c * g) : gt(g) { }

SquareEditor * guiGridType_c::getGridEditor(int x, int y, int w, int h, puzzle_c * puzzle) {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return new SquareEditor(x, y, w, h, puzzle);
  }

  return 0;
}

VoxelDrawer * guiGridType_c::getVoxelDrawer(int x, int y, int w, int h) {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return new VoxelDrawer(x, y, w, h);
  }

  return 0;
}

/* returns a group to edit the parameters for this grid type
 * is is used in the new puzzle grid selection dialog
 * and also in the later possible grid parameters dialog
 */
LFl_Group * guiGridType_c::getCofigurationDialog(void) {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return 0;
  }

  return 0;
}

/* return icon and text for the current grid type */
char * guiGridType_c::getIcon(void) {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return 0;
  }

  return 0;
}

const char * guiGridType_c::getName(void) {
  switch(gt->getType()) {
    case gridType_c::GT_BRICKS: return "Brick";
  }

  return 0;
}

