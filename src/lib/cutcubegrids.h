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
#ifndef __CUT_CUBE_GRIDS_H__
#define __CUT_CUBE_GRIDS_H__

/**
 * Where rhombic and tetra-octa puzzles live (step C1 of
 * design/2026-11-19-ng-merge-plan.md). By default on BurrTools NG's grids,
 * GT_RHOMBIC_TETRAHEDRA and GT_OCTAHEDRAL_TETRAHEDRA: files of the native
 * types GT_RHOMBIC and GT_TETRA_OCTA are converted when they are read, new
 * puzzles and conversions from bricks get NG's grids, and files are written
 * with NG's types. NG's rhombic grid has more piece positions (half a cube
 * diagonal) and both of NG's grids disassemble. The FLTK GUI asks for the
 * native grids, which it keeps with their editors until it retires.
 */
enum class cutCubeGrids_e {
  ng,       ///< NG's grids, the default
  native    ///< GT_RHOMBIC and GT_TETRA_OCTA as they are (the FLTK GUI)
};

#endif
