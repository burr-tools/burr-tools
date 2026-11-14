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
#ifndef BTUI_SOLUTIONSCENE_H
#define BTUI_SOLUTIONSCENE_H

#include "scenemesh.h"
#include "solver.h"

#include <memory>
#include <vector>

class assembly_c;
class piecePositions_c;
class problem_c;
class voxel_c;

/* The Solver's 3D scene (C21): one assembly, every piece where the
 * disassembly puts it at the player's position, each solid, wireframe or
 * hidden. The pieces' meshes are built once per assembly and voxel style;
 * a move position only moves them (compose), so the player can redraw every
 * frame (AC-C20-02).
 */
namespace btui {

  /* A wireframe's line: an edge of a piece's hull, with the normals of the
   * two faces meeting there, so the view can draw the back edges fainter
   * (AC-C21-02). */
  struct HullEdge {
    Vec3 a, b;
    Vec3 n1, n2;
  };

  /* The edges of a mesh's hull: where faces meet at more than `creaseDeg`,
   * and where a face has no neighbour; not the diagonals that split a face
   * into triangles, nor the seams between coplanar faces of two voxels. A
   * round mesh (spheres) wants a coarse angle, or every facet is an edge. */
  std::vector<HullEdge> hullEdges(const ShapeMesh & mesh, float creaseDeg = 2.5f);
  /* the crease angle a voxel type's wireframe uses: 25° for spheres */
  float wireCreaseAngle(int gridType);

  class SolutionScene {

  public:

    /* the wireframe pieces' faces: tinted at about 6 % (C21) */
    static constexpr float wireTint = 0.06f;

    SolutionScene(void);
    ~SolutionScene(void);
    SolutionScene(SolutionScene &&) noexcept;
    SolutionScene & operator=(SolutionScene &&) noexcept;

    /* The assembly's pieces in the problem's shapes, each in its shape's
     * colour (options.shapeColours, a copy by its copy number) and the
     * options' voxel style; pieces the assembly does not place are left out.
     * Empty for no assembly or no valid result. Assembler states (C21) are
     * assemblies too, with fewer pieces placed. */
    static SolutionScene build(const problem_c & prob, const assembly_c * assembly, const MeshOptions & options);

    bool empty(void) const { return pieces.empty(); }
    /* the pieces placed, by their number in the assembly */
    int pieceCount(void) const { return int(pieces.size()); }

    /* What the camera frames: the assembled pieces. */
    Vec3 centre{ 0, 0, 0 };
    float radius = 0.5f;

    /* One frame: the pieces at `positions` (null: assembled; a piece whose
     * alpha there is 0 has left), in `states` by piece number (missing ones
     * solid). Solid pieces are their faces; wireframe ones their hull's
     * edges in `wire` (moved with the piece) and their faces at wireTint;
     * hidden ones nothing. `mesh` and `wire` are cleared first. */
    void compose(ShapeMesh & mesh, std::vector<HullEdge> & wire, std::vector<Rgba8> & wireColours,
                 piecePositions_c * positions, const std::vector<PieceState> & states) const;

  private:

    struct Piece;
    std::vector<std::unique_ptr<Piece>> pieces;
  };
}

#endif
