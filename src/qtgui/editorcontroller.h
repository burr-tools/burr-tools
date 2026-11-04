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
#ifndef BTQT_EDITORCONTROLLER_H
#define BTQT_EDITORCONTROLLER_H

#include "documentcontroller.h"

#include "../uicore/voxeledit.h"

#include <QColor>
#include <QList>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class ColoursModel;
class ShapesModel;
class StatusController;
class voxel_c;

/* The voxel editor (C07, C08, C02): the tool and the quick tool the held
 * modifiers pick, Mirror and Span, the plane and the active layer, the
 * strokes the 2D grid paints, and the selected shape's grid size. The rules
 * are btui::voxeledit's; here they meet the document (every change one undo
 * step: a stroke, a size edit) and the views.
 *
 * The 2D grid (VoxelGridItem) reports the pointer in cells of the current
 * plane and layer: hover(), press(), dragTo(), release(). The tool a press
 * uses is captured at the press and kept for the whole stroke (AC-C08-10).
 * The 3D view shows the plane and layer as its slab (ViewportController).
 */
class EditorController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

public:

  /* the tools 1-4 (btui::Tool) and the planes (btui::Plane), for QML */
  enum Tool { Fixed = 0, Variable = 1, Erase = 2, Paint = 3 };
  Q_ENUM(Tool)
  enum Plane { XY = 0, XZ = 1, YZ = 2 };
  Q_ENUM(Plane)

private:

  Q_PROPERTY(Tool tool READ tool WRITE setTool NOTIFY toolChanged FINAL)
  /* the tool a press on the grid would use now: the quick tool of the
   * modifiers held over it, the right button's Erase while it is down, else
   * `tool` -- the cursor and the Edit button follow it */
  Q_PROPERTY(Tool toolInEffect READ toolInEffect NOTIFY toolChanged FINAL)
  /* a quick tool (or the right button) overrides `tool` */
  Q_PROPERTY(bool momentary READ momentary NOTIFY toolChanged FINAL)
  /* 3D edit mode (C06, key E): a plain click in the 3D view applies `tool` */
  Q_PROPERTY(bool editMode READ editMode WRITE setEditMode NOTIFY editModeChanged FINAL)
  /* the 3D view would edit now: a quick tool held over it, or Edit mode
   * (C06 "viewport action"); the Edit button shows the override state */
  Q_PROPERTY(bool viewportActive READ viewportActive NOTIFY toolChanged FINAL)
  /* a quick tool's modifier is held over the 3D view: the Edit button's
   * override state (C06), whatever the tool */
  Q_PROPERTY(bool quickHeld3d READ quickHeld3d NOTIFY toolChanged FINAL)
  /* what a click in the 3D view would do where the pointer is, for the
   * status bar and the empty-shape message (C06; see target3d()) */
  Q_PROPERTY(int targetSource READ targetSource NOTIFY previewChanged FINAL)
  /* Mirror and Span: a bit per axis, X 1, Y 2, Z 4 */
  Q_PROPERTY(int mirror READ mirror WRITE setMirror NOTIFY modifiersChanged FINAL)
  Q_PROPERTY(int span READ span WRITE setSpan NOTIFY modifiersChanged FINAL)
  /* Area mode (legacy's rubber band, EDT_RUBBER): a press and a drag on the
   * grid span an area of cells (btui::areaCells), applied on release as one
   * undo step; off, each cell the pointer passes is painted. App keeps it
   * in the settings. */
  Q_PROPERTY(bool areaMode READ areaMode WRITE setAreaMode NOTIFY areaModeChanged FINAL)
  Q_PROPERTY(Plane plane READ plane WRITE setPlane NOTIFY layerChanged FINAL)
  Q_PROPERTY(int layer READ layer WRITE setLayer NOTIFY layerChanged FINAL)
  Q_PROPERTY(int layerCount READ layerCount NOTIFY layerChanged FINAL)
  /* the axis the layers count along, 0 X 1 Y 2 Z, and its letter */
  Q_PROPERTY(int layerAxis READ layerAxis NOTIFY layerChanged FINAL)
  Q_PROPERTY(QString layerAxisName READ layerAxisName NOTIFY layerChanged FINAL)
  /* per layer, whether it holds a voxel (the layer strip's dots) */
  Q_PROPERTY(QList<bool> layerDots READ layerDots NOTIFY layerDotsChanged FINAL)
  /* XZ and YZ exist (bricks only: btui::planeAvailable) */
  Q_PROPERTY(bool allPlanes READ allPlanes NOTIFY shapeChanged FINAL)
  Q_PROPERTY(bool hasShape READ hasShape NOTIFY shapeChanged FINAL)
  /* the shape's size in steps (C02; btui::sizeStep voxels each) */
  Q_PROPERTY(int sizeX READ sizeX NOTIFY sizeChanged FINAL)
  Q_PROPERTY(int sizeY READ sizeY NOTIFY sizeChanged FINAL)
  Q_PROPERTY(int sizeZ READ sizeZ NOTIFY sizeChanged FINAL)
  Q_PROPERTY(int maxSize READ maxSize CONSTANT FINAL)
  Q_PROPERTY(bool applyToAll READ applyToAll WRITE setApplyToAll NOTIFY applyToAllChanged FINAL)
  /* changes whenever the grid must be drawn again: voxels, colours, size,
   * shape, plane or layer */
  Q_PROPERTY(quint64 revision READ revision NOTIFY revisionChanged FINAL)

public:

  EditorController(DocumentController * doc, ShapesModel * shapes, ColoursModel * colours, StatusController * status,
                   QObject * parent = nullptr);

  Tool tool(void) const { return m_tool; }
  void setTool(Tool t);
  Tool toolInEffect(void) const;
  bool momentary(void) const { return toolInEffect() != m_tool || m_rightDown; }
  int mirror(void) const { return m_mirror; }
  void setMirror(int bits);
  int span(void) const { return m_span; }
  void setSpan(int bits);
  Q_INVOKABLE void toggleMirror(int axis);
  Q_INVOKABLE void toggleSpan(int axis);
  bool areaMode(void) const { return m_areaMode; }
  void setAreaMode(bool on);

  bool editMode(void) const { return m_editMode; }
  void setEditMode(bool on);
  Q_INVOKABLE void toggleEditMode(void) { setEditMode(!m_editMode); }

  Plane plane(void) const { return m_plane; }
  void setPlane(Plane p);
  btui::Plane planeValue(void) const { return btui::Plane(int(m_plane)); }
  int layer(void) const { return m_layer; }
  void setLayer(int l);
  Q_INVOKABLE void layerStep(int delta);
  int layerCount(void) const;
  int layerAxis(void) const;
  QString layerAxisName(void) const;
  QList<bool> layerDots(void) const;
  /* whether a plane can be edited for the shape's voxel type */
  Q_INVOKABLE bool planeAvailable(int plane) const;
  bool allPlanes(void) const { return planeAvailable(XZ); }

  bool hasShape(void) const { return shape() != nullptr; }
  /* the selected shape, null for none; read only */
  const voxel_c * shape(void) const;
  int shapeIndex(void) const { return m_shapeIndex; }
  const DocumentController * document(void) const { return m_doc; }
  /* the status bar, whose cursor readout the 3D view's pointer writes too */
  StatusController * status(void) const { return m_status; }

  int sizeX(void) const { return size(0); }
  int sizeY(void) const { return size(1); }
  int sizeZ(void) const { return size(2); }
  Q_INVOKABLE int size(int axis) const;
  int maxSize(void) const { return btui::maxGridSteps; }
  bool applyToAll(void) const { return m_applyToAll; }
  void setApplyToAll(bool v);
  /* Set an axis's size, in steps, as one undo step: on the selected shape,
   * or -- Apply to all -- by the same steps on every shape that allows it.
   * A shrink that would cut voxels is refused with a status message. */
  Q_INVOKABLE void setSize(int axis, int steps);

  quint64 revision(void) const { return m_revision; }

  /* the drawing colour a tool places (0 Default), and as the cursor shows
   * it: invalid for Default, which the cursor draws in the theme's tones */
  int drawColour(void) const;
  QColor cursorColour(void) const;

  // --- the pointer over the grid, in cells of the plane's current layer ----

  /* the cell under the pointer, (-1, -1) when it left the grid; the status
   * bar's cursor readout follows */
  void hover(int col, int row);
  int hoverCol(void) const { return m_hoverCol; }
  int hoverRow(void) const { return m_hoverRow; }
  /* the modifiers held while the pointer is over the grid (quick tools) */
  void setModifiers(Qt::KeyboardModifiers m);
  /* Begin a stroke: the tool in effect (the right button erases) is applied
   * to the cell and kept until release(). False when there is no shape. */
  bool press(int col, int row, bool rightButton, Qt::KeyboardModifiers m);
  /* the stroke reached another cell */
  void dragTo(int col, int row);
  /* the stroke ends: one undo step if it changed anything */
  void release(void);
  bool stroking(void) const { return m_stroking; }
  /* Area mode: an area is being dragged, and its cells (Mirror and Span
   * included) -- the grid outlines those of its layer; cancelArea() drops
   * it unapplied (Esc, another shape) */
  bool areaDragging(void) const { return m_stroking && m_area; }
  std::vector<btui::Cell3> area(void) const;
  Q_INVOKABLE void cancelArea(void);

  /* "Cursor X1 Y2 · Z0" for a cell of the current plane and layer */
  QString cursorText(int col, int row) const;

  /* The hover preview (C06, AC-C08-10): what the tool in effect would do at
   * the cell under the pointer, Mirror and Span included -- the 3D view
   * draws it as ghosts. Empty while the pointer is off the grid and while a
   * stroke paints. previewChanged() whenever it may have changed. */
  std::vector<btui::GhostCell> preview(void) const;

  // --- the pointer over the 3D view (C06 3D editing) ------------------------

  /* The tool a click in the 3D view applies with these modifiers held: the
   * quick tool, else (Edit mode) `tool`, else none. */
  std::optional<btui::Tool> viewportAction(Qt::KeyboardModifiers m) const;
  bool viewportActive(void) const { return m_over3d && viewportAction(m_keys3d).has_value(); }
  bool quickHeld3d(void) const;
  /* The pointer is over the 3D view with these modifiers, and a click would
   * act on `t` (ViewportController picks it); the hover preview follows.
   * leave3d(): it left, or a drag began. */
  void hover3d(const btui::Target3d & t, Qt::KeyboardModifiers m);
  void leave3d(void);
  bool over3d(void) const { return m_over3d; }
  const btui::Target3d & target3d(void) const { return m_target3d; }
  int targetSource(void) const { return m_over3d ? int(m_target3d.source) : 0; }
  /* A click in the 3D view: the tool the modifiers pick applied at `t`,
   * Mirror and Span included, as one undo step. False when nothing changed
   * (no action, no target, or the cells already as the tool leaves them). */
  bool click3d(const btui::Target3d & t, Qt::KeyboardModifiers m);

signals:

  void toolChanged(void);
  void modifiersChanged(void);
  void layerChanged(void);
  void layerDotsChanged(void);
  void shapeChanged(void);
  void sizeChanged(void);
  void applyToAllChanged(void);
  void revisionChanged(void);
  void flashRequested(const QString & text);
  void drawColourChanged(void);
  void previewChanged(void);
  void editModeChanged(void);
  void areaModeChanged(void);

private:

  void refreshShape(void);
  void contentEdited(void);
  void clampLayer(void);
  void bump(void);
  void apply(int col, int row);
  btui::Modifiers modifiers(void) const;

  DocumentController * m_doc;
  ShapesModel * m_shapes;
  ColoursModel * m_colours;
  StatusController * m_status;

  Tool m_tool = Fixed;
  int m_mirror = 0;
  int m_span = 0;
  Plane m_plane = XY;
  int m_layer = 0;
  int m_shapeIndex = -1;
  bool m_applyToAll = false;
  quint64 m_revision = 0;

  int m_hoverCol = -1, m_hoverRow = -1;
  btui::ModifierKeys m_keys;               ///< held over the grid
  bool m_editMode = false;
  bool m_over3d = false;                   ///< the pointer is over the 3D view
  Qt::KeyboardModifiers m_keys3d;          ///< held over it
  btui::Target3d m_target3d;
  bool m_stroking = false;
  bool m_rightDown = false;
  btui::Tool m_strokeTool = btui::Tool::Fixed;
  int m_lastCol = -1, m_lastRow = -1;      ///< the stroke's last cell
  bool m_areaMode = false;
  bool m_area = false;                     ///< the stroke spans an area (Area mode at the press)
  int m_areaCol0 = 0, m_areaRow0 = 0, m_areaCol1 = 0, m_areaRow1 = 0;
};

#endif
