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
#include "solutionscene.h"

#include "../lib/assembly.h"
#include "../lib/disasmtomoves.h"
#include "../lib/gridtype.h"
#include "../lib/problem.h"
#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <tuple>

namespace btui {

  namespace {

    /* a vertex's place on a 1/1024 lattice: the meshes' corners are lattice
     * points or simple fractions of them, so equal corners meet exactly */
    using Key = std::tuple<long, long, long>;
    Key keyOf(const float * p) {
      return { std::lround(p[0] * 1024.0f), std::lround(p[1] * 1024.0f), std::lround(p[2] * 1024.0f) };
    }


    std::uint8_t scaled(std::uint8_t c, float f) {
      return std::uint8_t(std::clamp(c * f + 0.5f, 0.0f, 255.0f));
    }
  }

  float wireCreaseAngle(int gridType) {
    return gridType == gridType_c::GT_SPHERES ? 25.0f : 2.5f;
  }

  std::vector<HullEdge> hullEdges(const ShapeMesh & mesh, float creaseDeg) {
    const float sameCos = std::cos(creaseDeg * 3.14159265f / 180.0f);
    auto sameDirection = [sameCos](Vec3 a, Vec3 b) { return dot(a, b) > sameCos; };
    struct Seen {
      Vec3 a, b;
      std::vector<Vec3> normals;
    };
    std::map<std::pair<Key, Key>, Seen> edges;
    for (const auto * list : { &mesh.opaque, &mesh.translucent })
      for (size_t t = 0; t + 2 < list->size(); t += 3) {
        const MeshVertex * v = &(*list)[t];
        const Vec3 p[3] = { { v[0].pos[0], v[0].pos[1], v[0].pos[2] },
                            { v[1].pos[0], v[1].pos[1], v[1].pos[2] },
                            { v[2].pos[0], v[2].pos[1], v[2].pos[2] } };
        const Vec3 n = normalize(cross(p[1] - p[0], p[2] - p[0]));
        if (length(n) == 0)
          continue;                       // a degenerate triangle has no side
        for (int e = 0; e < 3; e++) {
          const int i = e, j = (e + 1) % 3;
          Key ki = keyOf(v[i].pos), kj = keyOf(v[j].pos);
          Vec3 a = p[i], b = p[j];
          if (kj < ki) {
            std::swap(ki, kj);
            std::swap(a, b);
          }
          Seen & s = edges[{ ki, kj }];
          if (s.normals.empty()) {
            s.a = a;
            s.b = b;
          }
          s.normals.push_back(n);
        }
      }

    std::vector<HullEdge> out;
    for (const auto & [key, s] : edges) {
      // a crease: some face here faces another way than the first
      const Vec3 n1 = s.normals.front();
      Vec3 n2 = n1;
      bool crease = s.normals.size() == 1;
      for (const Vec3 & n : s.normals)
        if (!sameDirection(n, n1)) {
          n2 = n;
          crease = true;
          break;
        }
      if (crease)
        out.push_back({ s.a, s.b, n1, n2 });
    }
    return out;
  }

  struct SolutionScene::Piece {
    int number = 0;                  ///< in the assembly
    std::unique_ptr<voxel_c> voxel;  ///< the shape as placed, for drawing-space positions
    Vec3 hotspot{};                  ///< in drawing space
    Vec3 at{};                       ///< where the assembly puts the mesh's origin
    ShapeMesh mesh;                  ///< at its place in the assembly
    std::vector<HullEdge> edges;     ///< the same
    Rgba8 colour;                    ///< the wireframe's lines
  };

  SolutionScene::SolutionScene(void) = default;
  SolutionScene::~SolutionScene(void) = default;
  SolutionScene::SolutionScene(SolutionScene &&) noexcept = default;
  SolutionScene & SolutionScene::operator=(SolutionScene &&) noexcept = default;

  SolutionScene SolutionScene::build(const problem_c & prob, const assembly_c * assm, const MeshOptions & options) {
    SolutionScene scene;
    if (!assm || !prob.resultValid())
      return scene;
    const puzzle_c & puz = prob.getPuzzle();

    Vec3 lo{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
    Vec3 hi = lo * -1.0f;
    unsigned int number = 0;
    for (unsigned p = 0; p < prob.getNumberOfParts(); p++)
      for (unsigned q = 0; q < prob.getPartMaximum(p); q++, number++) {
        if (number >= assm->placementCount() || !assm->isPlaced(number))
          continue;
        auto piece = std::make_unique<Piece>();
        piece->number = int(number);
        piece->voxel.reset(puz.getGridType()->getVoxel(prob.getPartShape(p)));
        if (!piece->voxel->transform(assm->getTransformation(number)))
          continue;
        float hx = float(piece->voxel->getHx()), hy = float(piece->voxel->getHy()), hz = float(piece->voxel->getHz());
        piece->voxel->recalcSpaceCoordinates(&hx, &hy, &hz);
        piece->hotspot = { hx, hy, hz };

        // at its place: legacy setSpacePosition() less the hotspot
        float x = float(assm->getX(number)), y = float(assm->getY(number)), z = float(assm->getZ(number));
        piece->voxel->recalcSpaceCoordinates(&x, &y, &z);
        const Vec3 at = Vec3{ x, y, z } - piece->hotspot;
        piece->at = at;

        MeshOptions opt = options;
        const int shape = int(prob.getShapeIdOfPart(p));
        const Rgb c = opt.shapeColours.of(shape, int(q));
        opt.piece = c;
        piece->mesh = buildShapeMesh(*piece->voxel, opt);
        // the wireframe from the plain faces, whatever the voxel style
        MeshOptions flat = opt;
        flat.style = VoxelStyle::Flat;
        flat.variableAlpha = 1;
        piece->edges = hullEdges(opt.style == VoxelStyle::Flat ? piece->mesh : buildShapeMesh(*piece->voxel, flat),
                                 wireCreaseAngle(int(puz.getGridType()->getType())));
        piece->colour = { scaled(255, c.r), scaled(255, c.g), scaled(255, c.b), 255 };

        for (auto * list : { &piece->mesh.opaque, &piece->mesh.translucent })
          for (MeshVertex & v : *list) {
            v.pos[0] += at.x;
            v.pos[1] += at.y;
            v.pos[2] += at.z;
            // the layer dimming must not touch an assembly
            v.cell[0] = v.cell[1] = v.cell[2] = -1e4f;
            lo = { std::min(lo.x, v.pos[0]), std::min(lo.y, v.pos[1]), std::min(lo.z, v.pos[2]) };
            hi = { std::max(hi.x, v.pos[0]), std::max(hi.y, v.pos[1]), std::max(hi.z, v.pos[2]) };
          }
        for (LineSeg & e : piece->mesh.edges) {
          e.a = e.a + at;
          e.b = e.b + at;
        }
        for (HullEdge & e : piece->edges) {
          e.a = e.a + at;
          e.b = e.b + at;
        }
        scene.pieces.push_back(std::move(piece));
      }

    if (!scene.pieces.empty()) {
      scene.centre = (lo + hi) * 0.5f;
      scene.radius = std::max(0.5f, length(hi - lo) * 0.5f);
    }
    return scene;
  }

  void SolutionScene::compose(ShapeMesh & mesh, std::vector<HullEdge> & wire, std::vector<Rgba8> & wireColours,
                              piecePositions_c * positions, const std::vector<PieceState> & states) const {
    mesh.opaque.clear();
    mesh.translucent.clear();
    mesh.edges.clear();
    wire.clear();
    wireColours.clear();
    Vec3 lo{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
    Vec3 hi = lo * -1.0f;

    for (const auto & piece : pieces) {
      const PieceState state = size_t(piece->number) < states.size() ? states[size_t(piece->number)] : PieceState::Solid;
      if (state == PieceState::Hidden)
        continue;

      Vec3 offset{ 0, 0, 0 };
      float alpha = 1;
      if (positions) {
        const unsigned n = unsigned(piece->number);
        alpha = positions->getA(n);
        if (alpha <= 0)
          continue;
        // where the disassembly has it, less where the assembly does
        float x = positions->getX(n), y = positions->getY(n), z = positions->getZ(n);
        piece->voxel->recalcSpaceCoordinates(&x, &y, &z);
        // the mesh lies at the assembled place already: move by the difference
        const Vec3 now = Vec3{ x, y, z } - piece->hotspot;
        offset = now - piece->at;
      }

      const bool wireframe = state == PieceState::Wireframe;
      for (const auto * list : { &piece->mesh.opaque, &piece->mesh.translucent }) {
        const bool fromTranslucent = list == &piece->mesh.translucent;
        for (MeshVertex v : *list) {
          v.pos[0] += offset.x;
          v.pos[1] += offset.y;
          v.pos[2] += offset.z;
          const float a = wireframe ? wireTint * alpha : alpha;
          if (a < 1.0f)
            v.color.a = scaled(v.color.a, a);
          lo = { std::min(lo.x, v.pos[0]), std::min(lo.y, v.pos[1]), std::min(lo.z, v.pos[2]) };
          hi = { std::max(hi.x, v.pos[0]), std::max(hi.y, v.pos[1]), std::max(hi.z, v.pos[2]) };
          (fromTranslucent || a < 1.0f ? mesh.translucent : mesh.opaque).push_back(v);
        }
      }
      if (!wireframe)
        for (LineSeg e : piece->mesh.edges) {
          e.a = e.a + offset;
          e.b = e.b + offset;
          if (alpha < 1.0f)
            e.color.a = scaled(e.color.a, alpha);
          mesh.edges.push_back(e);
        }
      else {
        Rgba8 c = piece->colour;
        c.a = scaled(c.a, alpha);
        for (HullEdge e : piece->edges) {
          e.a = e.a + offset;
          e.b = e.b + offset;
          wire.push_back(e);
          wireColours.push_back(c);
        }
      }
    }
    if (lo.x <= hi.x) {
      mesh.boundsMin = lo;
      mesh.boundsMax = hi;
    } else
      mesh.boundsMin = mesh.boundsMax = centre;
  }
}
