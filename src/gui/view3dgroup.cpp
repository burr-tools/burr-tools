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
#include "view3dgroup.h"

#include "voxelframe.h"

#include "../lib/puzzle.h"
#include "../lib/disasmtomoves.h"

#include <FL/Fl_Pixmap.H>
#include <FL/Fl_Multiline_Output.H>

#include <math.h>

// some tool widgets, that may be swapped out later into another file

static void cb_View3dGroupSlider_stub(Fl_Widget* o, void* /*v*/) { ((LView3dGroup*)(o->parent()))->cb_slider(); }

void LView3dGroup::cb_slider(void) {
  View3D->setSize(exp(6-slider->value()));
}

static void cb_View3dGroupVoxel_stub(Fl_Widget* o, void* /*v*/) { ((LView3dGroup*)(o->parent()))->do_callback(); }

LView3dGroup::LView3dGroup(int x, int y, int w, int h) : Fl_Group(0, 0, 50, 50), layoutable_c(x, y, w, h) {

  x = y = 0;
  w = h = 50;

  box(FL_DOWN_BOX);

  View3D = new voxelFrame_c(x, y, w-15, h);
  View3D->tooltip(" Rotate the puzzle by dragging with the mouse ");
  View3D->box(FL_NO_BOX);
  View3D->callback(cb_View3dGroupVoxel_stub, this);

  slider = new Fl_Slider(x+w-15, y, 15, h);
  slider->tooltip("Zoom view.");
  slider->maximum(6);
  slider->minimum(0);
  slider->step(0.01);
  slider->value(2);
  slider->callback(cb_View3dGroupSlider_stub);
  slider->clear_visible_focus();

  cb_slider();

  resizable(View3D);
  end();
}

int LView3dGroup::handle(int event) {

  Fl_Group::handle(event);

  switch(event)
  {
    case FL_MOUSEWHEEL:
      slider->value(slider->value() + 0.1*Fl::e_dy);
      View3D->setSize(exp(6-slider->value()));
      return 1;
  }

  return 0;
}

void LView3dGroup::redraw(void)
{
  View3D->redraw();
}

