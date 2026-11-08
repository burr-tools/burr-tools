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
#ifndef BTUI_DEPTHSORT_H
#define BTUI_DEPTHSORT_H

#include "vecmath.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace btui {

  /* The back-to-front order of translucent triangles for one view: the 3D
   * view blends every translucent layer, farthest first (voxel faces never
   * cross, so ordering whole triangles by their centres is exact).
   *
   * The 3D view sorts again whenever the view turns -- every frame of an
   * orbit -- so a sorter keeps its storage: once it has sorted a list, it
   * sorts that list again for any other view without allocating.
   */
  class DepthSorter {
  public:
    /* Indices of the triangles whose centres are given, three per triangle
     * (vertex `base + 3 i` onward for triangle i), farthest first as seen
     * through `view` (which looks down -z). Fills `out`, reusing its storage. */
    void sort(const std::vector<Vec3> & centres, const Mat4 & view, std::uint32_t base,
              std::vector<std::uint32_t> & out);

  private:
    std::vector<std::pair<float, std::uint32_t>> m_order;
  };

  /* Whether two views put the scene in the same depth order: the same
   * rotation. Panning and zooming move the view but keep the order. */
  bool sameRotation(const Mat4 & a, const Mat4 & b);
}

#endif
