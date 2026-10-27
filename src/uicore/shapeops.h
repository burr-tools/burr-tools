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
#ifndef BTUI_SHAPEOPS_H
#define BTUI_SHAPEOPS_H

#include "vecmath.h"

#include "../lib/gridtype.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

class puzzle_c;
class voxel_c;

/* The inspector's operations on a shape (C03 Fit & scale, C04 Transform,
 * C05 Repair & surface), without the inspector.
 *
 * Every operation is legacy's tool-tab command for the voxel type
 * (src/gui/tooltabs.cpp, ToolTab_0 ... _4: applyTask), keyed by legacy's
 * button number, so a button here runs exactly what its legacy twin runs --
 * the lattice-parity rules of Center and To origin included. What the
 * inspector adds: an operation is checked first and blocked with a reason
 * instead of losing voxels (a nudge out of the grid: the spec; legacy
 * drops them), Minimize and the scales say why they are unavailable, and
 * Fill and the surface tools report what they changed.
 */
namespace btui {

  /* legacy's button numbers for the operations that are not transforms */
  namespace task {
    inline constexpr int Prune = 15;          ///< legacy "Minimize size of grid"
    inline constexpr int FixedInner = 16;
    inline constexpr int FixedOuter = 17;
    inline constexpr int VariableInner = 18;
    inline constexpr int VariableOuter = 19;
    inline constexpr int ClearInner = 20;
    inline constexpr int ClearOuter = 21;
    inline constexpr int Scale2 = 22;
    inline constexpr int Scale3 = 23;
    inline constexpr int Origin = 24;
    inline constexpr int Center = 25;
    inline constexpr int Minimize = 26;       ///< legacy "Try to minimize size of shape"
    inline constexpr int FillHoles = 40;
  }

  enum class OpKind { Flip, Rotate, Nudge };

  /* One Transform button (C04, C14): its legacy task and how it is shown.
   * The matrix layout puts it at row `axis`; the nudge pads by `layer`
   * (+1 a layer up, 0 in the layer, −1 a layer down) and `arrowDeg`, the
   * direction in the layer from +X counter-clockwise. */
  struct TransformButton {
    int task = 0;
    OpKind kind = OpKind::Flip;
    int axis = 0;                    ///< 0 X, 1 Y, 2 Z
    int sign = 0;                    ///< −1, +1, or 0 for a single rotation button
    std::string_view glyph;          ///< a glyph name; empty: an arrow at arrowDeg
    float arrowDeg = 0;
    int layer = 0;
    std::string_view caption;
    std::string_view tooltip;
    std::string_view id;             ///< after "entities.transform."
  };

  /* What the inspector offers for a voxel type (C14's type descriptor). */
  struct OpsDescriptor {
    /* the two-block Transform layout -- a Flip & Rotate matrix, then a
     * nudge pad -- for types whose nudges are not ±X/±Y/±Z */
    bool pads = false;
    bool scale = true;               ///< ×2 and ×3 (legacy's Shape column)
    bool minimize = false;           ///< legacy implements it for bricks only
    std::vector<TransformButton> buttons;
  };
  const OpsDescriptor & opsDescriptor(gridType_c::gridType t);

  /* Run a task on one shape as legacy does for its voxel type, then
   * initHotspot() as legacy does. No checks. False when the library
   * refused (a transform the grid cannot take). */
  bool applyTask(voxel_c & v, int task);

  /* Whether a task may run on these shapes, and if not why ("Blocked: the
   * piece would leave the grid", "Already at minimum resolution", ...).
   * Empty shapes are left alone by Prune, Center and To origin; a transform
   * of an empty shape is allowed and does nothing (C04). */
  struct OpCheck {
    bool allowed = true;
    std::string reason;
  };
  OpCheck checkTask(const puzzle_c & p, const std::vector<unsigned int> & shapes, int task);

  /* Run a checked task on the shapes; what to say afterwards (C05's "Filled
   * 3 interior voxels", Minimize's factor), empty when nothing is said. */
  struct OpResult {
    bool changed = false;
    std::string message;
  };
  OpResult runTask(puzzle_c & p, const std::vector<unsigned int> & shapes, int task);

  /* the factor Minimize divides by, over every shape given (legacy: every
   * prime that divides all of them, as often as it does); 1 for none */
  int minimizeFactor(const puzzle_c & p, const std::vector<unsigned int> & shapes);

  /* C05's surface sets: a filled voxel is outer when a face neighbour is
   * outside the grid or an empty cell reachable from outside through empty
   * cells (exterior space); every other filled voxel is inner -- walls of
   * an enclosed cavity too (AC-C05-04; legacy counts those as outer). */
  std::vector<bool> outerVoxels(const voxel_c & v);

  /* The hover preview of a Transform button (legacy previewTransform): the
   * shape as the task leaves it, and what to draw: a rotation's axis and
   * angle, a mirror's plane normal, a nudge's direction (drawing space).
   * Blocked: the task would lose voxels -- no result to show. */
  struct TaskPreview {
    std::unique_ptr<voxel_c> result;
    enum class Hint { None, Rotate, Mirror, Nudge } hint = Hint::None;
    Vec3 axis{ 0, 0, 1 };
    float angleDeg = 0;
    bool blocked = false;
  };
  TaskPreview previewTask(const voxel_c & v, int task);
}

#endif
