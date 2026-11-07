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
#include "puzzlescene.h"

#include "palette.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace btui {

  namespace {

    constexpr int kColumns = 5;           // C18: pieces five to a row
    constexpr float kSpacing = 1.3f;      // an object's width and the room for its caption
    constexpr float kOptionalAlpha = 0.45f;

    /* a shape's mesh around its own centre */
    struct Prepared {
      ShapeMesh mesh;
      float radius = 0.5f;
    };

    Prepared prepare(const voxel_c & shape, int index, const MeshOptions & options) {
      MeshOptions o = options;
      o.piece = pieceColor(index);
      Prepared out;
      out.mesh = buildShapeMesh(shape, o);
      if (out.mesh.empty())
        return out;
      const Vec3 centre = (out.mesh.boundsMin + out.mesh.boundsMax) * 0.5f;
      out.radius = std::max(0.5f, length(out.mesh.boundsMax - out.mesh.boundsMin) * 0.5f);
      for (auto * list : { &out.mesh.opaque, &out.mesh.translucent })
        for (MeshVertex & v : *list) {
          v.pos[0] -= centre.x;
          v.pos[1] -= centre.y;
          v.pos[2] -= centre.z;
        }
      for (LineSeg & s : out.mesh.edges) {
        s.a = s.a - centre;
        s.b = s.b - centre;
      }
      return out;
    }

    void place(ShapeMesh & dst, const ShapeMesh & src, const SceneObject & o) {
      for (const auto * list : { &src.opaque, &src.translucent })
        for (MeshVertex v : *list) {
          v.cell[0] = o.anchor.x;
          v.cell[1] = o.anchor.y;
          v.cell[2] = o.anchor.z;
          if (o.optional) {
            v.color.a = std::uint8_t(std::lround(v.color.a * kOptionalAlpha));
            v.edge[3] = 1.0f;              // its outline dashed
          }
          (o.optional || list == &src.translucent ? dst.translucent : dst.opaque).push_back(v);
        }
      for (LineSeg s : src.edges) {
        s.offset = o.anchor;
        dst.edges.push_back(s);
      }
    }
  }

  PuzzleScene buildPuzzleScene(const puzzle_c & p, int problem, int selected, const MeshOptions & options) {
    PuzzleScene scene;
    const std::vector<GoalSlot> goal = goalSlots(p, problem);
    if (problem < 0 || unsigned(problem) >= p.getNumberOfProblems())
      return scene;

    std::map<int, Prepared> shapes;
    const auto prepared = [&](int s) -> const Prepared & {
      auto at = shapes.find(s);
      if (at == shapes.end())
        at = shapes.emplace(s, prepare(*p.getShape(unsigned(s)), s, options)).first;
      return at->second;
    };

    // the top row: the goal, then the selected piece
    std::vector<SceneObject> top;
    for (const GoalSlot & slot : goal)
      if (slot.shape >= 0) {
        SceneObject o;
        o.role = SceneRole::Goal;
        o.goal = slot.role;
        o.shape = slot.shape;
        o.radius = prepared(slot.shape).radius;
        top.push_back(o);
      }
    if (selected >= 0 && unsigned(selected) < p.getNumberOfShapes() && !isGoalShape(p, problem, selected)) {
      SceneObject o;
      o.role = SceneRole::Selected;
      o.shape = selected;
      o.radius = prepared(selected).radius;
      top.push_back(o);
    }

    const auto vertices = [&](int s) {
      const ShapeMesh & m = prepared(s).mesh;
      return m.opaque.size() + m.translucent.size();
    };
    size_t drawn = 0;
    for (const SceneObject & o : top)
      drawn += vertices(o.shape);

    // below it, one object per copy, in shape order, then copy order
    std::vector<SceneObject> pieces;
    for (int s = 0; s < int(p.getNumberOfShapes()); s++) {
      const PieceCount c = pieceCount(p, problem, s);
      for (int k = 0; k < c.max; k++) {
        if (int(pieces.size()) >= maxSceneObjects || drawn + vertices(s) > maxSceneVertices) {
          scene.hidden += c.max - k;
          break;
        }
        drawn += vertices(s);
        SceneObject o;
        o.shape = s;
        o.copy = k;
        o.copies = c.max;
        o.optional = k >= c.min;
        o.radius = prepared(s).radius;
        pieces.push_back(o);
      }
    }

    float topRadius = 0.5f, pieceRadius = 0.5f;
    for (const SceneObject & o : top)
      topRadius = std::max(topRadius, o.radius);
    for (const SceneObject & o : pieces)
      pieceRadius = std::max(pieceRadius, o.radius);

    const float topPitch = 2 * topRadius * kSpacing;
    const float pitch = 2 * pieceRadius * kSpacing;
    const int rows = (int(pieces.size()) + kColumns - 1) / kColumns;
    const float height = (top.empty() ? 0 : topPitch) + float(rows) * pitch;
    float y = height / 2;

    const auto lay = [](std::vector<SceneObject> & row, int from, int n, float pitchX, float at) {
      for (int i = 0; i < n; i++)
        row[size_t(from + i)].anchor = Vec3{ (float(i) - float(n - 1) / 2) * pitchX, at, 0 };
    };
    if (!top.empty()) {
      lay(top, 0, int(top.size()), topPitch, y - topPitch / 2);
      y -= topPitch;
    }
    for (int r = 0; r < rows; r++) {
      const int from = r * kColumns;
      lay(pieces, from, std::min(kColumns, int(pieces.size()) - from), pitch, y - pitch / 2);
      y -= pitch;
    }

    const float width = std::max(float(top.size()) * topPitch, float(std::min(kColumns, int(pieces.size()))) * pitch);
    scene.radius = std::max({ 0.5f, width / 2, height / 2 });

    scene.objects = std::move(top);
    scene.objects.insert(scene.objects.end(), pieces.begin(), pieces.end());
    size_t opaque = 0, translucent = 0, edges = 0;
    for (const SceneObject & o : scene.objects) {
      const ShapeMesh & m = prepared(o.shape).mesh;
      (o.optional ? translucent : opaque) += m.opaque.size();
      translucent += m.translucent.size();
      edges += m.edges.size();
    }
    scene.mesh.opaque.reserve(opaque);
    scene.mesh.translucent.reserve(translucent);
    scene.mesh.edges.reserve(edges);
    for (const SceneObject & o : scene.objects)
      place(scene.mesh, prepared(o.shape).mesh, o);
    scene.mesh.boundsMin = Vec3{ -scene.radius, -scene.radius, -scene.radius };
    scene.mesh.boundsMax = Vec3{ scene.radius, scene.radius, scene.radius };
    return scene;
  }
}
