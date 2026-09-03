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
#ifndef __SCADLOADER_H__
#define __SCADLOADER_H__

/* Load cubic voxel pieces from a puzzlecad-style OpenSCAD file.
 *
 * puzzlecad (bt2scad) stores each piece as strings inside burr_plate / burr_piece:
 *   - each string is one Z layer (z increasing)
 *   - '|' separates Y rows (y increasing)
 *   - each character is one X cell (x increasing)
 *   - '.' is empty, any other cell character is filled
 *   - '{...}' annotations after a cell are ignored
 *
 * packing_box uses the same layer strings and becomes the problem result shape
 * when present. Calls disabled with OpenSCAD's '*' prefix are skipped.
 *
 * When no result shape is found, a cube of variable voxels is created. Its
 * size is the longest occupied extent of any piece along X, Y or Z.
 *
 * Either returns a puzzle, or nil when failed / nothing usable was found.
 */

#include <iostream>

class puzzle_c;

puzzle_c * loadOpenScadPuzzle(std::istream * str);

/* Write puzzlecad OpenSCAD using the MagellanTIC.scad layout as a template.
 * sourceName is placed in the header comment. problemIndex selects which
 * problem's parts to export; the result shape is omitted. Returns false
 * when there is nothing to write.
 */
bool saveOpenScadPuzzle(std::ostream & str, const puzzle_c * p,
                        const char * sourceName, unsigned int problemIndex);

#endif
