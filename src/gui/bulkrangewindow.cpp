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

#include "bulkrangewindow.h"

#include "Layouter.h"
#include "separator.h"

#include <stdlib.h>
#include <FL/Fl_Int_Input.H>

static void cb_Ok_stub    (Fl_Widget* /*o*/, void* v) { ((bulkRangeWindow_c*)v)->okay_cb(); }
static void cb_Cancel_stub(Fl_Widget* /*o*/, void* v) { ((bulkRangeWindow_c*)v)->hide(); }

void bulkRangeWindow_c::okay_cb(void) {
  _ok = true;
  hide();
}

bulkRangeWindow_c::bulkRangeWindow_c(void) : LFl_Double_Window(false), _ok(false) {

  int ypos = 0;

  layouter_c * o = new layouter_c(0, 0);
  o->pitch(5);

  new LSeparator_c(0, ypos++, 1, 1, "Set Range for All Pieces", false);

  {
    layouter_c * row = new layouter_c(0, ypos++);

    (new LFl_Box("Min count", 0, 0))->stretchRight();
    (new LFl_Box("Max count", 0, 1))->stretchRight();
    (new LFl_Box("", 1, 0))->setMinimumSize(5, 0);
    minInput = new LFl_Int_Input(2, 0);
    maxInput = new LFl_Int_Input(2, 1);
    minInput->value("0");
    maxInput->value("1");

    ((LFl_Int_Input*)minInput)->weight(1, 0);

    row->end();
  }

  new LSeparator_c(0, ypos++, 1, 1, "", false);

  o->end();

  o = new layouter_c(0, 1);
  o->pitch(5);

  (new LFl_Button("Apply to All", 0, 0, 1, 1))->callback(cb_Ok_stub, this);
  (new LFl_Button("Cancel",       1, 0, 1, 1))->callback(cb_Cancel_stub, this);

  o->end();

  label("Bulk Set Piece Range");
}

unsigned int bulkRangeWindow_c::getMin(void) const { return (unsigned int)atoi(minInput->value()); }
unsigned int bulkRangeWindow_c::getMax(void) const { return (unsigned int)atoi(maxInput->value()); }
