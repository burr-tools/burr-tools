/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
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
#include "disassemblerhashes.h"

#include "disassemblernode.h"

size_t disassemblerNodePtrHash::operator()(const disassemblerNode_c * n) const noexcept {
  return n->hash();
}

bool disassemblerNodePtrEqual::operator()(const disassemblerNode_c * a, const disassemblerNode_c * b) const noexcept {
  return *a == *b;
}

nodeHash::nodeHash(void) {
}

nodeHash::~nodeHash(void) {
  clear();
}

void nodeHash::clear(void)
{
  for (disassemblerNode_c * n : tab) {
    if (n->decRefCount())
      delete n;
  }

  tab.clear();
}

const disassemblerNode_c * nodeHash::insert(disassemblerNode_c * n) {

  // single lookup: insert() returns the existing element on collision,
  // so no separate find() probe (which would hash and walk twice on miss)
  auto [it, inserted] = tab.insert(n);

  if (!inserted) {
    disassemblerNode_c * hn = *it;

    // let's see, a node for this state already exists, if the found way to this
    // node is longer than the current way, we replace it with the data of the current
    // node
    if (hn->getWaylength() > n->getWaylength())
      hn->replaceNode(n);

    return hn;
  }

  /* node not in table, insert */
  n->incRefCount();

  return 0;
}

bool nodeHash::contains(const disassemblerNode_c * n) const {
  // unordered_set::find takes the key type (non-const pointer); the lookup
  // does not mutate the node, so the const_cast is safe
  return tab.find(const_cast<disassemblerNode_c*>(n)) != tab.end();
}



countingNodeHash::countingNodeHash(void) {
}

countingNodeHash::~countingNodeHash(void)
{
  clear();
}

/* delete all nodes and empty table for new usage */
void countingNodeHash::clear(void)
{
  for (disassemblerNode_c * n : order) {
    if (n->decRefCount())
      delete n;
  }

  tab.clear();
  order.clear();
  scanPos = 0;
  scanActive = false;
}

bool countingNodeHash::insert(disassemblerNode_c * n) {

  // single lookup, see nodeHash::insert
  auto [it, inserted] = tab.insert(n);

  if (!inserted)
    return true;

  /* node not in table, insert */
  n->incRefCount();

  order.push_back(n);

  return false;
}

void countingNodeHash::initScan(void) {

  bt_assert(!scanActive);

  scanPos = order.size();
  scanActive = true;
}

const disassemblerNode_c * countingNodeHash::nextScan(void) {

  bt_assert(scanActive);

  if (scanPos == 0) {
    scanActive = false;
    return 0;

  } else {

    return order[--scanPos];
  }
}
