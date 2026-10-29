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
#include "voxelgriditem.h"

#include "editcursor.h"
#include "guarded.h"
#include "shapesmodel.h"
#include "theme.h"

#include "../uicore/drawcolours.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <QGuiApplication>
#include <QHash>
#include <QPainter>
#include <QtMath>
#include <QPainterPath>
#include <QPolygonF>
#include <QQuickWindow>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <utility>

namespace {
  // room for the rulers: the cell size formula's 70 x 60 dp (C08)
  constexpr qreal kRulerLeft = 34;
  constexpr qreal kRulerBottom = 26;
  constexpr qreal kAxisLine = 3;

  QColor withAlpha(QColor c, qreal a) {
    c.setAlphaF(a);
    return c;
  }

  /* A variable voxel's diagonal hatch (C08): stripes of its colour at 33 %
   * and 13 %, 6 dp each along a row; one tile per colour, kept */
  const QBrush & hatch(const QColor & c) {
    static QHash<QRgb, QBrush> tiles;
    auto it = tiles.constFind(c.rgba());
    if (it != tiles.constEnd())
      return *it;
    if (tiles.size() > 256)
      tiles.clear();
    QImage tile(12, 12, QImage::Format_ARGB32_Premultiplied);
    const QColor on = withAlpha(c, 0.33), off = withAlpha(c, 0.13);
    for (int y = 0; y < 12; y++)
      for (int x = 0; x < 12; x++)
        tile.setPixelColor(x, y, (x + y) % 12 < 6 ? on : off);
    return *tiles.insert(c.rgba(), QBrush(tile));
  }

  /* A variable voxel's square cell, hatched and with its 2 dp dashed
   * border (white at 75 % over its colour), drawn once per colour, size and
   * pixel ratio: stroking dashes cell by cell costs more than the 8 ms a
   * 32 x 32 grid may take (AC-C08-08) */
  const QImage & variableSquare(const QColor & c, QSizeF size, qreal dpr) {
    static QHash<QString, QImage> cells;
    const QString key = QStringLiteral("%1/%2x%3/%4").arg(c.rgba()).arg(size.width()).arg(size.height()).arg(dpr);
    auto it = cells.constFind(key);
    if (it != cells.constEnd())
      return *it;
    if (cells.size() > 256)
      cells.clear();
    QImage img(qCeil(size.width() * dpr), qCeil(size.height() * dpr), QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::transparent);
    QPainter p(&img);
    const QRectF cell(QPointF(0, 0), size);
    p.fillRect(cell, hatch(c));
    const QRectF edge = cell.adjusted(1, 1, -1, -1);
    p.setPen(QPen(c, 2, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    p.drawRect(edge);
    QPen dash(withAlpha(QColor(255, 255, 255), 0.75), 2, Qt::CustomDashLine, Qt::FlatCap, Qt::MiterJoin);
    dash.setDashPattern({ 2.0, 2.0 });
    p.setPen(dash);
    p.drawRect(edge);
    p.end();
    return *cells.insert(key, img);
  }
}

VoxelGridItem::VoxelGridItem(QQuickItem * parent) : QQuickPaintedItem(parent) {
  setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  setAcceptHoverEvents(true);
  setAntialiasing(true);
  if (Theme * t = Theme::instance())
    connect(t, &Theme::changed, this, [this] {
      updateCursor();
      update();
    });
}

void VoxelGridItem::setEditor(EditorController * e) {
  if (e == m_editor)
    return;
  for (const QMetaObject::Connection & c : std::as_const(m_connections))
    disconnect(c);
  m_connections.clear();
  m_editor = e;
  if (e) {
    m_connections << connect(e, &EditorController::revisionChanged, this, [this] {
      relayout();
      update();
    });
    m_connections << connect(e, &EditorController::toolChanged, this, &VoxelGridItem::updateCursor);
    // an area dragged in Area mode grows and shrinks with the pointer
    m_connections << connect(e, &EditorController::previewChanged, this, [this] {
      if (m_editor->areaDragging() || m_areaShown)
        update();
    });
    m_connections << connect(e, &EditorController::drawColourChanged, this, &VoxelGridItem::updateCursor);
  }
  m_layoutRevision = ~quint64(0);
  relayout();
  updateCursor();
  update();
  emit editorChanged();
}

// --- geometry ------------------------------------------------------------------

void VoxelGridItem::geometryChange(const QRectF & newGeometry, const QRectF & oldGeometry) {
  QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
  if (newGeometry.size() != oldGeometry.size()) {
    m_layoutRevision = ~quint64(0);
    relayout();
  }
}

void VoxelGridItem::relayout(void) {
  if (!m_editor) {
    m_layout = btui::GridLayout{};
    return;
  }
  if (m_layoutRevision != m_editor->revision()) {
    m_layoutRevision = m_editor->revision();
    const voxel_c * s = m_editor->shape();
    m_layout = s ? guarded([&] { return btui::gridLayout(*s, m_editor->planeValue(), m_editor->layer()); })
                 : btui::GridLayout{};
  }
  const int cell = btui::cellSize(width(), height(), m_layout.width, m_layout.height);
  // centred in the area left of and above the rulers
  const qreal gw = m_layout.width * cell, gh = m_layout.height * cell;
  m_origin = QPointF(std::round(kRulerLeft + (width() - kRulerLeft - gw) / 2),
                     std::round((height() - kRulerBottom - gh) / 2));
  if (cell != m_cell) {
    m_cell = cell;
    emit cellSizeChanged();
  }
}

QPointF VoxelGridItem::toItem(btui::Pt u) const {
  return { m_origin.x() + u.x * m_cell, m_origin.y() + (m_layout.height - u.y) * m_cell };
}

btui::Pt VoxelGridItem::toUnits(QPointF p) const {
  return { (p.x() - m_origin.x()) / m_cell, m_layout.height - (p.y() - m_origin.y()) / m_cell };
}

QPoint VoxelGridItem::cellAt(QPointF p) const {
  const int i = btui::cellAt(m_layout, toUnits(p));
  if (i < 0)
    return { -1, -1 };
  const btui::GridCell & c = m_layout.cells[size_t(i)];
  return { c.col, c.row };
}

QPointF VoxelGridItem::cellCentre(int col, int row) const {
  for (const btui::GridCell & c : m_layout.cells)
    if (c.col == col && c.row == row) {
      if (c.round)
        return toItem(c.centre);
      btui::Pt s;
      for (const btui::Pt & q : c.outline) {
        s.x += q.x / double(c.outline.size());
        s.y += q.y / double(c.outline.size());
      }
      return toItem(s);
    }
  return { -1, -1 };
}

// --- painting --------------------------------------------------------------------

QColor VoxelGridItem::colourOf(unsigned colour) const {
  // Default is the shape's colour (C08); the grid always shows voxel colours
  const puzzle_c & puz = m_editor->document()->session().puzzle();
  if (colour > 0 && int(colour) < btui::colourCount(puz)) {
    const btui::Rgb8 c = btui::colourRgb(puz, int(colour));
    return QColor(c.r, c.g, c.b);
  }
  return ShapesModel::chipColor(m_editor->shapeIndex());
}

void VoxelGridItem::paint(QPainter * p) {
  const Theme * t = Theme::instance();
  if (!m_editor || !t)
    return;
  relayout();
  const voxel_c * s = m_editor->shape();
  if (!s || m_layout.cells.empty())
    return;

  p->setRenderHint(QPainter::Antialiasing, !m_layout.squares);
  const qreal gw = m_layout.width * m_cell, gh = m_layout.height * m_cell;
  const QRectF grid(m_origin, QSizeF(gw, gh));

  // the grid lines (line2) show as the gaps between square cells
  if (m_layout.squares)
    p->fillRect(grid.adjusted(-1, -1, 1, 1), t->line2());

  const QColor white(255, 255, 255);
  const QPen gapPen(t->line2(), 1);
  std::array<QColor, 64> colours;
  p->setBrushOrigin(m_origin);              // the hatch continues from cell to cell
  for (const btui::GridCell & c : m_layout.cells) {
    const unsigned x = unsigned(c.voxel.x), y = unsigned(c.voxel.y), z = unsigned(c.voxel.z);
    const int state = s->getState(x, y, z);
    QColor colour;
    if (state != voxel_c::VX_EMPTY) {
      // the colours looked up once per paint, not per cell
      const unsigned id = std::min(s->getColor(x, y, z), unsigned(colours.size() - 1));
      if (!colours[id].isValid())
        colours[id] = colourOf(id);
      colour = colours[id];
    }

    // squares: plain rectangles, the 1 dp gaps between them the grid lines
    if (m_layout.squares) {
      const QRectF cell = QRectF(toItem(c.outline[3]), toItem(c.outline[1])).normalized().adjusted(0.5, 0.5, -0.5, -0.5);
      switch (state) {
        case voxel_c::VX_FILLED:
          p->fillRect(cell, colour);
          break;
        case voxel_c::VX_VARIABLE:
          p->drawImage(cell.topLeft(), variableSquare(colour, cell.size(), p->device()->devicePixelRatioF()));
          break;
        default:
          p->fillRect(cell, t->panel());
          break;
      }
      continue;
    }

    QPainterPath path;
    if (c.round) {
      path.addEllipse(toItem(c.centre), c.radius * m_cell - 0.5, c.radius * m_cell - 0.5);
    } else {
      QPolygonF poly;
      for (const btui::Pt & q : c.outline)
        poly << toItem(q);
      path.addPolygon(poly);
      path.closeSubpath();
    }
    switch (state) {
      case voxel_c::VX_FILLED:
        p->fillPath(path, colour);
        break;
      case voxel_c::VX_VARIABLE: {
        p->fillPath(path, hatch(colour));
        // the border inside the cell's shape: half of a 4 dp stroke, clipped
        p->save();
        p->setClipPath(path);
        p->strokePath(path, QPen(colour, 4));
        QPen dash(withAlpha(white, 0.75), 4, Qt::CustomDashLine, Qt::FlatCap);
        dash.setDashPattern({ 1.0, 1.0 });
        p->strokePath(path, dash);
        p->restore();
        break;
      }
      default:
        p->fillPath(path, t->panel());
        break;
    }
    p->strokePath(path, gapPen);
  }

  // Area mode: the dragged area's cells of this layer tinted, and their
  // outline -- the sides no other cell of the area shares -- in accent, so
  // the area has the voxel type's own shape (legacy drawTileCursor)
  m_areaShown = m_editor->areaDragging();
  if (m_areaShown) {
    const std::vector<btui::Cell3> area = m_editor->area();
    std::vector<const btui::GridCell *> in;
    for (const btui::GridCell & c : m_layout.cells)
      if (std::binary_search(area.begin(), area.end(), c.voxel))
        in.push_back(&c);
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    QColor tint = t->accent();
    tint.setAlphaF(0.18f);
    // each side by its ends rounded to 1/1024 of a cell, either way round
    auto key = [](btui::Pt a, btui::Pt b) {
      auto q = [](double v) { return qint64(std::llround(v * 1024.0)); };
      std::array<qint64, 4> k{ q(a.x), q(a.y), q(b.x), q(b.y) };
      if (std::make_pair(k[2], k[3]) < std::make_pair(k[0], k[1]))
        k = { k[2], k[3], k[0], k[1] };
      return k;
    };
    std::map<std::array<qint64, 4>, std::pair<QLineF, int>> sides;
    QPainterPath circles;
    for (const btui::GridCell * c : in) {
      if (c->round) {
        circles.addEllipse(toItem(c->centre), c->radius * m_cell, c->radius * m_cell);
        continue;
      }
      QPolygonF poly;
      for (size_t i = 0; i < c->outline.size(); i++) {
        const btui::Pt a = c->outline[i], b = c->outline[(i + 1) % c->outline.size()];
        poly << toItem(a);
        auto & s = sides[key(a, b)];
        s.first = QLineF(toItem(a), toItem(b));
        s.second++;
      }
      p->setPen(Qt::NoPen);
      p->setBrush(tint);
      p->drawPolygon(poly);
    }
    p->setBrush(tint);
    p->setPen(QPen(t->accent(), 2));
    p->drawPath(circles);
    for (const auto & [k, s] : sides)
      if (s.second == 1)
        p->drawLine(s.first);
    p->restore();
  }

  // the hovered cell: a 2 dp accent outline inside it
  const int hc = m_editor->hoverCol(), hr = m_editor->hoverRow();
  if (hc >= 0)
    for (const btui::GridCell & c : m_layout.cells)
      if (c.col == hc && c.row == hr) {
        QPainterPath path;
        if (c.round) {
          path.addEllipse(toItem(c.centre), c.radius * m_cell, c.radius * m_cell);
        } else {
          QPolygonF poly;
          for (const btui::Pt & q : c.outline)
            poly << toItem(q);
          path.addPolygon(poly);
          path.closeSubpath();
        }
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setClipPath(path);
        p->strokePath(path, QPen(t->accent(), 4));
        p->restore();
        break;
      }

  // axis lines: the row axis at the left, the column axis at the bottom
  const btui::PlaneAxes a = btui::planeAxes(m_editor->planeValue());
  const QColor axis[3] = { t->axisX(), t->axisY(), t->axisZ() };
  p->setRenderHint(QPainter::Antialiasing, false);
  p->fillRect(QRectF(grid.left() - 1 - kAxisLine, grid.top() - 1, kAxisLine, gh + 2), axis[a.row]);
  p->fillRect(QRectF(grid.left() - 1, grid.bottom() + 1, gw + 2, kAxisLine), axis[a.col]);

  // rulers: row numbers at the left (the highest at the top), columns below
  if (m_layout.squares) {
    QFont f = p->font();
    f.setPixelSize(11);
    p->setFont(f);
    p->setPen(t->muted());
    // laid out once, not at every paint
    auto number = [&](int n) -> const QStaticText & {
      if (size_t(n) >= m_numbers.size())
        m_numbers.resize(size_t(n) + 1);
      QStaticText & s = m_numbers[size_t(n)];
      if (s.text().isEmpty()) {
        s.setText(QString::number(n));
        s.setTextFormat(Qt::PlainText);
        s.prepare(QTransform(), f);
      }
      return s;
    };
    const int step = m_cell < 22 ? 2 : 1;      // every other number when they would touch
    for (int r = 0; r < m_layout.rows; r += step) {
      const QStaticText & s = number(r);
      p->drawStaticText(QPointF(grid.left() - kAxisLine - 6 - s.size().width(),
                                grid.bottom() - (r + 0.5) * m_cell - s.size().height() / 2), s);
    }
    for (int c = 0; c < m_layout.cols; c += step) {
      const QStaticText & s = number(c);
      p->drawStaticText(QPointF(grid.left() + (c + 0.5) * m_cell - s.size().width() / 2, grid.bottom() + kAxisLine + 3), s);
    }
  }
}

// --- the pointer ---------------------------------------------------------------------

void VoxelGridItem::updateCursor(void) {
  const Theme * t = Theme::instance();
  if (!m_editor || !t || !m_editor->hasShape()) {
    unsetCursor();
    return;
  }
  const qreal scale = window() ? window()->effectiveDevicePixelRatio() : 1.0;
  setCursor(editcursor::cursor(btui::Tool(int(m_editor->toolInEffect())), m_editor->cursorColour(), t->dark(), scale));
}

void VoxelGridItem::itemChange(ItemChange change, const ItemChangeData & value) {
  QQuickPaintedItem::itemChange(change, value);
  if (change == ItemDevicePixelRatioHasChanged)
    updateCursor();
}

void VoxelGridItem::hoverAt(QPointF p) {
  if (!m_editor)
    return;
  const QPoint c = cellAt(p);
  const int before = m_editor->hoverCol() * 100000 + m_editor->hoverRow();
  m_editor->hover(c.x(), c.y());
  if (m_editor->hoverCol() * 100000 + m_editor->hoverRow() != before)
    update();
}

void VoxelGridItem::hoverEnterEvent(QHoverEvent * e) {
  m_hovered = true;
  if (!m_filtering && QGuiApplication::instance()) {
    QGuiApplication::instance()->installEventFilter(this);
    m_filtering = true;
  }
  if (m_editor)
    m_editor->setModifiers(e->modifiers());
  hoverAt(e->position());
}

void VoxelGridItem::hoverMoveEvent(QHoverEvent * e) {
  if (m_editor)
    m_editor->setModifiers(e->modifiers());
  hoverAt(e->position());
}

void VoxelGridItem::hoverLeaveEvent(QHoverEvent *) {
  m_hovered = false;
  if (m_filtering && !m_pressed) {
    QGuiApplication::instance()->removeEventFilter(this);
    m_filtering = false;
  }
  if (m_editor && !m_pressed) {
    m_editor->hover(-1, -1);
    update();
  }
}

bool VoxelGridItem::eventFilter(QObject * watched, QEvent * e) {
  if ((e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) && m_editor) {
    const int key = static_cast<QKeyEvent *>(e)->key();
    if (key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Control || key == Qt::Key_Meta)
      m_editor->setModifiers(QGuiApplication::queryKeyboardModifiers());
  }
  return QQuickPaintedItem::eventFilter(watched, e);
}

void VoxelGridItem::mousePressEvent(QMouseEvent * e) {
  if (!m_editor || (e->button() != Qt::LeftButton && e->button() != Qt::RightButton) || m_pressed) {
    e->ignore();
    return;
  }
  const QPoint c = cellAt(e->position());
  if (c.x() < 0) {
    e->ignore();               // outside the cells: nothing to paint
    return;
  }
  m_pressed = true;
  m_last = e->position();
  m_keyCol = c.x();                    // the keys go on from here
  m_keyRow = c.y();
  forceActiveFocus(Qt::MouseFocusReason);
  m_editor->hover(c.x(), c.y());
  m_editor->press(c.x(), c.y(), e->button() == Qt::RightButton, e->modifiers());
  e->accept();
}

// --- the keyboard ---------------------------------------------------------------

int VoxelGridItem::keyCell(void) const {
  for (size_t i = 0; i < m_layout.cells.size(); i++)
    if (m_layout.cells[i].col == m_keyCol && m_layout.cells[i].row == m_keyRow)
      return int(i);
  return -1;
}

void VoxelGridItem::setKeyCell(int index) {
  if (!m_editor || index < 0 || size_t(index) >= m_layout.cells.size())
    return;
  m_keyCol = m_layout.cells[size_t(index)].col;
  m_keyRow = m_layout.cells[size_t(index)].row;
  m_editor->hover(m_keyCol, m_keyRow);
  update();
}

void VoxelGridItem::focusInEvent(QFocusEvent * e) {
  QQuickPaintedItem::focusInEvent(e);
  relayout();
  // the cell it had, else the hovered one, else the one in the middle
  int i = keyCell();
  if (i < 0 && m_editor && m_editor->hoverCol() >= 0) {
    m_keyCol = m_editor->hoverCol();
    m_keyRow = m_editor->hoverRow();
    i = keyCell();
  }
  if (i < 0)
    i = btui::nearestCell(m_layout, { m_layout.width / 2, m_layout.height / 2 });
  setKeyCell(i);
}

void VoxelGridItem::focusOutEvent(QFocusEvent * e) {
  QQuickPaintedItem::focusOutEvent(e);
  if (m_editor && !m_hovered && !m_pressed) {
    m_editor->setModifiers(Qt::NoModifier);
    m_editor->hover(-1, -1);
    update();
  }
}

void VoxelGridItem::keyPressEvent(QKeyEvent * e) {
  if (!m_editor) {
    e->ignore();
    return;
  }
  if (e->key() == Qt::Key_Escape && m_editor->areaDragging()) {
    m_editor->cancelArea();               // the area goes, unapplied
    e->accept();
    return;
  }
  relayout();
  const int key = e->key();
  if (key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Control || key == Qt::Key_Meta) {
    m_editor->setModifiers(e->modifiers());             // the quick tool's cursor and ghost
    e->ignore();
    return;
  }
  int i = keyCell();
  if (i < 0)
    i = btui::nearestCell(m_layout, { m_layout.width / 2, m_layout.height / 2 });
  // layout units: y up, as the rows count
  const Qt::KeyboardModifiers mods = e->modifiers() & ~Qt::KeypadModifier;
  // Shift+Up / Down: a layer up or down, the cell cursor kept
  if ((key == Qt::Key_Up || key == Qt::Key_Down) && mods == Qt::ShiftModifier) {
    m_editor->layerStep(key == Qt::Key_Up ? 1 : -1);
    e->accept();
    return;
  }
  switch (key) {
    case Qt::Key_Left:  setKeyCell(btui::stepCell(m_layout, i, -1, 0)); break;
    case Qt::Key_Right: setKeyCell(btui::stepCell(m_layout, i, 1, 0)); break;
    case Qt::Key_Up:    setKeyCell(btui::stepCell(m_layout, i, 0, 1)); break;
    case Qt::Key_Down:  setKeyCell(btui::stepCell(m_layout, i, 0, -1)); break;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Delete:
    case Qt::Key_Backspace: {
      if (i < 0)
        break;
      setKeyCell(i);
      const bool erase = key == Qt::Key_Delete || key == Qt::Key_Backspace;
      // one press, one undo step, the tool in effect (Delete: Erase)
      if (m_editor->press(m_keyCol, m_keyRow, erase, erase ? Qt::NoModifier : mods))
        m_editor->release();
      break;
    }
    default:
      e->ignore();
      return;
  }
  e->accept();
}

void VoxelGridItem::keyReleaseEvent(QKeyEvent * e) {
  const int key = e->key();
  if (m_editor && (key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Control || key == Qt::Key_Meta))
    m_editor->setModifiers(e->modifiers());
  e->ignore();
}

void VoxelGridItem::strokeTo(QPointF p) {
  // sample the segment so a fast drag leaves no gaps (C08 [SHOULD])
  const QPointF d = p - m_last;
  const qreal len = std::hypot(d.x(), d.y());
  const int n = std::max(1, int(std::ceil(len / std::max(4.0, m_cell / 3.0))));
  for (int i = 1; i <= n; i++) {
    const QPoint c = cellAt(m_last + d * (qreal(i) / n));
    if (c.x() >= 0)
      m_editor->dragTo(c.x(), c.y());
  }
  m_last = p;
}

void VoxelGridItem::mouseMoveEvent(QMouseEvent * e) {
  if (!m_pressed || !m_editor)
    return;
  strokeTo(e->position());
  hoverAt(e->position());
}

void VoxelGridItem::mouseReleaseEvent(QMouseEvent * e) {
  if (!m_pressed)
    return;
  const bool left = e->button() == Qt::LeftButton, right = e->button() == Qt::RightButton;
  if (!left && !right)
    return;
  m_pressed = false;
  if (m_editor) {
    m_editor->release();
    if (!m_hovered || cellAt(e->position()).x() < 0)
      m_editor->hover(-1, -1);
  }
  if (!m_hovered && m_filtering) {
    QGuiApplication::instance()->removeEventFilter(this);
    m_filtering = false;
  }
  update();
}

void VoxelGridItem::mouseUngrabEvent(void) {
  // the window lost the mouse mid-stroke: the stroke ends as a release would;
  // an area, never seen to its end, goes unapplied
  if (m_pressed && m_editor) {
    if (m_editor->areaDragging())
      m_editor->cancelArea();
    else
      m_editor->release();
  }
  m_pressed = false;
}

bool VoxelGridItem::event(QEvent * e) {
  // Esc cancels a dragged area here, before the window's Esc ladder
  if (e->type() == QEvent::ShortcutOverride && m_editor && m_editor->areaDragging() &&
      static_cast<QKeyEvent *>(e)->key() == Qt::Key_Escape) {
    e->accept();
    return true;
  }
  return QQuickPaintedItem::event(e);
}
