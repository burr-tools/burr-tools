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
#include "voxeleditgroup.h"
#include "piececolor.h"
#include "guigridtype.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

#include <FL/Fl_Pixmap.H>
#include <FL/Fl_Multiline_Output.H>

#include <math.h>

// some tool widgets, that may be swapped out later into another file

// draws an definable evenly spaced number of lines in one direction
class LineSpacer : public Fl_Widget {

  int lines;
  bool vertical;
  int gap;

  public:

    LineSpacer(int x, int y, int w, int h, int borderSpace) : Fl_Widget(x, y, w, h), lines(2), vertical(true), gap(borderSpace) {}

    void draw(void) {

      fl_color(color());
      fl_rectf(x(), y(), w(), h());

      if (lines <= 1) return;

      fl_color(0);

      if (vertical) {

        for (int i = 0; i < lines; i++) {
          int ypos = y()+ gap + (h()-2*gap-1)*i/(lines-1);
          fl_line(x(), ypos, x()+w()-1, ypos);
        }

      } else {

        for (int i = 0; i < lines; i++) {
          int xpos = x()+ gap + (w()-2*gap-1)*i/(lines-1);
          fl_line(y(), xpos, y()+w()-1, xpos);
        }
      }

    }

    void setLines(int l, int vert) {
      lines = l;
      vertical = vert;
      redraw();
    }

};

static void cb_VoxelEditGroupZselect_stub(Fl_Widget* o, void* v) { static_cast<VoxelEditGroup_c*>(v)->cb_Zselect(static_cast<Fl_Slider*>(o)); }
static void cb_VoxelEditGroupSqedit_stub(Fl_Widget* /*o*/, void* v) { static_cast<VoxelEditGroup_c*>(v)->cb_Sqedit(); }

VoxelEditGroup_c::VoxelEditGroup_c(int x, int y, int w, int h, puzzle_c * puzzle, const guiGridType_c * ggt)
  : Fl_Group(0, 0, 300, 300), layoutable_c(x, y, w, h), sqedit(nullptr), noEditBox(nullptr) {

  x = 0;
  y = 0;
  w = 300;
  h = 300;

  zselect = new Fl_Slider(x, y, 15, h);
  zselect->tooltip(" Select Z Plane ");
  zselect->color((Fl_Color)237);
  zselect->step(1);
  zselect->callback(cb_VoxelEditGroupZselect_stub, this);
  zselect->clear_visible_focus();

  space = new LineSpacer(x+15, y, 5, h, 4);

  yAxisBar = new Fl_Box(x+25, y, 5, h-5);
  yAxisBar->box(FL_FLAT_BOX);
  yAxisBar->color(fl_rgb_color(0, 192, 0));

  xAxisBar = new Fl_Box(x+25, y+h-5, w-25, 5);
  xAxisBar->box(FL_FLAT_BOX);
  xAxisBar->color((Fl_Color)1);

  noEditBox = new Fl_Box(x+35, y, w-35, h-10,
      "2D voxel editing is not available for this grid in the legacy GUI.\n"
      "You can view shapes in 3D, inspect problems, and solve this puzzle.");
  noEditBox->box(FL_FLAT_BOX);
  noEditBox->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
  noEditBox->hide();

  sqedit = ggt->getGridEditor(x+35, y, w-35, h-10, puzzle);
  if (sqedit) {
    sqedit->tooltip(" Fill and empty voxels ");
    sqedit->box(FL_NO_BOX);
    sqedit->callback(cb_VoxelEditGroupSqedit_stub, this);
    sqedit->clear_visible_focus();
    resizable(sqedit);
  } else {
    zselect->hide();
    space->hide();
    yAxisBar->hide();
    xAxisBar->hide();
    noEditBox->resize(x, y, w, h);
    noEditBox->show();
    resizable(noEditBox);
  }
}

void VoxelEditGroup_c::newGridType(const guiGridType_c * ggt, puzzle_c * puzzle) {

  int ex = sqedit ? sqedit->x() : (x() + 35);
  int ey = sqedit ? sqedit->y() : y();
  int ew = sqedit ? sqedit->w() : (w() > 35 ? w() - 35 : 1);
  int eh = sqedit ? sqedit->h() : (h() > 10 ? h() - 10 : 1);

  if (sqedit) {
    remove(sqedit);
    delete sqedit;
    sqedit = nullptr;
  }

  sqedit = ggt->getGridEditor(ex, ey, ew, eh, puzzle);
  if (sqedit) {
    sqedit->tooltip(" Fill and empty voxels ");
    sqedit->box(FL_NO_BOX);
    sqedit->callback(cb_VoxelEditGroupSqedit_stub, this);
    sqedit->clear_visible_focus();
    noEditBox->hide();
    zselect->show();
    space->show();
    yAxisBar->show();
    xAxisBar->show();
    add(sqedit);
    resizable(sqedit);
  } else {
    zselect->hide();
    space->hide();
    yAxisBar->hide();
    xAxisBar->hide();
    noEditBox->resize(x(), y(), w(), h());
    noEditBox->show();
    resizable(noEditBox);
  }
  init_sizes();
  redraw();
}


void VoxelEditGroup_c::setZ(unsigned int val) {
  if (!sqedit) return;
  if (val > zselect->maximum()) val = (unsigned int)zselect->maximum();
  zselect->value(int(zselect->maximum()-val));
  sqedit->setZ(val);
}

void VoxelEditGroup_c::setPuzzle(puzzle_c * puzzle, unsigned int num) {
  if (!sqedit) return;
  sqedit->setPuzzle(puzzle, num);
  if (puzzle && (num < puzzle->getNumberOfShapes())) {
    voxel_c * v = puzzle->getShape(num);
    if (v) {
      zselect->bounds(0, v->getZ()-1);
      zselect->value(int(zselect->maximum()-sqedit->getZ()));
      space->setLines(v->getZ(), true);
    }
  }
}

void VoxelEditGroup_c::draw() {
  fl_push_clip(x(), y(), w(), h());
  Fl_Group::draw();
  fl_pop_clip();
}

void VoxelEditGroup_c::cb_Zselect(Fl_Slider* o) {
  if (!sqedit) return;
  sqedit->setZ(int(zselect->maximum() - o->value()));
}

