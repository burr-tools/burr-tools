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
#include "imageexportcontroller.h"

#include "app.h"
#include "documentcontroller.h"
#include "guarded.h"
#include "offscreenrenderer.h"
#include "pipelinecache.h"
#include "settingscontroller.h"
#include "shapesmodel.h"

#include "../uicore/imagepages.h"

#include "../lib/bt_assert.h"
#include "../lib/disasmtomoves.h"
#include "../lib/disassembly.h"
#include "../lib/problem.h"
#include "../lib/puzzle.h"
#include "../lib/solution.h"

#include <QDir>
#include <QFileInfo>
#include <QPainter>

#include <algorithm>
#include <optional>

namespace {

  // legacy draws the measuring pictures 600 x 200 and the final ones three
  // times as wide as high, then crops them to their content
  const int kMeasureHeight = 200;
  const int kWidthFactor = 3;

  /* the columns that hold something, as a multiple of `multiple` wide
   * (legacy image_c::minimizeWidth); at least one */
  QRect contentColumns(const QImage & img, int multiple) {
    int lo = img.width(), hi = -1;
    for (int y = 0; y < img.height(); y++) {
      const QRgb * line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
      for (int x = 0; x < img.width(); x++)
        if (qAlpha(line[x]) != 0) {
          lo = std::min(lo, x);
          hi = std::max(hi, x);
        }
    }
    if (hi < lo)
      return QRect(0, 0, std::max(1, multiple), img.height());
    int w = hi - lo + 1;
    w = (w + multiple - 1) / multiple * multiple;
    lo = std::min(lo, std::max(0, img.width() - w));
    return QRect(lo, 0, std::min(w, img.width()), img.height());
  }
}

ImageExportController::ImageExportController(SettingsController * settings, DocumentController * doc,
                                             ShapesModel * shapes, QObject * parent) :
  SceneController(settings, parent), m_doc(doc), m_shapes(shapes)
{
  m_timer.setSingleShot(true);
  m_timer.setInterval(0);
  connect(&m_timer, &QTimer::timeout, this, &ImageExportController::step);
}

ImageExportController::~ImageExportController() = default;

// --- what to export ---------------------------------------------------------

bool ImageExportController::canShape(void) const {
  return m_doc->session().puzzle().getNumberOfShapes() > 0;
}

bool ImageExportController::canProblem(void) const {
  return m_doc->session().puzzle().getNumberOfProblems() > 0;
}

/* legacy looks at the first saved solution to decide */
bool ImageExportController::canAssembly(void) const {
  const puzzle_c & p = m_doc->session().puzzle();
  if (unsigned(m_problem) >= p.getNumberOfProblems())
    return false;
  const problem_c * pr = p.getProblem(unsigned(m_problem));
  return pr->resultValid() && pr->getNumberOfSavedSolutions() > 0 && pr->getSavedSolution(0)->getAssembly();
}

bool ImageExportController::canSolution(void) const {
  if (!canAssembly())
    return false;
  const problem_c * pr = m_doc->session().puzzle().getProblem(unsigned(m_problem));
  return pr->getSavedSolution(0)->getDisassembly() != nullptr;
}

/* legacy cb_Update3DView: an unavailable choice falls back to the next */
void ImageExportController::fixMode(void) {
  if ((m_mode == QLatin1String("disassembly") || m_mode == QLatin1String("solution")) && !canSolution())
    m_mode = QStringLiteral("assembly");
  if (m_mode == QLatin1String("assembly") && !canAssembly())
    m_mode = QStringLiteral("problem");
  if (m_mode == QLatin1String("problem") && !canProblem())
    m_mode = QStringLiteral("shape");
}

void ImageExportController::setMode(const QString & m) {
  static const QStringList modes{ "shape", "problem", "assembly", "solution", "disassembly" };
  if (!modes.contains(m) || m == m_mode)
    return;
  m_mode = m;
  fixMode();
  rebuildPreview();
}

void ImageExportController::setShape(int s) {
  const int n = int(m_doc->session().puzzle().getNumberOfShapes());
  s = std::clamp(s, 0, std::max(0, n - 1));
  if (s == m_shape)
    return;
  m_shape = s;
  rebuildPreview();
}

void ImageExportController::setProblem(int p) {
  const int n = int(m_doc->session().puzzle().getNumberOfProblems());
  p = std::clamp(p, 0, std::max(0, n - 1));
  if (p == m_problem)
    return;
  m_problem = p;
  fixMode();
  rebuildPreview();
}

btui::VoxelStyle ImageExportController::voxelStyle(void) const {
  return m_settings->voxelStyle() == QLatin1String("legacy") ? btui::VoxelStyle::Legacy : btui::VoxelStyle::Flat;
}

btui::ColorMode ImageExportController::colorMode(void) const {
  return m_constraint ? btui::ColorMode::Voxel : btui::ColorMode::Piece;
}

btui::SceneContent ImageExportController::sceneFor(const Job & j) const {
  const puzzle_c & p = m_doc->session().puzzle();
  if (!j.assembly)
    return btui::buildShapeScene(p, j.shape, colorMode(), voxelStyle());
  const problem_c & pr = *p.getProblem(j.problem);
  if (j.step < 0)
    return btui::buildAssemblyScene(pr, j.solution, colorMode(), nullptr, false, voxelStyle());
  const separation_c * t = pr.getSavedSolution(j.solution)->getDisassembly();
  disasmToMoves_c dtm(t, 20, pr.getNumberOfPieces());
  dtm.setStep(float(j.step), false, true);
  return btui::buildAssemblyScene(pr, j.solution, colorMode(), &dtm, j.dim, voxelStyle());
}

/* the preview shows what legacy's does: the shape, the problem's result,
 * or the first solution's assembly */
void ImageExportController::rebuildPreview(void) {
  const puzzle_c & p = m_doc->session().puzzle();
  std::optional<Job> j;
  if (m_mode == QLatin1String("shape")) {
    if (canShape())
      j = Job{ false, unsigned(m_shape) };
  } else if (m_mode == QLatin1String("problem")) {
    if (canProblem() && p.getProblem(unsigned(m_problem))->resultValid())
      j = Job{ false, p.getProblem(unsigned(m_problem))->getResultId() };
  } else if (canAssembly()) {
    Job a;
    a.assembly = true;
    a.problem = unsigned(m_problem);
    j = a;
  }

  m_mesh.reset();
  if (j) {
    // reached from every option QML sets
    guarded([&] {
      btui::SceneContent c = sceneFor(*j);
      m_camera.setScene(c.centre, c.radius);
      m_mesh = std::make_shared<btui::ShapeMesh>(std::move(c.mesh));
    });
  }
  m_meshRevision++;
  emit contentChanged();
  emit frameChanged();
}

SceneFrame ImageExportController::frame(float devicePixelRatio) const {
  SceneFrame f = baseFrame(devicePixelRatio);
  if (m_mesh) {
    f.mesh = m_mesh;
    f.meshRevision = m_meshRevision;
  }
  return f;
}

// --- how ----------------------------------------------------------------------

void ImageExportController::setTransparent(bool t) {
  if (t == m_transparent) return;
  m_transparent = t;
  emit optionsChanged();
}

void ImageExportController::setSupersampling(int aa) {
  aa = std::clamp(aa, 1, 5);
  if (aa == m_aa) return;
  m_aa = aa;
  emit optionsChanged();
}

void ImageExportController::setConstraintColours(bool c) {
  if (c == m_constraint) return;
  m_constraint = c;
  emit optionsChanged();
  rebuildPreview();
}

void ImageExportController::setDimStatic(bool d) {
  if (d == m_dim) return;
  m_dim = d;
  emit optionsChanged();
}

// --- pages --------------------------------------------------------------------

/* legacy cb_SzUpdate: a paper size sets the millimetres, and millimetres
 * with a DPI set the pixels; an empty (0) field leaves its pixels alone */
void ImageExportController::recomputePixels(void) {
  if (m_mmX > 0 && m_dpi > 0) m_pxX = int(btui::pixelsFor(unsigned(m_mmX), unsigned(m_dpi)));
  if (m_mmY > 0 && m_dpi > 0) m_pxY = int(btui::pixelsFor(unsigned(m_mmY), unsigned(m_dpi)));
  emit sizeChanged();
}

void ImageExportController::setPaper(const QString & p) {
  btui::PaperSize s{ 0, 0 };
  if (p == QLatin1String("a4p")) s = btui::kA4Portrait;
  else if (p == QLatin1String("a4l")) s = btui::kA4Landscape;
  else if (p == QLatin1String("letterp")) s = btui::kLetterPortrait;
  else if (p == QLatin1String("letterl")) s = btui::kLetterLandscape;
  else if (p != QLatin1String("manual")) return;
  m_paper = p;
  if (s.widthMm) {
    m_mmX = int(s.widthMm);
    m_mmY = int(s.heightMm);
  }
  recomputePixels();
}

void ImageExportController::setSizeXmm(int v) { m_mmX = std::max(0, v); recomputePixels(); }
void ImageExportController::setSizeYmm(int v) { m_mmY = std::max(0, v); recomputePixels(); }
void ImageExportController::setDpi(int v) { m_dpi = std::max(0, v); recomputePixels(); }
void ImageExportController::setPixelX(int v) { if (v == m_pxX) return; m_pxX = std::max(1, v); emit sizeChanged(); }
void ImageExportController::setPixelY(int v) { if (v == m_pxY) return; m_pxY = std::max(1, v); emit sizeChanged(); }
void ImageExportController::setPages(int v) { if (v == m_pages) return; m_pages = std::max(1, v); emit sizeChanged(); }

QUrl ImageExportController::folder(void) const {
  // legacy: the puzzle's folder, else the home directory
  if (m_doc->session().fileName().empty())
    return QUrl::fromLocalFile(QDir::homePath());
  return m_doc->folder();
}

QString ImageExportController::suggestedName(void) const {
  if (m_doc->session().fileName().empty())
    return QStringLiteral("test.png");      // legacy's default name
  return QFileInfo(m_doc->fileName()).completeBaseName() + QStringLiteral(".png");
}

// --- the dialog -------------------------------------------------------------

void ImageExportController::begin(void) {
  // a fresh dialog has legacy's defaults
  m_mode = QStringLiteral("solution");
  m_shape = 0;
  m_problem = 0;
  m_transparent = m_constraint = m_dim = false;
  m_aa = 3;
  m_paper = QStringLiteral("manual");
  m_mmX = m_mmY = 0;
  m_dpi = 300;
  m_pxX = m_pxY = 300;
  m_pages = 1;
  m_progress.clear();
  m_camera.home();
  fixMode();
  emit optionsChanged();
  emit sizeChanged();
  emit progressChanged();
  rebuildPreview();
}

void ImageExportController::end(void) {
  cancel();
  m_renderer.reset();
  m_mesh.reset();
  m_meshRevision++;
  emit frameChanged();
}

// --- the export -------------------------------------------------------------

/* legacy cb_Export */
std::vector<ImageExportController::Job> ImageExportController::jobs(void) const {
  std::vector<Job> out;
  const puzzle_c & p = m_doc->session().puzzle();

  if (m_mode == QLatin1String("shape")) {
    if (canShape())
      out.push_back(Job{ false, unsigned(m_shape) });
    return out;
  }
  if (!canProblem())
    return out;
  const unsigned int prob = unsigned(m_problem);
  const problem_c * pr = p.getProblem(prob);

  if (m_mode == QLatin1String("problem")) {
    if (pr->resultValid())
      out.push_back(Job{ false, pr->getResultId() });
    for (unsigned int part = 0; part < pr->getNumberOfParts(); part++)
      out.push_back(Job{ false, pr->getShapeIdOfPart(part) });
    return out;
  }
  if (!canAssembly())
    return out;
  Job base;
  base.assembly = true;
  base.problem = prob;
  if (m_mode == QLatin1String("assembly")) {
    out.push_back(base);      // the first solution, as legacy exports it
    return out;
  }

  // the steps of the last solution's disassembly ("for the moment only for
  // the last solution", legacy says)
  base.solution = pr->getNumberOfSavedSolutions() - 1;
  const separation_c * t = pr->getSavedSolution(base.solution)->getDisassembly();
  if (!t)
    return out;
  const int moves = int(t->sumMoves());
  base.dim = m_dim;
  if (m_mode == QLatin1String("disassembly")) {
    for (int s = 0; s < moves; s++) {
      Job j = base;
      j.step = s;
      out.push_back(j);
    }
  } else {
    // the assembly: the disassembly backwards, ending assembled, undimmed
    for (int s = moves - 1; s > 0; s--) {
      Job j = base;
      j.step = s;
      out.push_back(j);
    }
    Job last = base;
    last.step = 0;
    last.dim = false;
    out.push_back(last);
  }
  return out;
}

/* One picture: drawn kWidthFactor times as wide as high, supersampled,
 * cropped to the columns with content and scaled down. */
QImage ImageExportController::drawPicture(const Job & j, int height, int aa) {
  static quint64 revision = 0;
  const btui::SceneContent c = sceneFor(j);
  const int h = height * aa, w = height * kWidthFactor * aa;

  btui::Camera cam;
  cam.setViewport(float(w), float(h));
  cam.setProjection(m_camera.projection());
  cam.setOrientation(m_camera.orientation());
  cam.setScene(c.centre, c.radius);

  SceneFrame f;
  f.clear = QColor(0, 0, 0, 0);
  f.view = cam.viewMatrix();
  f.projection = cam.projectionMatrix();
  f.mesh = std::make_shared<btui::ShapeMesh>(c.mesh);
  f.meshRevision = ++revision;
  f.lighting = m_settings->lighting();
  f.translucentLayers = voxelStyle() == btui::VoxelStyle::Flat;     // as the 3D view

  QImage img = m_renderer->render(f, QSize(w, h));
  if (img.isNull())
    return img;
  img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  img = img.copy(contentColumns(img, aa));
  if (aa > 1)
    img = img.scaled(img.width() / aa, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  return img;
}

void ImageExportController::start(const QUrl & file) {
  if (busy())
    return;
  QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
  if (path.isEmpty())
    return;
  if (path.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
    path.chop(4);
  m_base = path;
  m_written.clear();

  m_jobs = jobs();
  if (m_jobs.empty()) {
    stop(QString());
    return;
  }
  if (!m_renderer) {
    m_renderer = std::make_unique<OffscreenRenderer>();
    m_renderer->setPipelineCacheFile(PipelineCache::offscreenFile(m_settings->file()));
  }
  if (!m_renderer->isValid()) {
    stop(tr("Failed"));
    emit failed(tr("There is no graphics backend to draw the images with."));
    return;
  }
  m_ratios.clear();
  m_pictures.clear();
  m_next = 0;
  m_phase = Phase::Measure;
  m_progress = tr("create Preview image %1 / %2").arg(1).arg(m_jobs.size());
  emit progressChanged();
  m_timer.start();
}

void ImageExportController::cancel(void) {
  if (busy())
    stop(QString());
}

void ImageExportController::stop(const QString & text) {
  m_timer.stop();
  m_phase = Phase::Idle;
  m_jobs.clear();
  m_pictures.clear();
  m_progress = text;
  emit progressChanged();
}

void ImageExportController::step(void) {
  try {
    if (m_phase == Phase::Measure) {
      const QImage img = drawPicture(m_jobs[m_next], kMeasureHeight, 1);
      if (img.isNull()) {
        stop(tr("Failed"));
        emit failed(tr("The image could not be drawn."));
        return;
      }
      m_ratios.push_back(double(img.width()) / kMeasureHeight);
      if (++m_next < m_jobs.size()) {
        m_progress = tr("create Preview image %1 / %2").arg(m_next + 1).arg(m_jobs.size());
      } else {
        m_lineHeight = int(btui::planLineHeight(m_ratios, unsigned(m_pxX), unsigned(m_pxY), unsigned(m_pages)));
        m_next = 0;
        m_phase = Phase::Draw;
        m_progress = tr("create image %1 / %2").arg(1).arg(m_jobs.size());
      }
    } else if (m_phase == Phase::Draw) {
      const QImage img = drawPicture(m_jobs[m_next], m_lineHeight, m_aa);
      if (img.isNull()) {
        stop(tr("Failed"));
        emit failed(tr("The image could not be drawn."));
        return;
      }
      m_pictures.push_back(img);
      if (++m_next < m_jobs.size()) {
        m_progress = tr("create image %1 / %2").arg(m_next + 1).arg(m_jobs.size());
      } else {
        writePages();
        return;
      }
    } else {
      return;
    }
  } catch (const assert_exception & e) {
    stop(tr("Failed"));
    if (App * app = App::instance())
      app->handleInternalError(e);
    return;
  }
  emit progressChanged();
  m_timer.start();
}

void ImageExportController::writePages(void) {
  std::vector<unsigned int> widths;
  for (const QImage & p : m_pictures)
    widths.push_back(unsigned(p.width()));
  const auto places = btui::placePictures(widths, unsigned(m_pxX), unsigned(m_pxY), unsigned(m_lineHeight));
  const unsigned int pageCount = places.empty() ? 0 : places.back().page + 1;

  for (unsigned int page = 0; page < pageCount; page++) {
    QImage out(m_pxX, m_pxY, QImage::Format_ARGB32_Premultiplied);
    out.fill(m_transparent ? QColor(0, 0, 0, 0) : QColor(Qt::white));
    QPainter painter(&out);
    for (size_t i = 0; i < places.size(); i++)
      if (places[i].page == page)
        painter.drawImage(int(places[i].x), int(places[i].y), m_pictures[i]);
    painter.end();

    const QString name = m_base + QStringLiteral("%1.png").arg(page, 3, 10, QLatin1Char('0'));
    m_progress = tr("save page %1").arg(page);
    emit progressChanged();
    if (!out.save(name, "PNG")) {
      // one failed page fails the export: the rest would fail the same way
      stop(tr("Failed"));
      emit failed(tr("Could not write the image to\n%1\n\nCheck that the path exists and is writable.")
                    .arg(QDir::toNativeSeparators(name)));
      return;
    }
    m_written << name;
  }
  stop(tr("Done"));
  emit finished(int(pageCount), m_written.isEmpty() ? QString() : QFileInfo(m_written.first()).fileName());
}

void ImageExportController::finish(void) {
  while (busy()) {
    m_timer.stop();
    step();
  }
}
