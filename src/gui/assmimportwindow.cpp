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

#include "assmimportwindow.h"

#include "guigridtype.h"
#include "BlockList.h"
#include "Layouter.h"
#include "blocklistgroup.h"
#include "separator.h"

#include "../lib/puzzle.h"

static void cb_WindowButton_stub(Fl_Widget * /*o*/, void *v) { ((Fl_Double_Window*)(v))->hide(); }
static void cb_WindowButton_stub2(Fl_Widget * /*o*/, void *v) { ((assmImportWindow_c*)(v))->okay_cb(); }
void assmImportWindow_c::okay_cb(void) {
  _ok = true;
  hide();
}

assmImportWindow_c::assmImportWindow_c(const puzzle_c * puzzle) : LFl_Double_Window(false), _ok(false)
{
  int ypos = 0;

  layouter_c * o = new layouter_c(0, 0);
  o->pitch(5);

  new LSeparator_c(0, ypos++, 1, 1, "Source", false);

  problemSelectorSrc = new ProblemSelector(0, 0, 100, 100, puzzle);
  LBlockListGroup_c * probGroup = new LBlockListGroup_c(0, ypos++, 1, 1, problemSelectorSrc);
  probGroup->setMinimumSize(0, 70);

  new LSeparator_c(0, ypos++, 1, 1, "Destination", false);

  rdDontAdd = new LFl_Radio_Button("Just add shapes to puzzle", 0, ypos++, 1, 1);
  rdAddNew = new LFl_Radio_Button("Add shapes to a new problem", 0, ypos++, 1, 1);
  rdAddDst = new LFl_Radio_Button("Add shapes to existing problem", 0, ypos++, 1, 1);
  rdDontAdd->set();

  problemSelectorDst = new ProblemSelector(0, 0, 100, 100, puzzle);
  probGroup = new LBlockListGroup_c(0, ypos++, 1, 1, problemSelectorDst);
  probGroup->setMinimumSize(0, 70);

  new LSeparator_c(0, ypos++, 1, 1, "Range", false);

  {
    layouter_c * o = new layouter_c(0, ypos++);

    (new LFl_Box("Range min", 0, 0))->stretchRight();
    (new LFl_Box("Range max", 0, 1))->stretchRight();
    (new LFl_Box("", 1, 0))->setMinimumSize(5, 0);
    min = new LFl_Int_Input(2, 0);
    max = new LFl_Int_Input(2, 1);
    min->value("0");
    max->value("1");

    ((LFl_Int_Input*)min)->weight(1, 0);

    o->end();
  }


  new LSeparator_c(0, ypos++, 1, 1, "Filter", false);

  // The filter group
  ckDrpDisconnected = new LFl_Check_Button("Drop disonnected shapes", 0, ypos++, 1, 1);
  ckDrpMirror = new LFl_Check_Button("Drop shapes with mirror symmetry", 0, ypos++, 1, 1);
  ckDrpSymm = new LFl_Check_Button("Drop all shapes with a symmetry", 0, ypos++, 1, 1);
  if (puzzle->getGridType()->getType() == gridType_c::GT_BRICKS)
  {
    ckDrpMillable = new LFl_Check_Button("Drop non millable shapes", 0, ypos++, 1, 1);
    ckDrpNotchable = new LFl_Check_Button("Drop non notchable shapes", 0, ypos++, 1, 1);
  }

//  ckDrpIdentical = new LFl_Check_Button("Remove identical shapes", 0, ypos++, 1, 1);
  ckDrpDisconnected->value(1);
  ckDrpIdentical->value(1);

  new LSeparator_c(0, ypos++, 1, 1, "", false);

  o->end();

  o = new layouter_c(0, 1);
  o->pitch(5);

  (new LFl_Button("Continue", 0, 0, 1, 1))->callback(cb_WindowButton_stub2, this);
  (new LFl_Button("Abort", 1, 0, 1, 1))->callback(cb_WindowButton_stub, this);
  (new LFl_Button("Count", 2, 0, 1, 1))->callback(cb_WindowButton_stub, this);

  o->end();

  label("Convert Assemblies to Shapes");

}

int assmImportWindow_c::getSrcProblem(void) { return problemSelectorSrc->getSelection(); }
int assmImportWindow_c::getDstProblem(void) { return problemSelectorDst->getSelection(); }

int assmImportWindow_c::getAction(void) {

  if (rdDontAdd->value()) return A_DONT_ADD;
  if (rdAddNew->value()) return A_ADD_NEW;
  if (rdAddDst->value()) return A_ADD_DST;

  return A_DONT_ADD;
}

unsigned int assmImportWindow_c::getFilter(void)
{
  unsigned int filter = 0;

  if (ckDrpDisconnected->value()) filter |= dropDisconnected;
  if (ckDrpMirror->value()) filter |= dropMirror;
  if (ckDrpSymm->value()) filter |= dropSymmetric;
  if (ckDrpMillable && ckDrpMillable->value()) filter |= dropNonMillable;
  if (ckDrpNotchable && ckDrpNotchable->value()) filter |= dropNonNotchable;
//  if (ckDrpIdentical->value()) filter |= dropIdentical;

  return filter;
}

unsigned int assmImportWindow_c::getMin(void) { return atoi(min->value()); }
unsigned int assmImportWindow_c::getMax(void) { return atoi(max->value()); }

