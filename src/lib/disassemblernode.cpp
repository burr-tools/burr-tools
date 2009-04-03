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

disassemblerNode_c::disassemblerNode_c(unsigned int pn, disassemblerNode_c * comf, int _dir, int _amount, int step) :
    comefrom(comf), piecenumber(pn), dat(new int16_t[4*piecenumber]),
    refcount(1), dir(_dir), amount(_amount), hashValue(0)
{
  bt_assert(comefrom);

  comefrom->incRefCount();
  waylength = comf->waylength+step;
}

disassemblerNode_c::disassemblerNode_c(unsigned int pn) :
    comefrom(0), piecenumber(pn), dat(new int16_t[4*piecenumber]),
    refcount(1), dir(0), amount(0), hashValue(0), waylength(0)
{
}

disassemblerNode_c::disassemblerNode_c(const assembly_c * assm) :
    comefrom(0), piecenumber(0), refcount(1), dir(0), amount(0), hashValue(0), waylength(0)
{
  /* create the first node with the start state
   * here all pieces are at position (0; 0; 0)
   */
  for (unsigned int j = 0; j < assm->placementCount(); j++)
    if (assm->isPlaced(j))
      piecenumber++;

  dat = new int16_t[4*piecenumber];

  /* create pieces field. This field contains the
   * names of all present pieces. Because at the start
   * all pieces are still there we fill the array
   * with all the numbers
   */
  unsigned int pc = 0;
  for (unsigned int j = 0; j < assm->placementCount(); j++)
    if (assm->isPlaced(j)) {
      bt_assert(
          abs(assm->getX(j)) < maxMove &&
          abs(assm->getY(j)) < maxMove &&
          abs(assm->getZ(j)) < maxMove);

      dat[4*pc+0] = assm->getX(j);
      dat[4*pc+1] = assm->getY(j);
      dat[4*pc+2] = assm->getZ(j);
      dat[4*pc+3] = assm->getTransformation(j);
      pc++;
    }
}

disassemblerNode_c::~disassemblerNode_c() {

  delete [] dat;

  if (comefrom && comefrom->decRefCount())
    delete comefrom;
}

void disassemblerNode_c::replaceNode(const disassemblerNode_c *n) {

  // both nodes must be equal, in the sense that they represent the same state
  // the thing is just that the whole puzzle may be shifted around
  bt_assert(piecenumber == n->piecenumber);
  bt_assert(hashValue == n->hashValue);

  memcpy(dat, n->dat, 4*piecenumber*sizeof(int16_t));
  dir = n->dir;
  amount = n->amount;
  waylength = n->waylength;

  if (comefrom && comefrom->decRefCount())
    delete comefrom;

  comefrom = n->comefrom;

  if (comefrom)
    comefrom->incRefCount();
}

unsigned int disassemblerNode_c::hash(void) const
{
  if (hashValue) return hashValue;

  unsigned int h = 0x17fe3b3c;

  // as the zero-th entry of the transformation
  // is not included in the loop below add it manually
  h += dat[3];

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

  if (h == 0) h = 1;

  hashValue = h;
  return h;
}

bool disassemblerNode_c::operator == (const disassemblerNode_c &b) const
{
  // as the zero-th entry of the transformation
  // is not included in the loop below add it manually
  if (dat[3] != b.dat[3]) return false;

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

