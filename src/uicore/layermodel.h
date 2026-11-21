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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef BTUI_LAYERMODEL_H
#define BTUI_LAYERMODEL_H

/* The layers a 2D editor shows of a voxel space on a described grid (BurrTools
 * NG's grids as data): the editing contract of section 5.6 of
 * design/2026-11-19-ng-merge-plan.md. One layer family, horizontal planes:
 * for a layer, the cells the plane meets, each with its 2D outline (the
 * cross-section of the cell with the plane, in physical x and y), and a hit
 * test from a point to a cell. A GUI needs no per-grid editor on top of it.
 *
 * The planes are at the heights where cells are centred (the middle of a
 * cell's extent in z), so every cell is in at least one layer, its home
 * (layerOf()). A plane lists every cell it cuts, so a grid whose cells were
 * cut by more than one plane would show them in each, with the outline they
 * have there; on NG's six grids every plane cuts only the cells centred on
 * it.
 *
 * Toolkit-free (see AGENTS.md); the FLTK GUI's editor for described grids and
 * the Qt GUI's editor draw from it.
 */

#include "../lib/gridtype.h"

#include <array>
#include <cstddef>
#include <vector>

namespace btui {

  struct Point2 { double x = 0, y = 0; };

  /* one cell of a layer: its voxel in the space, and its outline in the
   * layer's plane, a convex polygon, counter-clockwise */
  struct LayerCell {
    std::array<int, 3> voxel{};
    std::vector<Point2> outline;
  };

  class LayerModel {
  public:
    /* the layers of a voxel space of size sx x sy x sz on the described grid
     * gt; on another grid (the native ones have their own editors), and for
     * an empty space, no layers */
    LayerModel(const gridType_c & gt, unsigned int sx, unsigned int sy, unsigned int sz);

    unsigned int layerCount(void) const { return static_cast<unsigned int>(layers.size()); }

    /* the physical height of layer l's plane; layers go up with l */
    double height(unsigned int l) const { return l < layers.size() ? layers[l].height : 0.0; }

    /* the cells layer l's plane meets (none for a layer that does not exist) */
    const std::vector<LayerCell> & cells(unsigned int l) const { return l < layers.size() ? layers[l].cells : none; }

    /* the index into cells(l) of the cell whose outline holds p, -1 for none
     * (a point on a shared edge goes to one of the two cells) */
    int hit(unsigned int l, Point2 p) const;

    /* the box around every outline of every layer, for fitting the view (the
     * same for all layers, so the drawing does not jump between them) */
    void bounds(Point2 & lo, Point2 & hi) const { lo = boxLo; hi = boxHi; }

    /* the layer whose plane is at the centre of the voxel's cell, -1 if the
     * voxel is no cell of the space */
    int layerOf(int x, int y, int z) const;

  private:
    struct layer_s {
      double height;
      std::vector<LayerCell> cells;
    };
    std::vector<layer_s> layers;
    std::vector<LayerCell> none;  // cells() of a layer that does not exist
    std::vector<int> home;  // per voxel index of the space: layerOf, -1 for none
    unsigned int sx, sy, sz;
    Point2 boxLo, boxHi;
  };

  /* the area of a polygon (counter-clockwise: positive) */
  double polygonArea(const std::vector<Point2> & poly);

  /* whether p lies in the convex, counter-clockwise polygon (on the border
   * counts) */
  bool inConvexPolygon(const std::vector<Point2> & poly, Point2 p);

}

#endif
