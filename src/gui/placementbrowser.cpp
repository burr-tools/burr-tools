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
#include "placementbrowser.h"

#include "voxelframe.h"

#include "../lib/assembler.h"
#include "../lib/problem.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#pragma GCC diagnostic pop

#include "view3dgroup.h"

static void cb_close_stub(Fl_Widget* /*o*/, void* v) { ((placementBrowser_c*)v)->hide(); }
static void cb_piece_stub(Fl_Widget* /*o*/, void* v) { ((placementBrowser_c*)v)->cb_piece(); }
static void cb_placement_stub(Fl_Widget* o, void* v) { ((placementBrowser_c*)v)->cb_placement((Fl_Value_Slider*)o); }

void placementBrowser_c::cb_piece(void) {

  placementSelector->value(0);

  unsigned int piece = (int)pieceSelector->value();
  unsigned int placements = puzzle->getAssembler()->getPiecePlacementCount(piece);
  unsigned char trans;
  int x, y, z;

  if (placements) {
    placementSelector->activate();
    placementSelector->range(0, placements-1);

    node = puzzle->getAssembler()->getPiecePlacement(0, 0, piece, &trans, &x, &y, &z);

  } else {

    placementSelector->deactivate();

    trans = 0xFF;
    x = y = z = 20000;
  }

  placement = 0;
  view3d->getView()->showPlacement(puzzle, piece, trans, x, y, z);
}

void placementBrowser_c::cb_placement(Fl_Value_Slider* o) {

  unsigned int piece = (int)pieceSelector->value();
  int val = (unsigned int)o->value();

  unsigned char trans;
  int x, y, z;

  node = puzzle->getAssembler()->getPiecePlacement(node, val-placement, piece, &trans, &x, &y, &z);
  placement = val;

  view3d->getView()->showPlacement(puzzle, piece, trans, x, y, z);
}


placementBrowser_c::placementBrowser_c(problem_c * p) :
  LFl_Double_Window(true), puzzle(p) {

  bt_assert(puzzle->getAssembler());
  bt_assert(puzzle->getAssembler()->getPiecePlacementSupported());

  view3d = new LView3dGroup(1, 1, 1, 1);
  view3d->weight(1, 1);
  view3d->setMinimumSize(300, 300);
  view3d->clear_visible_focus();

  pieceSelector = new LFl_Value_Slider(0, 0, 2, 1);
  pieceSelector->type(FL_HOR_SLIDER);
  pieceSelector->range(0, puzzle->getNumberOfPieces()-1);
  pieceSelector->precision(0);
  pieceSelector->callback(cb_piece_stub, this);
  pieceSelector->tooltip(" Select the piece whose placements you want to see ");
  pieceSelector->setMinimumSize(0, 20);
  pieceSelector->clear_visible_focus();

  placementSelector = new LFl_Value_Slider(0, 1, 1, 1);
  placementSelector->precision(0);
  placementSelector->callback(cb_placement_stub, this);
  placementSelector->tooltip(" Browse the placements ");
  placementSelector->setMinimumSize(20, 0);
  placementSelector->clear_visible_focus();

  LFl_Button * b = new LFl_Button("Close", 0, 2, 2, 1);
  b->tooltip(" Close the window ");
  b->callback(cb_close_stub, this);
  b->pitch(5);
  b->clear_visible_focus();

  label("Placement Browser");

  end();

  if ((puzzle->getAssembler()->getFinished() > 0) &&
      (puzzle->getAssembler()->getFinished() < 1))
    fl_message("Attention: The assembler is neither in initial nor in final position\n"
               "The displayed placements may not be what you expect\n"
               "Read the documentation");

  cb_piece();

  set_modal();
}

int placementBrowser_c::handle(int event)
{
  switch(event) {
  case FL_KEYDOWN:
    switch (Fl::event_key()) {
    case FL_Up:
      placementSelector->handle(event);
      return 1;
    case FL_Down:
      placementSelector->handle(event);
      return 1;
    case FL_Right:
      pieceSelector->handle(event);
      return 1;
    case FL_Left:
      pieceSelector->handle(event);
      return 1;
    }
  }

  if (Fl_Double_Window::handle(event))
    return 1;

  return 0;
}
