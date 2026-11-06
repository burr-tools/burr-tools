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
#ifndef BTUI_OKHSL_H
#define BTUI_OKHSL_H

#include "palette.h"

namespace btui {

  /* Okhsl, Björn Ottosson's perceptual hue / saturation / lightness space
   * (https://bottosson.github.io/posts/colorpicker/): colours of one
   * lightness look equally bright whatever their hue, and saturation 1 is
   * the most saturated sRGB colour of that hue and lightness. hue is in
   * degrees, saturation and lightness in [0, 1]. The result is sRGB, each
   * channel in [0, 1]. */
  Rgb okhslToSrgb(float hueDegrees, float saturation, float lightness);
}

#endif
