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

puzzle_c::bitmap_c::bitmap_c(unsigned int col) : colors(col) {
  if (colors)
    map = new unsigned char[(col*col + 7) >> 3];
  else
    map = 0;
}

void puzzle_c::bitmap_c::add(void) {

  unsigned char *m2 = new unsigned char[((colors+1)*(colors+1) + 7) >> 3];

  if (map) {

    for (int i = 0; i < colors; i++)
      for (int j = 0; j < colors; j++) {
        int idx = j * (colors+1) + i;
    
        if (get(i, j))
          m2[idx >> 3] |= (1 << (idx & 7));
        else
          m2[idx >> 3] &= ~(1 << (idx & 7));
      }

    delete [] map;
  }

  map = m2;
  colors++;
}


void puzzle_c::bitmap_c::remove(unsigned int col) {

  assert(col < colors);

  unsigned char *m2 = new unsigned char[((colors-1)*(colors-1) + 7) >> 3];

  for (int i = 0; i < colors-1; i++)
    for (int j = 0; j < colors-1; j++) {
      int idx = j * (colors-1) + i;

      int k, l;

      if (i < col) k = i; else k = i+1;
      if (j < col) l = j; else l = j+1;
  
      if (get(k, l))
        m2[idx >> 3] |= (1 << (idx & 7));
      else
        m2[idx >> 3] &= ~(1 << (idx & 7));
    }

  delete [] map;

  map = m2;
  colors--;
}




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
puzzle_c::puzzle_c(istream * str) : colorConstraints(0) {

  results.push_back(new pieceVoxel_c(str));
  assert(results[0]);

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

puzzle_c::puzzle_c(const puzzle_c * orig) : colorConstraints(0) {

  for (int i = 0; i < orig->results.size(); i++)
    results.push_back(new pieceVoxel_c(orig->results[i]));

  for (int i = 0; i < orig->getShapeNumber(); i++) {
    shapeInfo pi;
    pi.piece = new pieceVoxel_c(orig->getShape(i));
    pi.count = orig->getShapeCount(i);
    shapes.push_back(pi);
  }
}

void puzzle_c::save(ostream * str) const {

  results[0]->save(str);

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

  *str << "RESULT " << results[0]->getX() << "," << results[0]->getY() << "," << results[0]->getZ() << std::endl;
  for (int y = 0; y < results[0]->getY(); y++) {
    for (int z = 0; z < results[0]->getZ(); z++) {
      for (int x = 0; x < results[0]->getX(); x++) {
        if (results[0]->getState(x, y, z) != pieceVoxel_c::VX_EMPTY)
          *str << 'A';
        else
          *str << ' ';
      }
      if (z < results[0]->getZ() - 1)
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

  results[0]->print('a');

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


void puzzle_c::addColor(void) {

  colorDef c;

  c.r = rand() & 0xff;
  c.g = rand() & 0xff;
  c.b = rand() & 0xff;

  colors.push_back(c);

  colorConstraints.add();
}

void puzzle_c::removeColor(unsigned int col) {

// FIXME, remove from color definition array

  colorConstraints.remove(col);

}

void puzzle_c::setColor(unsigned int col, unsigned char r, unsigned char g, unsigned char b) {

  assert(col < colors.size());

  colors[col].r = r;
  colors[col].g = g;
  colors[col].b = b;
}

void puzzle_c::allowPlacement(unsigned int pc, unsigned int res) {

  colorConstraints.set(pc, res, true);
}

void puzzle_c::disallowPlacement(unsigned int pc, unsigned int res) {

  colorConstraints.set(pc, res, false);
}

bool puzzle_c::placementAllowed(unsigned int pc, unsigned int res) const {

  if (colors.size() == 0)
    return true;

  return colorConstraints.get(pc, res);
}

xml::node puzzle_c::save(void) const {

  xml::node nd("puzzle");

  char tmp[50];

  for (int i = 0; i < shapes.size(); i++) {
    xml::node::iterator it = nd.insert(xml::node("shape"));

    snprintf(tmp, 50, "%i", shapes[i].count);
    it->get_attributes().insert("count", tmp);
    it->insert(shapes[i].piece->save());
  }

  for (int i = 0; i < results.size(); i++)
    nd.insert(results[i]->save());

  if (colors.size() > 0) {

    xml::node::iterator it = nd.insert(xml::node("constraints"));

    snprintf(tmp, 50, "%i", colors.size());
    it->get_attributes().insert("count", tmp);

    for (int i = 0; i < colors.size(); i++) {

      xml::node::iterator it2 = it->insert(xml::node("color"));

      snprintf(tmp, 50, "%i", colors[i].r);
      it2->get_attributes().insert("red", tmp);

      snprintf(tmp, 50, "%i", colors[i].g);
      it2->get_attributes().insert("green", tmp);

      snprintf(tmp, 50, "%i", colors[i].b);
      it2->get_attributes().insert("blue", tmp);
    }

    for (int i = 0; i < colors.size(); i++)
      for (int j = 0; j < colors.size(); j++)
        if (colorConstraints.get(i, j)) {

          xml::node::iterator it2 = it->insert(xml::node("pair"));
    
          snprintf(tmp, 50, "%i", i);
          it2->get_attributes().insert("piece", tmp);
    
          snprintf(tmp, 50, "%i", j);
          it2->get_attributes().insert("result", tmp);
        }
  }

  return nd;
}

