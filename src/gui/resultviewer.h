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
#ifndef __RESULT_VIEWER_H__
#define __RESULT_VIEWER_H__

#include "Layouter.h"

#include <FL/Fl_Box.H>

class problem_c;

/* a widget showing the number and name of the result shape for
 * a given problem
 */
class ResultViewer_c : public Fl_Box, public layoutable_c {

private:

  problem_c * puzzle;
  Fl_Color bg;

public:

  ResultViewer_c(int x, int y, int w, int h);
  void setPuzzle(problem_c * p);
  void draw(void);

  virtual void getMinSize(int *width, int *height) const {
    *width = 4;
    *height = 4;
  }
};

#endif
