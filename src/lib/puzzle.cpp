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


#include "puzzle.h"

#include <stdio.h>

using namespace std;

/* load the puzzle from the file
 *
 * the format is quite simple:
 * first line is x y z size of the assembled block separated by space
 * all other lines stand one line for one piece
 * the first 3 number are the dimensions followed by a string in ""
 * in this string everything else esxept for space is asumed to be filled
 * the string is a linear list of all blocks inside the piece block
 * with x the lowest and z the highest dimension. So the formula to get
 * the position of one block is x+xs*(y+ys*z)
 */
puzzle_c::puzzle_c(istream * str) {

  result = new pieceVoxel_c(str);
  assert(result);

  int pieces;

  *str >> pieces;

  if ((pieces < 0) || (pieces > 500))
    throw load_error("too many pieces in file? probably voxel space not defined correctly");

  while (pieces > 0) {

    int nr;

    *str >> nr;

    if ((nr < 0) || (nr > 500))
      throw load_error("too many instances of one piece? probably voxel space not defined correctly");

    pieceVoxel_c * pc = new pieceVoxel_c(str);
    assert(pc);

    addShape(pc, nr);

    pieces--;
  }
}

puzzle_c::puzzle_c(const puzzle_c * orig) {

  result = new pieceVoxel_c(orig->result);

  for (int i = 0; i < orig->getShapeNumber(); i++) {
    shapeInfo pi;
    pi.piece = new pieceVoxel_c(orig->getShape(i));
    pi.count = orig->getShapeCount(i);
    shapes.push_back(pi);
  }
}

void puzzle_c::save(ostream * str) const {

  result->save(str);

  *str << shapes.size() << endl;

  for (unsigned int i = 0; i < shapes.size(); i++) {

    *str << shapes[i].count << " ";
    shapes[i].piece->save(str);
  }
}

void puzzle_c::PS3Dsave(std::ostream * str) const {
  for (unsigned int i = 0; i < shapes.size(); i++) {
    *str << "PIECE " << shapes[i].piece->getX() << "," << shapes[i].piece->getY() << "," << shapes[i].piece->getZ() << std::endl;
    for (int y = 0; y < shapes[i].piece->getY(); y++) {
      for (int z = 0; z < shapes[i].piece->getZ(); z++) {
        for (int x = 0; x < shapes[i].piece->getX(); x++) {
          if (shapes[i].piece->getState(x, y, z) != pieceVoxel_c::VX_EMPTY)
            *str << char('A' + i);
          else
            *str << ' ';
        }
        if (z < shapes[i].piece->getZ() - 1)
          *str << ",";
      }
      *str << std::endl;
    }
  }

  *str << "RESULT " << result->getX() << "," << result->getY() << "," << result->getZ() << std::endl;
  for (int y = 0; y < result->getY(); y++) {
    for (int z = 0; z < result->getZ(); z++) {
      for (int x = 0; x < result->getX(); x++) {
        if (result->getState(x, y, z) != pieceVoxel_c::VX_EMPTY)
          *str << 'A';
        else
          *str << ' ';
      }
      if (z < result->getZ() - 1)
        *str << ",";
    }
    *str << std::endl;
  }
}


puzzle_c::~puzzle_c(void) {

  for (unsigned int i = 0; i < shapes.size(); i++)
    delete shapes[i].piece;
}

void puzzle_c::print(void) {

  result->print('a');

  for (unsigned int i = 0; i < shapes.size(); i++)
    shapes[i].piece->print('a');
}

void puzzle_c::removeShape(unsigned int nr) {

  assert (nr < shapes.size());

  std::vector<shapeInfo>::iterator i = shapes.begin();

  while (nr) {
    i++;
    nr--;
  }

  shapes.erase(i);
}

void puzzle_c::addShape(pieceVoxel_c * p, int nr) {
  shapeInfo i;
  i.piece = p;
  i.count = nr;
  shapes.push_back(i);
}

void puzzle_c::addShape(int sx, int sy, int sz, int nr) {
  shapeInfo i;
  i.piece = new pieceVoxel_c(sx, sy, sz);
  i.count = nr;
  shapes.push_back(i);
}

int puzzle_c::getPieces(void) const {
  int erg = 0;

  for (unsigned int i = 0; i < shapes.size(); i++)
    erg += shapes[i].count;

  return erg;
}

void puzzle_c::orthogonalize(void) {
  // for the moment a simple version not looking for rotated pieces

  int p = 1;
  bool found;

  while (p < getShapeNumber()) {

    found = 0;

    for (int p2 = 0; p2 < p; p2++)
      if (*(shapes[p].piece) == *(shapes[p2].piece)) {
        shapes[p2].count++;
//        shapes.erase(p);  FIXME
        found = true;
        break;
      }

    if (!found)
      p++;
  }
}
