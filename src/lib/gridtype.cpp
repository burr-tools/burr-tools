#include "gridtype.h"

#include "assm_0_frontend_0.h"
#include "disassembler_0.h"
#include "voxel.h"
#include "symmetries.h"

#include <xmlwrapp/attributes.h>

/**
 * load from xml node
 */
gridType_c::gridType_c(const xml::node & node) {

}

/* used to save to XML */
xml::node gridType_c::save(void) const {
  xml::node nd("gridType");

  char tmp[50];

  snprintf(tmp, 50, "%i", type);
  nd.get_attributes().insert("type", tmp);

  switch(type) {

    case GT_BRICKS:
        if (parameters.brick.y_differs_x)
          nd.get_attributes().insert("y_differs_x", "");

        break;
  }

  return nd;
}

/* some specializes constructors */

/* create a cube grid */
gridType_c::gridType_c(void) {
  type = GT_BRICKS;

  parameters.brick.y_differs_x = false;
  parameters.brick.z_differs_x = false;
  parameters.brick.z_differs_y = false;

  parameters.brick.axy_ortho = true;
  parameters.brick.axz_ortho = true;
  parameters.brick.ayz_ortho = true;

  parameters.brick.axy_differs_axz = false;
  parameters.brick.axy_differs_ayz = false;
  parameters.brick.ayz_differs_ayz = false;

  sym = new symmetries_0_c(this);
}



/* these functions return assembler and disassemble for the current space grid
 * if the requied functionality is not available, return 0
 */
assembler_c * gridType_c::getAssembler(void) const {

  switch (type) {
    //case GT_BRICKS: return new assm_0_frontend_0();
    default: return 0;
  }

}
disassembler_c * gridType_c::getDisassembler(void) const {

  switch (type) {
//    case GT_BRICKS: return new disassembler_0();
    default: return 0;
  }
}

/* voxel spaces have different implementatios for rotation, and mirror functions */
voxel_c * gridType_c::getVoxel(unsigned int x, unsigned int y, unsigned int z, voxel_type init, voxel_type outs) const {
  switch (type) {
    case GT_BRICKS: return new voxel_c(x, y, z, this, init, outs);
    default: return 0;
  }
}
voxel_c * gridType_c::getVoxel(const xml::node & node) const {
  switch (type) {
    case GT_BRICKS: return new voxel_c(node, this);
    default: return 0;
  }
}


const symmetries_c * gridType_c::getSymmetries(void) const {
  return sym;
}

/* sometimes it might be possible to convert from the current grid
 * to anothe e.g. hexagonal to triangular prisms
 */
#if 0
converter_c * gridType_c::getConveter(gridType target) {
  return 0;
}
#endif



#if 0

GridEditor * guiGridType::getGridEditor(void) {
  switch (type) {
    case GT_BRICKS: return new GridEditor();
    default: return 0;
  }
}

VoxelDrawer * guiGridType::getVoxelDrawer(void) {
  switch (type) {
    case GT_BRICKS: return new VoxelDrawer();
    default: return 0;
  }
}

/* returns a group to edit the parameters for this grid type
 * is is used in the new puzzle grid selection dialog
 * and also in the later possible grid parameters dialog
 */
LFl_Group * guiGridType::getCofigurationDialog(void) {
  return 0;
}

/* return icon and text for the current grid type */
char * guiGridType::getIcon(void) {
  return 0;
}

const char * guiGridType::getName(void) {
  switch (type) {
    case GT_BRICKS: return "Parallelepipedion";
    default: return 0;
  }
}
#endif
