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
#ifndef __MOVEMENTCACHE_0_H__
#define __MOVEMENTCACHE_0_H__

#include "movementcache.h"

/* this class calculates the possible movement between 2 pieces
 * because that calculation is relatively expensive it caches
 * that value. That's the reason for the name
 */
class movementCache_0_c : public movementCache_c {

  public:

    movementCache_0_c(const puzzle_c * puz, unsigned int problem) : movementCache_c(puz, problem, 3) {}

  private:

    void calcValues(entry * e, const voxel_c * sh1, const voxel_c * sh2, int dx, int dy, int dz);

    virtual unsigned int numDirections(void);
    virtual void getDirection(unsigned int dir, int * x, int * y, int * z);
};

#endif
