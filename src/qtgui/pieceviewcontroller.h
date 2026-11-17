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
#ifndef BTQT_PIECEVIEWCONTROLLER_H
#define BTQT_PIECEVIEWCONTROLLER_H

#include "scenecontroller.h"

#include "../uicore/scenemesh.h"

#include <memory>

class DocumentController;
class LayoutController;
class PuzzleController;
class ViewportController;

/* The Selected piece card's view of the piece (C17; product decision
 * 2026-11-17, which took the piece out of the 3D view's layout): the
 * selected shape alone, turned as the 3D view is. Dragging it turns the 3D
 * view, and the 3D view turns it: the two share one orientation, so what
 * the card shows is what the copies in the scene show. Its zoom and pan are
 * its own. */
class PieceViewController : public SceneController {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  // a piece is selected and has voxels to show
  Q_PROPERTY(bool hasPiece READ hasPiece NOTIFY meshChanged FINAL)

public:

  PieceViewController(SettingsController * settings, DocumentController * doc, LayoutController * layout,
                      PuzzleController * puzzle, ViewportController * main, QObject * parent = nullptr);

  bool hasPiece(void) const { return m_mesh && !m_mesh->empty(); }

  /* the mesh as it is now, built at once when a change is still queued */
  void flush(void);

  SceneFrame frame(float devicePixelRatio) const override;

signals:

  void meshChanged(void);

private:

  /* built while Puzzle is on screen, which alone shows it; a change
   * elsewhere waits for it */
  void schedule(void);
  void build(void);
  /* one orientation for both views: `to` takes `from`'s, when they differ */
  static void follow(const SceneController * from, SceneController * to);

  DocumentController * m_doc;
  LayoutController * m_layout;
  PuzzleController * m_puzzle;
  ViewportController * m_main;

  std::shared_ptr<const btui::ShapeMesh> m_mesh;
  quint64 m_meshRevision = 0;
  btui::VoxelStyle m_meshStyle = btui::VoxelStyle::Flat;
  btui::ColorMode m_meshColours = btui::ColorMode::Piece;
  bool m_dirty = true;
  bool m_queued = false;
};

#endif
