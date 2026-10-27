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
#include "layoutmodel.h"

#include <algorithm>

namespace btui {

  const DensityMetrics & densityMetrics(Density d) {
    // foundations/density.md section 1; the Focus 2D centre width is from C11
    static const DensityMetrics standard = { 36, 24, 60, 320, 340, 48, 4, 4, 360 };
    static const DensityMetrics minimal  = { 32, 22, 44, 264, 320, 40, 0, 1, 360 };
    return d == Density::Minimal ? minimal : standard;
  }

  bool LayoutModel::leftShownAsRail(void) const {
    return foc != Focus::None || leftCollapsed();
  }

  bool LayoutModel::rightShownAsRail(void) const {
    if (foc == Focus::Focus3d) return true;
    if (foc == Focus::Focus2d) return false;
    return rightCollapsed();
  }

  bool LayoutModel::editorVisible(void) const {
    if (ws != Workspace::Entities) return false;
    if (foc == Focus::Focus2d) return true;
    return foc == Focus::None && !rightCollapsed();
  }

  void LayoutModel::setWorkspace(Workspace w) {
    ws = w;
    foc = Focus::None;
  }

  void LayoutModel::setLeftCollapsed(bool c) {
    if (!c) foc = Focus::None;
    left[idx(ws)] = c;
  }

  void LayoutModel::setRightCollapsed(bool c) {
    if (!c) foc = Focus::None;
    right[idx(ws)] = c;
  }

  void LayoutModel::toggleLeft(void) {
    if (foc != Focus::None)
      setLeftCollapsed(false);
    else
      setLeftCollapsed(!leftCollapsed());
  }

  void LayoutModel::toggleRight(void) {
    if (foc != Focus::None)
      setRightCollapsed(false);
    else
      setRightCollapsed(!rightCollapsed());
  }

  void LayoutModel::toggleFocus2d(void) {
    if (ws != Workspace::Entities) return;
    foc = (foc == Focus::Focus2d) ? Focus::None : Focus::Focus2d;
  }

  void LayoutModel::toggleFocus3d(void) {
    foc = (foc == Focus::Focus3d) ? Focus::None : Focus::Focus3d;
  }

  bool LayoutModel::escape(void) {
    if (foc == Focus::None) return false;
    foc = Focus::None;
    return true;
  }

  void LayoutModel::restoreCollapsed(Workspace w, bool leftC, bool rightC) {
    left[idx(w)] = leftC;
    right[idx(w)] = rightC;
  }

  ColumnWidths LayoutModel::columns(double windowWidth, Density d) const {
    const DensityMetrics & m = densityMetrics(d);

    ColumnWidths c;
    c.leftIsRail = leftShownAsRail();
    c.rightIsRail = rightShownAsRail();

    // the workspace rail, the body padding on both sides and the two gaps
    // between the three cards
    const double avail = windowWidth - m.workspaceRail - 2 * m.bodyPadding - 2 * m.cardGap;

    c.left = c.leftIsRail ? m.collapsedRail : m.leftCard;

    if (foc == Focus::Focus2d) {
      c.centre = m.focus2dCentre;
      c.right = std::max(0.0, avail - c.left - c.centre);
    } else {
      c.right = c.rightIsRail ? m.collapsedRail : m.rightCard;
      c.centre = std::max(0.0, avail - c.left - c.right);
    }

    return c;
  }
}
