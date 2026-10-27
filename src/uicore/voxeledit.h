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
#ifndef BTUI_VOXELEDIT_H
#define BTUI_VOXELEDIT_H

#include "commands.h"
#include "scenemesh.h"

#include "../lib/gridtype.h"

#include <array>
#include <compare>
#include <optional>
#include <vector>

class puzzle_c;
class voxel_c;

/* The voxel editor's rules (C07, C08, C02), without the editor: the four
 * tools and the quick tools the modifiers pick, Mirror and Span, the 2D
 * planes, what a tool does to the cells it reaches, and the grid size.
 * Legacy's gridEditor_c (setRecursive, setLayer) and ToolTab (resizeSpace)
 * without the window; the differences are the spec's and named where made.
 *
 * Coordinates the GUI can produce but that name no cell -- outside the
 * grid, or not a cell of the voxel type (voxel_c::validCoordinate) -- are
 * skipped, never asserted.
 */
namespace btui {

  /* the tools 1-4 (C07): their numbers are the keys, badges and quick tools */
  enum class Tool { Fixed = 0, Variable = 1, Erase = 2, Paint = 3 };

  struct Cell3 {
    int x = 0, y = 0, z = 0;
    auto operator<=>(const Cell3 &) const = default;
  };

  /* Mirror and Span, per axis X Y Z (C07) */
  struct Modifiers {
    std::array<bool, 3> mirror{};
    std::array<bool, 3> span{};
  };

  // --- planes (C08) ---------------------------------------------------------

  /* the grid axes (0 X, 1 Y, 2 Z) a plane shows: XY columns X, rows Y,
   * layers Z; XZ columns X, rows Z, layers Y; YZ columns Z, rows Y, layers X */
  struct PlaneAxes { int col = 0, row = 1, layer = 2; };
  PlaneAxes planeAxes(Plane p);

  /* the voxel a cell of a plane's layer is */
  Cell3 voxelAt(Plane p, int col, int row, int layer);

  /* Only bricks have a 2D grid in every plane; the other voxel types are
   * edited in the XY plane, one Z layer at a time, as legacy does (product
   * decision 2026-10-27: their XZ and YZ cuts are not designed). */
  bool planeAvailable(gridType_c::gridType type, Plane p);

  /* "X", "Y", "Z" for axis 0, 1, 2 */
  char axisLetter(int axis);

  /* whether a layer of a plane holds at least one voxel (the layer strip's dot) */
  bool layerHasVoxels(const voxel_c & shape, Plane p, int layer);

  // --- tools ------------------------------------------------------------------

  /* The modifier keys as Qt reports them: control is Ctrl, or Command on
   * macOS; meta is the Windows key, or Control on macOS. */
  struct ModifierKeys { bool shift = false, alt = false, control = false, meta = false; };

  /* The quick tool the held modifiers pick (C06, C08): Shift = Fixed,
   * Alt = Variable (Option or Command on macOS), Ctrl = Erase (Control on
   * macOS); with several held, Ctrl > Alt/Command > Shift. None without. */
  std::optional<Tool> quickTool(Platform p, ModifierKeys m);

  /* The tool a press uses: the right button erases whatever else holds
   * (AC-C08-RC), then the quick tool, then the active tool. */
  Tool toolInEffect(Tool active, Platform p, ModifierKeys m, bool rightButton);

  /* The cells one placement at t reaches (C07): Span first -- the whole
   * line along each spanned axis, several making a plane or the volume --
   * then Mirror about each mirrored axis's centre plane. Sorted, without
   * duplicates, inside dims. */
  std::vector<Cell3> expand(Cell3 t, std::array<int, 3> dims, const Modifiers & m);

  /* Apply a tool to cells of a shape (legacy setRecursive): Fixed and
   * Variable set the state and the colour, Erase empties (and clears the
   * colour), Paint recolours a voxel that is there and leaves empty cells
   * alone (AC-C07-02). Cells outside the shape or not valid for its voxel
   * type are skipped. Returns whether anything changed. */
  bool applyTool(voxel_c & shape, const std::vector<Cell3> & cells, Tool t, unsigned int colour);

  // --- the 2D grid's cell size (C08) -----------------------------------------

  /* floor(min((areaW - 70) / unitsW, (areaH - 60) / unitsH)), clamped to
   * 16 ... 120 dp: units are the grid's extent in cells (or super-cells) */
  int cellSize(double areaW, double areaH, double unitsW, double unitsH);

  // --- grid size (C02) --------------------------------------------------------

  /* Rhombic shapes are sized in steps of 5 voxels, tetra-octa ones in 3;
   * the steppers count those steps, as legacy's size fields do. */
  int sizeStep(gridType_c::gridType type);

  /* the largest size a stepper takes, in steps (legacy's fields: 1 ... 1000) */
  constexpr int maxGridSteps = 1000;

  /* a shape's size along an axis, in steps */
  int gridSteps(const voxel_c & shape, int axis);

  enum class ResizeResult { Changed, Unchanged, Blocked };

  /* Set the size along an axis to `steps` (clamped to 1 ... maxGridSteps).
   * A shrink that would cut off voxels is refused (OQ-5): Blocked, nothing
   * changed. */
  ResizeResult resizeShape(voxel_c & shape, int axis, int steps);

  struct ResizeAllResult { int changed = 0, blocked = 0; };

  /* Apply to all shapes (C02): every shape's axis changes by the same
   * number of steps, where that is legal -- clamped to the limits, a shape
   * a shrink would cut is left as it is and counted as blocked. */
  ResizeAllResult resizeAllShapes(puzzle_c & p, int axis, int deltaSteps);
}

#endif
