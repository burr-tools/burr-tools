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
#include "depthsort.h"

#include <algorithm>

namespace btui {

  void DepthSorter::sort(const std::vector<Vec3> & centres, const Mat4 & view, std::uint32_t base,
                         std::vector<std::uint32_t> & out) {
    const size_t n = centres.size();
    m_order.resize(n);
    for (size_t i = 0; i < n; i++) {
      const Vec3 & c = centres[i];
      // view-space z; the view looks down -z, so the most negative is farthest
      m_order[i] = { view(2, 0) * c.x + view(2, 1) * c.y + view(2, 2) * c.z + view(2, 3), std::uint32_t(i) };
    }
    std::sort(m_order.begin(), m_order.end(), [](const auto & a, const auto & b) { return a.first < b.first; });
    out.resize(n * 3);
    for (size_t i = 0; i < n; i++) {
      const std::uint32_t first = base + m_order[i].second * 3;
      out[i * 3] = first;
      out[i * 3 + 1] = first + 1;
      out[i * 3 + 2] = first + 2;
    }
  }

  bool sameRotation(const Mat4 & a, const Mat4 & b) {
    for (int r = 0; r < 3; r++)
      for (int c = 0; c < 3; c++)
        if (a(r, c) != b(r, c))
          return false;
    return true;
  }
}
