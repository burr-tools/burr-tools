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
#include "solver/solverspec.h"

#include <algorithm>
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

movementCache_c::movementCache_c(std::shared_ptr<const solverSpec_t> puz)
  : moTable(1024),
    moMask(1023),
    shapes(puz->getNumberOfParts(), std::vector<const voxel_c*>(puz->getGridType()->getSymmetries()->getNumTransformations(), nullptr)),
    pieces(puz->getNumberOfPieces()),
    num_shapes(puz->getNumberOfParts()),
    num_transformations(puz->getGridType()->getSymmetries()->getNumTransformations()),
    gt(puz->getGridType()),
    spec(std::move(puz)) {

  const solverSpec_t & puzzle = *spec;

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

void movementCache_c::rehashMoTable(size_t new_cap) {
  std::vector<moEntry> new_table(new_cap);
  std::vector<unsigned int> new_wide(new_cap * wideDirections);
  size_t new_mask = new_cap - 1;
  for (const auto &entry : moTable) {
    if (entry.occupied) {
      unsigned int h = moHashValue(entry.key.s1, entry.key.s2, entry.key.dx, entry.key.dy, entry.key.dz, entry.key.t1, entry.key.t2);
      size_t idx = h & new_mask;
      while (new_table[idx].occupied) {
        idx = (idx + 1) & new_mask;
      }
      new_table[idx] = entry;
      if (wideDirections)
        std::copy_n(moWide.begin() + (&entry - moTable.data()) * wideDirections, wideDirections,
                    new_wide.begin() + idx * wideDirections);
    }
  }
  moTable = std::move(new_table);
  moWide = std::move(new_wide);
  moMask = new_mask;
}

void movementCache_c::getMoValue(int dx, int dy, int dz, unsigned char t1, unsigned char t2, unsigned int p1, unsigned int p2, unsigned int * movements)
{
  /* find out the shapes that the pieces have */
  unsigned int s1 = pieces[p1];
  unsigned int s2 = pieces[p2];

  moKey key{s1, s2, dx, dy, dz, t1, t2};
  unsigned int h = moHashValue(s1, s2, dx, dy, dz, t1, t2);

  /* the direction counts are fixed per grid; asked for once, the lookup
   * below runs a few million times per disassembly */
  if (!inlineDirections) [[unlikely]] {
    const unsigned int dirs = numDirections();
    inlineDirections = std::min(dirs, INLINE_DIRECTIONS);
    if (dirs > INLINE_DIRECTIONS) {
      wideDirections = dirs - INLINE_DIRECTIONS;
      moWide.assign(moTable.size() * wideDirections, 0);
    }
  }
  const unsigned int inl = inlineDirections;
  const unsigned int dirs = inl + wideDirections;

  size_t idx = h & moMask;
  while (moTable[idx].occupied) {
    if (moTable[idx].key == key) {
      memcpy(movements, moTable[idx].values.data(), inl * sizeof(unsigned int));
      if (wideDirections) [[unlikely]]
        memcpy(movements + INLINE_DIRECTIONS, moWide.data() + idx * wideDirections, wideDirections * sizeof(unsigned int));
      return;
    }
    idx = (idx + 1) & moMask;
  }

  /* key not found, calculate and enter a new entry into the table */
  std::vector<unsigned int> vals = moCalcValues(getTransformedShape(s1, t1), getTransformedShape(s2, t2), dx, dy, dz);
  bt_assert(vals.size() == dirs);
  moTable[idx].occupied = true;
  moTable[idx].key = key;
  for (size_t i = 0; i < inl; ++i)
    moTable[idx].values[i] = vals[i];
  for (size_t i = INLINE_DIRECTIONS; i < dirs; ++i)
    moWide[idx * wideDirections + (i - INLINE_DIRECTIONS)] = vals[i];

  memcpy(movements, vals.data(), dirs * sizeof(unsigned int));
  moCount++;

  if (moCount * 10 >= moTable.size() * 7) {
    rehashMoTable(moTable.size() * 2);
  }
}
