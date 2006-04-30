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
#include "PlacementBrowser.h"

#include "../lib/assembler_0.h"
#include "../lib/puzzle.h"

#include <FL/fl_ask.H>

#define WINDOWSIZE_X 400
#define WINDOWSIZE_Y 400

static void cb_close_stub(Fl_Widget* o, void* v) { ((PlacementBrowser*)v)->hide(); }
static void cb_piece_stub(Fl_Widget* o, void* v) { ((PlacementBrowser*)v)->cb_piece((Fl_Value_Slider*)o); }
static void cb_placement_stub(Fl_Widget* o, void* v) { ((PlacementBrowser*)v)->cb_placement((Fl_Value_Slider*)o); }

void PlacementBrowser::cb_piece(Fl_Value_Slider* o) {

  placementSelector->value(0);

  unsigned int piece = (int)pieceSelector->value();
  unsigned int placements = ((assembler_0_c*)puzzle->probGetAssembler(problem))->getPiecePlacementCount(piece);
  unsigned char trans;
  int x, y, z;

  if (placements) {
    placementSelector->activate();
    placementSelector->range(0, placements-1);

    node = ((assembler_0_c*)puzzle->probGetAssembler(problem))->getPiecePlacement(0, 0, piece, &trans, &x, &y, &z);

  } else {

    placementSelector->deactivate();

    trans = 0xFF;
    x = y = z = 20000;
  }

  placement = 0;
  view3d->showPlacement(puzzle, problem, piece, trans, x, y, z);
}

void PlacementBrowser::cb_placement(Fl_Value_Slider* o) {

  unsigned int piece = (int)pieceSelector->value();
  int val = (unsigned int)o->value();

  unsigned char trans;
  int x, y, z;

  node = ((assembler_0_c*)puzzle->probGetAssembler(problem))->getPiecePlacement(node, val-placement, piece, &trans, &x, &y, &z);
  placement = val;

  view3d->showPlacement(puzzle, problem, piece, trans, x, y, z);
}


PlacementBrowser::PlacementBrowser(puzzle_c * p, unsigned int prob, const guiGridType_c * ggt) :
  Fl_Double_Window(WINDOWSIZE_X, WINDOWSIZE_Y), puzzle(p), problem(prob) {

  bt_assert(puzzle->probGetAssembler(problem));

  view3d = new View3dGroup(20, 20, WINDOWSIZE_X - 20, WINDOWSIZE_Y - (20 + 30), ggt);

  pieceSelector = new Fl_Value_Slider(0, 0, WINDOWSIZE_X, 20);
  pieceSelector->type(FL_HOR_SLIDER);
  pieceSelector->range(0, puzzle->probPieceNumber(problem)-1);
  pieceSelector->precision(0);
  pieceSelector->callback(cb_piece_stub, this);
  pieceSelector->box(FL_THIN_DOWN_BOX);
  pieceSelector->tooltip(" Select the piece whose placements you want to see ");

  placementSelector = new Fl_Value_Slider(0, 20, 20, WINDOWSIZE_Y - (20+30));
  placementSelector->precision(0);
  placementSelector->callback(cb_placement_stub, this);
  placementSelector->box(FL_THIN_DOWN_BOX);
  placementSelector->tooltip(" Browse the placements ");

  (new FlatButton(5, WINDOWSIZE_Y - 25, WINDOWSIZE_X - 10, 20, "Close", "Close the window"))->callback(cb_close_stub, this);

  label("Placement Browser");

  end();

  if ((puzzle->probGetAssembler(problem)->getFinished() > 0) &&
      (puzzle->probGetAssembler(problem)->getFinished() < 1))
    fl_message("Attention: The assembler is neither in intitial nor in final position\n"
	       "The displayed placements may not be what you expect\n"
	       "Read the documentation");

  cb_piece(0);

  set_modal();
}

int PlacementBrowser::handle(int event) {

  if (Fl_Double_Window::handle(event))
    return 1;

  switch(event) {
  case FL_SHORTCUT:
    switch (Fl::event_key()) {
    case FL_Up:
      placementSelector->value(placementSelector->value()-1);
      return 1;
    case FL_Down:
      placementSelector->value(placementSelector->value()+1);
      return 1;
    case FL_Right:
      pieceSelector->value(pieceSelector->value()+1);
      return 1;
    case FL_Left:
      pieceSelector->value(pieceSelector->value()-1);
      return 1;
    }
  }

  return 0;
}
