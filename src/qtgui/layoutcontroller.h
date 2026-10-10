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
#ifndef BTQT_LAYOUTCONTROLLER_H
#define BTQT_LAYOUTCONTROLLER_H

#include "../uicore/layoutmodel.h"

#include <QObject>
#include <QtQml/qqmlregistration.h>

class SettingsController;

/* The workspace layout for QML (contract section 1, spec C11/C15): a thin
 * wrapper around btui::LayoutModel that persists the collapse flags and
 * hands QML the column widths for the current window width and density.
 */
class LayoutController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  Q_PROPERTY(int workspace READ workspace WRITE setWorkspace NOTIFY changed FINAL)
  Q_PROPERTY(int focus READ focus NOTIFY changed FINAL)
  Q_PROPERTY(bool leftCollapsed READ leftCollapsed WRITE setLeftCollapsed NOTIFY changed FINAL)
  Q_PROPERTY(bool rightCollapsed READ rightCollapsed WRITE setRightCollapsed NOTIFY changed FINAL)
  Q_PROPERTY(bool leftShownAsRail READ leftShownAsRail NOTIFY changed FINAL)
  Q_PROPERTY(bool rightShownAsRail READ rightShownAsRail NOTIFY changed FINAL)
  Q_PROPERTY(bool editorVisible READ editorVisible NOTIFY changed FINAL)
  Q_PROPERTY(double windowWidth READ windowWidth WRITE setWindowWidth NOTIFY changed FINAL)
  Q_PROPERTY(double leftWidth READ leftWidth NOTIFY changed FINAL)
  Q_PROPERTY(double centreWidth READ centreWidth NOTIFY changed FINAL)
  Q_PROPERTY(double rightWidth READ rightWidth NOTIFY changed FINAL)

public:

  enum FocusMode { FocusNone = 0, Focus2d = 1, Focus3d = 2 };
  Q_ENUM(FocusMode)

  enum WorkspaceId { Entities = 0, Puzzle = 1, Solver = 2 };
  Q_ENUM(WorkspaceId)

  explicit LayoutController(SettingsController * settings, QObject * parent = nullptr);

  int workspace(void) const { return int(m_model.workspace()); }
  void setWorkspace(int w);
  int focus(void) const;
  bool leftCollapsed(void) const { return m_model.leftCollapsed(); }
  void setLeftCollapsed(bool c);
  bool rightCollapsed(void) const { return m_model.rightCollapsed(); }
  void setRightCollapsed(bool c);
  bool leftShownAsRail(void) const { return m_model.leftShownAsRail(); }
  bool rightShownAsRail(void) const { return m_model.rightShownAsRail(); }
  bool editorVisible(void) const { return m_model.editorVisible(); }
  qreal windowWidth(void) const { return m_windowWidth; }
  void setWindowWidth(qreal w);
  qreal leftWidth(void) const { return columns().left; }
  qreal centreWidth(void) const { return columns().centre; }
  qreal rightWidth(void) const { return columns().right; }

  Q_INVOKABLE void toggleLeft(void);
  Q_INVOKABLE void toggleRight(void);
  Q_INVOKABLE void toggleFocus2d(void);
  Q_INVOKABLE void toggleFocus3d(void);
  /* the layout's rung of the Esc ladder; true when it was used */
  Q_INVOKABLE bool escape(void);

  const btui::LayoutModel & model(void) const { return m_model; }

signals:

  void changed(void);

private:

  btui::ColumnWidths columns(void) const;
  btui::Density density(void) const;
  void persist(void);

  SettingsController * m_settings;
  btui::LayoutModel m_model;
  qreal m_windowWidth = 1600;
};

#endif
