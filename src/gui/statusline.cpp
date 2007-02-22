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
#include "statusline.h"

#include "WindowWidgets.h"

LStatusLine::LStatusLine(int x, int y, int w, int h) : layouter_c(x, y, w, h) {

  text = new LFl_Box(0, 0, 1, 1);
  text->box(FL_UP_BOX);
  text->color(FL_BACKGROUND_COLOR);
  text->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
  text->weight(1, 0);

  mode = new ButtonGroup(1, 0, 1, 1);
  mode->addButton()->image(pm.get(ViewModeNormal_xpm));
  mode->addButton()->image(pm.get(ViewModeColor_xpm));
  mode->addButton()->image(pm.get(ViewMode3D_xpm));

  clear_visible_focus();

  end();
}

void LStatusLine::setText(const char * t) {

  text->copy_label(t);
}

voxelDrawer_c::colorMode LStatusLine::getColorMode(void) const {
  return mode->getSelected()==0
    ?voxelDrawer_c::pieceColor
    :(mode->getSelected()==1
        ?voxelDrawer_c::paletteColor
        :voxelDrawer_c::anaglyphColor);
}

void LStatusLine::callback(Fl_Callback* fkt, void * dat) { mode->callback(fkt, dat); }

