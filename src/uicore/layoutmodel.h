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
#ifndef BTUI_LAYOUTMODEL_H
#define BTUI_LAYOUTMODEL_H

#include <array>

/* The workspace layout: which workspace is shown, which side cards are
 * collapsed, and the focus modes (spec C11, C15; foundations/density.md).
 *
 * Rules kept here rather than in the view:
 *  - collapse state is remembered per workspace and persisted; focus is
 *    session-only and resets whenever the workspace changes;
 *  - a focus mode overrides the collapse flags without changing them, so
 *    leaving it restores the layout the user had;
 *  - Focus 2D exists only in Entities (it enlarges the voxel editor);
 *  - the layer slab and dim-other-layers in the 3D view are drawn only while
 *    the voxel editor is on screen (editorVisible()).
 * All widths are in dp and come from the density tables, which win over the
 * older numbers in the component specs.
 */
namespace btui {

  enum class Workspace { Entities = 0, Puzzle = 1, Solver = 2 };
  enum class Focus { None, Focus2d, Focus3d };
  enum class Density { Standard, Minimal };

  /* The geometry tokens of one density that the layout needs. */
  struct DensityMetrics {
    double topBar, statusBar, workspaceRail;
    double leftCard, rightCard, collapsedRail;
    double bodyPadding, cardGap;
    double focus2dCentre;
  };

  const DensityMetrics & densityMetrics(Density d);

  /* The horizontal split of the window body, in dp. */
  struct ColumnWidths {
    double left = 0;     ///< left card, or its collapsed rail
    double centre = 0;   ///< the 3D view card
    double right = 0;    ///< right card, or its collapsed rail
    bool leftIsRail = false;
    bool rightIsRail = false;
  };

  class LayoutModel {

    public:

      Workspace workspace(void) const { return ws; }
      Focus focus(void) const { return foc; }

      bool leftCollapsed(Workspace w) const { return left[idx(w)]; }
      bool rightCollapsed(Workspace w) const { return right[idx(w)]; }
      bool leftCollapsed(void) const { return leftCollapsed(ws); }
      bool rightCollapsed(void) const { return rightCollapsed(ws); }

      /* The rail the card shows right now: collapsed, or hidden by a focus mode. */
      bool leftShownAsRail(void) const;
      bool rightShownAsRail(void) const;

      /* True while the voxel editor is visible (spec: editor_visible). */
      bool editorVisible(void) const;

      /* Switching workspace keeps each workspace's collapse state and leaves
       * any focus mode.
       */
      void setWorkspace(Workspace w);

      /* The header collapse buttons. Expanding a card from its rail also
       * leaves a focus mode, as the rails' expand buttons do in the spec.
       */
      void setLeftCollapsed(bool c);
      void setRightCollapsed(bool c);

      /* Ctrl+[ / Ctrl+]. Outside a focus mode they toggle; inside one -- where
       * both side cards are rails -- they leave focus and show that card.
       */
      void toggleLeft(void);
      void toggleRight(void);

      /* Focus buttons and keys. Entering one replaces the other; Focus 2D is
       * ignored outside Entities.
       */
      void toggleFocus2d(void);
      void toggleFocus3d(void);

      /* The layout's rung of the Esc ladder: leave a focus mode. Returns
       * whether Esc was used, so the caller can try the next rung.
       */
      bool escape(void);

      /* Restore persisted state (focus is never restored). */
      void restoreCollapsed(Workspace w, bool leftC, bool rightC);

      ColumnWidths columns(double windowWidth, Density d) const;

    private:

      static int idx(Workspace w) { return static_cast<int>(w); }

      Workspace ws = Workspace::Entities;
      Focus foc = Focus::None;
      std::array<bool, 3> left = { false, false, false };
      std::array<bool, 3> right = { false, false, false };
  };
}

#endif
