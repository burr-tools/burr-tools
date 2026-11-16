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
#ifndef BTUI_PUZZLESCENE_H
#define BTUI_PUZZLESCENE_H

#include "problemlist.h"
#include "scenemesh.h"

#include <cstddef>
#include <vector>

class puzzle_c;

/* The Puzzle workspace's 3D scene (C18): the problem's goal shapes, the
 * selected piece, and one object per copy of every piece.
 *
 * The arrangement stays on screen while every object turns about its own
 * centre, which no single view matrix can do. So each object's vertices are
 * kept around its own centre, and its place in the arrangement -- the anchor,
 * in view space -- is carried in each vertex's `cell`, which only the layer
 * dimming reads and this scene has none of. The renderer adds the anchor
 * after the rotation (SceneFrame::anchored). One mesh, built once per change,
 * however the camera turns.
 */
namespace btui {

  /* more copies than anyone can look at are left out of the picture, and
   * counted; a count can be as high as 65535 */
  constexpr int maxSceneObjects = 100;
  /* so are copies of shapes so detailed that the picture would cost more than
   * a click can wait for: the goal and the selected piece always show */
  constexpr size_t maxSceneVertices = 3'000'000;

  enum class SceneRole { Goal, Selected, Piece };

  struct SceneObject {
    SceneRole role = SceneRole::Piece;
    GoalRole goal = GoalRole::Result;    ///< which slot, for SceneRole::Goal
    int shape = -1;
    int copy = 0;              ///< 0-based, among `copies`
    int copies = 1;
    bool optional = false;     ///< a range's copy beyond the minimum (C18)
    Vec3 anchor;               ///< its centre on the view plane
    float radius = 0.5f;       ///< what it can reach, however it turns
  };

  struct PuzzleScene {
    ShapeMesh mesh;
    std::vector<SceneObject> objects;
    float radius = 0.5f;       ///< how much the camera must show around the origin
    int hidden = 0;            ///< copies left out (maxSceneObjects, maxSceneVertices)
  };

  /* `options` gives the voxel style, the colour view and the palette; each
   * shape takes its own colour. `selected` is the shape of the Selected
   * slot; the slot is left out for none and for a goal shape. */
  PuzzleScene buildPuzzleScene(const puzzle_c & p, int problem, int selected, const MeshOptions & options);
}

#endif
