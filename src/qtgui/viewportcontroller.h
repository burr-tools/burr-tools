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
#ifndef BTQT_VIEWPORTCONTROLLER_H
#define BTQT_VIEWPORTCONTROLLER_H

#include "scenecontroller.h"

#include <QColor>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <memory>

class DocumentController;
class LayoutController;
class SettingsController;
class ShapesModel;

/* The 3D view's state and behaviour (contract section 4, spec C06 / C13):
 * the camera, the navigation mode, the Display menu options, the scene of
 * the selected shape, and the active layer the slab shows.
 *
 * Pointer input arrives from the view item already in dp; the mouse mapping
 * is the spec's: left-drag does what the toolbar mode says, Shift+left-drag
 * the other thing, middle-drag always pans, the wheel always zooms, and a
 * press that moves less than 4 dp before release is a click, which never
 * navigates (and, from the editing phase on, edits).
 *
 * Plane and layer are owned here until the voxel editor (phase P4) takes
 * them over; PgUp / PgDn already step the layer.
 */
class ViewportController : public SceneController {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  Q_PROPERTY(QString navMode READ navMode WRITE setNavMode NOTIFY optionsChanged FINAL)
  Q_PROPERTY(bool displayAxes READ displayAxes WRITE setDisplayAxes NOTIFY optionsChanged FINAL)
  Q_PROPERTY(bool displayBounds READ displayBounds WRITE setDisplayBounds NOTIFY optionsChanged FINAL)
  Q_PROPERTY(bool displayLayerSlab READ displayLayerSlab WRITE setDisplayLayerSlab NOTIFY optionsChanged FINAL)
  Q_PROPERTY(bool displayDimOtherLayers READ displayDimOtherLayers WRITE setDisplayDimOtherLayers NOTIFY optionsChanged FINAL)
  Q_PROPERTY(QString projection READ projection WRITE setProjection NOTIFY optionsChanged FINAL)
  Q_PROPERTY(QString colourView READ colourView WRITE setColourView NOTIFY optionsChanged FINAL)
  Q_PROPERTY(int plane READ plane WRITE setPlane NOTIFY layerChanged FINAL)
  Q_PROPERTY(int layer READ layer WRITE setLayer NOTIFY layerChanged FINAL)
  Q_PROPERTY(int layerCount READ layerCount NOTIFY layerChanged FINAL)
  Q_PROPERTY(bool editorVisible READ editorVisible NOTIFY optionsChanged FINAL)
  Q_PROPERTY(bool showViewCube READ showViewCube NOTIFY optionsChanged FINAL)
  Q_PROPERTY(bool hasShape READ hasShape NOTIFY sceneChanged FINAL)
  Q_PROPERTY(bool emptyShape READ emptyShape NOTIFY sceneChanged FINAL)
  Q_PROPERTY(QString shapeId READ shapeId NOTIFY sceneChanged FINAL)
  Q_PROPERTY(QString shapeLabel READ shapeLabel NOTIFY sceneChanged FINAL)
  Q_PROPERTY(QColor shapeColor READ shapeColor NOTIFY sceneChanged FINAL)

public:

  ViewportController(SettingsController * settings, DocumentController * doc, ShapesModel * shapes,
                     LayoutController * layout, QObject * parent = nullptr);

  QString navMode(void) const { return m_pan ? QStringLiteral("pan") : QStringLiteral("orbit"); }
  void setNavMode(const QString & m);
  bool displayAxes(void) const { return m_display.axes; }
  void setDisplayAxes(bool v);
  bool displayBounds(void) const { return m_display.bounds; }
  void setDisplayBounds(bool v);
  bool displayLayerSlab(void) const { return m_display.slab; }
  void setDisplayLayerSlab(bool v);
  bool displayDimOtherLayers(void) const { return m_display.dim; }
  void setDisplayDimOtherLayers(bool v);
  QString projection(void) const;
  void setProjection(const QString & p);
  QString colourView(void) const;
  void setColourView(const QString & v);
  int plane(void) const { return m_plane; }
  void setPlane(int p);
  int layer(void) const { return m_layer; }
  void setLayer(int l);
  int layerCount(void) const;
  bool editorVisible(void) const;
  bool showViewCube(void) const;
  bool hasShape(void) const { return m_shape != nullptr; }
  bool emptyShape(void) const { return m_shape && m_mesh && m_mesh->empty(); }
  /* the selected shape's mesh is built (a change schedules the build) */
  bool meshReady(void) const { return !m_meshDirty && (!m_shape || m_mesh); }
  QString shapeId(void) const;
  QString shapeLabel(void) const;
  QColor shapeColor(void) const;

  Q_INVOKABLE void layerStep(int delta);

  /* Export ▸ Vector Image: write the view as it is now to file in the
   * format btui::VectorFormat names (0 PS, 1 EPS, 2 TeX, 3 PDF, 4 SVG,
   * 5 PGF). False when the file could not be written. */
  Q_INVOKABLE bool exportVector(const QUrl & file, int format) const;
  /* the extension of a format, without the dot */
  Q_INVOKABLE static QString vectorExtension(int format);

  // --- rendering -----------------------------------------------------------

  /* the frame to draw now; colours come from the theme */
  SceneFrame frame(float devicePixelRatio) const override;

signals:

  void optionsChanged(void);
  void layerChanged(void);
  void sceneChanged(void);

private:

  void refreshShape(void);
  void scheduleMesh(void);
  void buildMesh(void);
  void clampLayer(void);
  void readDisplay(void);
  void setDisplay(const char * key, bool & option, bool v);

  DocumentController * m_doc;
  ShapesModel * m_shapes;
  LayoutController * m_layout;

  const voxel_c * m_shape = nullptr;
  int m_shapeIndex = -1;
  std::shared_ptr<const btui::ShapeMesh> m_mesh;
  quint64 m_meshRevision = 0;
  btui::VoxelStyle m_meshStyle = btui::VoxelStyle::Flat;   ///< the voxel style m_mesh was built in
  bool m_meshDirty = false;        ///< m_mesh is not the selected shape's as it is now
  bool m_meshQueued = false;       ///< a buildMesh() is posted

  int m_plane = 0;
  int m_layer = 0;

  /* the persisted Display options, kept here for frame() (readDisplay) */
  struct Display {
    bool axes = true, bounds = true, slab = true, dim = false;
  } m_display;
};

#endif
