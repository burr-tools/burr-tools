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
#ifndef BTQT_PIPELINECACHE_H
#define BTQT_PIPELINECACHE_H

// QString first: QByteArray's header alone leaves QChar incomplete for
// GCC 16's -Wsfinae-incomplete (an error in the --werror release build)
#include <QString>
#include <QByteArray>

class QQuickWindow;

/* The graphics pipeline caches: the compiled shaders and pipeline states
 * of the main window and of the image export's offscreen renderer, kept
 * between runs so the second start skips compiling them.
 *
 * They live next to the settings file -- for ".burrtools-qt.rc",
 * ".burrtools-qt.pipelines" and ".burrtools-qt.offscreen.pipelines" -- so
 * a run on a scratch settings file (BURRTOOLS_QT_SETTINGS, the tests) never
 * touches the user's caches. A cache made by another driver, Qt version or
 * graphics API is refused by QRhi and simply rebuilt; a missing or broken
 * file is the same as none.
 */
namespace PipelineCache {

  /* the main window's and the offscreen renderer's cache for a settings file */
  QString windowFile(const QString & settingsFile);
  QString offscreenFile(const QString & settingsFile);

  /* Load from and save to `file` (Qt Quick does both, the saving when the
   * window goes away). Call before the window first renders. */
  void configureWindow(QQuickWindow * window, const QString & file);

  /* the file's bytes, empty when it is missing or unreadable */
  QByteArray read(const QString & file);

  /* replace the file, all or nothing; false (and nothing said) on failure */
  bool write(const QString & file, const QByteArray & data);
}

#endif
