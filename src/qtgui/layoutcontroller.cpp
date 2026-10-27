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
#include "layoutcontroller.h"
#include "settingscontroller.h"

#include <tuple>

namespace {

  const char * const workspaceKeys[] = { "entities", "puzzle", "solver" };

  QString key(int ws, const char * side) {
    return QStringLiteral("layout.%1.%2Collapsed").arg(QLatin1String(workspaceKeys[ws]), QLatin1String(side));
  }
}

LayoutController::LayoutController(SettingsController * settings, QObject * parent) :
  QObject(parent), m_settings(settings)
{
  for (int w = 0; w < 3; w++)
    m_model.restoreCollapsed(btui::Workspace(w),
                             m_settings->boolValue(key(w, "left"), false),
                             m_settings->boolValue(key(w, "right"), false));

  // a density change moves every column
  connect(m_settings, &SettingsController::changed, this, &LayoutController::changed);
}

btui::Density LayoutController::density(void) const {
  return m_settings->densityValue();
}

btui::ColumnWidths LayoutController::columns(void) const {
  return m_model.columns(m_windowWidth, density());
}

int LayoutController::focus(void) const {
  switch (m_model.focus()) {
    case btui::Focus::Focus2d: return Focus2d;
    case btui::Focus::Focus3d: return Focus3d;
    case btui::Focus::None:    return FocusNone;
  }
  return FocusNone;
}

void LayoutController::persist(void) {
  const int w = workspace();
  m_settings->setBoolValue(key(w, "left"), m_model.leftCollapsed());
  m_settings->setBoolValue(key(w, "right"), m_model.rightCollapsed());
}

/* Emit changed only when the layout did change: every emit re-evaluates the
 * menus' enabled states and redraws the 3D view. The whole state is
 * compared, not the one flag -- expanding a sidebar shown as a rail exits a
 * Focus mode while the flag stays as it was. */
template <typename Change>
void LayoutController::changeLayout(Change change) {
  const auto state = [this] {
    return std::tuple(m_model.workspace(), m_model.focus(), m_model.leftCollapsed(), m_model.rightCollapsed());
  };
  const auto before = state();
  change();
  if (state() != before)
    emit changed();
}

void LayoutController::setWorkspace(int w) {
  // the workspace already shown is no switch: its Focus mode stays
  if (w < 0 || w > 2 || btui::Workspace(w) == m_model.workspace())
    return;
  changeLayout([&] { m_model.setWorkspace(btui::Workspace(w)); });
}

void LayoutController::setLeftCollapsed(bool c) {
  changeLayout([&] {
    m_model.setLeftCollapsed(c);
    persist();
  });
}

void LayoutController::setRightCollapsed(bool c) {
  changeLayout([&] {
    m_model.setRightCollapsed(c);
    persist();
  });
}

void LayoutController::setWindowWidth(qreal w) {
  if (qFuzzyCompare(w, m_windowWidth))
    return;
  m_windowWidth = w;
  emit changed();
}

void LayoutController::toggleLeft(void) {
  m_model.toggleLeft();
  persist();
  emit changed();
}

void LayoutController::toggleRight(void) {
  m_model.toggleRight();
  persist();
  emit changed();
}

void LayoutController::toggleFocus2d(void) {
  m_model.toggleFocus2d();
  emit changed();
}

void LayoutController::toggleFocus3d(void) {
  m_model.toggleFocus3d();
  emit changed();
}

bool LayoutController::escape(void) {
  bool used = m_model.escape();
  if (used)
    emit changed();
  return used;
}
