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
#include "statuswindow.h"
#include "piececolor.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

#include <FL/Fl.H>

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

static void cb_Close_stub(Fl_Widget*, void* v) { ((statusWindow_c*)v)->hide(); }
static void cb_RemoveSelected_stub(Fl_Widget*, void* v) { ((statusWindow_c*)v)->cb_removeSelected(); }

class StatusProgress : public LFl_Double_Window {

  private:

    LFl_Progress * p;

  public:

    StatusProgress(void) : LFl_Double_Window(false) {

      label("Progress");

      (new LFl_Box("Calculating Status information.\n"
                  "This might take some time...", 0, 0, 1, 1))->pitch(3);

      p = new LFl_Progress(0, 1, 1, 1);
      p->minimum(0);
      p->maximum(1);
      p->selection_color((Fl_Color)4);
      p->pitch(3);

      LFl_Button * btn = new LFl_Button("Cancel", 0, 2, 1, 1);
      btn->pitch(3);
      btn->callback(cb_Close_stub, this);

      end();

      set_modal();
    }

    void setProgress(float value) {
      p->value(value);
    }
};

void statusWindow_c::cb_removeSelected(void) {

  /* we have to go up from the bottom as otherwise the indixes may shift */

  for (unsigned int s = puz->shapeNumber(); s > 0; s--) {

    for (unsigned int i = 0; i < puz->problemNumber(); i++)
      if (puz->probContainsShape(i, s))
        puz->probRemoveAllSolutions(i);

    if (selection[s-1]->value())
      puz->removeShape(s-1);
  }


  again = true;
  hide();
}

statusWindow_c::statusWindow_c(puzzle_c * p) : LFl_Double_Window(true), puz(p), again(false) {

  StatusProgress *  stp = new StatusProgress;
  stp->show();

  begin();

  char tmp[200];

  label("Shape Information");

  unsigned int lines = p->shapeNumber();
  unsigned int head = 3;

  layouter_c * fr = new layouter_c(0, 0, 1, 1);
  fr->pitch(7);

  (new LFl_Scroll(0, 0, 1, 1))->type(Fl_Scroll::VERTICAL_ALWAYS);

  for (unsigned int s = 0; s < p->shapeNumber(); s++) {

    LFl_Box * b;

    if (s & 1) {
      b = new LFl_Box("", 1, s+head, 25, 1);
      b->color(fl_rgb_color(150, 150, 150));
      b->box(FL_FLAT_BOX);
    }

    const voxel_c * v = p->getShape(s);

    unsigned int col = 0;

    selection.push_back(new LFl_Check_Button(" ", col, s+head));
    col+=2;

    if (v->getName().length())
      snprintf(tmp, 200, "S%i - %s", s+1, v->getName().c_str());
    else
      snprintf(tmp, 200, "S%i", s+1);

    b = new LFl_Box("", col, s+head);
    b->copy_label(tmp);
    b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
    if (3*pieceColorRi(s) + 6*pieceColorGi(s) + pieceColorBi(s) < 1275)
      b->labelcolor(fl_rgb_color(255, 255, 255));
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
    Fl::wait(0);

    for (unsigned int s2 = 0; s2 < s; s2++)
      if (v->identicalWithRots(p->getShape(s2), true, false)) {
        snprintf(tmp, 200, "%i", s2+1);
        b = new LFl_Box("", col, s+head);
        b->copy_label(tmp);
        b->color(fl_rgb_color(pieceColorRi(s2), pieceColorGi(s2), pieceColorBi(s2)));
        if (3*pieceColorRi(s2) + 6*pieceColorGi(s2) + pieceColorBi(s2) < 1275)
          b->labelcolor(fl_rgb_color(255, 255, 255));
        b->box(FL_FLAT_BOX);
        break;
      }
    col += 2;
    Fl::wait(0);

    for (unsigned int s2 = 0; s2 < s; s2++)
      if (v->identicalWithRots(p->getShape(s2), false, false)) {
        snprintf(tmp, 200, "%i", s2+1);
        b = new LFl_Box("", col, s+head);
        b->copy_label(tmp);
        b->color(fl_rgb_color(pieceColorRi(s2), pieceColorGi(s2), pieceColorBi(s2)));
        if (3*pieceColorRi(s2) + 6*pieceColorGi(s2) + pieceColorBi(s2) < 1275)
          b->labelcolor(fl_rgb_color(255, 255, 255));
        b->box(FL_FLAT_BOX);
        break;
      }
    col += 2;
    Fl::wait(0);

    for (unsigned int s2 = 0; s2 < s; s2++)
      if (v->identicalWithRots(p->getShape(s2), false, true)) {
        snprintf(tmp, 200, "%i", s2+1);
        b = new LFl_Box("", col, s+head);
        b->copy_label(tmp);
        b->color(fl_rgb_color(pieceColorRi(s2), pieceColorGi(s2), pieceColorBi(s2)));
        if (3*pieceColorRi(s2) + 6*pieceColorGi(s2) + pieceColorBi(s2) < 1275)
          b->labelcolor(fl_rgb_color(255, 255, 255));
        b->box(FL_FLAT_BOX);
        break;
      }
    col +=2 ;
    Fl::wait(0);

    if (v->connected(0, true, 0)) {
      new LFl_Box("X", col, s+head);
    } else {
      b = new LFl_Box("", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      b->box(FL_FLAT_BOX);
    }
    col += 2;
    Fl::wait(0);

    if (v->connected(1, true, 0)) {
      new LFl_Box("X", col, s+head);
    } else {
      b = new LFl_Box("", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      b->box(FL_FLAT_BOX);
    }
    col += 2;
    Fl::wait(0);

    if (v->connected(2, true, 0)) {
      new LFl_Box("X", col, s+head);
    } else {
      b = new LFl_Box("", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      b->box(FL_FLAT_BOX);
    }
    col += 2;
    Fl::wait(0);

    if (!v->connected(0, false, 0, false)) {
      b = new LFl_Box("X", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      if (3*pieceColorRi(s) + 6*pieceColorGi(s) + pieceColorBi(s) < 1275)
        b->labelcolor(fl_rgb_color(255, 255, 255));
      b->box(FL_FLAT_BOX);
    }
    col += 2;
    Fl::wait(0);

    if (!v->connected(0, false, 0)) {
      b = new LFl_Box("X", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      if (3*pieceColorRi(s) + 6*pieceColorGi(s) + pieceColorBi(s) < 1275)
        b->labelcolor(fl_rgb_color(255, 255, 255));
      b->box(FL_FLAT_BOX);
    }
    col += 2;
    Fl::wait(0);

    if (!p->getGridType()->getSymmetries()->symmetryKnown(v)) {
      b = new LFl_Box("---", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      if (3*pieceColorRi(s) + 6*pieceColorGi(s) + pieceColorBi(s) < 1275)
        b->labelcolor(fl_rgb_color(255, 255, 255));
      b->box(FL_FLAT_BOX);
    } else {
      snprintf(tmp, 200, "%i", p->getGridType()->getSymmetries()->calculateSymmetry(v));
      b = new LFl_Box("", col, s+head);
      b->copy_label(tmp);
      b->box(FL_NO_BOX);
    }
    col += 2;

    stp->setProgress(1.0*s/p->shapeNumber());
    Fl::wait(0);
    if (!stp->visible())
      break;
  }

  stp->hide();
  delete stp;

  unsigned int col = 1;

  new LFl_Line(col++, 0, 1, lines+head, 2);

  (new LFl_Box("Shape", col++, 0))->pitch(2);
  new LFl_Line(col++, 0, 1, lines+head, 2);

  (new LFl_Box("Units", col, 0, 5))->pitch(2);
  (new LFl_Box("Normal", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("Variable", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("Total", col++, 1))->pitch(2);
  new LFl_Line(col++, 0, 1, lines+head, 2);

  (new LFl_Box("Identical", col, 0, 5))->pitch(2);
  (new LFl_Box("Mirror", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("Shape", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("Complete", col++, 1))->pitch(2);
  new LFl_Line(col++, 0, 1, lines+head, 2);

  (new LFl_Box("Connectivity", col, 0, 5))->pitch(2);
  (new LFl_Box("Face", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("Edge", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("Corner", col++, 1))->pitch(2);
  new LFl_Line(col++, 0, 1, lines+head, 2);

  (new LFl_Box("Holes", col, 0, 3))->pitch(2);
  (new LFl_Box("2D", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("3D", col++, 1))->pitch(2);

  new LFl_Line(col++, 0, 1, lines+head, 2);
  (new LFl_Box("Sym", col++, 0, 1))->pitch(2);

  new LFl_Line(0, 2, 27, 1, 2);

  fr->end();
  fr->setMinimumSize(10, 200);
  fr->weight(1, 1);

  fr = new layouter_c(0, 1, 1, 1);
  fr->pitch(7);

  LFl_Button * btn = new LFl_Button("Close", 0, 1);
  btn->callback(cb_Close_stub, this);
  btn->weight(1, 0);

  (new LFl_Box(1, 1))->setMinimumSize(5, 0);

  btn = new LFl_Button("Remove selected", 2, 1);
  btn->callback(cb_RemoveSelected_stub, this);
  btn->weight(1, 0);

  fr->end();


  set_modal();
}
