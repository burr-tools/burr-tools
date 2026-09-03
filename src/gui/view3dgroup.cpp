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
#include "configuration.h"

#include "../lib/puzzle.h"
#include "../lib/disasmtomoves.h"

#include <FL/Fl_Pixmap.H>
#include <FL/Fl_Multiline_Output.H>

#include <math.h>

const double LView3dGroup::defaultZoom = 3.2;

// some tool widgets, that may be swapped out later into another file

static void cb_View3dGroupSlider_stub(Fl_Widget* o, void* /*v*/) { ((LView3dGroup*)(o->parent()))->cb_slider(); }

void LView3dGroup::cb_slider(void) {
  View3D->setSize(exp(6-slider->value()));
}

static void cb_View3dGroupVoxel_stub(Fl_Widget* o, void* /*v*/) { ((LView3dGroup*)(o->parent()))->do_callback(); }
static void cb_View3dHome_stub(Fl_Widget* /*o*/, void* v) { ((LView3dGroup*)v)->goHome(); }

LView3dGroup::LView3dGroup(int x, int y, int w, int h) : Fl_Group(0, 0, 50, 50), layoutable_c(x, y, w, h) {

  x = y = 0;
  w = h = 50;

  box(FL_DOWN_BOX);

  View3D = new voxelFrame_c(x, y, w-15, h);
  View3D->tooltip(" Rotate the puzzle by dragging with the mouse. Use the cube in the corner to snap views. ");
  View3D->box(FL_NO_BOX);
  View3D->callback(cb_View3dGroupVoxel_stub, this);
  View3D->setHomeCallback(cb_View3dHome_stub, this);

  slider = new Fl_Slider(x+w-15, y, 15, h);
  slider->tooltip("Zoom view.");
  slider->maximum(6);
  slider->minimum(0);
  slider->step(0.01);
  slider->value(defaultZoom);
  slider->callback(cb_View3dGroupSlider_stub);
  slider->clear_visible_focus();

  cb_slider();

  resizable(View3D);
  end();
}

void LView3dGroup::goHome(void) {
  resetZoomToDefault();
  View3D->resetViewRotation();
  redraw();
}

int LView3dGroup::handle(int event) {

  if (event == FL_MOUSEWHEEL) {
    if (!Fl::event_inside(this))
      return 0;

    int dy = Fl::event_dy();
    if (config.reverseScrollZoom())
      dy = -dy;
    slider->value(slider->value() + 0.1 * dy);
    View3D->setSize(exp(6 - slider->value()));
    return 1;
  }

  return Fl_Group::handle(event);
}

void LView3dGroup::redraw(void)
{
  View3D->redraw();
}

