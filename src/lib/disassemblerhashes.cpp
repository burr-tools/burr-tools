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
#include "disassemblerhashes.h"

#include "disassemblernode.h"

nodeHash::nodeHash(void) {

  tab_size = 11;
  tab_entries = 0;

  tab = new disassemblerNode_c* [tab_size];

  memset(tab, 0, tab_size*sizeof(disassemblerNode_c*));
}

nodeHash::~nodeHash(void) {
  clear();

  delete [] tab;
}

void nodeHash::clear(void)
{
  for (unsigned int i = 0; i < tab_size; i++) {
    while (tab[i]) {
      disassemblerNode_c * n = tab[i];
      tab[i] = n->next;

      if (n->decRefCount())
        delete n;
    }
  }

  tab_entries = 0;
}

const disassemblerNode_c * nodeHash::insert(disassemblerNode_c * n) {

  unsigned long h = n->hash() % tab_size;

  disassemblerNode_c * hn = tab[h];

  while (hn) {
    if (*hn == *n) {

      // let's see, a node for this state already exists, if the found way to this
      // node is longer than the current way, we replace it with the data of the current
      // node
      if (hn->getWaylength() > n->getWaylength())
        hn->replaceNode(n);

      return hn;
    }

    hn = hn->next;
  }

  /* node not in table, insert */
  n->incRefCount();

  n->next = tab[h];
  tab[h] = n;

  tab_entries++;
  if (tab_entries > tab_size) {
    // rehash

    unsigned long new_size = tab_size * 4 + 1;

    disassemblerNode_c ** new_tab = new disassemblerNode_c* [new_size];
    memset(new_tab, 0, new_size*sizeof(disassemblerNode_c*));

    for (unsigned int i = 0; i < tab_size; i++) {
      while (tab[i]) {
        disassemblerNode_c * n = tab[i];
        tab[i] = n->next;
        unsigned long h = n->hash() % new_size;
        n->next = new_tab[h];
        new_tab[h] = n;
      }
    }

    delete[] tab;
    tab = new_tab;
    tab_size = new_size;
  }

  return 0;
}

bool nodeHash::contains(const disassemblerNode_c * n) const {
  unsigned long h = n->hash() % tab_size;

  disassemblerNode_c * hn = tab[h];

  while (hn) {
    if (*hn == *n)
      return true;

    hn = hn->next;
  }

  return false;
}



countingNodeHash::countingNodeHash(void) {

  tab_size = 100;
  tab_entries = 0;

  tab = new hashNode * [tab_size];

  memset(tab, 0, tab_size*sizeof(hashNode*));

  scanPtr = 0;
  scanActive = false;

  linkStart = 0;
}

countingNodeHash::~countingNodeHash(void)
{
  clear();
  delete [] tab;
}

/* delete all nodes and empty table for new usage */
void countingNodeHash::clear(void)
{
  hashNode * hn = linkStart;

  while (hn) {
    hashNode * hn2 = hn->link;

    if (hn->dat->decRefCount())
      delete hn->dat;

    delete hn;

    hn = hn2;
  }

  memset(tab, 0, tab_size*sizeof(hashNode*));
  tab_entries = 0;
  linkStart = 0;
}

bool countingNodeHash::insert(disassemblerNode_c * n) {

  unsigned long h = n->hash() % tab_size;

  hashNode * hn = tab[h];

  while (hn) {
    if (*(hn->dat) == *n)
      return true;

    hn = hn->next;
  }

  /* node not in table, insert */
  n->incRefCount();

  hn = new hashNode;
  hn->dat = n;

  hn->next = tab[h];
  tab[h] = hn;

  hn->link = linkStart;
  linkStart = hn;

  tab_entries++;
  if (tab_entries > tab_size) {

    unsigned long new_size = tab_size * 4 + 1;

    hashNode ** new_tab = new hashNode* [new_size];
    memset(new_tab, 0, new_size*sizeof(hashNode*));

    for (unsigned int i = 0; i < tab_size; i++) {
      while (tab[i]) {
        hashNode * hn = tab[i];
        tab[i] = hn->next;
        unsigned long h = hn->dat->hash() % new_size;
        hn->next = new_tab[h];
        new_tab[h] = hn;
      }
    }

    delete[] tab;
    tab = new_tab;
    tab_size = new_size;
  }

  return false;
}

void countingNodeHash::initScan(void) {

  bt_assert(!scanActive);

  scanPtr = linkStart;
  scanActive = true;
}

const disassemblerNode_c * countingNodeHash::nextScan(void) {

  bt_assert(scanActive);

  if (!scanPtr) {
    scanActive = false;
    return 0;

  } else {

    disassemblerNode_c * res = scanPtr->dat;
    scanPtr = scanPtr->link;

    return res;
  }
}


