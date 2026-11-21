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
#ifndef __GRID_EDITOR_LAYERS_H__
#define __GRID_EDITOR_LAYERS_H__

#include "grideditor.h"

#include "../uicore/layermodel.h"

#include <memory>

class voxel_c;

/**
 * the editor of the described grids (BurrTools NG's grids as data): one layer
 * of btui::LayerModel, each cell drawn as its outline in the layer's plane.
 * The model does the geometry (outlines, hit test, the cells a rubber band
 * and the mirror and stack tools take), so there is no code per grid here.
 */
class layerGridEditor_c : public gridEditor_c {

  std::unique_ptr<btui::LayerModel> model;

  // the shape and grid the model was built for
  const voxel_c * modelShape = nullptr;
  unsigned int modelX = 0, modelY = 0, modelZ = 0;
  int modelGrid = -1;
  // whether mirror x and y have images in the model's space
  bool mirrorX = false, mirrorY = false;

  // the cells (indices into the current layer's cells) under the mouse and
  // where the rubber band started, -1 for none
  int hoverCell = -1;
  int startCell = -1;
  // the layer they are cells of; when the layer changed, there are none
  unsigned int hoverLayer = 0;

  // the current shape's layers, rebuilt when the shape or its size changed;
  // nullptr when there is no shape
  const btui::LayerModel * layers(void);

  // screen = (ox + s * x, oy - s * y)
  struct view_s { double s, ox, oy; };
  view_s view(const btui::LayerModel & m) const;

  // the cells the cursor marks: the rubber band from startCell to hoverCell
  // widened by the tools
  std::vector<btui::CellRef> marked(const btui::LayerModel & m, unsigned int * missing = nullptr) const;

  // does the task on every marked cell, returns whether something changed
  bool apply(const btui::LayerModel & m);

  // the cell under the screen position, -1 for none
  int cellAt(const btui::LayerModel & m, int sx, int sy) const;

  // sets hoverCell and the mouse coordinates of the base to the cell
  void hover(const btui::LayerModel & m, int cell);

protected:

  int handle(int event) override;
  void draw() override;

public:

  layerGridEditor_c(int x, int y, int w, int h, puzzle_c * p);
  ~layerGridEditor_c() override;

  unsigned int layerCount(void) override;
  bool getMarkedCells(std::vector<std::array<int, 3>> & cells) override;
  bool canMirror(int tool) override;
  // on a cell of the current layer, not just over the widget
  bool getMouse(void) override { return inside && hoverCell >= 0 && hoverLayer == currentZ; }
  unsigned int getMissingMirrors(void) override;
};

#endif
