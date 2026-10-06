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
#include "imagepages.h"

#include <algorithm>

namespace btui {

  unsigned int pixelsFor(unsigned int mm, unsigned int dpi) {
    return (unsigned int)(int(dpi * mm * 0.03937 + 0.5));
  }

  unsigned int planLineHeight(const std::vector<double> & ratios, unsigned int pageW, unsigned int pageH,
                              unsigned int pages) {
    if (ratios.empty() || pageW == 0 || pageH == 0)
      return std::max(1u, pageH);
    if (pages == 0) pages = 1;
    if (pages > ratios.size()) pages = unsigned(ratios.size());

    // legacy starts at the page height and lowers the line height until
    // everything fits; it could reach 0 and divide by it, so 1 is the floor
    for (unsigned int h = pageH; h > 1; h--) {
      unsigned int curWidth = 0, curLine = 0, curPage = 0;
      const unsigned int linesPerPage = pageH / h;
      for (double r : ratios) {
        const unsigned int w = pictureWidth(r, h);
        if (curWidth + w < pageW || curWidth == 0) {
          curWidth += w + pageW / 60;
        } else {
          curWidth = w + pageW / 60;
          curLine++;
          if (curLine >= linesPerPage) {
            curLine = 0;
            curPage++;
          }
        }
      }
      if (curPage < pages || (curPage == pages && curLine == 0 && curWidth == 0))
        return h;
    }
    return 1;
  }

  std::vector<Placement> placePictures(const std::vector<unsigned int> & widths, unsigned int pageW,
                                       unsigned int pageH, unsigned int lineHeight) {
    std::vector<Placement> out;
    if (lineHeight == 0)
      lineHeight = 1;
    const unsigned int linesPerPage = std::max(1u, pageH / lineHeight);
    unsigned int curWidth = 0, curLine = 0, curPage = 0;
    for (unsigned int w : widths) {
      if (curWidth + w < pageW || curWidth == 0) {
        out.push_back({ curPage, curWidth, curLine * lineHeight });
        curWidth += w + pageW / 60;
      } else {
        curWidth = w + pageW / 60;
        curLine++;
        if (curLine >= linesPerPage) {
          curLine = 0;
          curPage++;
        }
        out.push_back({ curPage, 0, curLine * lineHeight });
      }
    }
    return out;
  }
}
