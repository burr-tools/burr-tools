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
#ifndef BTUI_SHAPECOLOURS_H
#define BTUI_SHAPECOLOURS_H

#include "palette.h"

/* The colours a shape is shown in (Settings ▸ General ▸ Shape colours).
 *
 * Classic is the table in palette.h, pure primaries that legacy has always
 * used. Muted keeps each hue and halves its chroma in OKLCH, the lightness
 * spread over a band that suits the theme: the shapes' light and dark tiers
 * stay apart, every shape reaches 3:1 against its panel in both themes (dark
 * blue is 1.2:1 on the dark panel in Classic, yellow 1.1:1 on the light one),
 * and text on a shape keeps 4.5:1.
 *
 * This is a mapping for the screen only: palette.h, the legacy GUI, the
 * puzzle file and exported images keep the table's colours.
 */
namespace btui {

  enum class ShapePalette { Muted, Classic };

  /* a colour of the table as Muted shows it on a dark or a light theme */
  Rgb mutedColour(const Rgb & classic, bool dark);

  /* which colours are on show. The default is the table's own, for whatever
   * is not the screen: an export, a test. */
  struct ShapeColours {
    ShapePalette palette = ShapePalette::Classic;
    bool dark = false;

    Rgb of(int shape) const;
    Rgb of(int shape, int copy) const;
    /* text on the shape: the better of white and black, as prefersWhiteText */
    bool whiteText(int shape) const;

    bool operator==(const ShapeColours &) const = default;
  };
}

#endif
