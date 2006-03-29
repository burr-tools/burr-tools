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
#include "gridtype.h"

#include "assm_0_frontend_0.h"
#include "disassembler_0.h"
#include "voxel_0.h"
#include "symmetries.h"

#include <xmlwrapp/attributes.h>

/**
 * load from xml node
 */
gridType_c::gridType_c(const xml::node & node) {
  // we must have a real node and the following attributes
  if ((node.get_type() != xml::node::type_element) ||
      (strcmp(node.get_name(), "gridType") != 0))
    throw load_error("not the right type of node for a grid type", node);

  if (node.get_attributes().find("type") == node.get_attributes().end())
    throw load_error("grid type with no type attribute encountered", node);

  // set to the correct size
  type = (gridType)atoi(node.get_attributes().find("type")->get_value());

  switch (type) {

    case GT_BRICKS:
      parameters.brick.x_differs_y = node.get_attributes().find("x_differs_y") != node.get_attributes().end();
      parameters.brick.x_differs_z = node.get_attributes().find("x_differs_z") != node.get_attributes().end();
      parameters.brick.y_differs_z = node.get_attributes().find("y_differs_z") != node.get_attributes().end();

      parameters.brick.axy_ortho = node.get_attributes().find("axy_ortho") != node.get_attributes().end();
      parameters.brick.axz_ortho = node.get_attributes().find("axz_ortho") != node.get_attributes().end();
      parameters.brick.ayz_ortho = node.get_attributes().find("ayz_ortho") != node.get_attributes().end();

      parameters.brick.axy_differs_axz = node.get_attributes().find("axy_differs_axz") != node.get_attributes().end();
      parameters.brick.axy_differs_ayz = node.get_attributes().find("axy_differs_ayz") != node.get_attributes().end();
      parameters.brick.axz_differs_ayz = node.get_attributes().find("axz_differs_ayz") != node.get_attributes().end();

      break;

    case GT_TRIANGULAR_PRISM:
      break;
  }

  sym = 0;
}

/* used to save to XML */
xml::node gridType_c::save(void) const {
  xml::node nd("gridType");

  char tmp[50];

  snprintf(tmp, 50, "%i", type);
  nd.get_attributes().insert("type", tmp);

  switch (type) {

    case GT_BRICKS:
      if (parameters.brick.x_differs_y) nd.get_attributes().insert("x_differs_y", "");
      if (parameters.brick.x_differs_z) nd.get_attributes().insert("x_differs_z", "");
      if (parameters.brick.y_differs_z) nd.get_attributes().insert("y_differs_z", "");

      if (parameters.brick.axy_ortho) nd.get_attributes().insert("axy_ortho", "");
      if (parameters.brick.axz_ortho) nd.get_attributes().insert("axz_ortho", "");
      if (parameters.brick.ayz_ortho) nd.get_attributes().insert("ayz_ortho", "");

      if (parameters.brick.axy_differs_axz) nd.get_attributes().insert("axy_differs_axz", "");
      if (parameters.brick.axy_differs_ayz) nd.get_attributes().insert("axy_differs_ayz", "");
      if (parameters.brick.axz_differs_ayz) nd.get_attributes().insert("axz_differs_ayz", "");

      break;

    case GT_TRIANGULAR_PRISM:
      break;
  }

  return nd;
}

/* some specializes constructors */

/* create a cube grid */
gridType_c::gridType_c(void) {
  type = GT_BRICKS;

  parameters.brick.x_differs_y = false;
  parameters.brick.x_differs_z = false;
  parameters.brick.y_differs_z = false;

  parameters.brick.axy_ortho = true;
  parameters.brick.axz_ortho = true;
  parameters.brick.ayz_ortho = true;

  parameters.brick.axy_differs_axz = false;
  parameters.brick.axy_differs_ayz = false;
  parameters.brick.axz_differs_ayz = false;

  sym = 0;
}

gridType_c::gridType_c(gridType gt) {

  type = gt;

  switch (type) {
    case GT_BRICKS:

      parameters.brick.x_differs_y = false;
      parameters.brick.x_differs_z = false;
      parameters.brick.y_differs_z = false;

      parameters.brick.axy_ortho = true;
      parameters.brick.axz_ortho = true;
      parameters.brick.ayz_ortho = true;

      parameters.brick.axy_differs_axz = false;
      parameters.brick.axy_differs_ayz = false;
      parameters.brick.axz_differs_ayz = false;

      break;

    case GT_TRIANGULAR_PRISM:

      break;
  }

  sym = 0;
}


gridType_c::~gridType_c(void) {
  if (sym)
    delete sym;
}


/* these functions return assembler and disassemble for the current space grid
 * if the requied functionality is not available, return 0
 */
assembler_0_c * gridType_c::getAssembler(void) const {

  switch (type) {
    case GT_BRICKS: return new assm_0_frontend_0_c();
    default: return 0;
  }

}
disassembler_c * gridType_c::getDisassembler(const puzzle_c * puz, unsigned int prob) const {

  switch (type) {
    case GT_BRICKS: return new disassembler_0_c(puz, prob);
    default: return 0;
  }
}

/* voxel spaces have different implementatios for rotation, and mirror functions */
voxel_c * gridType_c::getVoxel(unsigned int x, unsigned int y, unsigned int z, voxel_type init, voxel_type outs) const {
  switch (type) {
    case GT_BRICKS: return new voxel_0_c(x, y, z, this, init, outs);
    case GT_TRIANGULAR_PRISM: return new voxel_0_c(x, y, z, this, init, outs);     // TODO: add proper value
    default: return 0;
  }
}
voxel_c * gridType_c::getVoxel(const xml::node & node) const {
  switch (type) {
    case GT_BRICKS: return new voxel_0_c(node, this);
    case GT_TRIANGULAR_PRISM: return new voxel_0_c(node, this);    // TODO: add proper value
    default: return 0;
  }
}

voxel_c * gridType_c::getVoxel(const voxel_c & orig, unsigned int transformation) const {
  switch (type) {
    case GT_BRICKS: return new voxel_0_c(orig, transformation);
    case GT_TRIANGULAR_PRISM: return new voxel_0_c(orig, transformation);   // TODO: add proper value
    default: return 0;
  }
}

voxel_c * gridType_c::getVoxel(const voxel_c * orig, unsigned int transformation) const {
  switch (type) {
    case GT_BRICKS: return new voxel_0_c(orig, transformation);
    case GT_TRIANGULAR_PRISM: return new voxel_0_c(orig, transformation);  // TODO: add proper value
    default: return 0;
  }
}

const symmetries_c * gridType_c::getSymmetries(void) const {
  if (!sym) {
    switch(type) {
      case GT_BRICKS:
        const_cast<gridType_c*>(this)->sym = new symmetries_0_c(this);
        break;
      case GT_TRIANGULAR_PRISM:
        const_cast<gridType_c*>(this)->sym = new symmetries_0_c(this);   //TODO: add proper value
        break;
    }
  }

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

