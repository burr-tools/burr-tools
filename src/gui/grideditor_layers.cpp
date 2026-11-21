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
#include "grideditor_layers.h"
#include "piececolor.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#pragma GCC diagnostic pop

#include <algorithm>
#include <cmath>

using btui::CellRef;
using btui::LayerCell;
using btui::LayerModel;
using btui::Point2;

layerGridEditor_c::layerGridEditor_c(int x, int y, int w, int h, puzzle_c * p) : gridEditor_c(x, y, w, h, p) {}

layerGridEditor_c::~layerGridEditor_c() = default;

const LayerModel * layerGridEditor_c::layers(void) {

  if (!puzzle || piecenumber >= puzzle->getNumberOfShapes()) return nullptr;

  const voxel_c * space = puzzle->getShape(piecenumber);
  const gridType_c * gt = puzzle->getGridType();

  if (!model || space != modelShape || space->getX() != modelX || space->getY() != modelY ||
      space->getZ() != modelZ || (int)gt->getType() != modelGrid) {
    model = std::make_unique<LayerModel>(*gt, space->getX(), space->getY(), space->getZ());
    modelShape = space;
    modelX = space->getX();
    modelY = space->getY();
    modelZ = space->getZ();
    modelGrid = gt->getType();
    mirrorX = model->hasMirror(LayerModel::TOOL_MIRROR_X);
    mirrorY = model->hasMirror(LayerModel::TOOL_MIRROR_Y);
    hoverCell = startCell = -1;
  }

  return model.get();
}

unsigned int layerGridEditor_c::layerCount(void) {
  const LayerModel * m = layers();
  return m ? m->layerCount() : 0;
}

layerGridEditor_c::view_s layerGridEditor_c::view(const LayerModel & m) const {

  Point2 lo, hi;
  m.bounds(lo, hi);

  const double dx = std::max(hi.x - lo.x, 1e-6);
  const double dy = std::max(hi.y - lo.y, 1e-6);

  // as large as fits, at most 40 pixels to the unit (the cube editor's
  // largest square)
  double s = std::min((w() - 3) / dx, (h() - 3) / dy);
  s = std::clamp(s, 0.0, 40.0);

  view_s v;
  v.s = s;
  v.ox = x() + (w() - s * dx) / 2 - s * lo.x;
  v.oy = y() + h() - (h() - s * dy) / 2 + s * lo.y;
  return v;
}

int layerGridEditor_c::cellAt(const LayerModel & m, int sx, int sy) const {
  const view_s v = view(m);
  if (v.s <= 0) return -1;
  return m.hit(currentZ, {(sx - v.ox) / v.s, (v.oy - sy) / v.s});
}

std::vector<CellRef> layerGridEditor_c::marked(const LayerModel & m, unsigned int * missing) const {

  if (missing) *missing = 0;
  const unsigned int n = static_cast<unsigned int>(m.cells(currentZ).size());
  if (hoverLayer != currentZ || hoverCell < 0 || (unsigned)hoverCell >= n) return {};

  const int start = (editType == EDT_RUBBER && startCell >= 0 && (unsigned)startCell < n) ? startCell : hoverCell;

  return m.region(currentZ, m.centre(currentZ, start), m.centre(currentZ, hoverCell), activeTools, missing);
}

bool layerGridEditor_c::canMirror(int tool) {
  if (!layers()) return false;
  if (tool == TOOL_MIRROR_X) return mirrorX;
  if (tool == TOOL_MIRROR_Y) return mirrorY;
  return true;
}

unsigned int layerGridEditor_c::getMissingMirrors(void) {
  const LayerModel * m = layers();
  if (!m || !inside) return 0;
  unsigned int missing = 0;
  marked(*m, &missing);
  return missing;
}

bool layerGridEditor_c::getMarkedCells(std::vector<std::array<int, 3>> & cells) {
  cells.clear();
  const LayerModel * m = layers();
  if (m && inside)
    for (const CellRef & r : marked(*m))
      cells.push_back(m->cells(r.layer)[r.index].voxel);
  return true;
}

bool layerGridEditor_c::apply(const LayerModel & m) {

  bool changed = false;
  for (const CellRef & r : marked(m)) {
    const std::array<int, 3> & v = m.cells(r.layer)[r.index].voxel;
    changed |= setVoxel(v[0], v[1], v[2]);
  }

  if (changed)
    puzzle->getShape(piecenumber)->initHotspot();

  return changed;
}

void layerGridEditor_c::hover(const LayerModel & m, int cell) {
  hoverCell = cell;
  hoverLayer = currentZ;
  if (cell >= 0) {
    const std::array<int, 3> & v = m.cells(currentZ)[cell].voxel;
    mX = v[0];
    mY = v[1];
    mZ = v[2];
  }
  // the box the base class reports is the cell itself: the 3D view takes the
  // marked cells from getMarkedCells()
  startX = mX;
  startY = mY;
}

int layerGridEditor_c::handle(int event) {

  // not active, nothing to do. Normally we wouldn't require this
  // but mouse move events are still delivered and this would unnecessarily
  // update the cursor
  if (!active())
    return 0;

  const LayerModel * m = layers();
  if (!m || m->layerCount() == 0)
    return 0;

  switch (event) {
  case FL_FOCUS:
    return 1;

  case FL_RELEASE:
    // the rubber band ends: do the task on it, if the mouse is still on a
    // cell. RS_STROKEEND must always fire to close the stroke
    if (editType == EDT_RUBBER && cellAt(*m, Fl::event_x(), Fl::event_y()) >= 0 && apply(*m)) {
      callbackReason = RS_CHANGESQUARE;
      do_callback();
    }

    callbackReason = RS_STROKEEND;
    do_callback();

    state = 0;
    startCell = hoverCell;
    redraw();
    return 1;

  case FL_ENTER:
    // we want to get mouse movement events
    Fl::belowmouse(this);
    // fall through

  case FL_PUSH:
    inside = true;

    // we save the mouse button in state, so that we can do different actions
    // depending on the mouse button
    state = Fl::event_button();

    if (event == FL_PUSH) {
      // take the focus to make the input lines in the main window take over their value
      set_visible_focus();
      take_focus();
      clear_visible_focus();

      callbackReason = RS_STROKEBEGIN;
      do_callback();
    }
    // fall through

  case FL_DRAG:
  case FL_MOVE:
    {
      int cell = cellAt(*m, Fl::event_x(), Fl::event_y());

      // dragging off the cells keeps the last one, as the cube editor clamps
      // to the space
      if (cell < 0 && event == FL_DRAG && hoverLayer == currentZ) cell = hoverCell;

      if (event == FL_PUSH || event == FL_MOVE || event == FL_ENTER) {
        hover(*m, cell);
        startCell = cell;
        redraw();
        callbackReason = RS_MOUSEMOVE;
        do_callback();
      } else if (cell != hoverCell || hoverLayer != currentZ) {
        hover(*m, cell);
        if (editType == EDT_SINGLE) startCell = cell;
        redraw();
        callbackReason = RS_MOUSEMOVE;
        do_callback();
      }

      // if we are not in rubber band mode, we need to update the voxel space
      if ((event == FL_DRAG || event == FL_PUSH) && editType == EDT_SINGLE && apply(*m)) {
        callbackReason = RS_CHANGESQUARE;
        redraw();
        do_callback();
      }
    }
    return 1;

  case FL_LEAVE:
    // the mouse leaves the widget space, remove cursor
    inside = false;
    hoverCell = startCell = -1;
    redraw();
    callbackReason = RS_MOUSEMOVE;
    do_callback();
    return 1;
  }

  return 0;
}

namespace {

  // the outline in screen coordinates
  std::vector<Point2> toScreen(const std::vector<Point2> & poly, double s, double ox, double oy) {
    std::vector<Point2> r;
    r.reserve(poly.size());
    for (const Point2 & p : poly) r.push_back({ox + s * p.x, oy - s * p.y});
    return r;
  }

  void fillPolygon(const std::vector<Point2> & poly) {
    fl_begin_polygon();
    for (const Point2 & p : poly) fl_vertex(p.x, p.y);
    fl_end_polygon();
  }

  void framePolygon(const std::vector<Point2> & poly) {
    fl_begin_loop();
    for (const Point2 & p : poly) fl_vertex(p.x, p.y);
    fl_end_loop();
  }

  // the polygon with every corner pulled d pixels towards the centre
  std::vector<Point2> shrink(const std::vector<Point2> & poly, double d) {
    Point2 c;
    for (const Point2 & p : poly) { c.x += p.x; c.y += p.y; }
    c.x /= poly.size();
    c.y /= poly.size();
    std::vector<Point2> r;
    for (const Point2 & p : poly) {
      const double len = std::hypot(c.x - p.x, c.y - p.y);
      const double t = (len > d) ? d / len : 1.0;
      r.push_back({p.x + t * (c.x - p.x), p.y + t * (c.y - p.y)});
    }
    return r;
  }

  bool samePoint(Point2 a, Point2 b) { return std::fabs(a.x - b.x) < 1e-6 && std::fabs(a.y - b.y) < 1e-6; }

}

void layerGridEditor_c::draw() {

  // draw the background, as we don't cover the whole area
  fl_color(color());
  fl_rectf(x(), y(), w(), h());

  const LayerModel * m = layers();
  if (!m || m->layerCount() == 0 || currentZ >= m->layerCount())
    return;

  const view_s v = view(*m);
  if (v.s <= 0) return;

  const voxel_c * space = puzzle->getShape(piecenumber);

  // the background colour, for the dimmed cells of the layer below
  unsigned char bgr, bgg, bgb;
  Fl::get_color(color(), bgr, bgg, bgb);

  const unsigned char r = int(255*lightPieceColor(pieceColorR(piecenumber)));
  const unsigned char g = int(255*lightPieceColor(pieceColorG(piecenumber)));
  const unsigned char b = int(255*lightPieceColor(pieceColorB(piecenumber)));

  // the filled cells of the layer below, very dimmed
  if (currentZ > 0) {
    fl_color(((int)bgr*5+r)/6, ((int)bgg*5+g)/6, ((int)bgb*5+b)/6);
    for (const LayerCell & c : m->cells(currentZ - 1))
      if (space->getState(c.voxel[0], c.voxel[1], c.voxel[2]) != voxel_c::VX_EMPTY)
        fillPolygon(toScreen(c.outline, v.s, v.ox, v.oy));
  }

  for (const LayerCell & c : m->cells(currentZ)) {

    const std::vector<Point2> poly = toScreen(c.outline, v.s, v.ox, v.oy);
    const int state = space->getState(c.voxel[0], c.voxel[1], c.voxel[2]);

    switch (state) {
    case voxel_c::VX_FILLED:
      fl_color(r, g, b);
      fillPolygon(poly);
      break;
    case voxel_c::VX_VARIABLE:
      fl_color(r, g, b);
      fillPolygon(shrink(poly, 4));
      break;
    }

    // a voxel with a colour constraint gets a marker in its first corner
    const unsigned int col = space->getColor(c.voxel[0], c.voxel[1], c.voxel[2]);
    if (state != voxel_c::VX_EMPTY && col) {
      unsigned char cr, cg, cb;
      puzzle->getColor(col - 1, &cr, &cg, &cb);
      fl_color(cr, cg, cb);
      const Point2 p0 = poly[0], p1 = poly[1], pn = poly.back();
      fillPolygon({p0, {p0.x + 0.4 * (p1.x - p0.x), p0.y + 0.4 * (p1.y - p0.y)},
                   {p0.x + 0.4 * (pn.x - p0.x), p0.y + 0.4 * (pn.y - p0.y)}});
    }

    // the grid
    fl_color(active() ? labelcolor() : color());
    framePolygon(poly);
  }

  // the cursor: the edges of the marked cells of this layer that no other
  // marked cell shares
  if (inside && active()) {

    std::vector<const LayerCell *> cur;
    for (const CellRef & c : marked(*m))
      if (c.layer == currentZ)
        cur.push_back(&m->cells(currentZ)[c.index]);

    fl_color(labelcolor());
    fl_line_style(FL_SOLID, 3);
    for (const LayerCell * c : cur)
      for (size_t i = 0; i < c->outline.size(); i++) {
        const Point2 a = c->outline[i];
        const Point2 b = c->outline[(i + 1) % c->outline.size()];
        bool shared = false;
        for (const LayerCell * o : cur) {
          if (o == c) continue;
          for (size_t j = 0; j < o->outline.size() && !shared; j++)
            shared = samePoint(o->outline[j], b) && samePoint(o->outline[(j + 1) % o->outline.size()], a);
          if (shared) break;
        }
        if (!shared)
          fl_line(int(std::lround(v.ox + v.s * a.x)), int(std::lround(v.oy - v.s * a.y)),
                  int(std::lround(v.ox + v.s * b.x)), int(std::lround(v.oy - v.s * b.y)));
      }
    fl_line_style(0);
  }
}
