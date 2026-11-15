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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "movementcache_lattice.h"

#include "latticegridinfo.h"
#include "voxel.h"
#include "bt_assert.h"

movementCache_lattice_c::movementCache_lattice_c(const problem_c & puz, const latticeGridInfo_c & info)
  : movementCache_c(puz), inf(info) {
  bt_assert(inf.canDisassemble());
}

unsigned int movementCache_lattice_c::numDirections(void) {
  return inf.directions();
}

void movementCache_lattice_c::getDirection(unsigned int dir, int * x, int * y, int * z) {
  bt_assert(dir < inf.directions());
  const auto & d = inf.direction(dir);
  *x = d[0];
  *y = d[1];
  *z = d[2];
}

/* How far the first shape can move in each direction before it hits the
 * second, which sits at (dx, dy, dz) relative to it: every voxel of the
 * first steps along the direction until one of the cells it must find
 * empty is filled in the second, or until it is past the second's box. The
 * same scheme as movementCache_1_c, with NG's check cells.
 */
std::vector<unsigned int> movementCache_lattice_c::moCalcValues(const voxel_c * sh1, const voxel_c * sh2, int dx, int dy, int dz) {

  // the offsets are between hotspots, the loops need them between the spaces
  dx += (sh1->getHx() - sh2->getHx());
  dy += (sh1->getHy() - sh2->getHy());
  dz += (sh1->getHz() - sh2->getHz());

  const unsigned int dirs = inf.directions();
  const int ext = inf.checkReach() + 1;
  std::vector<unsigned int> move(dirs);

  for (unsigned int dir = 0; dir < dirs; dir++) {

    const auto & d = inf.direction(dir);
    unsigned int m = 32000;

    for (unsigned int z = sh1->boundZ1(); z <= sh1->boundZ2() && m; z++)
      for (unsigned int y = sh1->boundY1(); y <= sh1->boundY2() && m; y++)
        for (unsigned int x = sh1->boundX1(); x <= sh1->boundX2() && m; x++) {

          if (sh1->isEmpty(x, y, z)) continue;

          // the voxel's kind: both spaces are a lattice vector apart, so it is the same in either
          const int base = inf.base(x, y, z);
          bt_assert(base >= 0);
          const auto & cells = inf.moveChecks(base, dir);

          int xp = (int)x - dx;
          int yp = (int)y - dy;
          int zp = (int)z - dz;

          for (unsigned int s = 0; s < m; s++) {

            // past the second shape's box in the direction of travel: it can not be hit any more
            if (d[0] > 0 && xp > (int)sh2->boundX2() + ext) break;
            if (d[0] < 0 && xp < (int)sh2->boundX1() - ext) break;
            if (d[1] > 0 && yp > (int)sh2->boundY2() + ext) break;
            if (d[1] < 0 && yp < (int)sh2->boundY1() - ext) break;
            if (d[2] > 0 && zp > (int)sh2->boundZ2() + ext) break;
            if (d[2] < 0 && zp < (int)sh2->boundZ1() - ext) break;

            bool hit = false;
            for (const auto & c : cells)
              if (sh2->isFilled2(xp + c[0], yp + c[1], zp + c[2])) {
                hit = true;
                break;
              }
            if (hit) {
              m = s;
              break;
            }

            xp += d[0];
            yp += d[1];
            zp += d[2];
          }
        }

    move[dir] = m;
  }

  return move;
}
