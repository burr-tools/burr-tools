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
#include "movementcache_1.h"

#include "voxel.h"

#define NUM_DIRECTIONS 4
#define NUM_CHECKS 3
#define NUM_VOXELTYPES 2

/* the directions to move one voxel exacly one unit in the diven direction
 */
const static int dirs[NUM_DIRECTIONS][3] = {
  {  0,  0,  1 },
  {  2,  0,  0 },
  {  1,  1,  0 },
  { -1,  1,  0 },
};

/* this array contains
 *   for all NUM_DIRECTIONS directions
 *   for all 2 voxel types
 *   up to x coordinates that must be empty to move that voxel
 *   one unit in that direction
 *
 *   in this case the table was manually created
 */
const static int checks[NUM_DIRECTIONS][NUM_VOXELTYPES][NUM_CHECKS][3] = {
  { // direction 0  -> 0 0 1
    {  { 0,  0,  1}, {-10,  0,  0}  }, // 1 entry
    {  { 0,  0,  1}, {-10,  0,  0}  }, // 1 entry
  },
  { // direction 1  -> 2 0 0
    {  { 1,  0,  0}, { 2,  0,  0}, {-10,  0,  0}  }, // 2 entries
    {  { 1,  0,  0}, { 2,  0,  0}, {-10,  0,  0}  }, // 2 entries
  },
  { // direction 2  -> 1 1 0
    {  { 1,  0,  0}, { 1,  1,  0}, {-10,  0,  0}  }, // 2 entries
    {  { 0,  1,  0}, { 1,  1,  0}, {-10,  0,  0}  }, // 1 entries
  },
  { // direction 2  -> -1 1 0
    {  {-1,  0,  0}, {-1,  1,  0}, {-10,  0,  0}  }, // 2 entries
    {  { 0,  1,  0}, {-1,  1,  0}, {-10,  0,  0}  }, // 1 entries
  }
};

movementCache_1_c::movementCache_1_c(const problem_c & puz) : movementCache_c(puz) {
}

/* calculate the required movement possibilities */
unsigned int* movementCache_1_c::moCalcValues(const voxel_c * sh1, const voxel_c * sh2, int dx, int dy, int dz) {

  /* because the dx, dy and dz values are calculated using the hotspot we need to reverse
   * that process
   */
  dx += (sh1->getHx() - sh2->getHx());
  dy += (sh1->getHy() - sh2->getHy());
  dz += (sh1->getHz() - sh2->getHz());

  unsigned int * move = new unsigned int[NUM_DIRECTIONS];

  for (unsigned int dir = 0; dir < NUM_DIRECTIONS; dir++) {

    unsigned int m = 32000;

    for (unsigned int x = sh1->boundX1(); x <= sh1->boundX2(); x++)
      for (unsigned int y = sh1->boundY1(); y <= sh1->boundY2(); y++)
        for (unsigned int z = sh1->boundZ1(); z <= sh1->boundZ2(); z++) {

          if (sh1->isEmpty(x, y, z)) continue;

          int voxel = (x+y) % 2;

          int xp = (int)x-dx;
          int yp = (int)y-dy;
          int zp = (int)z-dz;

          bt_assert(sh2->isEmpty2(xp, yp, zp));

          /* lets check, we only need to check up to the current maximum possible
           * distance, everything above is not possible any way
           */
          for (unsigned int d = 0; d < m; d++) {

            /* first see, if we are within the bounding box of sh1 and if we ever
             * might get in there */
            if (dirs[dir][0] > 0 && xp > (int)sh2->boundX2()+2) break;
            if (dirs[dir][0] < 0 && xp < (int)sh2->boundX1()-2) break;
            if (dirs[dir][1] > 0 && yp > (int)sh2->boundY2()+2) break;
            if (dirs[dir][1] < 0 && yp < (int)sh2->boundY1()-2) break;
            if (dirs[dir][2] > 0 && zp > (int)sh2->boundZ2()+2) break;
            if (dirs[dir][2] < 0 && zp < (int)sh2->boundZ1()-2) break;

            for (const int (*ch)[3] = checks[dir][voxel]; (*ch)[0] != -10; ch++)
            {
              if (sh2->isFilled2(xp+ (*ch)[0], yp+ (*ch)[1], zp+ (*ch)[2]))
              {
                if (d < m) m = d;
                break;
              }
            }

            xp += dirs[dir][0];
            yp += dirs[dir][1];
            zp += dirs[dir][2];
          }

          // shortcut for m == 0 we don't need to continue

          if (m == 0) {
            x = sh1->boundX2();
            y = sh1->boundY2();
            z = sh1->boundZ2();
          }
        }

    move[dir] = m;
  }

  return move;
}

unsigned int movementCache_1_c::numDirections(void) { return NUM_DIRECTIONS; }
void movementCache_1_c::getDirection(unsigned int dir, int * x, int * y, int * z)
{
  bt_assert(dir < NUM_DIRECTIONS);

  *x = dirs[dir][0];
  *y = dirs[dir][1];
  *z = dirs[dir][2];
}
