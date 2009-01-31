/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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
#include "disassemblernode.h"

#include "assembly.h"

disassemblerNode_c::disassemblerNode_c(int pn, disassemblerNode_c * comf, int _dir, int _amount, int step) : comefrom(comf), piecenumber(pn), refcount(1), dir(_dir), amount(_amount) {
  dx = new int[piecenumber];
  dy = new int[piecenumber];
  dz = new int[piecenumber];
  trans = new unsigned int[piecenumber];

  if (comefrom)
    comefrom->refcount++;

  hashValue = 0;

  if (comf)
    waylength = comf->waylength + step;
  else
    waylength = step;
}

disassemblerNode_c::disassemblerNode_c(const assembly_c * assm) : comefrom(0), piecenumber(0), refcount(1), dir(0), amount(0), waylength(0) {

  /* create the first node with the start state
   * here all pieces are at position (0; 0; 0)
   */
  for (unsigned int j = 0; j < assm->placementCount(); j++)
    if (assm->isPlaced(j))
      piecenumber++;

  dx = new int[piecenumber];
  dy = new int[piecenumber];
  dz = new int[piecenumber];
  trans = new unsigned int[piecenumber];

  hashValue = 0;

  /* create pieces field. This field contains the
   * names of all present pieces. Because at the start
   * all pieces are still there we fill the array
   * with all the numbers
   */
  unsigned int pc = 0;
  for (unsigned int j = 0; j < assm->placementCount(); j++)
    if (assm->isPlaced(j)) {
      dx[pc] = assm->getX(j);
      dy[pc] = assm->getY(j);
      dz[pc] = assm->getZ(j);
      trans[pc] = assm->getTransformation(j);
      pc++;
    }
}

disassemblerNode_c::~disassemblerNode_c() {
  delete [] dx;
  delete [] dy;
  delete [] dz;
  delete [] trans;

  if (comefrom) {
    comefrom->refcount--;
    if (comefrom->refcount == 0)
      delete comefrom;
  }
}

void disassemblerNode_c::replaceNode(const disassemblerNode_c *n) {
  bt_assert(piecenumber = n->piecenumber);

  memcpy(dx, n->dx, piecenumber*sizeof(int));
  memcpy(dy, n->dy, piecenumber*sizeof(int));
  memcpy(dz, n->dz, piecenumber*sizeof(int));
  memcpy(trans, n->trans, piecenumber*sizeof(int));

  dir = n->dir;
  amount = n->amount;

  hashValue = n->hashValue;

  waylength = n->waylength;

  if (comefrom) {
    comefrom->refcount--;
    if (comefrom->refcount == 0)
      delete comefrom;
  }
  comefrom = n->comefrom;
  comefrom->incRefCount();
}

unsigned int disassemblerNode_c::hash(void) const {

  if (hashValue) return hashValue;

  unsigned int h = 0x17fe3b3c;

  for (int i = 1; i < piecenumber; i++) {
    h += (dx[i]-dx[0]);
    h *= 1343;
    h += (dy[i]-dy[0]);
    h *= 923;
    h += (dz[i]-dz[0]);
    h *= 113;
  }

  const_cast<disassemblerNode_c*>(this)->hashValue = h;
  return h;
}

bool disassemblerNode_c::operator == (const disassemblerNode_c &b) const {

  for (int i = 1; i < piecenumber; i++) {
    if (dx[i] - dx[0] != b.dx[i] - b.dx[0]) return false;
    if (dy[i] - dy[0] != b.dy[i] - b.dy[0]) return false;
    if (dz[i] - dz[0] != b.dz[i] - b.dz[0]) return false;
    // FIXME: transformation is missing
  }

  return true;
}

bool disassemblerNode_c::is_separation() const {
  for (int i = 0; i < piecenumber; i++)
    if (is_piece_removed(i))
      return true;

  return false;
}

