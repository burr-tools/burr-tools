/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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
#include "statuswindow.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

statusWindow_c::statusWindow_c(const puzzle_c * p) {

  char tmp[200];

  new LFl_Box("Shape", 0, 0);

  new LFl_Box("Units", 1, 0, 3);
  new LFl_Box("Normal", 1, 1);
  new LFl_Box("Variable", 2, 1);
  new LFl_Box("Sum", 3, 1);

  new LFl_Box("Identical", 4, 0, 2);
  new LFl_Box("Shape", 4, 1);
  new LFl_Box("Complete", 5, 1);

  unsigned int head = 2;

  for (unsigned int s = 0; s < p->shapeNumber(); s++) {

    const voxel_c * v = p->getShape(s);

    if (v->getName().length())
      snprintf(tmp, 200, "S%i - %s", s+1, v->getName().c_str());
    else
      snprintf(tmp, 200, "S%i", s+1);

    (new LFl_Box("", 0, s+head))->copy_label(tmp);

    snprintf(tmp, 200, "%i", v->countState(voxel_c::VX_FILLED));
    (new LFl_Box("", 1, s+head))->copy_label(tmp);

    snprintf(tmp, 200, "%i", v->countState(voxel_c::VX_VARIABLE));
    (new LFl_Box("", 2, s+head))->copy_label(tmp);

    snprintf(tmp, 200, "%i", v->countState(voxel_c::VX_VARIABLE) + v->countState(voxel_c::VX_FILLED));
    (new LFl_Box("", 3, s+head))->copy_label(tmp);

    for (unsigned int s2 = 1; s2 < s; s2++)
      if (v->identicalWithRots(p->getShape(s2), false, false)) {
        snprintf(tmp, 200, "%i", s2+1);
        (new LFl_Box("", 4, s+head))->copy_label(tmp);
        break;
      }

    for (unsigned int s2 = 1; s2 < s; s2++)
      if (v->identicalWithRots(p->getShape(s2), false, true)) {
      snprintf(tmp, 200, "%i", s2+1);
      (new LFl_Box("", 5, s+head))->copy_label(tmp);
      break;
    }
  }

  new LFl_Button("Close", 0, p->shapeNumber()+head, 6, 1);
}
