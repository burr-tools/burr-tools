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
#include "movementcache.h"

#include "voxel.h"
#include "problem.h"
#include "puzzle.h"

#include <string.h>

/* the hash function. I don't know how well it performs, but it seems to be okay */
static unsigned int moHashValue(unsigned int s1, unsigned int s2, int dx, int dy, int dz, unsigned char t1, unsigned char t2) {
  /* unsigned constants so the multiplications are done in unsigned
   * arithmetic; with signed int operands dx/dy/dz these products overflowed,
   * which is undefined behaviour. The computed hash is unchanged: two's
   * complement +/- and * agree with unsigned arithmetic modulo 2^32.
   */
  unsigned int val = dx * 0x10101010u;
  val +=             dy * 0x14814814u;
  val +=             dz * 0x95145951u;
  val +=             t1 * 0x1A54941Au;
  val +=             t2 * 0x5AA59401u;
  val +=             s1 * 0x01059a04u;
  val +=             s2 * 0x9af42682u;
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
  std::vector<moEntry*> newHash(moTableSize, nullptr);

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

  moHash = std::move(newHash);
}

movementCache_c::movementCache_c(const problem_c & puzzle)
  : moHash(101, nullptr),
    moTableSize(101),
    moEntries(0),
    shapes(puzzle.getNumberOfParts(), std::vector<const voxel_c*>(puzzle.getPuzzle().getGridType()->getSymmetries()->getNumTransformations(), nullptr)),
    pieces(puzzle.getNumberOfPieces()),
    num_shapes(puzzle.getNumberOfParts()),
    num_transformations(puzzle.getPuzzle().getGridType()->getSymmetries()->getNumTransformations()),
    gt(puzzle.getPuzzle().getGridType()) {

  /* Initialise the shape array with the shapes from the
   * puzzle problem. The shape with transformation 0 is just
   * a pointer into the puzzle, so don't delete them later on
   */
  for (unsigned int s = 0; s < num_shapes; s++) {
    shapes[s][0] = puzzle.getPartShape(s);
  }

  /* Initialise the piece array */
  int pos = 0;

  for (unsigned int s = 0; s < puzzle.getNumberOfParts(); s++)
    for (unsigned int i = 0; i < puzzle.getPartMaximum(s); i++)
      pieces[pos++] = s;
}

movementCache_c::~movementCache_c() {

  /* delete the hash nodes */
  for (unsigned int i = 0; i < moTableSize; i++) {

    while (moHash[i]) {
      moEntry * e = moHash[i];
      moHash[i] = e->next;

      delete e;
    }
  }

  /* the shape with transformation 0 is just
   * a pointer into the puzzle, so don't delete them
   *
   * but all the others are created by us, so we free them
   */
  for (unsigned int s = 0; s < num_shapes; s++) {
    for (unsigned int t = 1; t < num_transformations; t++)
      if (shapes[s][t])
        delete shapes[s][t];
  }
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
  memcpy(movements, e->move.data(), numDirections()*sizeof(unsigned int));
}
