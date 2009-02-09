/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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

#include "convertwindow.h"

#include "guigridtype.h"

#include "../lib/converter.h"

static void cb_convertSelect_stub(Fl_Widget * /*o*/, void *v) { ((convertWindow_c*)(v))->select_cb(); }
void convertWindow_c::select_cb(void) {
  for (unsigned int i = 0; i < gti.size(); i++) {
    if (gti[i]->value()) {
      current = i;
    }
  }
}

static void cb_WindowButton_stub(Fl_Widget * /*o*/, void *v) { ((Fl_Double_Window*)(v))->hide(); }
static void cb_WindowButton_stub2(Fl_Widget * /*o*/, void *v) { ((convertWindow_c*)(v))->okay_cb(); }
void convertWindow_c::okay_cb(void) {
  _ok = true;
  hide();
}

convertWindow_c::convertWindow_c(gridType_c::gridType srcType) : LFl_Double_Window(false), _ok(false), current(0)
{
  bool found = false;
  unsigned int yPos = 0;

  layouter_c * o = new layouter_c(0, 1, 1, 1);
  o->pitch(5);

  for (int i = 0; i < gridType_c::GT_NUM_GRIDS; i++)
  {
    if (canConvert(srcType, (gridType_c::gridType)i))
    {
      found = true;

      gridType_c::gridType g = (gridType_c::gridType)i;
      gridType_c gt(g);
      guiGridType_c ggt(&gt);


      gti.push_back(new LFl_Radio_Button(ggt.getName(), 0, yPos, 1, 1));
      gridTypes.push_back(gridType_c::gridType(i));

      gti[yPos]->callback(cb_convertSelect_stub, this);

      if (yPos == 0)
        gti[yPos]->set();

      yPos++;
    }
  }

  if (!found) {
    new LFl_Box("There are no grids that this puzzle can be converted to, sorry", 0, yPos, 1, 1);
    yPos++;
  }

  o->end();

  if (found)
  {
    new LFl_Box("Please Select Target grid", 0, 0, 1, 1);

    o = new layouter_c(0, 2, 1, 1);
    o->pitch(5);

    (new LFl_Button("Continue", 0, 0, 1, 1))->callback(cb_WindowButton_stub2, this);
    (new LFl_Button("Abort", 1, 0, 1, 1))->callback(cb_WindowButton_stub, this);

    o->end();
  }
  else
  {
    (new LFl_Button("Ok", 0, 2, 1, 1))->callback(cb_WindowButton_stub, this);
  }
}

gridType_c::gridType convertWindow_c::getTargetType(void)
{
  return gridTypes[current];
}
