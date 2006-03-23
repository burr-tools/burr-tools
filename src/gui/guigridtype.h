#ifndef __GUI_GRID_TYPE__
#define __GUI_GRID_TYPE__

#include "../lib/gridtype.h"

class SquareEditor;
class VoxelDrawer;
class LFl_Group;

class guiGridType_c {

  private:

    gridType_c * gt;

  public:

    guiGridType_c(gridType_c * gt);

    SquareEditor * getGridEditor(int x, int y, int w, int h, puzzle_c * puzzle);
    VoxelDrawer * getVoxelDrawer(int x, int y, int w, int h);

    /* returns a group to edit the parameters for this grid type
     * is is used in the new puzzle grid selection dialog
     * and also in the later possible grid parameters dialog
     */
    LFl_Group * getCofigurationDialog(void);

    /* return icon and text for the current grid type */
    char * getIcon(void);
    const char * getName(void);
};

#endif
