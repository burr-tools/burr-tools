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

#include "symmetries.h"

/** @ file
 * The Implementation of the voxel space class.
 */

#include <string.h>
#include <stdio.h>

#include <xmlwrapp/attributes.h>

using namespace std;

voxel_c::voxel_c(unsigned int x, unsigned int y, unsigned int z, voxel_type init) : sx(x), sy(y), sz(z), voxels(x*y*z) {

  space = new voxel_type[voxels];
  assert(space);

  memset(space, init, voxels);
}

voxel_c::voxel_c(const voxel_c & orig, unsigned int transformation) : sx(orig.sx), sy(orig.sy), sz(orig.sz), voxels(orig.voxels) {

  space = new voxel_type[voxels];
  assert(space);

  memcpy(space, orig.space, voxels);

  transform(transformation);
}

voxel_c::voxel_c(const voxel_c * orig, unsigned int transformation) : sx(orig->sx), sy(orig->sy), sz(orig->sz), voxels(orig->voxels) {

  space = new voxel_type[voxels];
  assert(space);

  memcpy(space, orig->space, voxels);

  transform(transformation);
}

voxel_c::~voxel_c() {
  delete [] space;
}

bool voxel_c::operator ==(const voxel_c & op) const {

  if (sx != op.sx) return false;
  if (sy != op.sy) return false;
  if (sz != op.sz) return false;

  for (unsigned int i = 0; i < voxels; i++)
    if (space[i] != op.space[i])
      return false;

  return true;
}


void voxel_c::rotatex() {

  int tmp = sy;
  sy = sz;
  sz = tmp;

  voxel_type *s = new voxel_type[voxels];

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        s[x+sx*(y+sy*z)] = space[x+sx*(z+sz*(sy-y-1))];

  delete [] space;
  space = s;
}

void voxel_c::rotatey() {

  int tmp = sx;
  sx = sz;
  sz = tmp;

  voxel_type *s = new voxel_type[voxels];

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        s[x+sx*(y+sy*z)] = space[z+sz*(y+sy*(sx-x-1))];

  delete [] space;
  space = s;
}

void voxel_c::rotatez() {

  int tmp = sy;
  sy = sx;
  sx = tmp;

  voxel_type *s = new voxel_type[voxels];

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        s[x+sx*(y+sy*z)] = space[y+sy*((sx-x-1)+sx*z)];

  delete [] space;
  space = s;
}

void voxel_c::minimize(voxel_type val) {

  unsigned int x1, x2, y1, y2, z1, z2;

  x1 = y1 = z1 = sx+sy+sz;
  x2 = y2 = z2 = 0;

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        if (get(x, y, z) != val) {
          if (x < x1) x1 = x;
          if (x > x2) x2 = x;

          if (y < y1) y1 = y;
          if (y > y2) y2 = y;

          if (z < z1) z1 = z;
          if (z > z2) z2 = z;
        }

  if ((x1 != 0) || (y1 != 0) || (z1 != 0) || (x2 != sx-1) || (y2 != sy-1) || (z2 != sz-1)) {
    if (x1 > x2) {
      sx = sy = sz = 0;
      if (space) {
        delete [] space;
        space = 0;
      }
    }

    voxel_type * s2 = new voxel_type[(x2-x1+1) * (y2-y1+1) * (z2-z1+1)];

    for (unsigned int x = x1; x <= x2; x++)
      for (unsigned int y = y1; y <= y2; y++)
        for (unsigned int z = z1; z <= z2; z++)
          s2[(x-x1) + (x2-x1+1) * ((y-y1) + (y2-y1+1) * (z-z1))] = get(x, y, z);

    delete [] space;
    space = s2;

    sx = x2-x1+1;
    sy = y2-y1+1;
    sz = z2-z1+1;
    voxels = sx*sy*sz;
  }
}

void voxel_c::resize(unsigned int nsx, unsigned int nsy, unsigned int nsz, voxel_type filler) {
  voxel_type * s2 = new voxel_type[nsx*nsy*nsz];
  memset(s2, filler, nsx*nsy*nsz);

  unsigned int mx = (sx < nsx) ? sx : nsx;
  unsigned int my = (sy < nsy) ? sy : nsy;
  unsigned int mz = (sz < nsz) ? sz : nsz;

  for (unsigned int x = 0; x < mx; x++)
    for (unsigned int y = 0; y < my; y++)
      for (unsigned int z = 0; z < mz; z++)
        s2[x + nsx * (y + nsy * z)] = get(x, y, z);

  delete [] space;
  space = s2;

  sx = nsx;
  sy = nsy;
  sz = nsz;
  voxels = sx*sy*sz;
}

unsigned int voxel_c::count(voxel_type val) const {
  unsigned int count = 0;
  for (unsigned int i = 0; i < getXYZ(); i++)
    if (get(i) == val)
      count ++;
  return count;
}

unsigned int pieceVoxel_c::countState(int state) const {
  unsigned int count = 0;
  for (unsigned int i = 0; i < getXYZ(); i++)
    if ((get(i) & 3) == state)
      count ++;
  return count;
}

void voxel_c::mirrorX(void) {

  for (unsigned int x = 0; x < sx/2; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++) {
        voxel_type tmp = get(x, y, z);
        set(x, y, z, get(sx-x-1, y, z));
        set(sx-x-1, y, z, tmp);
      }

}

void voxel_c::mirrorY(void) {
  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy/2; y++)
      for (unsigned int z = 0; z < sz; z++) {
        voxel_type tmp = get(x, y, z);
        set(x, y, z, get(x, sy-y-1, z));
        set(x, sy-y-1, z, tmp);
      }
}

void voxel_c::mirrorZ(void) {
  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz/2; z++) {
        voxel_type tmp = get(x, y, z);
        set(x, y, z, get(x, y, sz-z-1));
        set(x, y, sz-z-1, tmp);
      }
}

void voxel_c::translate(int dx, int dy, int dz, voxel_type filler) {
  voxel_type * s2 = new voxel_type[sx*sy*sz];
  memset(s2, filler, sx*sy*sz);

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        if ((x+dx >= 0) && (x+dx < sx) &&
            (y+dy >= 0) && (y+dy < sy) &&
            (z+dz >= 0) && (z+dz < sz))
          s2[(x+dx)+sx*((y+dy)+sy*(z+dz))] = get(x, y, z);

  delete [] space;
  space = s2;
}

bool voxel_c::connected(char type, bool inverse, voxel_type value) const {

  /* union find algorithm:
   * 1. put all voxels that matter in an own set
   * 2. unify all sets whose voxels are neibors
   * 3. check if all voxels are in the same set
   */

  /* allocate enough space for all voxels */
  int * tree = new int[voxels];


  /* initialize tree */
  for (unsigned int i = 0; i < voxels; i++)
    tree[i] = 0;

  /* merge all neigboring voxels */
  for (unsigned int x = 1; x < sx; x++)
    for (unsigned int y = 1; y < sy; y++)
      for (unsigned int z = 1; z < sz; z++)
        if ((inverse && (get(x, y, z) != value)) ||
            (!inverse && (get(x, y, z) == value))) {

          int root1 = x+sx*(y+sy*z);
          while (tree[root1]) root1 = tree[root1];


          if ((inverse && (get(x-1, y, z) != value)) ||
              (!inverse && (get(x-1, y, z) == value))) {

            int root2 = (x-1)+sx*(y+sy*z);
            while (tree[root2]) root2 = tree[root2];

            if (root1 != root2) tree[root2] = root1;
          }

          if ((inverse && (get(x, y-1, z) != value)) ||
              (!inverse && (get(x, y-1, z) == value))) {

            int root2 = x+sx*((y-1)+sy*z);
            while (tree[root2]) root2 = tree[root2];

            if (root1 != root2) tree[root2] = root1;
          }
  
          if ((inverse && (get(x, y, z-1) != value)) ||
              (!inverse && (get(x, y, z-1) == value))) {

            int root2 = x+sx*(y+sy*(z-1));
            while (tree[root2]) root2 = tree[root2];

            if (root1 != root2) tree[root2] = root1;
          }

          if (type > 0) {
            if ((inverse && (get(x-1, y, z-1) != value)) ||
                (!inverse && (get(x-1, y, z-1) == value))) {
  
              int root2 = (x-1)+sx*(y+sy*(z-1));
              while (tree[root2]) root2 = tree[root2];
  
              if (root1 != root2) tree[root2] = root1;
            }

            if ((inverse && (get(x-1, y-1, z) != value)) ||
                (!inverse && (get(x-1, y-1, z) == value))) {
  
              int root2 = (x-1)+sx*((y-1)+sy*z);
              while (tree[root2]) root2 = tree[root2];
  
              if (root1 != root2) tree[root2] = root1;
            }

            if ((inverse && (get(x, y-1, z-1) != value)) ||
                (!inverse && (get(x, y-1, z+1) == value))) {
  
              int root2 = x+sx*((y-1)+sy*(z-1));
              while (tree[root2]) root2 = tree[root2];
  
              if (root1 != root2) tree[root2] = root1;
            }
          }

          if (type > 1) {
            if ((inverse && (get(x-1, y-1, z-1) != value)) ||
                (!inverse && (get(x-1, y-1, z-1) == value))) {
  
              int root2 = (x-1)+sx*(y+sy*(z-1));
              while (tree[root2]) root2 = tree[root2];
  
              if (root1 != root2) tree[root2] = root1;
            }
          }
        }


  int root = -1;

  /* finally check, if all voxels are in the same set */
  { for (unsigned int x = 1; x < sx; x++)
      for (unsigned int y = 1; y < sy; y++)
        for (unsigned int z = 1; z < sz; z++)
          if ((inverse && (get(x, y, z) != value)) ||
              (!inverse && (get(x, y, z) == value))) {
            if (root == -1) {
              root = x+sx*(y+sy*z);
              while (tree[root]) root = tree[root];
  
            } else {
  
              int root2 = x+sx*(y+sy*z);
              while (tree[root2]) root2 = tree[root2];
  
              if (root2 != root) {
                delete [] tree;
                return false;
              }
            }
          }
  }

  delete [] tree;
  return true;
}

void voxel_c::copy(const voxel_c * orig) {

  delete [] space;

  space = new voxel_type [orig->getXYZ()];

  memcpy(space, orig->space, orig->getXYZ());

  sx = orig->sx;
  sy = orig->sy;
  sz = orig->sz;

  voxels = orig->voxels;
}

bool voxel_c::neighbour(unsigned int p, voxel_type val) const {

  unsigned int x = p % sx;
  unsigned int y = ((p - x) / sx) % sy;
  unsigned int z = (((p - x) / sx) - y) / sy;

  assert(x + sx * (y + sy * z) == p);

  if ((x > 0   ) && (space[p-1] == val)) return true;
  if ((x < sx-1) && (space[p+1] == val)) return true;

  if ((y > 0   ) && (space[p-sx] == val)) return true;
  if ((y < sy-1) && (space[p+sx] == val)) return true;

  if ((z > 0   ) && (space[p-sx*sy] == val)) return true;
  if ((z < sz-1) && (space[p+sx*sy] == val)) return true;

  return false;
}

void voxel_c::transform(unsigned int nr) {

  assert(nr < 48);

  int i;

  if (nr >= 24) {
    mirrorX();
    nr -= 24;
  }

  for (i = 0; i < rotx(nr); i++) rotatex();
  for (i = 0; i < roty(nr); i++) rotatey();
  for (i = 0; i < rotz(nr); i++) rotatez();
}

symmetries_t voxel_c::selfSymmetries(void) const {

  symmetries_t result = 1;

  for (int i = 1; i < 48; i++) {

    voxel_c * rot = new voxel_c(this, i);

    if (*rot == *this)
      result |= ((symmetries_t)1 << i);

    delete rot;
  }

  return result;
}

void pieceVoxel_c::minimizePiece(void) {

  unsigned int x1, x2, y1, y2, z1, z2;

  x1 = y1 = z1 = getXYZ();
  x2 = y2 = z2 = 0;

  for (unsigned int x = 0; x < getX(); x++)
    for (unsigned int y = 0; y < getY(); y++)
      for (unsigned int z = 0; z < getZ(); z++)
        if (getState(x, y, z) != VX_EMPTY) {
          if (x < x1) x1 = x;
          if (x > x2) x2 = x;

          if (y < y1) y1 = y;
          if (y > y2) y2 = y;

          if (z < z1) z1 = z;
          if (z > z2) z2 = z;
        }

  if ((x1 != 0) || (y1 != 0) || (z1 != 0) || (x2 != getX()-1) || (y2 != getY()-1) || (z2 != getZ()-1)) {

    translate(-x1, -y1, -z1, 0);
    resize(x2-x1+1, y2-y1+1, z2-z1+1, 0);
  }
}


void pieceVoxel_c::makeInsideHoly(void) {

  if (!getX() || !getY() ||!getZ())
    return;

  for (unsigned int x = 1; x < getX()-1; x++)
    for (unsigned int y = 1; y < getY()-1; y++)
      for (unsigned int z = 1; z < getZ()-1; z++)
        if (getState(x, y, z) != VX_EMPTY) {
          if ((getState(x-1, y, z) == VX_EMPTY) || (getState(x+1, y, z) == VX_EMPTY) ||
              (getState(x, y-1, z) == VX_EMPTY) || (getState(x, y+1, z) == VX_EMPTY) ||
              (getState(x, y, z-1) == VX_EMPTY) || (getState(x, y, z+1) == VX_EMPTY))
            setState(x, y, z, VX_FILLED);
          else
            setState(x, y, z, VX_VARIABLE);
        }
}

xml::node pieceVoxel_c::save(void) const {

  xml::node nd("voxel");

  char tmp[50];

  snprintf(tmp, 50, "%i", getX());
  nd.get_attributes().insert("x", tmp);

  snprintf(tmp, 50, "%i", getY());
  nd.get_attributes().insert("y", tmp);

  snprintf(tmp, 50, "%i", getZ());
  nd.get_attributes().insert("z", tmp);

  // this might allow us to later add another format
  nd.get_attributes().insert("type", "0");

  std::string cont;

  for (unsigned int i = 0; i < getXYZ(); i++)
    switch(getState(i)) {
    case VX_EMPTY:
      cont += "_";
      break;
    case VX_VARIABLE:
      cont += "+";
      if (getColor(i)) {
        assert(getColor(i) < 100);
        if (getColor(i) > 9)
          cont += ('0' + (getColor(i) / 10));
        cont += ('0' + (getColor(i) % 10));
      }
      break;
    case VX_FILLED:
      cont += "#";
      if (getColor(i)) {
        assert(getColor(i) < 100);
        if (getColor(i) > 9)
          cont += ('0' + (getColor(i) / 10));
        cont += ('0' + (getColor(i) % 10));
      }
      break;
    }

  nd.set_content(cont.c_str());

  return nd;
}

pieceVoxel_c::pieceVoxel_c(const xml::node & node) : voxel_c(0, 0, 0, 0) {

  // we must have a real node and the following attributes
  assert(node.get_type() == xml::node::type_element);
  assert(strcmp(node.get_name(), "voxel") == 0);

  if (node.get_attributes().find("x") == node.get_attributes().end())
    throw load_error("piece Voxel with no attribut 'x' encountered", node);
  if (node.get_attributes().find("y") == node.get_attributes().end())
    throw load_error("piece Voxel with no attribut 'y' encountered", node);
  if (node.get_attributes().find("z") == node.get_attributes().end())
    throw load_error("piece Voxel with no attribut 'z' encountered", node);
  if (node.get_attributes().find("type") == node.get_attributes().end())
    throw load_error("piece Voxel with no attribut 'type' encountered", node);

  // set to the correct size
  resize(atoi(node.get_attributes().find("x")->get_value()),
         atoi(node.get_attributes().find("y")->get_value()),
         atoi(node.get_attributes().find("z")->get_value()), 0);

  unsigned int type = atoi(node.get_attributes().find("type")->get_value());

  const char * c = node.get_content();
  unsigned int idx = 0;
  unsigned int color = 0;

  if (c) {

    if (type != 0)
      throw load_error("piece Voxel with type not equal to 0 encountetred", node);

    while (*c) {
      switch(*c) {
      case '#':
        setState(idx++, VX_FILLED);
        color = 0;
        break;
      case '+':
        setState(idx++, VX_VARIABLE);
        color = 0;
        break;
      case '_':
        setState(idx++, VX_EMPTY);
        color = 0;
        break;
      case '0': color = color * 10 + 0; break;
      case '1': color = color * 10 + 1; break;
      case '2': color = color * 10 + 2; break;
      case '3': color = color * 10 + 3; break;
      case '4': color = color * 10 + 4; break;
      case '5': color = color * 10 + 5; break;
      case '6': color = color * 10 + 6; break;
      case '7': color = color * 10 + 7; break;
      case '8': color = color * 10 + 8; break;
      case '9': color = color * 10 + 9; break;
      default:
        throw load_error("unrecognized character in piece voxel space", node);
      }

      if (idx > 0)
        setColor(idx-1, color);

      c++;
      if (idx > getXYZ())
        throw load_error("too many voxels defined in voxelspace", node);
    }
    if (idx < getXYZ())
      throw load_error("not enough voxels defined in voxelspace", node);
  }
}

xml::node assemblyVoxel_c::save(void) const {

  xml::node nd("voxel");

  char tmp[50];

  snprintf(tmp, 50, "%i", getX());
  nd.get_attributes().insert("x", tmp);

  snprintf(tmp, 50, "%i", getY());
  nd.get_attributes().insert("y", tmp);

  snprintf(tmp, 50, "%i", getZ());
  nd.get_attributes().insert("z", tmp);

  // this might allow us to later add another format
  nd.get_attributes().insert("type", "1");

  std::string cont;

  /* the format is as follows:
   *  - under line "_" for empty voxels
   *  - 2* 26 letters corresponding to numbers (first the capital and then the lower case letters
   *  - assumption that the base 52 numbers have only one digit
   *  - if they do have more prepend a decimal number corresponding to the number of digits for
   *    the base 52 values
   */

  for (unsigned int i = 0; i < getXYZ(); i++) {
    if (isEmpty(i))
      cont += "_";
    else {
      unsigned int val = pieceNumber(i);

      if (val == 0)
        cont += 'A';
      else {

        unsigned int tmp2 = val;
        unsigned int len = 0;

        while (tmp2) {
          tmp2 /= 52;
          len ++;
        }

        if (len > 1) {

          snprintf(tmp, 50, "%i", len);
          cont += tmp;
        }

        while (len) {
          unsigned int len2 = len-1;
          tmp2 = val;
          while (len2) {
            tmp2 /= 52;
            len2--;
          }

          tmp2 %= 52;

          if (tmp2 < 26)
            cont += char('A' + tmp2);
          else
            cont += char('a' + (tmp2-26));

          len--;
        }
      }
    }
  }

  nd.set_content(cont.c_str());

  return nd;
}


assemblyVoxel_c::assemblyVoxel_c(const xml::node & node) : voxel_c(0, 0, 0, 0) {

  // we must have a real node and the following attributes
  assert(node.get_type() == xml::node::type_element);
  assert(strcmp(node.get_name(), "voxel") == 0);

  if (node.get_attributes().find("x") == node.get_attributes().end())
    throw load_error("assembly Voxel with no attribut 'x' encountered", node);
  if (node.get_attributes().find("y") == node.get_attributes().end())
    throw load_error("assembly Voxel with no attribut 'y' encountered", node);
  if (node.get_attributes().find("z") == node.get_attributes().end())
    throw load_error("assembly Voxel with no attribut 'z' encountered", node);
  if (node.get_attributes().find("type") == node.get_attributes().end())
    throw load_error("assembly Voxel with no attribut 'type' encountered", node);

  setOutside(VX_EMPTY);

  // set to the correct size
  resize(atoi(node.get_attributes().find("x")->get_value()),
         atoi(node.get_attributes().find("y")->get_value()),
         atoi(node.get_attributes().find("z")->get_value()), 0);

  unsigned int type = atoi(node.get_attributes().find("type")->get_value());


  const char * c = node.get_content();
  int idx = 0;

  if (c) {

    if (type != 1)
      throw load_error("assembly Voxel with type not equal to 1 encountetred", node);

    unsigned int len;
    unsigned int val;

    len = 0;
    val = 0;
    bool charMode = false;

    while (*c) {

      if (*c == '_') {

        if (charMode)
          throw load_error("'_' in the middle of a multi digit color name encountered in assembly voxel space", node);
        clear(idx++);

      } else if (*c >= '0' && *c <= '9') {

        if (charMode)
          throw load_error("number in the middle of a multi digit color name encountered in assembly voxel space", node);
        len = len * 10 + *c - '0';

      } else {

        if (*c >= 'A' && *c <= 'Z') {
          val = val * 52 + *c - 'A';
          charMode = true;
        } else if (*c >= 'a' && *c <= 'z') {
          val = val * 52 + *c - 'a' + 26;
          charMode = true;
        } else {
          throw load_error("unrecognized character in assembly voxel space", node);
        }

        if (len) len--;

        if (!len) {
          setPiece(idx++, val);
          val = 0;
          charMode = false;
        }
      }

      c++;
    }
  }
}

