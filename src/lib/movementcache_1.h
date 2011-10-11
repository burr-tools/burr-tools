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
#ifndef __MOVEMENTCACHE_1_H__
#define __MOVEMENTCACHE_1_H__

#include "movementcache.h"

/** movement cache for the triangle grid */
class movementCache_1_c : public movementCache_c {

  public:

    movementCache_1_c(const problem_c & puz);

  private:

    unsigned int* moCalcValues(const voxel_c * sh1, const voxel_c * sh2, int dx, int dy, int dz);

    unsigned int numDirections(void);
    void getDirection(unsigned int dir, int * x, int * y, int * z);

  private:

    // no copying and assigning
    movementCache_1_c(const movementCache_1_c&);
    void operator=(const movementCache_1_c&);
};

#endif
