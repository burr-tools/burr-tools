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
#ifndef BTQT_INSPECTORCONTROLLER_H
#define BTQT_INSPECTORCONTROLLER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <map>
#include <vector>

namespace btui { struct OpsDescriptor; }

class DocumentController;
class EditorController;
class SettingsController;
class ShapesModel;
class ViewportController;

/* The inspector under the Shapes list (C03 Fit & scale, C04 Transform,
 * C05 Repair & surface; contract section 5): which sections are open
 * (persisted), the Transform buttons of the file's voxel type, which
 * operations may run and why not, running them as one undo step each, and
 * the 3D preview of a Transform button under the pointer.
 *
 * The rules are btui::shapeops' (legacy's tool-tab commands per voxel
 * type). Fit & scale's operations act on every shape while the Grid size
 * header's Apply to all shapes is on, as legacy's; the others on the
 * selected shape. A Transform button is always enabled and checked when
 * pressed -- a nudge out of the grid is refused with its reason in the
 * status bar -- so an edit never re-runs every transform on a copy; Fit &
 * scale's states are worked out again shortly after the shapes change.
 */
class InspectorController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

public:

  /* legacy's numbers for the operations that are not transforms (btui::task);
   * QML takes them from `tasks` -- the compiled QML cannot read the values
   * of an enum that does not count up from 0 */
  enum Task {
    Prune = 15, FixedInner = 16, FixedOuter = 17, VariableInner = 18, VariableOuter = 19,
    ClearInner = 20, ClearOuter = 21, Scale2 = 22, Scale3 = 23, Origin = 24, Center = 25,
    Minimize = 26, FillHoles = 40,
  };
  Q_ENUM(Task)

private:

  Q_PROPERTY(bool transformOpen READ transformOpen WRITE setTransformOpen NOTIFY sectionsChanged FINAL)
  Q_PROPERTY(bool fitOpen READ fitOpen WRITE setFitOpen NOTIFY sectionsChanged FINAL)
  Q_PROPERTY(bool repairOpen READ repairOpen WRITE setRepairOpen NOTIFY sectionsChanged FINAL)
  /* the Task numbers by name: { prune, center, origin, minimize, scale2,
   * scale3, fillHoles, fixedInner, fixedOuter, variableInner,
   * variableOuter, clearInner, clearOuter } */
  Q_PROPERTY(QVariantMap tasks READ tasks CONSTANT FINAL)
  /* the voxel type's Transform buttons, as { task, kind ("flip" | "rotate" |
   * "nudge"), axis, sign, glyph, arrowDeg, layer, caption, tip, id } */
  Q_PROPERTY(QVariantList transformButtons READ transformButtons NOTIFY typeChanged FINAL)
  /* the two-block Transform layout: a Flip & Rotate matrix and a nudge pad */
  Q_PROPERTY(bool pads READ pads NOTIFY typeChanged FINAL)
  /* ×2 / ×3 (C14 supports_scale) and Minimize for the voxel type */
  Q_PROPERTY(bool scaleShown READ scaleShown NOTIFY typeChanged FINAL)
  /* changes whenever what may run changed: bind allowed() / reason() to it */
  Q_PROPERTY(int revision READ revision NOTIFY stateChanged FINAL)
  /* the selected shape, for the head; -1 for none */
  Q_PROPERTY(int shape READ shape NOTIFY stateChanged FINAL)
  /* what Minimize divides by now (1: it cannot) */
  Q_PROPERTY(int minimizeFactor READ minimizeFactor NOTIFY stateChanged FINAL)
  /* the Transform button the 3D view previews, -1 for none */
  Q_PROPERTY(int previewTask READ previewTask NOTIFY previewChanged FINAL)

public:

  InspectorController(SettingsController * settings, DocumentController * doc, ShapesModel * shapes,
                      EditorController * editor, ViewportController * viewport, QObject * parent = nullptr);

  bool transformOpen(void) const { return m_open[0]; }
  void setTransformOpen(bool v) { setOpen(0, v); }
  bool fitOpen(void) const { return m_open[1]; }
  void setFitOpen(bool v) { setOpen(1, v); }
  bool repairOpen(void) const { return m_open[2]; }
  void setRepairOpen(bool v) { setOpen(2, v); }

  QVariantMap tasks(void) const;
  QVariantList transformButtons(void) const;
  bool pads(void) const;
  bool scaleShown(void) const;
  int revision(void) const { return m_revision; }
  int shape(void) const;
  int minimizeFactor(void) const { return m_minimizeFactor; }
  int previewTask(void) const { return m_previewTask; }

  /* Fit & scale's and Repair's operations: may it run now, and if not why.
   * A Transform button: always (checked when pressed). */
  Q_INVOKABLE bool allowed(int task) const;
  Q_INVOKABLE QString reason(int task) const;
  /* run it: one undo step, or the reason it cannot in the status bar */
  Q_INVOKABLE void run(int task);
  /* the 3D preview of a Transform button (legacy previewTransform), and its end */
  Q_INVOKABLE void preview(int task);
  Q_INVOKABLE void clearPreview(void);

signals:

  void sectionsChanged(void);
  void typeChanged(void);
  void stateChanged(void);
  void previewChanged(void);
  void flashRequested(const QString & text);

private:

  void setOpen(int i, bool v);
  const btui::OpsDescriptor & descriptor(void) const;
  bool isTransform(int task) const;
  /* the shapes a task acts on: every one for Fit & scale's while Apply to
   * all shapes is on, else the selected one; none without a selection */
  std::vector<unsigned int> targets(int task) const;
  void shapeEdited(void);
  void recompute(void);
  void refreshPreview(void);
  void setPreviewTask(int task);

  SettingsController * m_settings;
  DocumentController * m_doc;
  ShapesModel * m_shapes;
  EditorController * m_editor;
  ViewportController * m_viewport;

  bool m_open[3] = { true, false, false };
  struct Check {
    bool allowed = true;
    QString reason;
  };
  std::map<int, Check> m_checks;   ///< Fit & scale's and Repair's states, by task
  int m_minimizeFactor = 1;
  int m_revision = 0;
  int m_previewTask = -1;          ///< the Transform button previewed, -1 none
  QTimer m_settle;                 ///< recomputes the states once edits pause
};

#endif
