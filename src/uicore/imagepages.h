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
#ifndef BTUI_IMAGEPAGES_H
#define BTUI_IMAGEPAGES_H

#include <vector>

/* How Export ▸ Image lays its pictures out on pages (legacy imageExport_c
 * PostDraw): every picture is one line high, lines fill left to right with
 * a gap of 1/60 of the page width, and the line height is the largest that
 * fits all pictures onto the requested number of pages.
 */
namespace btui {

  /* the paper sizes the dialog offers, in mm, and the pixels they make */
  struct PaperSize { unsigned int widthMm, heightMm; };
  constexpr PaperSize kA4Portrait{ 210, 297 }, kA4Landscape{ 297, 210 };
  constexpr PaperSize kLetterPortrait{ 216, 279 }, kLetterLandscape{ 279, 216 };

  /* legacy: dpi * mm * 0.03937, rounded */
  unsigned int pixelsFor(unsigned int mm, unsigned int dpi);

  /* The line height for pictures of the given width/height ratios on
   * `pages` pages of pageW x pageH pixels (pages is clamped to 1 and to the
   * number of pictures, as legacy does). At least 1. */
  unsigned int planLineHeight(const std::vector<double> & ratios, unsigned int pageW, unsigned int pageH,
                              unsigned int pages);

  struct Placement { unsigned int page, x, y; };

  /* Where pictures of the given (final) widths go, at that line height;
   * the page count is the last placement's page + 1. */
  std::vector<Placement> placePictures(const std::vector<unsigned int> & widths, unsigned int pageW,
                                       unsigned int pageH, unsigned int lineHeight);

  /* legacy's width for a picture of that ratio at that height */
  inline unsigned int pictureWidth(double ratio, unsigned int height) {
    return (unsigned int)(height * ratio + 0.9);
  }
}

#endif
