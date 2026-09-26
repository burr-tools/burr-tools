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
#ifndef __VIEW3D_GROUP_H__
#define __VIEW3D_GROUP_H__

#include "Layouter.h"

#include <FL/Fl_Group.H>
#include <FL/Fl_Slider.H>

class piecePositions_c;
class voxelFrame_c;

// the groups with the 3d view and the zoom slider
class LView3dGroup : public Fl_Group, public layoutable_c {

  voxelFrame_c * View3D;
  Fl_Slider * slider;

  Fl_Callback * zoomChangeCb = nullptr;
  void * zoomChangeUser = nullptr;

public:

  static const double defaultZoom;

  LView3dGroup(int x, int y, int w, int h);

  void cb_slider(void);
  void applyWheelZoom(int dy);

  /* Fired only on an actual user zoom gesture (slider drag, mouse wheel) - not on
   * setZoom()/goHome() or the initial construction, which reapply a remembered
   * or fitted zoom rather than represent a fresh user choice. Lets callers
   * distinguish "the user picked this zoom" from "this zoom was restored",
   * which auto-fit-on-first-visit needs to not immediately re-mark itself as a
   * manual override. */
  void setZoomChangeCallback(Fl_Callback * cb, void * user) { zoomChangeCb = cb; zoomChangeUser = user; }
  void notifyZoomChanged(void) { if (zoomChangeCb) zoomChangeCb(this, zoomChangeUser); }

  double getZoom(void) { return slider->value(); }
  void setZoom(double v) { slider->value(v); cb_slider(); }
  void goHome(void);

  // zooms out (or in) just enough that everything currently shown fits in view
  void fitToContent(void);

  static void zoomAnimCbStub(void * u, double sz);

  // cppcheck-suppress duplInheritedMember
  void redraw(void);

  voxelFrame_c * getView(void) { return View3D; }

  virtual void getMinSize(int * w, int *h) const {
    *w = 40;
    *h = 40;
  }

  int handle(int event);
};

#endif
