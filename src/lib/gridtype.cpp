/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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

#include "assembler_0.h"
#include "assembler_1.h"
#include "assemblerfrontend_0.h"
#include "assemblerfrontend_1.h"
#include "assemblerfrontend_2.h"
#include "assemblerfrontend_3.h"
#include "movementcache_0.h"
#include "voxel_0.h"
#include "voxel_1.h"
#include "voxel_2.h"
#include "voxel_3.h"
#include "symmetries_0.h"
#include "symmetries_1.h"
#include "symmetries_2.h"
#include "stl_0.h"
#include "stl_2.h"
#include "problem.h"

#include <xmlwrapp/attributes.h>

#include <stdlib.h>

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
      break;

    case GT_TRIANGULAR_PRISM:
      break;

    case GT_SPHERES:
      break;

    case GT_RHOMBIC:
      break;

    default:
      throw load_error("puzzle with unknown grid type", node);
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
      break;

    case GT_TRIANGULAR_PRISM:
      break;

    case GT_SPHERES:
      break;

    case GT_RHOMBIC:
      break;
  }

  return nd;
}

/* some specializes constructors */

/* create a cube grid */
gridType_c::gridType_c(void) {
  type = GT_BRICKS;

  sym = 0;
}

gridType_c::gridType_c(gridType gt) {

  type = gt;

  switch (type) {
    case GT_BRICKS:
      break;

    case GT_TRIANGULAR_PRISM:
      break;

    case GT_SPHERES:
      break;

    case GT_RHOMBIC:
      break;

    default:
      bt_assert(0);
  }

  sym = 0;
}


gridType_c::~gridType_c(void) {
  if (sym)
    delete sym;
}


/* these functions return assembler and disassemble for the current space grid
 * if the required functionality is not available, return 0
 */
assemblerFrontend_c * gridType_c::getAssemblerFrontend(void) const {

  switch (type) {
    case GT_BRICKS:           return new assemblerFrontend_0_c();
    case GT_TRIANGULAR_PRISM: return new assemblerFrontend_1_c();
    case GT_SPHERES:          return new assemblerFrontend_2_c();
    case GT_RHOMBIC:          return new assemblerFrontend_3_c();
    default:                  return 0;
  }

}

movementCache_c * gridType_c::getMovementCache(const problem_c * puz) const {

  switch (type) {
    case GT_BRICKS:           return new movementCache_0_c(puz);
    default: return 0;
  }
}

/* voxel spaces have different implementations for rotation, and mirror functions */
voxel_c * gridType_c::getVoxel(unsigned int x, unsigned int y, unsigned int z, voxel_type init) const {
  switch (type) {
    case GT_BRICKS:           return new voxel_0_c(x, y, z, this, init);
    case GT_TRIANGULAR_PRISM: return new voxel_1_c(x, y, z, this, init);
    case GT_SPHERES:          return new voxel_2_c(x, y, z, this, init);
    case GT_RHOMBIC:          return new voxel_3_c(x, y, z, this, init);
    default: return 0;
  }
}
voxel_c * gridType_c::getVoxel(const xml::node & node) const {
  switch (type) {
    case GT_BRICKS:           return new voxel_0_c(node, this);
    case GT_TRIANGULAR_PRISM: return new voxel_1_c(node, this);
    case GT_SPHERES:          return new voxel_2_c(node, this);
    case GT_RHOMBIC:          return new voxel_3_c(node, this);
    default: return 0;
  }
}

voxel_c * gridType_c::getVoxel(const voxel_c & orig) const {
  switch (type) {
    case GT_BRICKS:           return new voxel_0_c(orig);
    case GT_TRIANGULAR_PRISM: return new voxel_1_c(orig);
    case GT_SPHERES:          return new voxel_2_c(orig);
    case GT_RHOMBIC:          return new voxel_3_c(orig);
    default: return 0;
  }
}

voxel_c * gridType_c::getVoxel(const voxel_c * orig) const {
  switch (type) {
    case GT_BRICKS:           return new voxel_0_c(orig);
    case GT_TRIANGULAR_PRISM: return new voxel_1_c(orig);
    case GT_SPHERES:          return new voxel_2_c(orig);
    case GT_RHOMBIC:          return new voxel_3_c(orig);
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
        const_cast<gridType_c*>(this)->sym = new symmetries_1_c(this);
        break;
      case GT_SPHERES:
        const_cast<gridType_c*>(this)->sym = new symmetries_2_c(this);
        break;
      case GT_RHOMBIC:
        const_cast<gridType_c*>(this)->sym = new symmetries_0_c(this);
        break;
    }
  }

  return sym;
}

unsigned int gridType_c::getCapabilities(void) const {
  switch (type) {
    case GT_BRICKS:           return CAP_ASSEMBLE | CAP_DISASSEMBLE | CAP_STLEXPORT;
    case GT_TRIANGULAR_PRISM: return CAP_ASSEMBLE;
    case GT_SPHERES:          return CAP_ASSEMBLE | CAP_STLEXPORT;
    case GT_RHOMBIC:          return CAP_ASSEMBLE;
    default: return 0;
  }
}

assembler_c * gridType_c::findAssembler(const problem_c * p) {

  if (assembler_0_c::canHandle(p)) {
    fprintf(stderr, "using assembler 0\n");
    return new assembler_0_c(p->getGridType()->getAssemblerFrontend());
  }
  if (assembler_1_c::canHandle(p)) {
    fprintf(stderr, "using assembler 1\n");
    return new assembler_1_c(p->getGridType()->getAssemblerFrontend());
  }

  return 0;
}

stlExporter_c * gridType_c::getStlExporter(void) const {
  switch (type) {
    case GT_BRICKS:           return new stlExporter_0_c();
    case GT_SPHERES:          return new stlExporter_2_c();
    default: return 0;
  }
}

