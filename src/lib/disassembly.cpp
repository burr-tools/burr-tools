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


#include "disassembly.h"

void state_c::print(assemblyVoxel_c *start, voxel_type * pieces) const {

  for (int i = 0; i < piecenumber; i++)
    printf("%c(%i; %i; %i), ", 'a'+pieces[i], dx[i], dy[i], dz[i]);

  printf("\n");

  for (int y = 0;  y < 2 * start->getY(); y++) {
    for (int z = 0; z < 2 * start->getZ(); z++) {
      for (int x = 0; x < 2 * start->getX(); x++) {

        bool foundpiece = false;

        for (int p = 0; p < piecenumber; p++) {
          int x1 = x - dx[p] - start->getX()/2;
          int y1 = y - dy[p] - start->getY()/2;
          int z1 = z - dz[p] - start->getZ()/2;

          if ((x1 >= 0) && (y1 >= 0) && (z1 >= 0) &&
              (x1 < start->getX()) &&
              (y1 < start->getY()) &&
              (z1 < start->getZ()))
            if (start->get(x1, y1, z1) == pieces[p]) {
              printf("%c", 'a' + pieces[p]);
              foundpiece = true;
            }
        }

        if (!foundpiece) printf(" ");
      }

      printf(" | ");
    }

    printf("\n");
  }

  printf("\n\n");
}

void separation_c::print(assemblyVoxel_c * start) const {
  for (int i = 0; i < moves; i++)
    movements[i]->print(start, pieces);

  printf(" puzzle separates into  2 parts\n");

  if (removed) removed->print(start);
  if (left) left->print(start);
}


