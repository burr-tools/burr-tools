/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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


/* this is going to contain tools to create all pieces that
 * fullfill some kind of property, e.g. all hexominos, or so
 */

#ifndef __PIECEGENERATOR_H__
#define __PIECEGENERATOR_H__

#include "voxel.h"

#include <vector>

class pieceGenerator_c {

  std::vector<pieceVoxel_c *> pieces;

  void recGen(pieceVoxel_c * p);

public:

  // this generates all possible piece variations of the
  // given piece varying the VARIANT voxels inside the
  // piece. The pieces are normal burr pieces for
  // the time being
  pieceGenerator_c(const pieceVoxel_c * piece);

  unsigned long pieceNum(void) const { return pieces.size(); }

  const pieceVoxel_c * getPiece(unsigned long num) {
    return pieces[num];
  }

};

#endif
