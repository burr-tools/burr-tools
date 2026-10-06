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
#include "stlexportcontroller.h"

#include "documentcontroller.h"
#include "shapesmodel.h"

#include "../halfedge/polyhedron.h"
#include "../halfedge/volume.h"
#include "../lib/puzzle.h"
#include "../lib/stl.h"
#include "../lib/voxel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QVariantMap>

#include <algorithm>

namespace {

  // legacy's message for anything but an stlException_c
  const char * const faultyMesh = "The generated mesh is faulty in some way, try to tweak the parameter";

  QString typeName(stlExporter_c::parameterTypes t) {
    switch (t) {
      case stlExporter_c::PAR_TYP_POS_DOUBLE:  return QStringLiteral("posDouble");
      case stlExporter_c::PAR_TYP_POS_INTEGER: return QStringLiteral("posInt");
      case stlExporter_c::PAR_TYP_SWITCH:      return QStringLiteral("switch");
      default:                                 return QStringLiteral("double");
    }
  }
}

StlExportController::StlExportController(SettingsController * settings, DocumentController * doc, ShapesModel * shapes,
                                         QObject * parent) :
  SceneController(settings, parent), m_doc(doc), m_shapes(shapes)
{
}

StlExportController::~StlExportController() = default;

bool StlExportController::available(const DocumentController * doc) {
  const puzzle_c & p = doc->session().puzzle();
  return (p.getGridType()->getCapabilities() & gridType_c::CAP_STLEXPORT) && p.getNumberOfShapes() > 0;
}

int StlExportController::parameterCount(void) const {
  return m_stl ? int(m_stl->numParameters()) : 0;
}

QVariantMap StlExportController::parameter(int index) const {
  QVariantMap e;
  if (index < 0 || index >= parameterCount())
    return e;
  const unsigned int i = unsigned(index);
  const auto type = m_stl->getParameterType(i);
  const double v = m_stl->getParameter(i);
  e.insert(QStringLiteral("index"), index);
  e.insert(QStringLiteral("name"), QString::fromUtf8(m_stl->getParameterName(i)));
  e.insert(QStringLiteral("tooltip"), QString::fromUtf8(m_stl->getParameterTooltip(i)).trimmed());
  e.insert(QStringLiteral("type"), typeName(type));
  e.insert(QStringLiteral("value"), v);
  // the legacy fields: "%2.2f" for numbers, "%i" for counts
  e.insert(QStringLiteral("text"), type == stlExporter_c::PAR_TYP_POS_INTEGER ? QString::number(int(v))
                                                                           : QString::number(v, 'f', 2));
  return e;
}

void StlExportController::begin(void) {
  m_stl.reset(m_doc->session().puzzle().getGridType()->getStlExporter());
  if (m_stl)
    m_binary = m_stl->getBinaryMode();
  m_insides = false;
  const int n = int(m_doc->session().puzzle().getNumberOfShapes());
  m_shape = std::clamp(m_shapes->selected(), 0, std::max(0, n - 1));
  m_camera.home();
  m_revision++;
  emit parametersChanged();
  emit valuesChanged();
  emit shapeChanged();
  emit optionsChanged();
  rebuild();
}

void StlExportController::end(void) {
  m_stl.reset();
  m_mesh.reset();
  m_meshRevision++;
  m_volumeText.clear();
  m_error.clear();
  emit parametersChanged();
  emit meshChanged();
  emit frameChanged();
}

void StlExportController::setShape(int s) {
  const int n = int(m_doc->session().puzzle().getNumberOfShapes());
  s = std::clamp(s, 0, std::max(0, n - 1));
  if (s == m_shape)
    return;
  m_shape = s;
  emit shapeChanged();
  rebuild();
}

void StlExportController::setBinary(bool b) {
  if (b == m_binary)
    return;
  m_binary = b;
  emit optionsChanged();
}

void StlExportController::setInsides(bool on) {
  if (on == m_insides)
    return;
  m_insides = on;
  emit optionsChanged();
  rebuild();
}

void StlExportController::setParameter(int index, double value) {
  if (!m_stl || index < 0 || unsigned(index) >= m_stl->numParameters())
    return;
  switch (m_stl->getParameterType(unsigned(index))) {
    case stlExporter_c::PAR_TYP_POS_DOUBLE:  value = std::max(0.0, value); break;
    case stlExporter_c::PAR_TYP_POS_INTEGER: value = std::max(0.0, std::floor(value)); break;
    case stlExporter_c::PAR_TYP_SWITCH:      value = value != 0 ? 1 : 0; break;
    default: break;
  }
  m_stl->setParameter(unsigned(index), value);
  m_revision++;
  emit valuesChanged();
  rebuild();
}

/* legacy cb_Update3DView */
void StlExportController::rebuild(void) {
  m_mesh.reset();
  m_volumeText.clear();
  m_error.clear();
  const puzzle_c & p = m_doc->session().puzzle();

  if (m_stl && unsigned(m_shape) < p.getNumberOfShapes()) {
    std::unique_ptr<Polyhedron> poly;
    try {
      poly.reset(m_stl->getMesh(*p.getShape(unsigned(m_shape))));
    } catch (const stlException_c & e) {
      m_error = QString::fromUtf8(e.comment);
    } catch (...) {
      m_error = QString::fromLatin1(faultyMesh);
    }
    if (poly && poly->vBegin() != poly->vEnd()) {
      auto mesh = std::make_shared<btui::ShapeMesh>(btui::buildPolyhedronMesh(*poly, m_insides));
      const btui::Vec3 c = (mesh->boundsMin + mesh->boundsMax) * 0.5f;
      m_camera.setScene(c, std::max(0.5f, btui::length(mesh->boundsMax - mesh->boundsMin) * 0.5f));
      m_mesh = std::move(mesh);
      m_volumeText = tr("Volume: %1 cubic-units").arg(double(volume(*poly)), 0, 'f', 1);
    }
  }
  m_meshRevision++;
  emit meshChanged();
  emit frameChanged();
}

SceneFrame StlExportController::frame(float devicePixelRatio) const {
  SceneFrame f = baseFrame(devicePixelRatio);
  if (m_mesh) {
    f.mesh = m_mesh;
    f.meshRevision = m_meshRevision;
    f.xray = m_insides;
  }
  return f;
}

QUrl StlExportController::folder(void) const {
  /* legacy: the puzzle's folder, or the home directory for an unsaved
   * puzzle -- it exists and is writable, unlike the working directory of an
   * application started from a bundle or a shortcut */
  const auto & name = m_doc->session().fileName();
  if (name.empty())
    return QUrl::fromLocalFile(QDir::homePath());
  return m_doc->folder();
}

QString StlExportController::suggestedName(void) const {
  const QString shape = QStringLiteral("S%1").arg(m_shape + 1);
  const auto & name = m_doc->session().fileName();
  if (name.empty())
    return shape + QStringLiteral(".stl");
  return QFileInfo(m_doc->fileName()).completeBaseName() + QLatin1Char('-') + shape + QStringLiteral(".stl");
}

bool StlExportController::exportTo(const QUrl & file) {
  const puzzle_c & p = m_doc->session().puzzle();
  if (!m_stl || unsigned(m_shape) >= p.getNumberOfShapes())
    return false;
  QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
  if (path.isEmpty())
    return false;
  if (QFileInfo(path).suffix().isEmpty())
    path += QStringLiteral(".stl");

  m_stl->setBinaryMode(m_binary);
  try {
    // the exporter opens the file with fopen(): a narrow, local 8-bit name,
    // the same limit legacy has on Windows outside the active code page
    m_stl->write(QFile::encodeName(QDir::toNativeSeparators(path)).constData(), *p.getShape(unsigned(m_shape)));
  } catch (const stlException_c & e) {
    emit failed(QString::fromUtf8(e.comment));
    return false;
  } catch (...) {
    emit failed(QString::fromLatin1(faultyMesh));
    return false;
  }
  emit exported(QFileInfo(path).fileName());
  return true;
}
