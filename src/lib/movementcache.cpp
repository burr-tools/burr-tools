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
#include "movementcache.h"

#include "voxel.h"
#include "problem.h"

#include <string.h>

/* the hash function. I don't know how well it performs, but it seems to be okay */
static unsigned int moHashValue(unsigned int s1, unsigned int s2, int dx, int dy, int dz, unsigned char t1, unsigned char t2) {
  unsigned int val = dx * 0x10101010;
  val +=             dy * 0x14814814;
  val +=             dz * 0x95145951;
  val +=             t1 * 0x1A54941A;
  val +=             t2 * 0x5AA59401;
  val +=             s1 * 0x01059a04;
  val +=             s2 * 0x9af42682;
  return val;
}

/* double the hash table size and copy the old elements into
 * the new table
 */
void movementCache_c::moRehash(void) {
  unsigned int oldSize = moTableSize;

  /* the new size, roughly twice the old size but odd */
  moTableSize = 2*moTableSize + 1;

  /* allocate new table */
  moEntry ** newHash = new moEntry * [moTableSize];
  memset(newHash, 0, moTableSize * sizeof(moEntry*));

  /* copy the elements */
  for (unsigned int i = 0; i < oldSize; i++) {

    while (moHash[i]) {

      /* remove from old table */
      moEntry * e = moHash[i];
      moHash[i] = e->next;

      /* enter into new one */
      unsigned int h = moHashValue(e->s1, e->s2, e->dx, e->dy, e->dz, e->t1, e->t2) % moTableSize;
      e->next = newHash[h];
      newHash[h] = e;
    }
  }

  /* delete the old table and make the new table the current one */
  delete [] moHash;
  moHash = newHash;
}

movementCache_c::movementCache_c(const problem_c * puzzle) : gt(puzzle->getGridType()) {

  /* initial table */
  moTableSize = 101;
  moHash = new moEntry * [moTableSize];
  memset(moHash, 0, moTableSize * sizeof(moEntry*));
  moEntries = 0;

  /* initialize the shape array with the shapes from the
   * puzzle problem. The shape with transformation 0 is just
   * a pointer into the puzzle, so don't delete them later on
   */
  num_shapes = puzzle->shapeNumber();

  num_transformations = puzzle->getGridType()->getSymmetries()->getNumTransformations();

  shapes = new const voxel_c ** [num_shapes];
  for (unsigned int s = 0; s < num_shapes; s++) {
    shapes[s] = new const voxel_c * [num_transformations];
    memset(shapes[s], 0, num_transformations * sizeof(voxel_c*));
    shapes[s][0] = puzzle->getShapeShape(s);
  }

  /* initialize the piece array */
  pieces = new unsigned int [puzzle->pieceNumber()];

  int pos = 0;

  for (unsigned int s = 0; s < puzzle->shapeNumber(); s++)
    for (unsigned int i = 0; i < puzzle->getShapeMax(s); i++)
      pieces[pos++] = s;

}

movementCache_c::~movementCache_c() {

  /* delete the hash nodes */
  for (unsigned int i = 0; i < moTableSize; i++) {

    while (moHash[i]) {
      moEntry * e = moHash[i];
      moHash[i] = e->next;

      delete [] e->move;
      delete e;
    }
  }
  delete [] moHash;

  /* the shape with transformation 0 is just
   * a pointer into the puzzle, so don't delete them
   *
   * but all the others are created by us, so we free them
   */
  for (unsigned int s = 0; s < num_shapes; s++) {
    for (unsigned int t = 1; t < num_transformations; t++)
      if (shapes[s][t])
        delete shapes[s][t];
    delete [] shapes[s];
  }

  delete [] shapes;
  delete [] pieces;
}

const voxel_c * movementCache_c::getTransformedShape(unsigned int s, unsigned char t) {

  if (!shapes[s][t])
  {
    // our required orientation doesn't exist, so we calculate it

    voxel_c * sh = gt->getVoxel(shapes[s][0]);
    bt_assert2(sh->transform(t));

    // in the single threaded case simply enter our new shape
    shapes[s][t] = sh;
  }

  return shapes[s][t];
}

void movementCache_c::getMoValue(int dx, int dy, int dz, unsigned char t1, unsigned char t2, unsigned int p1, unsigned int p2, unsigned int * movements)
{
  /* find out the shapes that the pieces have */
  unsigned int s1 = pieces[p1];
  unsigned int s2 = pieces[p2];

  unsigned int h = moHashValue(s1, s2, dx, dy, dz, t1, t2);

  moEntry * e = moHash[h % moTableSize];

  /* check the list of nodes in the current hash bucket */
  while (e && (e->dx != dx || e->dy != dy || e->dz != dz ||
               e->t1 != t1 || e->t2 != t2 || e->s1 != s1 || e->s2 != s2))
    e = e->next;

  /* check, if we found the required node */
  if (!e)
  {
    /* no is not found, enter a new node into the table */
    e = new moEntry;
    e->dx = dx; e->dy = dy; e->dz = dz;
    e->t1 = t1; e->t2 = t2;
    e->s1 = s1; e->s2 = s2;
    e->move = moCalcValues(getTransformedShape(s1, t1), getTransformedShape(s2, t2), dx, dy, dz);

    if (++moEntries > moTableSize) moRehash();

    e->next = moHash[h % moTableSize];
    moHash[h % moTableSize] = e;
  }

  /* return the values */
  memcpy(movements, e->move, numDirections()*sizeof(unsigned int));
}
