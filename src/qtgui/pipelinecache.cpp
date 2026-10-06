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
#include "pipelinecache.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QQuickGraphicsConfiguration>
#include <QQuickWindow>
#include <QSaveFile>

namespace {

  /* "<dir>/<settings file name without its last suffix><suffix>" */
  QString besideSettings(const QString & settingsFile, const char * suffix) {
    if (settingsFile.isEmpty())
      return QString();
    const QFileInfo fi(settingsFile);
    QString base = fi.fileName();
    const qsizetype dot = base.lastIndexOf(QLatin1Char('.'));
    if (dot > 0)
      base.truncate(dot);
    return fi.dir().filePath(base + QLatin1String(suffix));
  }
}

namespace PipelineCache {

  QString windowFile(const QString & settingsFile) {
    return besideSettings(settingsFile, ".pipelines");
  }

  QString offscreenFile(const QString & settingsFile) {
    return besideSettings(settingsFile, ".offscreen.pipelines");
  }

  void configureWindow(QQuickWindow * window, const QString & file) {
    if (!window || file.isEmpty())
      return;
    QQuickGraphicsConfiguration config = window->graphicsConfiguration();
    config.setPipelineCacheSaveFile(file);
    // a missing file is not even opened: Qt would mention that it is missing
    if (QFileInfo::exists(file))
      config.setPipelineCacheLoadFile(file);
    window->setGraphicsConfiguration(config);
  }

  QByteArray read(const QString & file) {
    if (file.isEmpty())
      return QByteArray();
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
      return QByteArray();
    return f.readAll();
  }

  bool write(const QString & file, const QByteArray & data) {
    if (file.isEmpty() || data.isEmpty())
      return false;
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly))
      return false;
    if (f.write(data) != data.size()) {
      f.cancelWriting();
      return false;
    }
    return f.commit();
  }
}
