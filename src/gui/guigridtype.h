#ifndef __GUI_GRID_TYPE__
#define __GUI_GRID_TYPE__

#include "../lib/gridtype.h"

class gridEditor_c;
class VoxelDrawer;
class LFl_Group;

class guiGridType_c {

  private:

    gridType_c * gt;

  public:

    guiGridType_c(gridType_c * gt);

    gridEditor_c * getGridEditor(int x, int y, int w, int h, puzzle_c * puzzle) const;
    VoxelDrawer * getVoxelDrawer(int x, int y, int w, int h) const;

    /* returns a group to edit the parameters for this grid type
     * is is used in the new puzzle grid selection dialog
     * and also in the later possible grid parameters dialog
     */
    LFl_Group * getCofigurationDialog(void) const;

    /* return icon and text for the current grid type */
    char * getIcon(void) const;
    const char * getName(void) const;
};

#endif
