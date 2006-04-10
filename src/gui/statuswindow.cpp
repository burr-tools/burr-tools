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
#include "pieceColor.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

class LFl_Line : public Fl_Box, public layoutable_c {

  private:

    int thickness;

  public:

  LFl_Line(int x, int y, int w, int h, int thick = 1, Fl_Color col = FL_BLACK) : Fl_Box(0, 0, 0, 0), layoutable_c(x, y, w, h), thickness(thick) {
    color(col);
    box(FL_FLAT_BOX);
  }

  virtual void getMinSize(int *width, int *height) const {
    *width = thickness;
    *height = thickness;
  }
};

static void cb_Close_stub(Fl_Widget* o, void* v) { ((statusWindow_c*)v)->hide(); }

statusWindow_c::statusWindow_c(const puzzle_c * p) {

  char tmp[200];

  label("Shape Information");

  unsigned int lines = p->shapeNumber();
  unsigned int head = 3;

  layouter_c * fr = new layouter_c(0, 0, 1, 1);
  fr->pitch(7);

  for (unsigned int s = 0; s < p->shapeNumber(); s++) {

    LFl_Box * b;

    if (s & 1) {
      b = new LFl_Box("", 1, s+head, 22, 1);
      b->color(fl_rgb_color(150, 150, 150));
      b->box(FL_FLAT_BOX);
    }

    const voxel_c * v = p->getShape(s);

    unsigned int col = 0;

    if (v->getName().length())
      snprintf(tmp, 200, "S%i - %s", s+1, v->getName().c_str());
    else
      snprintf(tmp, 200, "S%i", s+1);

    b = new LFl_Box("", col, s+head);
    b->copy_label(tmp);
    b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
    b->box(FL_FLAT_BOX);
    col += 2;

    snprintf(tmp, 200, "%i", v->countState(voxel_c::VX_FILLED));
    (new LFl_Box("", col, s+head))->copy_label(tmp);
    col += 2;

    snprintf(tmp, 200, "%i", v->countState(voxel_c::VX_VARIABLE));
    (new LFl_Box("", col, s+head))->copy_label(tmp);
    col += 2;

    snprintf(tmp, 200, "%i", v->countState(voxel_c::VX_VARIABLE) + v->countState(voxel_c::VX_FILLED));
    (new LFl_Box("", col, s+head))->copy_label(tmp);
    col += 2;

    for (unsigned int s2 = 0; s2 < s; s2++)
      if (v->identicalWithRots(p->getShape(s2), true, false)) {
        snprintf(tmp, 200, "%i", s2+1);
        b = new LFl_Box("", col, s+head);
        b->copy_label(tmp);
        b->color(fl_rgb_color(pieceColorRi(s2), pieceColorGi(s2), pieceColorBi(s2)));
        b->box(FL_FLAT_BOX);
        break;
      }
    col += 2;

    for (unsigned int s2 = 0; s2 < s; s2++)
      if (v->identicalWithRots(p->getShape(s2), false, false)) {
        snprintf(tmp, 200, "%i", s2+1);
        b = new LFl_Box("", col, s+head);
        b->copy_label(tmp);
        b->color(fl_rgb_color(pieceColorRi(s2), pieceColorGi(s2), pieceColorBi(s2)));
        b->box(FL_FLAT_BOX);
        break;
      }
    col += 2;

    for (unsigned int s2 = 0; s2 < s; s2++)
      if (v->identicalWithRots(p->getShape(s2), false, true)) {
        snprintf(tmp, 200, "%i", s2+1);
        b = new LFl_Box("", col, s+head);
        b->copy_label(tmp);
        b->color(fl_rgb_color(pieceColorRi(s2), pieceColorGi(s2), pieceColorBi(s2)));
        b->box(FL_FLAT_BOX);
        break;
      }
    col +=2 ;

    if (v->connected(0, true, 0)) {
      new LFl_Box("X", col, s+head);
    } else {
      b = new LFl_Box("", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      b->box(FL_FLAT_BOX);
    }
    col += 2;

    if (v->connected(1, true, 0)) {
      new LFl_Box("X", col, s+head);
    } else {
      b = new LFl_Box("", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      b->box(FL_FLAT_BOX);
    }
    col += 2;

    if (v->connected(2, true, 0)) {
      new LFl_Box("X", col, s+head);
    } else {
      b = new LFl_Box("", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      b->box(FL_FLAT_BOX);
    }
    col += 2;

    if (!v->connected(0, false, 0, false)) {
      b = new LFl_Box("X", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      b->box(FL_FLAT_BOX);
    }
    col += 2;

    if (!v->connected(0, false, 0)) {
      b = new LFl_Box("X", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      b->box(FL_FLAT_BOX);
    }
    col += 2;

  }

  (new LFl_Box("Shape", 0, 0))->pitch(2);
  new LFl_Line(1, 0, 1, lines+head, 2);

  (new LFl_Box("Units", 2, 0, 5))->pitch(2);
  (new LFl_Box("Normal", 2, 1))->pitch(2);
  new LFl_Line(3, 1, 1, lines+head-1, 1);
  (new LFl_Box("Variable", 4, 1))->pitch(2);
  new LFl_Line(5, 1, 1, lines+head-1, 1);
  (new LFl_Box("Sum", 6, 1))->pitch(2);
  new LFl_Line(7, 0, 1, lines+head, 2);

  (new LFl_Box("Identical", 8, 0, 5))->pitch(2);
  (new LFl_Box("Mirror", 8, 1))->pitch(2);
  new LFl_Line(9, 1, 1, lines+head-1, 1);
  (new LFl_Box("Shape", 10, 1))->pitch(2);
  new LFl_Line(11, 1, 1, lines+head-1, 1);
  (new LFl_Box("Complete", 12, 1))->pitch(2);
  new LFl_Line(13, 0, 1, lines+head, 2);

  (new LFl_Box("Connectivity", 14, 0, 5))->pitch(2);
  (new LFl_Box("Face", 14, 1))->pitch(2);
  new LFl_Line(15, 1, 1, lines+head-1, 1);
  (new LFl_Box("Edge", 16, 1))->pitch(2);
  new LFl_Line(17, 1, 1, lines+head-1, 1);
  (new LFl_Box("Corner", 18, 1))->pitch(2);
  new LFl_Line(19, 0, 1, lines+head, 2);

  (new LFl_Box("Holes", 20, 0, 3))->pitch(2);
  (new LFl_Box("2D", 20, 1))->pitch(2);
  new LFl_Line(21, 1, 1, lines+head-1, 1);
  (new LFl_Box("3D", 22, 1))->pitch(2);

  new LFl_Line(0, 2, 23, 1, 2);

  fr->end();

  LFl_Button * btn = new LFl_Button("Close", 0, 1);
  btn->pitch(7);
  btn->callback(cb_Close_stub, this);
  btn->box(FL_THIN_UP_BOX);
}
