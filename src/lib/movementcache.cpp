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
#include "movementcache.h"

#include "voxel.h"
#include "puzzle.h"

/* the hash function. I don't know how well it performs, but it seems to be okay */
static unsigned int hashValue(unsigned int s1, unsigned int s2, int dx, int dy, int dz, unsigned char t1, unsigned char t2, unsigned int tableSize) {
  unsigned int val = dx * 0x10101010;
  val +=             dy * 0x14814814;
  val +=             dz * 0x95145951;
  val +=             t1 * 0x1A54941A;
  val +=             t2 * 0x5AA59401;
  val +=             s1 * 0x01059a04;
  val +=             s2 * 0x9af42682;
  return val % tableSize;
}

/* double the hash table size and copy the old elements into
 * the new table
 */
void movementCache_c::rehash(void) {
  unsigned int oldSize = tableSize;

  /* the new size, roughly twice the old size but odd */
  tableSize = 2* tableSize + 1;

  /* allocate new table */
  entry ** newHash = new entry * [tableSize];
  memset(newHash, 0, tableSize * sizeof(entry*));

  /* copy the elements */
  for (unsigned int i = 0; i < oldSize; i++) {

#ifdef MV_CACHE_DEBUG
    unsigned long listLen = 0;
#endif

    while (hash[i]) {

      /* remove from old table */
      entry * e = hash[i];
      hash[i] = e->next;

      /* enter into new one */
      unsigned int h = hashValue(e->s1, e->s2, e->dx, e->dy, e->dz, e->t1, e->t2, tableSize);
      e->next = newHash[h];
      newHash[h] = e;

#ifdef MV_CACHE_DEBUG
      listLen++;
#endif
    }

#ifdef MV_CACHE_DEBUG
    if (listLen > maxListLen) maxListLen = listLen;
#endif
  }

  /* delete the old table and make the new table the current one */
  delete [] hash;
  hash = newHash;
}

movementCache_c::movementCache_c(const puzzle_c * puzzle, unsigned int problem, unsigned int dirs) : gt(puzzle->getGridType()), directions(dirs) {

  /* initial table */
  tableSize = 101;
  hash = new entry * [tableSize];
  memset(hash, 0, tableSize * sizeof(entry*));

  entries = 0;

  /* initialize the shape array with the shapes from the
   * puzzle problem. The shape with transformation 0 is just
   * a pointer into the puzzle, so don't delete them later on
   */
  num_shapes = puzzle->probShapeNumber(problem);

  num_transformations = puzzle->getGridType()->getSymmetries()->getNumTransformations();

  shapes = new const voxel_c ** [num_shapes];
  for (unsigned int s = 0; s < num_shapes; s++) {
    shapes[s] = new const voxel_c * [num_transformations];
    memset(shapes[s], 0, num_transformations * sizeof(voxel_c*));
    shapes[s][0] = puzzle->probGetShapeShape(problem, s);
  }

  /* initialize the piece array */
  pieces = new unsigned int [puzzle->probPieceNumber(problem)];

  int pos = 0;

  for (unsigned int s = 0; s < puzzle->probShapeNumber(problem); s++)
    for (unsigned int i = 0; i < puzzle->probGetShapeMax(problem, s); i++)
      pieces[pos++] = s;

#ifdef MV_CACHE_DEBUG
  cacheRequests = 0;
  cacheHits = 0;
  maxListLen = 0;
  cachCollisions = 0;
#endif
}

movementCache_c::~movementCache_c() {

#ifdef MV_CACHE_DEBUG
  fprintf(stderr, "cache had %li hits and %f%% successful retrievals \n", cacheRequests, cacheHits*100.0/cacheRequests);
  fprintf(stderr, "longest list ever seen %i\n", maxListLen);
  fprintf(stderr, "%li cache collisions over the whole and %f collisions per request\n", cachCollisions, cachCollisions*1.0/cacheRequests);
  fprintf(stderr, "last cache table is %i entries big and contains %i entries\n", tableSize, entries);

  maxListLen = 0;
  float avgListLen = 0;
#endif

  /* delete the hash nodes */
  for (unsigned int i = 0; i < tableSize; i++) {

#ifdef MV_CACHE_DEBUG
    unsigned int listLen = 0;
#endif

    while (hash[i]) {
      entry * e = hash[i];
      hash[i] = e->next;

#ifdef MV_CACHE_DEBUG
      listLen++;
#endif

      delete [] e->move;
      delete e;
    }

#ifdef MV_CACHE_DEBUG
    if (listLen > maxListLen) maxListLen = listLen;
    avgListLen += listLen;
#endif
  }

#ifdef MV_CACHE_DEBUG
  printf("last hashtable has average list len %f, max list len %li\n",
      avgListLen/tableSize,
      maxListLen);
#endif

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
  delete [] hash;
  delete [] pieces;
}

void movementCache_c::getValue(int dx, int dy, int dz, unsigned char t1, unsigned char t2, unsigned int p1, unsigned int p2,
    unsigned int dirs, int * movements) {

  bt_assert(dirs == directions);

  /* find out the shapes that the pieces have */
  unsigned int s1 = pieces[p1];
  unsigned int s2 = pieces[p2];

  unsigned int h = hashValue(s1, s2, dx, dy, dz, t1, t2, tableSize);

  entry * e = hash[h];

  /* check the list of nodes in the current hash bucket */
  while (e && ((e->dx != dx) || (e->dy != dy) || (e->dz != dz) ||
               (e->t1 != t1) || (e->t2 != t2) || (e->s1 != s1) || (e->s2 != s2))) {
    e = e->next;

#ifdef MV_CACHE_DEBUG
    cachCollisions++;
#endif

  }

  /* check, if we found the required node */
  if (!e) {

    /* no is not found, enter a new node into the table */

    /* first increase the number of entries, in also the table
     * size, if required */
    entries++;
    if (entries > tableSize) {
      rehash();
      /* after the resize we need to recalculate the hash for the
       * new table size */
      h = hashValue(s1, s2, dx, dy, dz, t1, t2, tableSize);
    }

    e = new entry;
    e->move = new int[directions];

    /* first get the shapes, create them when they are not available */
    const voxel_c * sh1 = shapes[s1][t1];

    if (!sh1) {
      voxel_c * tsh1 = gt->getVoxel(shapes[s1][0]);
      bt_assert(tsh1->transform(t1));
      shapes[s1][t1] = tsh1;
      sh1 = tsh1;
    }

    const voxel_c * sh2 = shapes[s2][t2];

    if (!sh2) {
      voxel_c * tsh2 = gt->getVoxel(shapes[s2][0]);
      bt_assert(tsh2->transform(t2));
      shapes[s2][t2] = tsh2;
      sh2 = tsh2;
    }

    e->dx = dx;
    e->dy = dy;
    e->dz = dz;

    e->t1 = t1;
    e->t2 = t2;

    e->s1 = s1;
    e->s2 = s2;

    /* calculate values and enter them into the table */
    calcValues(e, sh1, sh2, dx, dy, dz);

    e->next = hash[h];
    hash[h] = e;

  } else {
#ifdef MV_CACHE_DEBUG
    cacheHits++;
#endif
  }
#ifdef MV_CACHE_DEBUG
  cacheRequests++;
#endif

  /* return the values */
  for (unsigned int i = 0; i < directions; i++)
    movements[i] = e->move[i];
}

void movementCache_c::removePieceInfo(unsigned int s) {
  /* go through the complete cache and free entries that contain the piece */

  for (unsigned int i = 0; i < tableSize; i++) {
    entry * e = hash[i];
    entry * l = 0;
    while (e) {
      if ((e->s1 == s) || (e->s2 == s)) {
	if (l) {
	  l->next = e->next;
	} else {
	  hash[i] = e->next;
	}

	delete e;
        entries--;
      }
      l = e;
      e = e->next;
    }
  }
}
