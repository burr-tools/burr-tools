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
#include "toolscontroller.h"
#include "app.h"
#include "documentcontroller.h"
#include "shapesmodel.h"

#include "../uicore/palette.h"

#include "../lib/bt_assert.h"
#include "../lib/problem.h"
#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <QElapsedTimer>
#include <QVariantMap>

#include <algorithm>

namespace {

  // how long one slice of the status computation may hold the GUI thread
  const int sliceMs = 12;

  QString shapeIdText(const puzzle_c & p, unsigned int s) {
    const std::string & name = p.getShape(s)->getName();
    if (name.empty())
      return QStringLiteral("S%1").arg(s + 1);
    return QStringLiteral("S%1 - %2").arg(s + 1).arg(QString::fromStdString(name));
  }
}

// --- ShapeStatusModel --------------------------------------------------------

ShapeStatusModel::ShapeStatusModel(DocumentController * doc, QObject * parent) :
  QAbstractListModel(parent), m_doc(doc)
{
  m_timer.setSingleShot(true);
  m_timer.setInterval(0);
  connect(&m_timer, &QTimer::timeout, this, &ShapeStatusModel::step);
  // a table about another puzzle is meaningless
  connect(m_doc, &DocumentController::documentReplaced, this, &ShapeStatusModel::clear);
  connect(m_doc, &DocumentController::historyApplied, this, &ShapeStatusModel::clear);
}

ShapeStatusModel::~ShapeStatusModel() = default;

int ShapeStatusModel::rowCount(const QModelIndex & parent) const {
  return parent.isValid() ? 0 : int(m_rows.size());
}

double ShapeStatusModel::progress(void) const {
  return m_total == 0 ? 1.0 : double(m_rows.size()) / double(m_total);
}

int ShapeStatusModel::selectedCount(void) const {
  return int(std::count(m_selected.begin(), m_selected.end(), true));
}

QColor ShapeStatusModel::chipColor(int shape) { return ShapesModel::chipColor(shape); }

QColor ShapeStatusModel::chipTextColor(int shape) {
  return btui::prefersWhiteText(shape) ? QColor(Qt::white) : QColor(Qt::black);
}

QVariant ShapeStatusModel::data(const QModelIndex & index, int role) const {
  if (!index.isValid() || index.row() >= rowCount())
    return {};
  const int i = index.row();
  const btui::ShapeStatus & r = m_rows[size_t(i)];
  const puzzle_c & p = m_doc->session().puzzle();

  switch (role) {
    case IdTextRole:    return unsigned(i) < p.getNumberOfShapes() ? shapeIdText(p, unsigned(i)) : QString();
    case ColorRole:     return chipColor(i);
    case TextColorRole: return chipTextColor(i);
    case FixedRole:     return int(r.fixed);
    case VariableRole:  return int(r.variable);
    case TotalRole:     return int(r.fixed + r.variable);
    case MirrorRole:    return r.identicalMirror + 1;
    case ShapeRole:     return r.identicalShape + 1;
    case CompleteRole:  return r.identicalComplete + 1;
    case FaceRole:      return r.connectedFace;
    case EdgeRole:      return r.connectedEdge;
    case CornerRole:    return r.connectedCorner;
    case Holes2dRole:   return r.holes2d;
    case Holes3dRole:   return r.holes3d;
    case NotchRole:     return r.notchable;
    case MillRole:      return r.millable;
    case SymmetryRole:  return r.symmetryKnown ? QString::number(r.symmetry) : QStringLiteral("---");
    case SelectedRole:  return bool(m_selected[size_t(i)]);
    default:            return {};
  }
}

QHash<int, QByteArray> ShapeStatusModel::roleNames(void) const {
  return {
    { IdTextRole, "idText" },
    { ColorRole, "chipColor" },
    { TextColorRole, "chipTextColor" },
    { FixedRole, "fixed" },
    { VariableRole, "variable" },
    { TotalRole, "total" },
    { MirrorRole, "identicalMirror" },
    { ShapeRole, "identicalShape" },
    { CompleteRole, "identicalComplete" },
    { FaceRole, "face" },
    { EdgeRole, "edge" },
    { CornerRole, "corner" },
    { Holes2dRole, "holes2d" },
    { Holes3dRole, "holes3d" },
    { NotchRole, "notchable" },
    { MillRole, "millable" },
    { SymmetryRole, "symmetry" },
    { SelectedRole, "rowSelected" },
  };
}

void ShapeStatusModel::start(void) {
  m_timer.stop();
  beginResetModel();
  m_rows.clear();
  m_selected.clear();
  const puzzle_c & p = m_doc->session().puzzle();
  m_total = p.getNumberOfShapes();
  const bool bricks = p.getGridType()->getType() == gridType_c::GT_BRICKS;
  m_calc = std::make_unique<btui::ShapeStatusCalculator>(p);
  endResetModel();
  if (bricks != m_bricks) {
    m_bricks = bricks;
    emit bricksChanged();
  }
  emit busyChanged();
  emit progressChanged();
  emit selectionChanged();
  m_timer.start();
}

void ShapeStatusModel::stop(void) {
  m_timer.stop();
  if (!m_calc)
    return;
  m_calc.reset();
  emit busyChanged();
}

void ShapeStatusModel::cancel(void) { stop(); }

void ShapeStatusModel::clear(void) {
  stop();
  beginResetModel();
  m_rows.clear();
  m_selected.clear();
  m_total = 0;
  endResetModel();
  emit progressChanged();
  emit selectionChanged();
}

void ShapeStatusModel::step(void) {
  if (!m_calc)
    return;
  try {
    QElapsedTimer t;
    t.start();
    while (!m_calc->done() && t.elapsed() < sliceMs) {
      const int row = int(m_rows.size());
      btui::ShapeStatus r = m_calc->next();
      beginInsertRows(QModelIndex(), row, row);
      m_rows.push_back(r);
      m_selected.push_back(false);
      endInsertRows();
    }
  } catch (const assert_exception & e) {
    stop();
    if (App * app = App::instance())
      app->handleInternalError(e);
    return;
  }
  emit progressChanged();
  if (m_calc->done())
    stop();
  else
    m_timer.start();
}

void ShapeStatusModel::finish(void) {
  while (m_calc)
    step();
}

void ShapeStatusModel::setSelected(int row, bool selected) {
  if (row < 0 || row >= rowCount() || m_selected[size_t(row)] == selected)
    return;
  m_selected[size_t(row)] = selected;
  const QModelIndex i = index(row);
  emit dataChanged(i, i, { SelectedRole });
  emit selectionChanged();
}

void ShapeStatusModel::selectHoles(void) {
  for (size_t i = 0; i < m_rows.size(); i++)
    if (m_rows[i].holes2d || m_rows[i].holes3d)
      setSelected(int(i), true);
}

void ShapeStatusModel::selectIdentical(const QString & kind) {
  for (size_t i = 0; i < m_rows.size(); i++) {
    const btui::ShapeStatus & r = m_rows[i];
    const int other = kind == QLatin1String("mirror") ? r.identicalMirror
                    : kind == QLatin1String("complete") ? r.identicalComplete
                    : kind == QLatin1String("shape") ? r.identicalShape : -1;
    if (other >= 0)
      setSelected(int(i), true);
  }
}

int ShapeStatusModel::removeSelected(void) {
  std::vector<unsigned int> doomed;
  for (size_t i = 0; i < m_selected.size(); i++)
    if (m_selected[i])
      doomed.push_back(unsigned(i));
  if (doomed.empty())
    return 0;
  stop();
  btui::removeShapes(m_doc->session().puzzle(), doomed);
  m_doc->recordStructuralEdit();
  start();
  return int(doomed.size());
}

// --- ToolsController ---------------------------------------------------------

ToolsController::ToolsController(DocumentController * doc, QObject * parent) :
  QObject(parent), m_doc(doc), m_status(new ShapeStatusModel(doc, this))
{
}

QVariantList ToolsController::convertTargets(void) const {
  QVariantList out;
  for (auto t : btui::convertTargets(m_doc->session().puzzle().getGridType()->getType())) {
    QVariantMap e;
    e.insert(QStringLiteral("type"), int(t));
    e.insert(QStringLiteral("name"), DocumentController::gridTypeDisplayName(int(t)));
    out.append(e);
  }
  return out;
}

bool ToolsController::convert(int gridType) {
  return m_doc->convertTo(gridType);
}

bool ToolsController::isBricks(void) const {
  return m_doc->session().puzzle().getGridType()->getType() == gridType_c::GT_BRICKS;
}

QVariantList ToolsController::problems(void) const {
  QVariantList out;
  const puzzle_c & p = m_doc->session().puzzle();
  for (unsigned int i = 0; i < p.getNumberOfProblems(); i++) {
    const std::string & name = p.getProblem(i)->getName();
    QVariantMap e;
    e.insert(QStringLiteral("index"), int(i));
    e.insert(QStringLiteral("label"), name.empty() ? QStringLiteral("P%1").arg(i + 1)
                                                   : QStringLiteral("P%1 - %2").arg(i + 1).arg(QString::fromStdString(name)));
    e.insert(QStringLiteral("solutions"), int(p.getProblem(i)->getNumberOfSavedSolutions()));
    // legacy ProblemSelector colours problems like shapes, by position
    e.insert(QStringLiteral("color"), ShapeStatusModel::chipColor(int(i)));
    e.insert(QStringLiteral("textColor"), ShapeStatusModel::chipTextColor(int(i)));
    out.append(e);
  }
  return out;
}

btui::ImportAssembliesOptions ToolsController::importOptions(const QVariantMap & m) {
  btui::ImportAssembliesOptions o;
  auto uns = [&m](const char * key, unsigned int def) {
    const QVariant v = m.value(QLatin1String(key));
    // legacy reads the fields with atoi() into unsigned: no negatives here
    return v.isValid() ? unsigned(std::max(0, v.toInt())) : def;
  };
  auto flag = [&m](const char * key, bool def) {
    const QVariant v = m.value(QLatin1String(key));
    return v.isValid() ? v.toBool() : def;
  };
  o.sourceProblem = uns("source", o.sourceProblem);
  const QString dest = m.value(QStringLiteral("destination")).toString();
  if (dest == QLatin1String("new"))
    o.destination = btui::ImportAssembliesOptions::Destination::NewProblem;
  else if (dest == QLatin1String("existing"))
    o.destination = btui::ImportAssembliesOptions::Destination::ExistingProblem;
  o.destinationProblem = uns("target", o.destinationProblem);
  o.rangeMin = uns("rangeMin", o.rangeMin);
  o.rangeMax = uns("rangeMax", o.rangeMax);
  o.dropDisconnected = flag("dropDisconnected", o.dropDisconnected);
  o.dropMirror = flag("dropMirror", o.dropMirror);
  o.dropSymmetric = flag("dropSymmetric", o.dropSymmetric);
  o.dropNonMillable = flag("dropNonMillable", o.dropNonMillable);
  o.dropNonNotchable = flag("dropNonNotchable", o.dropNonNotchable);
  o.dropIdentical = flag("dropIdentical", o.dropIdentical);
  o.shapeMin = uns("shapeMin", o.shapeMin);
  o.shapeMax = uns("shapeMax", o.shapeMax);
  return o;
}

int ToolsController::importAssemblies(const QVariantMap & options) {
  const btui::ImportAssembliesOptions o = importOptions(options);
  puzzle_c & p = m_doc->session().puzzle();
  if (o.sourceProblem >= p.getNumberOfProblems())
    return 0;
  const unsigned int problemsBefore = p.getNumberOfProblems();
  try {
    const unsigned int added = btui::importAssemblies(p, o);
    // legacy records unconditionally; an import that changed nothing would
    // leave an undo step that does nothing
    if (added > 0 || p.getNumberOfProblems() != problemsBefore)
      m_doc->recordStructuralEdit();
    return int(added);
  } catch (const assert_exception & e) {
    if (App * app = App::instance())
      app->handleInternalError(e);
    return 0;
  }
}
