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
#include <limits>
#include <map>

namespace btui {

  namespace {

    constexpr int kColumns = 5;           // C18: a row holds five of the largest pieces
    constexpr float kSlot = 1.2f;         // an object's slot: its width over the object's diameter
    constexpr float kRunMin = 0.65f;      // a run spans this much of a large slot at least: room for its name
    constexpr float kRowGap = 0.9f;       // between rows, of the largest radius: a run's bracket and name
    constexpr float kZoneGap = 1.5f;      // between the zones, of the largest radius: a caption and a heading

    /* a shape's mesh around its own centre */
    struct Prepared {
      ShapeMesh mesh;
      float radius = 0.5f;
    };

    Prepared prepare(const voxel_c & shape, int index, const MeshOptions & options) {
      MeshOptions o = options;
      o.piece = o.shapeColours.of(index);
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

    std::uint8_t faded(std::uint8_t a, float alpha) {
      return std::uint8_t(float(a) * alpha + 0.5f);
    }

    /* `scale` lifts a selected copy; `alpha` below 1 makes another piece
     * step back, its faces then among the translucent ones */
    void place(ShapeMesh & dst, const ShapeMesh & src, const SceneObject & o, float scale, float alpha) {
      for (const auto * list : { &src.opaque, &src.translucent })
        for (MeshVertex v : *list) {
          for (float & c : v.pos)
            c *= scale;
          v.cell[0] = o.anchor.x;
          v.cell[1] = o.anchor.y;
          v.cell[2] = o.anchor.z;
          // an optional copy is drawn as the others (product decision
          // 2026-11-14, not C18's translucent dashed copy): its run's name says it
          if (alpha < 1) {
            v.color.a = faded(v.color.a, alpha);
            dst.translucent.push_back(v);
          } else
            (list == &src.translucent ? dst.translucent : dst.opaque).push_back(v);
        }
      for (LineSeg s : src.edges) {
        s.a = s.a * scale;
        s.b = s.b * scale;
        s.color.a = faded(s.color.a, alpha);
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

    // zone 1: the goal
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

    const auto vertices = [&](int s) {
      const ShapeMesh & m = prepared(s).mesh;
      return m.opaque.size() + m.translucent.size();
    };
    size_t drawn = 0;
    for (const SceneObject & o : top)
      drawn += vertices(o.shape);

    // zone 2: the pieces, a group's members together
    struct Entry { int shape; PieceCount count; int group; };
    std::map<int, int> lowest;
    for (const Group & g : problemGroups(p, problem))
      for (const GroupMember & m : g.members)
        if (m.count > 0) {
          auto [at, added] = lowest.emplace(m.shape, g.id);
          if (!added)
            at->second = std::min(at->second, g.id);
        }
    std::vector<Entry> entries;
    for (int s = 0; s < int(p.getNumberOfShapes()); s++) {
      const PieceCount c = pieceCount(p, problem, s);
      if (c.max <= 0)
        continue;
      const auto g = lowest.find(s);
      entries.push_back({ s, c, g == lowest.end() ? 0 : g->second });
      scene.shapes++;
      scene.copies += c.max;
      scene.optional += c.max - c.min;
      if (s == selected)
        scene.emphasis = true;
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry & a, const Entry & b) {
      const auto key = [](const Entry & e) { return e.group > 0 ? e.group : std::numeric_limits<int>::max(); };
      return key(a) < key(b);
    });

    float pieceRadius = 0.5f;
    for (const Entry & e : entries)
      pieceRadius = std::max(pieceRadius, prepared(e.shape).radius);
    const float big = 2 * pieceRadius * kSlot;
    const float maxWidth = kColumns * big;
    const float runMin = kRunMin * big;
    constexpr float slack = 1e-4f;

    /* each slot as wide as its own shape needs, so that small pieces pack
     * close; a run goes to the next row whole when it fits there but not in
     * what is left of this one */
    std::vector<SceneObject> pieces;
    std::vector<int> rowOf;
    float x = 0;
    int row = 0;
    for (const Entry & e : entries) {
      const int n = e.count.max;
      const float w = 2 * prepared(e.shape).radius * kSlot;
      if (x > 0 && x + std::min(std::max(float(n) * w, runMin), maxWidth) > maxWidth + slack) {
        row++;
        x = 0;
      }
      int run = -1;
      for (int k = 0; k < n; k++) {
        if (int(pieces.size()) >= maxSceneObjects || drawn + vertices(e.shape) > maxSceneVertices) {
          scene.hidden += n - k;
          break;
        }
        if (x > 0 && x + w > maxWidth + slack) {
          row++;
          x = 0;
          run = -1;
        }
        if (run < 0) {
          SceneRun r;
          r.shape = e.shape;
          r.first = int(pieces.size());
          r.row = row;
          r.continued = k > 0;
          r.group = e.group;
          scene.runs.push_back(r);
          run = int(scene.runs.size()) - 1;
        }
        drawn += vertices(e.shape);
        SceneObject o;
        o.shape = e.shape;
        o.copy = k;
        o.copies = n;
        o.optional = k >= e.count.min;
        o.run = run;
        o.radius = prepared(e.shape).radius * (scene.emphasis && e.shape == selected ? selectedLift : 1.0f);
        o.anchor = Vec3{ x + w / 2, 0, 0 };
        x += w;
        scene.runs[size_t(run)].count++;
        pieces.push_back(o);
        rowOf.push_back(row);
      }
      // a short run is widened, its copies centred in it, to make room for its name
      if (run >= 0) {
        const SceneRun & r = scene.runs[size_t(run)];
        const float width = float(r.count) * w;
        if (width < runMin) {
          for (int i = r.first; i < r.first + r.count; i++)
            pieces[size_t(i)].anchor.x += (runMin - width) / 2;
          x += runMin - width;
        }
      }
    }

    // rows top down, each as tall as its largest piece; the goal above them
    const int rows = pieces.empty() ? 0 : rowOf.back() + 1;
    std::vector<float> rowRadius(size_t(rows), 0.5f);
    float width = 0;
    for (size_t i = 0; i < pieces.size(); i++) {
      const size_t r = size_t(rowOf[i]);
      rowRadius[r] = std::max(rowRadius[r], pieces[i].radius);
      width = std::max(width, pieces[i].anchor.x + pieces[i].radius * kSlot);
    }
    float topRadius = 0.5f;
    for (const SceneObject & o : top)
      topRadius = std::max(topRadius, o.radius);
    const float unit = std::max(topRadius, pieceRadius);

    float y = 0;
    if (!top.empty()) {
      const float pitch = 2 * topRadius * kSlot;
      for (size_t i = 0; i < top.size(); i++)
        top[i].anchor = Vec3{ (float(i) - float(top.size() - 1) / 2) * pitch, -topRadius, 0 };
      y = -2 * topRadius - (rows > 0 ? kZoneGap * unit : 0.0f);
    }
    std::vector<float> rowY(static_cast<size_t>(rows));
    for (int r = 0; r < rows; r++) {
      rowY[size_t(r)] = y - rowRadius[size_t(r)];
      y -= 2 * rowRadius[size_t(r)] + kRowGap * pieceRadius;
    }
    // the last row's names below it
    const float height = -(rows > 0 ? y + kRowGap * pieceRadius / 2 : y);

    // rows start at the left, the block of them centred; the whole centred on the origin
    for (size_t i = 0; i < pieces.size(); i++) {
      pieces[i].anchor.x -= width / 2;
      pieces[i].anchor.y = rowY[size_t(rowOf[i])];
    }
    if (!top.empty())
      width = std::max(width, float(top.size()) * 2 * topRadius * kSlot);
    for (SceneObject & o : top)
      o.anchor.y += height / 2;
    for (SceneObject & o : pieces)
      o.anchor.y += height / 2;
    scene.radius = std::max({ 0.5f, width / 2, height / 2 });

    for (SceneRun & r : scene.runs)
      r.first += int(top.size());
    scene.objects = std::move(top);
    scene.objects.insert(scene.objects.end(), pieces.begin(), pieces.end());
    size_t opaque = 0, translucent = 0, edges = 0;
    for (const SceneObject & o : scene.objects) {
      const ShapeMesh & m = prepared(o.shape).mesh;
      opaque += m.opaque.size();
      translucent += m.translucent.size();
      edges += m.edges.size();
    }
    scene.mesh.opaque.reserve(opaque);
    scene.mesh.translucent.reserve(translucent);
    scene.mesh.edges.reserve(edges);
    for (const SceneObject & o : scene.objects) {
      const bool piece = o.role == SceneRole::Piece;
      const bool mine = piece && scene.emphasis && o.shape == selected;
      place(scene.mesh, prepared(o.shape).mesh, o, mine ? selectedLift : 1.0f,
            piece && scene.emphasis && !mine ? othersAlpha : 1.0f);
    }
    scene.mesh.boundsMin = Vec3{ -scene.radius, -scene.radius, -scene.radius };
    scene.mesh.boundsMax = Vec3{ scene.radius, scene.radius, scene.radius };
    return scene;
  }
}
