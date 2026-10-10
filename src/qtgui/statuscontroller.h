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
#ifndef BTQT_STATUSCONTROLLER_H
#define BTQT_STATUSCONTROLLER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

/* The status bar (spec C10, contract section 6): the live status text, the
 * cursor readout, and flash messages that replace the live text for 2400 ms
 * (PR-13). A new flash replaces an active one and restarts the timer.
 */
class StatusController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  /* what the bar shows: the flash while one is active, else the live text;
   * the text is styled markup (bold ids and counts) */
  Q_PROPERTY(QString text READ text NOTIFY changed FINAL)
  Q_PROPERTY(QString cursorText READ cursorText NOTIFY changed FINAL)
  Q_PROPERTY(bool flashing READ flashing NOTIFY changed FINAL)

public:

  explicit StatusController(QObject * parent = nullptr);

  QString text(void) const { return m_flashing ? m_flash : m_live; }
  const QString & cursorText(void) const { return m_cursor; }
  bool flashing(void) const { return m_flashing; }

  void setLiveText(const QString & markup);
  void setCursorText(const QString & t);

  Q_INVOKABLE void flash(const QString & text);

  /* how long a flash shows; flashMs unless a test shortens it */
  void setFlashDuration(int ms) { m_timer.setInterval(ms); }

  /* The legacy status sentence for a shape (StatPieceInfo), with the spec's
   * emphasis: "Shape <b>S2</b> has <b>14</b> voxels (13 fixed, 1 variable)".
   */
  static QString shapeText(unsigned int shapeIndex, unsigned int fixed, unsigned int variable);

  static constexpr int flashMs = 2400;

signals:

  void changed(void);

private:

  QString m_live, m_cursor, m_flash;
  bool m_flashing = false;
  QTimer m_timer;
};

#endif
