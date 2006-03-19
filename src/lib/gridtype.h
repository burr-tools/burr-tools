#ifndef __GRID_TYPE_H__
#define __GRID_TYPE_H__

#include "types.h"

#include <xmlwrapp/node.h>

class assembler_0_c;
class disassembler_c;
class symmetries_c;
class voxel_c;
class puzzle_c;

/* this class encapsulates all information required to handle the different grid types
 */
class gridType_c {

  public:

    typedef enum {
      GT_BRICKS,
//      GT_TRIANGULAR_PRISM,
//      GT_RHOMBIC_DODECAEDER,
    } gridType;

  protected:

    gridType type;

    union {

      /* the parameter for the GT_BRICKS type
       */
      struct {
        bool y_differs_x;  // length of y-axis is different from x axis
        bool z_differs_x;
        bool z_differs_y;

        bool axy_ortho;    // y axis is in a rectangle to x axis
        bool axz_ortho;
        bool ayz_ortho;

        bool axy_differs_axz;  // the angle between axes x and y is different from the angle between axes x and z
        bool axy_differs_ayz;
        bool ayz_differs_ayz;
      } brick;

    } parameters;

    symmetries_c * sym;

  public:

    /**
     * load from xml node
     */
    gridType_c(const xml::node & node);

    /* used to save to XML */
    xml::node save(void) const;

    /* some specializes constructors */

    /* create a cube grid */
    gridType_c(void);

    /* these functions return assembler and disassemble for the current space grid
     * if the requied functionality is not available, return 0
     */
    assembler_0_c * getAssembler(void) const;
    disassembler_c * getDisassembler(const puzzle_c * puz, unsigned int prob) const;

    /* voxel spaces have different implementatios for rotation, and mirror functions */
    voxel_c * getVoxel(unsigned int x, unsigned int y, unsigned int z, voxel_type init, voxel_type outs) const;
    voxel_c * getVoxel(const xml::node & node) const;

    const symmetries_c * getSymmetries(void) const;

    /* sometimes it might be possible to convert from the current grid
     * to anothe e.g. hexagonal to triangular prisms
     */
#if 0
    converter_c * getConveter(gridType target);
#endif
};


#if 0
class guiGridType_c : public gridType_c {

  GridEditor * getGridEditor(void);
  VoxelDrawer * getVoxelDrawer(void);

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

#endif
