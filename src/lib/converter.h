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
 * given gridType. Bricks convert to rhombic and tetra-octa, on NG's grids
 * (GT_RHOMBIC_TETRAHEDRA, GT_OCTAHEDRAL_TETRAHEDRA) by default and on the
 * native ones (GT_RHOMBIC, GT_TETRA_OCTA) for the FLTK GUI (cutCubeGrids_e).
 */
bool canConvert(gridType_c::gridType src, gridType_c::gridType dst, cutCubeGrids_e cut = cutCubeGrids_e::ng);

/**
 * Create a new puzzle with a new gridtype but identical shapes.
 *
 * If the conversion can't be done (you should check first with canConvert)
 * a NULL pointer will be returned
 */
puzzle_c * doConvert(puzzle_c * p, gridType_c::gridType type, cutCubeGrids_e cut = cutCubeGrids_e::ng);

/**
 * The described grid (BurrTools NG's grids as data) a native grid's
 * puzzles convert to, GT_NUM_GRIDS for none: bricks and triangular prisms
 * to their described copies (for cross-checks), rhombic to NG's rhombic
 * tetrahedra (each voxel becomes two), tetra-octa to NG's octahedral
 * tetrahedra (the same cells). The coordinate maps are NG's importer's.
 */
gridType_c::gridType describedCounterpart(gridType_c::gridType native);

/**
 * The transformation of describedCounterpart(native) that is native's
 * transformation t: the one with the same rotation matrix. Defined for the
 * grids that transform with the cube's matrices (bricks, rhombic,
 * tetra-octa); TND otherwise.
 */
unsigned char describedTransformation(gridType_c::gridType native, unsigned int t);

/**
 * A copy of the puzzle on describedCounterpart() of its grid, with the same
 * shapes, colours, comment and problems; null if its grid has none.
 *
 * The problems keep their solve state, counters and saved assemblies: each
 * piece covers the cells the importer's map gives for its native cells,
 * placed with describedTransformation(). Disassemblies and the search
 * state are dropped (see problem_c::adoptResults()). Triangular prisms,
 * converted only for cross-checks, keep no results.
 *
 * Reading a file converts rhombic and tetra-octa puzzles with this
 * (puzzle_c::read(), cutCubeGrids_e).
 */
puzzle_c * convertToDescribed(const puzzle_c * p);

#endif
