/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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

#include "assembly.h"

#include "puzzle.h"

assemblyVoxel_c * assembly_c::getVoxelSpace(const puzzle_c * puz, unsigned int prob) const {

  assemblyVoxel_c * res = new assemblyVoxel_c(puz->probGetResultShape(prob)->getX(),
                                              puz->probGetResultShape(prob)->getY(),
                                              puz->probGetResultShape(prob)->getZ());

  unsigned int idx = 0;

  for (unsigned int pc = 0; pc < puz->probShapeNumber(prob); pc++)
    for (unsigned int inst = 0; inst < puz->probGetShapeCount(prob, pc); inst++) {

      assert(idx < placements.size());

      pieceVoxel_c p(puz->probGetShapeShape(prob, pc), placements[idx].transformation);

      int dx = placements[idx].xpos;
      int dy = placements[idx].ypos;
      int dz = placements[idx].zpos;

      for (unsigned int x = 0; x < p.getX(); x++)
        for (unsigned int y = 0; y < p.getY(); y++)
          for (unsigned int z = 0; z < p.getZ(); z++)
            if (p.getState(x, y, z) == pieceVoxel_c::VX_FILLED) {

              assert(res->isEmpty2(x+dx, y+dy, z+dz));
              assert(puz->probGetResultShape(prob)->getState2(x+dx, y+dy, z+dz) != pieceVoxel_c::VX_EMPTY);

              res->setPiece(x+dx, y+dy, z+dz, idx);
            }

      idx++;
    }

  return res;

}

assembly_c::assembly_c(const xml::node & node) {

  // we must have a real node and the following attributes
  assert(node.get_type() == xml::node::type_element);
  assert(strcmp(node.get_name(), "assembly") == 0);

  const char * c = node.get_content();

  int x, y, z, trans, state, sign;

  x = y = z = trans = state = 0;
  sign = 0;

  while (*c) {

    if (*c == ' ') {
      if (state == 3) {

        if ((trans != 255) && ((trans < 0) || (trans >= 24)))
          throw load_error("transformations need to be either 255 or between 0 and 24", node);

        placements.push_back(placement_c(trans, x, y, z));
        x = y = z = trans = state = 0;
      } else
        state++;

    } else {

      if ((*c != '-') && ((*c < '0') || (*c > '9')))
        throw load_error("nun number character found where number is expected", node);

      switch(state) {
      case 0:
        if (*c == '-')
          sign = 1;
        else {
          x *= 10;
          x += *c - '0';
          if (x && sign) {
            sign = 0;
            x = -x;
          }
        }
        break;
      case 1:
        if (*c == '-')
          sign = 1;
        else {
          y *= 10;
          y += *c - '0';
          if (y && sign) {
            sign = 0;
            y = -y;
          }
        }
        break;
      case 2:
        if (*c == '-')
          sign = 1;
        else {
          z *= 10;
          z += *c - '0';
          if (z && sign) {
            sign = 0;
            z = -z;
          }
        }
        break;
      case 3:
        if (*c == '-')
          sign = 1;
        else {
          trans *= 10;
          trans += *c - '0';
          if (trans && sign) {
            sign = 0;
            trans = -trans;
          }
        }
        break;
      }
    }

    c++;
  }

  if (state != 3)
    throw load_error("not the right number of numbers in assembly", node);
  if ((trans != 255) && ((trans < 0) || (trans >= 24)))
    throw load_error("transformations need to be either 255 or between 0 and 24", node);

  placements.push_back(placement_c(trans, x, y, z));
}

xml::node assembly_c::save(void) const {

  xml::node nd("assembly");

  std::string cont;
  char tmp[50];

  for (unsigned int i = 0; i < placements.size(); i++) {
    snprintf(tmp, 50, "%i %i %i %i", placements[i].xpos, placements[i].ypos, placements[i].zpos, placements[i].transformation);

    cont += tmp;

    if (i < placements.size() - 1)
      cont += " ";
  }

  nd.set_content(cont.c_str());

  return nd;
}



