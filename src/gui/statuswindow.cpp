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

  (new LFl_Box("Shape", 0, 0))->pitch(2);
  new LFl_Line(1, 0, 1, lines+head, 2);

  (new LFl_Box("Units", 2, 0, 5))->pitch(2);
  (new LFl_Box("Normal", 2, 1))->pitch(2);
  new LFl_Line(3, 1, 1, lines+head-1, 1);
  (new LFl_Box("Variable", 4, 1))->pitch(2);
  new LFl_Line(5, 1, 1, lines+head-1, 1);
  (new LFl_Box("Sum", 6, 1))->pitch(2);
  new LFl_Line(7, 0, 1, lines+head, 2);

  (new LFl_Box("Identical", 8, 0, 3))->pitch(2);
  (new LFl_Box("Shape", 8, 1))->pitch(2);
  new LFl_Line(9, 1, 1, lines+head-1, 1);
  (new LFl_Box("Complete", 10, 1))->pitch(2);
  new LFl_Line(11, 0, 1, lines+head, 2);

  (new LFl_Box("Connectivity", 12, 0, 5))->pitch(2);
  (new LFl_Box("Face", 12, 1))->pitch(2);
  new LFl_Line(13, 1, 1, lines+head-1, 1);
  (new LFl_Box("Edge", 14, 1))->pitch(2);
  new LFl_Line(15, 1, 1, lines+head-1, 1);
  (new LFl_Box("Corner", 16, 1))->pitch(2);
  new LFl_Line(17, 0, 1, lines+head, 2);

  (new LFl_Box("Holes", 18, 0, 3))->pitch(2);
  (new LFl_Box("2D", 18, 1))->pitch(2);
  new LFl_Line(19, 1, 1, lines+head-1, 1);
  (new LFl_Box("3D", 20, 1))->pitch(2);

  new LFl_Line(0, 2, 21, 1, 2);

  for (unsigned int s = 0; s < p->shapeNumber(); s++) {

    const voxel_c * v = p->getShape(s);

    if (v->getName().length())
      snprintf(tmp, 200, "S%i - %s", s+1, v->getName().c_str());
    else
      snprintf(tmp, 200, "S%i", s+1);

    (new LFl_Box("", 0, s+head))->copy_label(tmp);

    snprintf(tmp, 200, "%i", v->countState(voxel_c::VX_FILLED));
    (new LFl_Box("", 2, s+head))->copy_label(tmp);

    snprintf(tmp, 200, "%i", v->countState(voxel_c::VX_VARIABLE));
    (new LFl_Box("", 4, s+head))->copy_label(tmp);

    snprintf(tmp, 200, "%i", v->countState(voxel_c::VX_VARIABLE) + v->countState(voxel_c::VX_FILLED));
    (new LFl_Box("", 6, s+head))->copy_label(tmp);

    for (unsigned int s2 = 1; s2 < s; s2++)
      if (v->identicalWithRots(p->getShape(s2), false, false)) {
        snprintf(tmp, 200, "%i", s2+1);
        (new LFl_Box("", 8, s+head))->copy_label(tmp);
        break;
      }

    for (unsigned int s2 = 1; s2 < s; s2++)
      if (v->identicalWithRots(p->getShape(s2), false, true)) {
        snprintf(tmp, 200, "%i", s2+1);
        (new LFl_Box("", 10, s+head))->copy_label(tmp);
        break;
      }

    if (v->connected(0, true, 0)) new LFl_Box("X", 12, s+head);
    if (v->connected(1, true, 0)) new LFl_Box("X", 14, s+head);
    if (v->connected(2, true, 0)) new LFl_Box("X", 16, s+head);

    voxel_c * tmp = p->getGridType()->getVoxel(v);
    tmp->resize(tmp->getX()+2, tmp->getY()+2, tmp->getZ(),  0);
    tmp->translate(1, 1, 0,  0);
    if (!tmp->connected(0, false, 0)) new LFl_Box("X", 18, s+head);

    tmp->resize(tmp->getX(), tmp->getY(), tmp->getZ()+2,  0);
    tmp->translate(0, 0, 1,  0);
    if (!tmp->connected(0, false, 0)) new LFl_Box("X", 20, s+head);

    delete tmp;
  }

  fr->end();

  LFl_Button * btn = new LFl_Button("Close", 0, 1);
  btn->pitch(7);
  btn->callback(cb_Close_stub, this);
  btn->box(FL_THIN_UP_BOX);
}
