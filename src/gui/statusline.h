/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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
#ifndef __STATUS_LINE_H__
#define __STATUS_LINE_H__

#include "Layouter.h"
#include "Images.h"
#include "voxeldrawer.h"

class ButtonGroup;

// a status line containing text and a button to toggle
// between coloured and normal view
class LStatusLine : public layouter_c {

private:

  ButtonGroup *mode;
  LFl_Box * text;
  pixmapList_c pm;

public:

  LStatusLine(int x, int y, int w, int h);

  void setText(const char * t);
  voxelDrawer_c::colorMode getColorMode(void) const;
  void callback(Fl_Callback* fkt, void * dat);

  virtual void getMinSize(int *width, int *height) const {
    *width = 30;
    *height = 25;
  }
};

#endif
