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
#include "coloursmodel.h"
#include "documentcontroller.h"
#include "guarded.h"
#include "shapesmodel.h"

#include "../uicore/drawcolours.h"

#include "../lib/puzzle.h"

#include <algorithm>

namespace {
  QColor toQColor(btui::Rgb8 c) { return QColor(c.r, c.g, c.b); }
  btui::Rgb8 toRgb8(const QColor & c) {
    return { std::uint8_t(c.red()), std::uint8_t(c.green()), std::uint8_t(c.blue()) };
  }
}

ColoursModel::ColoursModel(DocumentController * doc, ShapesModel * shapes, QObject * parent) :
  QAbstractListModel(parent), m_doc(doc), m_shapes(shapes)
{
  connect(doc, &DocumentController::documentReplaced, this, [this] { refresh(true); });
  connect(doc, &DocumentController::historyApplied, this, [this] { refresh(); });
  connect(doc, &DocumentController::puzzleEdited, this, [this] { refresh(); });
  // Default shows the selected shape's colour
  connect(shapes, &ShapesModel::selectedChanged, this, &ColoursModel::publishDraw);
  connect(shapes, &ShapesModel::countChanged, this, &ColoursModel::publishDraw);
  refresh(true);
}

int ColoursModel::rowCount(const QModelIndex & parent) const {
  return parent.isValid() ? 0 : m_rows;
}

QVariant ColoursModel::data(const QModelIndex & index, int role) const {
  const puzzle_c & p = m_doc->session().puzzle();
  // while rows are being removed the views may still ask for them
  if (!index.isValid() || index.row() >= rowCount() || index.row() >= btui::colourCount(p))
    return {};
  const int id = index.row();
  switch (role) {
    case NameRole:      return nameOf(id);
    case ColorRole:     return colourOf(id);
    case IsDefaultRole: return id == 0;
    default:            return {};
  }
}

QHash<int, QByteArray> ColoursModel::roleNames(void) const {
  static const QHash<int, QByteArray> names{
    { NameRole, "name" },
    { ColorRole, "rgb" },
    { IsDefaultRole, "isDefault" },
  };
  return names;
}

QString ColoursModel::nameOf(int id) {
  return id <= 0 ? tr("Default") : QString::fromStdString(btui::colourName(id));
}

QColor ColoursModel::drawShown(void) const {
  if (m_draw > 0)
    return drawRgb();
  const int s = m_shapes->selected();
  return s >= 0 ? ShapesModel::chipColor(s) : QColor();
}

bool ColoursModel::canAdd(void) const {
  return btui::canAddColour(m_doc->session().puzzle());
}

int ColoursModel::maxColours(void) {
  return btui::maxColours;
}

QColor ColoursModel::colourOf(int id) const {
  if (id < 1 || !isColour(id))
    return {};
  return guarded([&] { return toQColor(btui::colourRgb(m_doc->session().puzzle(), id)); });
}

void ColoursModel::select(int id) {
  const int v = std::clamp(id, 0, std::max(0, rowCount() - 1));
  if (v == m_draw)
    return;
  m_draw = v;
  publishDraw();
}

void ColoursModel::publishDraw(void) {
  const QColor shown = drawShown();
  if (m_draw == m_shownDraw && shown == m_shownColour)
    return;
  m_shownDraw = m_draw;
  m_shownColour = shown;
  emit drawColourChanged();
}

template <class F> bool ColoursModel::edit(F && f) {
  return guarded([&] {
    return m_doc->edit(puzzleHistory_c::AK_COLOR_PALETTE, m_shapes->selected(), std::forward<F>(f));
  });
}

void ColoursModel::addColour(void) {
  int added = -1;
  if (edit([&](puzzle_c & p) { added = btui::addColour(p, btui::presetColour(p)); return added > 0; }))
    select(added);                       // the new colour is the one to draw with (C09)
}

void ColoursModel::requestEdit(int id) {
  if (id < 1 || !isColour(id))
    return;
  m_choosing = id;
  emit chooserRequested(tr("Change colour %1").arg(nameOf(id)), colourOf(id));
}

void ColoursModel::chooseColour(const QColor & c) {
  const int what = m_choosing;
  m_choosing = -1;
  if (what < 1 || !c.isValid())
    return;
  edit([&](puzzle_c & p) { return btui::editColour(p, what, toRgb8(c)); });
}

void ColoursModel::cancelChoose(void) {
  m_choosing = -1;
}

void ColoursModel::requestRemove(int id) {
  if (id < 1 || !isColour(id))
    return;
  guarded([&] {
    if (btui::removeColourLosesSolutions(m_doc->session().puzzle(), id))
      emit removeConfirmRequested(id, nameOf(id));
    else
      removeColour(id);
  });
}

void ColoursModel::removeColour(int id) {
  if (id < 1 || !isColour(id))
    return;
  const int n = count();
  if (!edit([&](puzzle_c & p) { return btui::removeColour(p, id); }))
    return;
  select(0);                             // C09: Default to draw with
  // its voxels took the shape's colour, and the later ones moved down a number
  if (id == n - 2)                       // one after it
    emit flashRequested(tr("Colour %1 removed — %2 is now %1").arg(nameOf(id), nameOf(id + 1)));
  else if (id < n - 2)
    emit flashRequested(tr("Colour %1 removed — later colours renumbered %1…%2").arg(nameOf(id), nameOf(n - 2)));
  else
    emit flashRequested(tr("Colour %1 removed").arg(nameOf(id)));
}

void ColoursModel::refresh(bool reset) {
  const int old = m_rows;
  const puzzle_c & p = m_doc->session().puzzle();
  const int n = btui::colourCount(p);
  std::vector<QRgb> rgb;
  rgb.reserve(size_t(n - 1));
  for (int id = 1; id < n; id++) {
    const btui::Rgb8 c = btui::colourRgb(p, id);
    rgb.push_back(qRgb(c.r, c.g, c.b));
  }
  /* An Edit's chooser that is up when the colours are renumbered or the
   * document replaced would recolour another colour: it is closed. */
  if (m_choosing >= 0 && (reset || n != old)) {
    m_choosing = -1;
    emit chooserCancelled();
  }
  if (reset) {
    beginResetModel();
    m_rows = n;
    m_rgb = std::move(rgb);
    endResetModel();
  } else {
    if (n > old) {
      beginInsertRows(QModelIndex(), old, n - 1);
      m_rows = n;
      endInsertRows();
    } else if (n < old) {
      beginRemoveRows(QModelIndex(), n, old - 1);
      m_rows = n;
      endRemoveRows();
    }
    // only the colours that changed (a shape edit changes none)
    const int both = std::min(n, old);
    for (int id = 1; id < both; id++)
      if (size_t(id - 1) >= m_rgb.size() || m_rgb[size_t(id - 1)] != rgb[size_t(id - 1)])
        emit dataChanged(index(id), index(id), { ColorRole });
    m_rgb = std::move(rgb);
  }
  if (n != old)
    emit countChanged();
  if (reset)
    m_draw = 0;                          // a new document starts at Default
  else
    m_draw = std::min(m_draw, n - 1);
  publishDraw();
}