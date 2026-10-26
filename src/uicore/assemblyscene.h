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
#ifndef BTUI_ASSEMBLYSCENE_H
#define BTUI_ASSEMBLYSCENE_H

#include "scenemesh.h"

class piecePositions_c;
class problem_c;
class puzzle_c;

/* Scenes of several pieces in one mesh: a single shape, or a problem's
 * assembly with its pieces where a solution puts them -- optionally moved
 * to a step of the disassembly. This is what legacy voxelFrame_c's
 * showSingleShape(), showAssembly(), updatePositions() and dimStaticPieces()
 * set up, for the image export now and the Puzzle and Solver views later.
 */
namespace btui {

  struct SceneContent {
    ShapeMesh mesh;      ///< every piece, already in place
    Vec3 centre;         ///< where the camera looks
    float radius = 0.5f; ///< how much it must show around the centre
  };

  /* one shape in its own colour (piece colours) or the puzzle's
   * (colour constraint colours), in the 3D view's voxel style */
  SceneContent buildShapeScene(const puzzle_c & puz, unsigned int shape, ColorMode colors,
                               VoxelStyle style = VoxelStyle::Flat);

  /* Solution `solution` of the problem. With positions, each piece moves
   * to its place at that disassembly step and takes its alpha there
   * (pieces that have left fade out); dimStatic lightens the pieces that do
   * not move in the step, as legacy dimStaticPieces() does. Empty when the
   * problem has no such solution or no valid result. */
  SceneContent buildAssemblyScene(const problem_c & prob, unsigned int solution, ColorMode colors,
                                  piecePositions_c * positions = nullptr, bool dimStatic = false,
                                  VoxelStyle style = VoxelStyle::Flat);
}

#endif
