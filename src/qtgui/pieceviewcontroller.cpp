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
#include "pieceviewcontroller.h"

#include "documentcontroller.h"
#include "guarded.h"
#include "layoutcontroller.h"
#include "puzzlecontroller.h"
#include "settingscontroller.h"
#include "theme.h"
#include "viewportcontroller.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <algorithm>

PieceViewController::PieceViewController(SettingsController * settings, DocumentController * doc, LayoutController * layout,
                                         PuzzleController * puzzle, ViewportController * main, QObject * parent) :
  SceneController(settings, parent), m_doc(doc), m_layout(layout), m_puzzle(puzzle), m_main(main)
{
  connect(m_layout, &LayoutController::changed, this, [this] {
    if (m_dirty)
      schedule();
  });
  // the shape, its voxels, how voxels look
  connect(m_puzzle, &PuzzleController::selectedPieceChanged, this, &PieceViewController::schedule);
  connect(m_doc, &DocumentController::documentReplaced, this, &PieceViewController::schedule);
  connect(m_doc, &DocumentController::historyApplied, this, &PieceViewController::schedule);
  connect(m_doc, &DocumentController::puzzleEdited, this, &PieceViewController::schedule);
  connect(m_settings, &SettingsController::changed, this, [this] {
    if (m_voxelStyle != m_meshStyle || m_main->colourViewValue() != m_meshColours)
      schedule();
  });
  if (const Theme * t = Theme::instance())
    connect(t, &Theme::shapeColoursChanged, this, &PieceViewController::schedule);

  // one orientation for both views
  m_camera.setOrientation(m_main->camera().orientation());
  connect(m_main, &SceneController::frameChanged, this, [this] { follow(m_main, this); });
  connect(this, &SceneController::frameChanged, this, [this] { follow(this, m_main); });
}

void PieceViewController::follow(const SceneController * from, SceneController * to) {
  to->followOrientation(from->camera().orientation());
}

void PieceViewController::schedule(void) {
  m_dirty = true;
  if (m_queued || m_layout->workspace() != LayoutController::Puzzle)
    return;
  m_queued = true;
  QMetaObject::invokeMethod(this, [this] {
    m_queued = false;
    build();
  }, Qt::QueuedConnection);
}

void PieceViewController::flush(void) {
  build();
}

void PieceViewController::build(void) {
  if (!m_dirty)
    return;
  m_dirty = false;
  const puzzle_c & p = m_doc->session().puzzle();
  const int shape = m_puzzle->selectedPiece();
  const bool had = hasPiece();
  m_mesh.reset();
  m_meshStyle = m_voxelStyle;
  m_meshColours = m_main->colourViewValue();
  if (shape >= 0 && unsigned(shape) < p.getNumberOfShapes())
    guarded([&] {
      btui::MeshOptions opt = ViewportController::meshOptions(p, m_meshStyle, m_meshColours);
      opt.piece = opt.shapeColours.of(shape);
      auto mesh = std::make_shared<btui::ShapeMesh>(btui::buildShapeMesh(*p.getShape(unsigned(shape)), opt));
      if (!mesh->empty()) {
        const btui::Vec3 c = (mesh->boundsMin + mesh->boundsMax) * 0.5f;
        m_camera.setScene(c, std::max(0.5f, btui::length(mesh->boundsMax - mesh->boundsMin) * 0.5f));
      }
      m_mesh = std::move(mesh);
      return true;
    });
  m_meshRevision++;
  if (had != hasPiece() || hasPiece())
    emit meshChanged();
  emit frameChanged();
}

SceneFrame PieceViewController::frame(float devicePixelRatio) const {
  SceneFrame f = baseFrame(devicePixelRatio);
  applyStyle(f, m_meshStyle);
  if (m_mesh) {
    f.mesh = m_mesh;
    f.meshRevision = m_meshRevision;
  }
  return f;
}
