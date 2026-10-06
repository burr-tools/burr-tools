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
#ifndef BTQT_STLEXPORTCONTROLLER_H
#define BTQT_STLEXPORTCONTROLLER_H

#include "scenecontroller.h"

#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <memory>

class DocumentController;
class ShapesModel;
class stlExporter_c;

/* Export ▸ STL (legacy stlExport_c), without the dialog: the grid's STL
 * exporter with its parameters, the shape to export, binary or text, and
 * the preview of the exporter's own mesh, which is also what a
 * VoxelViewport in the dialog draws -- the preview handles like the main
 * view (SceneController).
 *
 * As in legacy, every parameter change rebuilds the preview, a mesh that
 * cannot be made empties it and says why, and the dialog stays open after
 * an export so more shapes can follow.
 */
class StlExportController : public SceneController {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  /* how many parameters the exporter has; changes only when the dialog
   * opens or closes, so the fields stay put while values change */
  Q_PROPERTY(int parameterCount READ parameterCount NOTIFY parametersChanged)
  /* bumps whenever a parameter value changes */
  Q_PROPERTY(int revision READ revision NOTIFY valuesChanged)
  Q_PROPERTY(int shape READ shape WRITE setShape NOTIFY shapeChanged)
  Q_PROPERTY(bool binary READ binary WRITE setBinary NOTIFY optionsChanged)
  /* the see-through view of the preview (legacy "insides" mode) */
  Q_PROPERTY(bool insides READ insides WRITE setInsides NOTIFY optionsChanged)
  /* "Volume: 12.3 cubic-units", empty without a mesh */
  Q_PROPERTY(QString volumeText READ volumeText NOTIFY meshChanged)
  /* why there is no preview, empty when there is one */
  Q_PROPERTY(QString error READ error NOTIFY meshChanged)
  Q_PROPERTY(bool hasMesh READ hasMesh NOTIFY meshChanged)
  /* where the save dialog starts and what it suggests */
  Q_PROPERTY(QUrl folder READ folder NOTIFY shapeChanged)
  Q_PROPERTY(QString suggestedName READ suggestedName NOTIFY shapeChanged)

public:

  StlExportController(SettingsController * settings, DocumentController * doc, ShapesModel * shapes,
                      QObject * parent = nullptr);
  ~StlExportController() override;

  /* the puzzle's grid can export, and there is a shape */
  static bool available(const DocumentController * doc);

  int parameterCount(void) const;
  int revision(void) const { return m_revision; }

  /* { index, name, tooltip, type ("double" | "posDouble" | "posInt" |
   * "switch"), value (number), text (as the legacy field showed it) };
   * empty for an index out of range */
  Q_INVOKABLE QVariantMap parameter(int index) const;
  int shape(void) const { return m_shape; }
  void setShape(int s);
  bool binary(void) const { return m_binary; }
  void setBinary(bool b);
  bool insides(void) const { return m_insides; }
  void setInsides(bool on);
  QString volumeText(void) const { return m_volumeText; }
  QString error(void) const { return m_error; }
  bool hasMesh(void) const { return m_mesh != nullptr; }
  QUrl folder(void) const;
  QString suggestedName(void) const;

  /* The dialog opens: a fresh exporter for the puzzle's grid with its
   * default parameters, the main view's shape, and its preview. */
  Q_INVOKABLE void begin(void);
  /* The dialog closed: drop the exporter and the preview. */
  Q_INVOKABLE void end(void);

  /* set parameter `index`; values are clamped as the field's type demands */
  Q_INVOKABLE void setParameter(int index, double value);

  /* Write the shape to file (".stl" added when missing). False, with
   * failed() telling why, when it could not be written. */
  Q_INVOKABLE bool exportTo(const QUrl & file);

  SceneFrame frame(float devicePixelRatio) const override;

signals:

  void parametersChanged(void);
  void valuesChanged(void);
  void shapeChanged(void);
  void optionsChanged(void);
  void meshChanged(void);
  void failed(const QString & message);
  void exported(const QString & fileName);

private:

  void rebuild(void);

  DocumentController * m_doc;
  ShapesModel * m_shapes;
  std::unique_ptr<stlExporter_c> m_stl;
  int m_shape = 0;
  int m_revision = 0;
  bool m_binary = true;
  bool m_insides = false;
  std::shared_ptr<const btui::ShapeMesh> m_mesh;
  quint64 m_meshRevision = 0;
  QString m_volumeText, m_error;
};

#endif
