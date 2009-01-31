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
#include "resultviewer.h"
#include "piececolor.h"

#include "../lib/problem.h"
#include "../lib/voxel.h"

ResultViewer_c::ResultViewer_c(int x, int y, int w, int h) : Fl_Box(0, 0, 10, 10), layoutable_c(x, y, w, h), puzzle(0) {
  bg = color();
  box(FL_BORDER_BOX);
  clear_visible_focus();
}

void ResultViewer_c::setPuzzle(problem_c * p) {
  puzzle = p;
  redraw();
}

void ResultViewer_c::draw(void) {
  if (!puzzle || puzzle->resultInvalid()) {
    label("No Result");
    color(bg);
    labelcolor(fl_rgb_color(255, 0, 0));
  } else {
    static char txt[120];

    unsigned int result = puzzle->getResult();

    if (puzzle->getResultShape()->getName().length())
      snprintf(txt, 120, "Result: S%i - %s", result+1, puzzle->getResultShape()->getName().c_str());
    else
      snprintf(txt, 19, "Result: S%i", result + 1);

    unsigned char r, g, b;

    r = pieceColorRi(result);
    g = pieceColorGi(result);
    b = pieceColorBi(result);

    color(fltkPieceColor(result));
    labelcolor(contrastPieceColor(result));

    label(txt);
  }
  Fl_Box::draw();
}

