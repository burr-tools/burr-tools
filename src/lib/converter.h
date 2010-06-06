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
#ifndef __CONVERTER_H__
#define __CONVERTER_H__

#include "gridtype.h"

class puzzle_c;

/**
 * returns true, if it is possible to convert this puzzle to the
 * given gridType
 */
bool canConvert(gridType_c::gridType src, gridType_c::gridType dst);

/**
 * Create a new puzzle with a new gridtype but identical shapes.
 *
 * If the conversion can't be done (you should check first with canConvert)
 * a NULL pointer will be returned
 */
puzzle_c * doConvert(puzzle_c * p, gridType_c::gridType type);

#endif
