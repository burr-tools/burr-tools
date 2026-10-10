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
#include "statuscontroller.h"

StatusController::StatusController(QObject * parent) : QObject(parent) {
  m_timer.setSingleShot(true);
  m_timer.setInterval(flashMs);
  connect(&m_timer, &QTimer::timeout, this, [this] {
    m_flashing = false;
    emit changed();
  });
}

void StatusController::setLiveText(const QString & markup) {
  if (markup == m_live)
    return;
  m_live = markup;
  if (!m_flashing)
    emit changed();
}

void StatusController::setCursorText(const QString & t) {
  if (t == m_cursor)
    return;
  m_cursor = t;
  emit changed();
}

void StatusController::flash(const QString & text) {
  m_flash = text.toHtmlEscaped();
  m_flashing = true;
  m_timer.start();
  emit changed();
}

QString StatusController::shapeText(unsigned int shapeIndex, unsigned int fixed, unsigned int variable) {
  return QStringLiteral("Shape <b>S%1</b> has <b>%2</b> voxels (%3 fixed, %4 variable)")
    .arg(shapeIndex + 1).arg(fixed + variable).arg(fixed).arg(variable);
}
