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
#ifndef BTUI_VECTOREXPORT_H
#define BTUI_VECTOREXPORT_H

#include "scenemesh.h"
#include "vecmath.h"

#include <string>
#include <string_view>
#include <vector>

/* Export ▸ Vector Image: the 3D view as it is on screen, written as a vector
 * file in the six formats legacy offered (vectorExportWindow_c).
 *
 * Legacy used gl2ps, which captures OpenGL's fixed-function output; the new
 * renderer has none, so the view is captured here instead: every triangle
 * and line is projected with the view's camera, back faces are dropped
 * (opaque ones kept for the Classic style, as the view draws it), the rest
 * shaded as the renderer shades them and sorted back to front
 * (painter's algorithm -- the meshes are closed and finely cut, so a depth
 * sort suffices where gl2ps used a BSP tree). As with gl2ps, there is no
 * background, the page is the view's size in points, and TeX output is
 * gl2ps's: a picture that includes the graphic of the same base name, for
 * text the view does not have.
 */
namespace btui {

  /* in legacy's (and gl2ps's) order */
  enum class VectorFormat { PS, EPS, TeX, PDF, SVG, PGF };

  /* "ps", "eps", "tex", "pdf", "svg", "pgf" */
  std::string_view vectorExtension(VectorFormat f);

  /* what a frame shows -- the parts of SceneFrame that are geometry */
  struct VectorInput {
    const ShapeMesh * mesh = nullptr;
    const std::vector<MeshVertex> * overlayFaces = nullptr;
    const std::vector<LineSeg> * lines = nullptr;
    Mat4 view, projection;
    float width = 0, height = 0;          ///< the view in dp, which become points
    bool lighting = true;
    int dimAxis = -1;
    float dimLayer = 0, dimAlpha = 0.28f;
    /* opaque faces seen from behind are dropped; false keeps them, as the
     * 3D view draws the Classic style (SceneFrame::cullBackFaces): its
     * bevelled mesh's seams show the far side */
    bool cullBackFaces = true;
  };

  struct VectorPrimitive {
    enum class Kind { Polygon, Line } kind = Kind::Polygon;
    std::vector<float> xy;                ///< x0, y0, x1, y1, ...: page points, y down
    float r = 0, g = 0, b = 0, a = 1;     ///< straight (not premultiplied) colour
    float width = 1, dash = 0, gap = 0;   ///< lines only
    float depth = 0;                      ///< distance from the eye along the view axis
  };

  struct VectorPage {
    float width = 0, height = 0;
    std::vector<VectorPrimitive> prims;   ///< back to front
  };

  VectorPage projectScene(const VectorInput & in);

  /* the file's content; baseName names the graphic TeX output includes */
  std::string writeVector(const VectorPage & page, VectorFormat f, const std::string & baseName);
}

#endif
