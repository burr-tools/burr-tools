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
#include "disassemblernode.h"

#include "assembly.h"

disassemblerNode_c::disassemblerNode_c(unsigned int pn, disassemblerNode_c * comf, int _dir, int _amount, int step) : comefrom(comf), piecenumber(pn), refcount(1), dir(_dir), amount(_amount) {
  dat = new signed char[4*piecenumber];

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

  dat = new signed char[4*piecenumber];

  hashValue = 0;

  /* create pieces field. This field contains the
   * names of all present pieces. Because at the start
   * all pieces are still there we fill the array
   * with all the numbers
   */
  unsigned int pc = 0;
  for (unsigned int j = 0; j < assm->placementCount(); j++)
    if (assm->isPlaced(j)) {
      bt_assert(
          abs(assm->getX(j)) < 128 &&
          abs(assm->getY(j)) < 128 &&
          abs(assm->getZ(j)) < 128);

      dat[4*pc+0] = assm->getX(j);
      dat[4*pc+1] = assm->getY(j);
      dat[4*pc+2] = assm->getZ(j);
      dat[4*pc+3] = assm->getTransformation(j);
      pc++;
    }
}

disassemblerNode_c::~disassemblerNode_c() {

  delete [] dat;

  if (comefrom) {
    comefrom->refcount--;
    if (comefrom->refcount == 0)
      delete comefrom;
  }
}

void disassemblerNode_c::replaceNode(const disassemblerNode_c *n) {
  bt_assert(piecenumber == n->piecenumber);

  memcpy(dat, n->dat, 4*piecenumber*sizeof(unsigned char));

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

  for (unsigned int i = 1; i < piecenumber; i++) {
    h += (dat[4*i+0]-dat[0]);
    h *= 1343;
    h += (dat[4*i+1]-dat[1]);
    h *= 923;
    h += (dat[4*i+2]-dat[2]);
    h *= 113;
    h += (dat[4*i+3]);
    h *= 23;
  }

  const_cast<disassemblerNode_c*>(this)->hashValue = h;
  return h;
}

bool disassemblerNode_c::operator == (const disassemblerNode_c &b) const {

  for (unsigned int i = 1; i < piecenumber; i++) {
    if (dat[4*i+0] - dat[0] != b.dat[4*i+0] - b.dat[0]) return false;
    if (dat[4*i+1] - dat[1] != b.dat[4*i+1] - b.dat[1]) return false;
    if (dat[4*i+2] - dat[2] != b.dat[4*i+2] - b.dat[2]) return false;
    if (dat[4*i+3] != b.dat[4*i+3]) return false;
  }

  return true;
}

bool disassemblerNode_c::is_separation() const {
  for (unsigned int i = 0; i < piecenumber; i++)
    if (is_piece_removed(i))
      return true;

  return false;
}

