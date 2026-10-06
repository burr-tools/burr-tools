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
#ifndef BTUI_SCENEMESH_H
#define BTUI_SCENEMESH_H

#include "palette.h"
#include "vecmath.h"

#include <cstdint>
#include <vector>

class voxel_c;
class Polyhedron;

/* GPU-ready geometry of a shape for the 3D view, plus picking and the
 * overlays (axes, grid boundary, layer slab).
 *
 * The geometry comes from the library's own meshes, so every grid type --
 * cubes, prisms, spheres, both tetrahedral grids -- has its true shape. Two
 * looks (VoxelStyle): the spec's flat faces in one colour, each voxel face
 * outlined (C06, the mock's render3d), or legacy's voxel style
 * (getDrawingMesh(): each voxel with its bevels and the alternating light
 * and dark shade). Picking uses the flat mesh, as legacy does, so a click
 * lands on the same voxel and face whichever style is shown.
 *
 * Coordinates are the shape's drawing space: the shape spans
 * [0, calculateSize()] on each axis.
 */
namespace btui {

  struct Rgba8 { std::uint8_t r = 0, g = 0, b = 0, a = 255; };

  /* the scene's light direction, in view space (C13); the renderer and the
   * vector export both shade with it */
  inline constexpr float kSceneLight[3] = { -0.35f, 0.75f, 0.55f };

  /* One vertex as the renderer's vertex buffer holds it. `cell` is the grid
   * coordinate of the voxel the face belongs to, so the shader can dim the
   * voxels outside the active layer without rebuilding the mesh.
   *
   * `edge` draws the face outlines in the shader: xyz are the vertex's
   * barycentric coordinates in its triangle -- each is the distance to the
   * opposite edge -- plus kNoOutline where that edge is not part of a face's
   * outline (the diagonals of a fanned face); w is 1 for a dashed outline.
   * The default is no outline at all. */
  inline constexpr float kNoOutline = 2.0f;
  struct MeshVertex {
    float pos[3]{};
    float normal[3]{};
    Rgba8 color;
    float cell[3]{};
    float edge[4]{ kNoOutline, kNoOutline, kNoOutline, 0.0f };
  };
  static_assert(sizeof(MeshVertex) == 56, "the renderer's vertex layout assumes 56 bytes");

  enum class ColorMode { Piece, Voxel };

  /* How voxels look (Settings ▸ 3D view ▸ Voxel style).
   *   Flat    the redesign's: plain faces in the voxel's colour, every voxel
   *           face outlined, variable voxels slightly see-through
   *           (variableAlpha) with a dashed outline (C06, the mock)
   *   Legacy  legacy BurrTools': bevelled voxels in alternating light and
   *           dark shades, variable voxels opaque with a black marker */
  enum class VoxelStyle { Flat, Legacy };

  struct MeshOptions {
    VoxelStyle style = VoxelStyle::Flat;
    ColorMode colors = ColorMode::Piece;
    Rgb piece{ 0.5f, 0.5f, 0.5f };   ///< the shape's colour
    std::vector<Rgb> palette;        ///< the puzzle's colours; voxel colour i uses palette[i-1]
    /* the flat style's variable voxels: translucent, so the variable voxels
     * inside show through the outer ones, layer over layer (C06 and the mock
     * say 50 %; user decision). 1 draws them opaque. */
    float variableAlpha = 0.6f;
  };

  struct ShapeMesh {
    std::vector<MeshVertex> opaque;       ///< fixed voxels, triangles
    std::vector<MeshVertex> translucent;  ///< variable voxels, triangles, alpha from MeshOptions
    Vec3 boundsMin, boundsMax;            ///< the shape's drawing-space box, [0, calculateSize]
    bool empty(void) const { return opaque.empty() && translucent.empty(); }
  };

  ShapeMesh buildShapeMesh(const voxel_c & shape, const MeshOptions & opt);

  /* An externally made mesh -- the STL exporter's -- the way legacy
   * voxelFrame_c::showMesh() draws it: one grey, the dark shade, no checker.
   * With insides, every face goes to the translucent list at 10 % for the
   * renderer's x-ray pass (legacy setInsideVisible). Bounds are the
   * vertices' box. */
  ShapeMesh buildPolyhedronMesh(const Polyhedron & poly, bool insides);

  // --- picking --------------------------------------------------------------

  struct PickTriangle {
    Vec3 a, b, c;
    std::uint32_t voxel = 0;   ///< voxel index in the shape
    std::int32_t face = 0;     ///< the face's neighbour index, as voxel_c::getNeighbor() takes it
  };

  struct PickHit {
    bool hit = false;
    float t = 0;           ///< ray parameter of the hit
    Vec3 point;
    std::uint32_t voxel = 0;
    std::int32_t face = 0;
  };

  std::vector<PickTriangle> buildPickMesh(const voxel_c & shape);

  /* the front-most triangle the ray meets (legacy: smallest depth) */
  PickHit pick(const std::vector<PickTriangle> & mesh, Vec3 origin, Vec3 dir);

  // --- overlays -------------------------------------------------------------

  struct Box { Vec3 min, max; };

  /* the 2D editing planes; the layer runs along Z, Y and X respectively */
  enum class Plane { XY = 0, XZ = 1, YZ = 2 };

  /* the grid axis a plane's layers run along (0 X, 1 Y, 2 Z) */
  int layerAxis(Plane p);

  /* number of layers of a shape for a plane */
  int layerCount(const voxel_c & shape, Plane p);

  Box gridBounds(const voxel_c & shape);

  /* The active-layer slab (C06): the whole layer, one layer thick, in
   * drawing space -- for every grid type, through the shape's own grid to
   * drawing-space mapping. */
  Box layerSlab(const voxel_c & shape, Plane p, int layer);

  /* A line for the overlay pass: drawn as a screen-space quad of widthDp,
   * dashed when dashDp > 0. */
  struct LineSeg {
    Vec3 a, b;
    Rgba8 color;
    float widthDp = 1;
    float dashDp = 0, gapDp = 0;
  };

  /* the 12 edges of a box */
  void addBoxEdges(std::vector<LineSeg> & out, const Box & b, Rgba8 color, float widthDp, float dashDp = 0, float gapDp = 0);

  /* the six faces of a box as triangles (36 vertices), one flat colour */
  void addBoxFaces(std::vector<MeshVertex> & out, const Box & b, Rgba8 color);

  /* the axes from the origin, reaching 0.7 of a cell beyond the grid (C06) */
  void addAxes(std::vector<LineSeg> & out, const voxel_c & shape, Rgba8 x, Rgba8 y, Rgba8 z, float widthDp);
}

#endif
