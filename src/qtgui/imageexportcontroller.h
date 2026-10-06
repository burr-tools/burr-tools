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
#ifndef BTQT_IMAGEEXPORTCONTROLLER_H
#define BTQT_IMAGEEXPORTCONTROLLER_H

#include "scenecontroller.h"

#include "../uicore/assemblyscene.h"

#include <QImage>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <memory>
#include <vector>

class DocumentController;
class OffscreenRenderer;
class ShapesModel;

/* Export ▸ Image (legacy imageExport_c), without the dialog: what to export
 * (a shape, a problem's pieces, an assembly, a solution's assembly or
 * disassembly steps), how (background, supersampling, colours, dimming of
 * static pieces), onto which pages (paper size and DPI, or pixels; how many
 * files), and the preview whose orientation the pictures take.
 *
 * An export runs a picture per timer tick, so the dialog stays responsive
 * and shows progress, as legacy's state machine did: first every picture is
 * drawn small to learn its width, then the line height that fits them all
 * onto the pages is chosen, then each picture is drawn supersampled,
 * cropped to its content, scaled down and placed, and the pages are saved
 * as <name>000.png, <name>001.png, ...
 */
class ImageExportController : public SceneController {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  /* "shape" | "problem" | "assembly" | "solution" | "disassembly" */
  Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY contentChanged)
  Q_PROPERTY(int shape READ shape WRITE setShape NOTIFY contentChanged)
  Q_PROPERTY(int problem READ problem WRITE setProblem NOTIFY contentChanged)
  Q_PROPERTY(bool canShape READ canShape NOTIFY contentChanged)
  Q_PROPERTY(bool canProblem READ canProblem NOTIFY contentChanged)
  Q_PROPERTY(bool canAssembly READ canAssembly NOTIFY contentChanged)
  Q_PROPERTY(bool canSolution READ canSolution NOTIFY contentChanged)

  Q_PROPERTY(bool transparent READ transparent WRITE setTransparent NOTIFY optionsChanged)
  Q_PROPERTY(int supersampling READ supersampling WRITE setSupersampling NOTIFY optionsChanged)
  /* the colour constraint colours instead of the piece colours */
  Q_PROPERTY(bool constraintColours READ constraintColours WRITE setConstraintColours NOTIFY optionsChanged)
  Q_PROPERTY(bool dimStatic READ dimStatic WRITE setDimStatic NOTIFY optionsChanged)

  /* "a4p" | "a4l" | "letterp" | "letterl" | "manual" */
  Q_PROPERTY(QString paper READ paper WRITE setPaper NOTIFY sizeChanged)
  Q_PROPERTY(int sizeXmm READ sizeXmm WRITE setSizeXmm NOTIFY sizeChanged)
  Q_PROPERTY(int sizeYmm READ sizeYmm WRITE setSizeYmm NOTIFY sizeChanged)
  Q_PROPERTY(int dpi READ dpi WRITE setDpi NOTIFY sizeChanged)
  Q_PROPERTY(int pixelX READ pixelX WRITE setPixelX NOTIFY sizeChanged)
  Q_PROPERTY(int pixelY READ pixelY WRITE setPixelY NOTIFY sizeChanged)
  Q_PROPERTY(int pages READ pages WRITE setPages NOTIFY sizeChanged)

  Q_PROPERTY(bool busy READ busy NOTIFY progressChanged)
  Q_PROPERTY(QString progressText READ progressText NOTIFY progressChanged)
  Q_PROPERTY(QUrl folder READ folder NOTIFY contentChanged)
  Q_PROPERTY(QString suggestedName READ suggestedName NOTIFY contentChanged)

public:

  ImageExportController(SettingsController * settings, DocumentController * doc, ShapesModel * shapes,
                        QObject * parent = nullptr);
  ~ImageExportController() override;

  QString mode(void) const { return m_mode; }
  void setMode(const QString & m);
  int shape(void) const { return m_shape; }
  void setShape(int s);
  int problem(void) const { return m_problem; }
  void setProblem(int p);
  bool canShape(void) const;
  bool canProblem(void) const;
  bool canAssembly(void) const;
  bool canSolution(void) const;

  bool transparent(void) const { return m_transparent; }
  void setTransparent(bool t);
  int supersampling(void) const { return m_aa; }
  void setSupersampling(int aa);
  bool constraintColours(void) const { return m_constraint; }
  void setConstraintColours(bool c);
  bool dimStatic(void) const { return m_dim; }
  void setDimStatic(bool d);

  QString paper(void) const { return m_paper; }
  void setPaper(const QString & p);
  int sizeXmm(void) const { return m_mmX; }
  void setSizeXmm(int v);
  int sizeYmm(void) const { return m_mmY; }
  void setSizeYmm(int v);
  int dpi(void) const { return m_dpi; }
  void setDpi(int v);
  int pixelX(void) const { return m_pxX; }
  void setPixelX(int v);
  int pixelY(void) const { return m_pxY; }
  void setPixelY(int v);
  int pages(void) const { return m_pages; }
  void setPages(int v);

  bool busy(void) const { return m_phase != Phase::Idle; }
  QString progressText(void) const { return m_progress; }
  QUrl folder(void) const;
  QString suggestedName(void) const;

  /* the dialog opens / closes */
  Q_INVOKABLE void begin(void);
  Q_INVOKABLE void end(void);

  /* Start exporting; pages go next to `file`, named after it without its
   * extension plus a three-digit page number. */
  Q_INVOKABLE void start(const QUrl & file);
  Q_INVOKABLE void cancel(void);

  /* run the export to its end now; for tests */
  void finish(void);

  /* the files the last export wrote */
  QStringList writtenFiles(void) const { return m_written; }

  SceneFrame frame(float devicePixelRatio) const override;

signals:

  void contentChanged(void);
  void optionsChanged(void);
  void sizeChanged(void);
  void progressChanged(void);
  void finished(int pages, const QString & firstFile);
  void failed(const QString & message);

private:

  struct Job {
    bool assembly = false;
    unsigned int shape = 0;          // a shape picture
    unsigned int problem = 0, solution = 0;
    int step = -1;                   // -1: the assembly as it is
    bool dim = false;
  };

  enum class Phase { Idle, Measure, Draw };

  btui::ColorMode colorMode(void) const;
  /* the pictures look like the 3D view: Settings ▸ Voxel style */
  btui::VoxelStyle voxelStyle(void) const;
  btui::SceneContent sceneFor(const Job & j) const;
  void fixMode(void);
  void rebuildPreview(void);
  void recomputePixels(void);
  std::vector<Job> jobs(void) const;
  QImage drawPicture(const Job & j, int height, int aa);
  void step(void);
  void stop(const QString & text);
  void writePages(void);

  DocumentController * m_doc;
  ShapesModel * m_shapes;

  QString m_mode = QStringLiteral("solution");
  int m_shape = 0, m_problem = 0;
  bool m_transparent = false, m_constraint = false, m_dim = false;
  int m_aa = 3;
  QString m_paper = QStringLiteral("manual");
  int m_mmX = 0, m_mmY = 0, m_dpi = 300, m_pxX = 300, m_pxY = 300, m_pages = 1;

  std::shared_ptr<const btui::ShapeMesh> m_mesh;
  quint64 m_meshRevision = 0;

  // the running export
  std::unique_ptr<OffscreenRenderer> m_renderer;
  Phase m_phase = Phase::Idle;
  std::vector<Job> m_jobs;
  std::vector<double> m_ratios;
  std::vector<QImage> m_pictures;
  size_t m_next = 0;
  int m_lineHeight = 0;
  QString m_base;
  QStringList m_written;
  QString m_progress;
  QTimer m_timer;
};

#endif
