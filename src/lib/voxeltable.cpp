/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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
#include "voxeltable.h"

#include "voxel.h"
#include "puzzle.h"

voxelTable_c::voxelTable_c(bool mirrors) : includeMirrors(mirrors) {

  tableSize = 101;
  hashTable = new hashNode*[tableSize];

  for (unsigned long i = 0; i < tableSize; i++)
    hashTable[i] = 0;

  tableEntries = 0;
}

voxelTable_c::~voxelTable_c(void) {

  for (unsigned int i = 0; i < tableSize; i++) {
    while (hashTable[i]) {

      hashNode * n = hashTable[i];
      hashTable[i] = n->next;

      delete n;
    }
  }

  delete [] hashTable;
}

static unsigned long calcHashValue(const voxel_c * v)
{
  unsigned long res = 101;

  res *= v->getX();
  res += 11;
  res *= v->getY();
  res += 13;
  res *= v->getZ();
  res += 17;

  for (unsigned int z = v->boundZ1(); z <= v->boundZ2(); z++)
    for (unsigned int y = v->boundY1(); y <= v->boundY2(); y++)
      for (unsigned int x = v->boundX1(); x <= v->boundX2(); x++)
      {
        res *= 101;
        res += v->getState(x, y, z);
      }

  return res;
}

static unsigned long calcColourHashValue(const voxel_c * v)
{
  unsigned long res = 101;

  res *= v->getX();
  res += 11;
  res *= v->getY();
  res += 13;
  res *= v->getZ();
  res += 17;

  for (unsigned int z = v->boundZ1(); z <= v->boundZ2(); z++)
    for (unsigned int y = v->boundY1(); y <= v->boundY2(); y++)
      for (unsigned int x = v->boundX1(); x <= v->boundX2(); x++)
      {
        res *= 101;
        res += v->get(x, y, z);
      }

  return res;
}

bool voxelTable_c::getSpace(const voxel_c *v, unsigned int *index, unsigned char * trans) const
{
  unsigned long hash = calcHashValue(v);

  hashNode * n = hashTable[hash % tableSize];

  while (n)
  {
    if (n->hash == hash)
    {
      voxel_c * v2 = v->getGridType()->getVoxel(findSpace(n->index));
      bool found = v2->transform(n->transformation) && v->identicalInBB(v2, false);
      delete v2;

      if (found)
      {
        if (index) *index = n->index;
        if (trans) *trans = n->transformation;

        return true;
      }
    }
    n = n->next;
  }

  return false;
}

bool voxelTable_c::getSpaceNoRot(const voxel_c *v, unsigned int *index, unsigned char * trans) const
{
  unsigned long hash = calcHashValue(v);

  hashNode * n = hashTable[hash % tableSize];

  while (n)
  {
    if ((n->hash == hash) && (n->transformation < v->getGridType()->getSymmetries()->getNumTransformations()))
    {
      voxel_c * v2 = v->getGridType()->getVoxel(findSpace(n->index));
      bool found = v2->transform(n->transformation) && v->identicalInBB(v2, false);
      delete v2;

      if (found)
      {
        if (index) *index = n->index;
        if (trans) *trans = n->transformation;

        return true;
      }
    }
    n = n->next;
  }

  return false;
}

bool voxelTable_c::getSpaceColour(const voxel_c *v, unsigned int *index, unsigned char * trans) const
{
  unsigned long hash = calcColourHashValue(v);

  hashNode * n = hashTable[hash % tableSize];

  while (n)
  {
    if (n->hash == hash)
    {
      voxel_c * v2 = v->getGridType()->getVoxel(findSpace(n->index));
      bool found = v2->transform(n->transformation) && v->identicalInBB(v2, true);
      delete v2;

      if (found)
      {
        if (index) *index = n->index;
        if (trans) *trans = n->transformation;

        return true;
      }
    }
    n = n->next;
  }

  return false;
}

void voxelTable_c::addSpace(const voxel_c * v, unsigned int index)
{
  const symmetries_c * symm = v->getGridType()->getSymmetries();

  unsigned char endTrans = (includeMirrors)
    ? symm->getNumTransformationsMirror()
    : symm->getNumTransformations();

  symmetries_t sym = v->selfSymmetries();

  if (tableEntries >= tableSize) {
    // rehash table

    unsigned long newSize = 2*tableSize + 1;
    hashNode ** t2 = new hashNode * [newSize];

    for (unsigned long i = 0; i < newSize; i++)
      t2[i] = 0;

    for (unsigned int i = 0; i < tableSize; i++) {
      while (hashTable[i]) {
        hashNode * n = hashTable[i];
        hashTable[i] = n->next;

        n->next = t2[n->hash % newSize];
        t2[n->hash % newSize] = n;
      }
    }

    delete [] hashTable;
    hashTable = t2;
    tableSize = newSize;
  }

  for (unsigned char trans = 0; trans < endTrans; trans++) {
    if (symm->isTransformationUnique(sym, trans)) {
      // add all transformations of the voxel space to the table, that are actually different
      // the additional trans==0 is necessary because the symmetry mask always contains the identity
      // transformation and thus we would not ass the untransformed space

      voxel_c * v2 = v->getGridType()->getVoxel(v);
      v2->transform(trans);
      unsigned long hash = calcHashValue(v2);
      unsigned long hashColour = calcColourHashValue(v2);
      delete v2;

      hashNode * n = new hashNode;
      n->index = index;
      n->transformation = trans;
      n->hash = hash;
      n->next = hashTable[hash % tableSize];
      hashTable[hash % tableSize] = n;
      tableEntries++;

      n = new hashNode;
      n->index = index;
      n->transformation = trans;
      n->hash = hashColour;
      n->next = hashTable[hashColour % tableSize];
      hashTable[hashColour % tableSize] = n;
      tableEntries++;
    }
  }
}

const voxel_c * voxelTablePuzzle_c::findSpace(unsigned int index) const { return puzzle->getShape(index); }

