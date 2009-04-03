/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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
#include "movementcache_0.h"
#include "movementcache_1.h"
#include "voxel_0.h"
#include "voxel_1.h"
#include "voxel_2.h"
#include "voxel_3.h"
#include "voxel_4.h"
#include "symmetries_0.h"
#include "symmetries_1.h"
#include "symmetries_2.h"
#include "stl_0.h"
#include "stl_2.h"
#include "problem.h"
#include "xml.h"

#include <stdlib.h>

gridType_c::gridType_c(xmlParser_c & pars)
{
  pars.require(xmlParser_c::START_TAG, "gridType");

  std::string typeStr = pars.getAttributeValue("type");

  if (!typeStr.length())
    pars.exception("grid type needs a valid 'type' attribute");

  // set to the correct size
  type = (gridType)atoi(typeStr.c_str());

  switch (type) {

    case GT_BRICKS:
    case GT_TRIANGULAR_PRISM:
    case GT_SPHERES:
    case GT_RHOMBIC:
    case GT_TETRA_OCTA:
      break;

    default:
      pars.exception("puzzle with unknown grid type");
  }

  pars.skipSubTree();

  sym = 0;
}

void gridType_c::save(xmlWriter_c & xml) const
{
  xml.newTag("gridType");

  xml.newAttrib("type", (unsigned long)type);

  xml.endTag("gridType");
}

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

    case GT_TETRA_OCTA:
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

movementCache_c * gridType_c::getMovementCache(const problem_c * puz) const
{
  switch (type) {
    case GT_BRICKS:           return new movementCache_0_c(puz);
    case GT_TRIANGULAR_PRISM: return new movementCache_1_c(puz);
    default: return 0;
  }
}

voxel_c * gridType_c::getVoxel(unsigned int x, unsigned int y, unsigned int z, voxel_type init) const
{
  switch (type) {
    case GT_BRICKS:           return new voxel_0_c(x, y, z, this, init);
    case GT_TRIANGULAR_PRISM: return new voxel_1_c(x, y, z, this, init);
    case GT_SPHERES:          return new voxel_2_c(x, y, z, this, init);
    case GT_RHOMBIC:          return new voxel_3_c(x, y, z, this, init);
    case GT_TETRA_OCTA:       return new voxel_4_c(x, y, z, this, init);
    default: return 0;
  }
}

voxel_c * gridType_c::getVoxel(xmlParser_c & pars) const
{
  switch (type) {
    case GT_BRICKS:           return new voxel_0_c(pars, this);
    case GT_TRIANGULAR_PRISM: return new voxel_1_c(pars, this);
    case GT_SPHERES:          return new voxel_2_c(pars, this);
    case GT_RHOMBIC:          return new voxel_3_c(pars, this);
    case GT_TETRA_OCTA:       return new voxel_4_c(pars, this);
    default: return 0;
  }
}

voxel_c * gridType_c::getVoxel(const voxel_c & orig) const
{
  switch (type) {
    case GT_BRICKS:           return new voxel_0_c(orig);
    case GT_TRIANGULAR_PRISM: return new voxel_1_c(orig);
    case GT_SPHERES:          return new voxel_2_c(orig);
    case GT_RHOMBIC:          return new voxel_3_c(orig);
    case GT_TETRA_OCTA:       return new voxel_4_c(orig);
    default: return 0;
  }
}

voxel_c * gridType_c::getVoxel(const voxel_c * orig) const
{
  switch (type) {
    case GT_BRICKS:           return new voxel_0_c(orig);
    case GT_TRIANGULAR_PRISM: return new voxel_1_c(orig);
    case GT_SPHERES:          return new voxel_2_c(orig);
    case GT_RHOMBIC:          return new voxel_3_c(orig);
    case GT_TETRA_OCTA:       return new voxel_4_c(orig);
    default: return 0;
  }
}

const symmetries_c * gridType_c::getSymmetries(void) const
{
  if (!sym) {
    switch(type) {
      case GT_BRICKS:
      case GT_RHOMBIC:
      case GT_TETRA_OCTA:
        sym = new symmetries_0_c();
        break;
      case GT_TRIANGULAR_PRISM:
        sym = new symmetries_1_c();
        break;
      case GT_SPHERES:
        sym = new symmetries_2_c();
        break;
      default:
        break;
    }
  }

  return sym;
}

unsigned int gridType_c::getCapabilities(void) const
{
  switch (type) {
    case GT_BRICKS:           return CAP_ASSEMBLE | CAP_DISASSEMBLE | CAP_STLEXPORT;
    case GT_TRIANGULAR_PRISM: return CAP_ASSEMBLE | CAP_DISASSEMBLE;
    case GT_SPHERES:          return CAP_ASSEMBLE | CAP_STLEXPORT;
    case GT_RHOMBIC:          return CAP_ASSEMBLE;
    case GT_TETRA_OCTA:       return CAP_ASSEMBLE;
    default: return 0;
  }
}

assembler_c * gridType_c::findAssembler(const problem_c * p)
{
  if (assembler_0_c::canHandle(p)) {
    fprintf(stderr, "using assembler 0\n");
    return new assembler_0_c();
  }
  if (assembler_1_c::canHandle(p)) {
    fprintf(stderr, "using assembler 1\n");
    return new assembler_1_c();
  }

  return 0;
}

stlExporter_c * gridType_c::getStlExporter(void) const
{
  switch (type) {
    case GT_BRICKS:           return new stlExporter_0_c();
    case GT_SPHERES:          return new stlExporter_2_c();
    default: return 0;
  }
}

