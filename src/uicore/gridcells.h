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
#ifndef BTUI_GRIDCELLS_H
#define BTUI_GRIDCELLS_H

#include "voxeledit.h"

#include <vector>

class voxel_c;

/* The 2D grid's cells (C08, "Cell shapes by voxel type"): one layer of a
 * shape as the shapes its voxel type has -- squares for bricks,
 * triangles for triangular prisms, touching circles on an offset lattice
 * for spheres, the triangles and squares of the 5- and 3-voxel super-cells
 * of rhombic and tetra-octa grids. Legacy's gridEditor_0 ... _4 drew them
 * (getTriangle, getPolygon); here they are geometry, for the toolkit to
 * draw and hit-test.
 *
 * Units: a brick cell, or a super-cell of the rhombic and tetra-octa grids,
 * is 1 x 1; a prism's triangle is 1 wide; y grows upwards (row 0 at the
 * bottom), so the toolkit flips it. The layout's width and height enclose
 * every cell, from (0, 0).
 */
namespace btui {

  struct Pt { double x = 0, y = 0; };

  struct GridCell {
    Cell3 voxel;                  ///< the voxel the cell is
    int col = 0, row = 0;         ///< its column and row in the plane's numbering
    std::vector<Pt> outline;      ///< convex, counter-clockwise; a circle's as a 24-gon
    bool round = false;           ///< a sphere: centre and radius are exact
    Pt centre;
    double radius = 0;
  };

  struct GridLayout {
    double width = 0, height = 0;
    std::vector<GridCell> cells;
    /* square cells, one per column and row: rulers can number them */
    bool squares = false;
    int cols = 0, rows = 0;
  };

  /* The cells of a layer of a plane. Prisms have the XY plane only
   * (planeAvailable); for their others, or a layer out of range, the layout
   * is empty. */
  GridLayout gridLayout(const voxel_c & shape, Plane p, int layer);

  /* the index of the cell at a point, -1 for none */
  int cellAt(const GridLayout & layout, Pt p);

  /* a cell's centre: the circle's, else its outline's corners' mean */
  Pt cellCentre(const GridCell & c);

  /* The cell an arrow key steps to from cell `from` (the grid by keyboard):
   * the nearest whose centre lies within 60 degrees of the direction
   * (dx, dy), in layout units (y up), straighter ones first; -1 at the
   * edge. Every voxel type's cells, squares or not. */
  int stepCell(const GridLayout & layout, int from, double dx, double dy);

  /* the cell nearest a point, -1 for none: where the keyboard starts */
  int nearestCell(const GridLayout & layout, Pt p);
}

#endif
