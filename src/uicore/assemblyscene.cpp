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
#include "assemblyscene.h"

#include "../lib/assembly.h"
#include "../lib/disasmtomoves.h"
#include "../lib/problem.h"
#include "../lib/puzzle.h"
#include "../lib/solution.h"
#include "../lib/voxel.h"

#include <algorithm>
#include <limits>
#include <memory>

namespace btui {

  namespace {

    std::vector<Rgb> paletteOf(const puzzle_c & puz) {
      std::vector<Rgb> out;
      for (unsigned i = 0; i < puz.colorNumber(); i++) {
        unsigned char r, g, b;
        puz.getColor(i, &r, &g, &b);
        out.push_back({ r / 255.0f, g / 255.0f, b / 255.0f });
      }
      return out;
    }

    /* the content's bounding box decides what the camera frames */
    void frame(SceneContent & c) {
      Vec3 lo{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
      Vec3 hi = lo * -1.0f;
      bool any = false;
      for (const auto * list : { &c.mesh.opaque, &c.mesh.translucent })
        for (const auto & v : *list) {
          lo = { std::min(lo.x, v.pos[0]), std::min(lo.y, v.pos[1]), std::min(lo.z, v.pos[2]) };
          hi = { std::max(hi.x, v.pos[0]), std::max(hi.y, v.pos[1]), std::max(hi.z, v.pos[2]) };
          any = true;
        }
      if (!any) {
        lo = hi = Vec3{ 0, 0, 0 };
      }
      c.mesh.boundsMin = lo;
      c.mesh.boundsMax = hi;
      c.centre = (lo + hi) * 0.5f;
      c.radius = std::max(0.5f, length(hi - lo) * 0.5f);
    }

    /* legacy dimStaticPieces(): the colour moved 80 % of the way to white */
    std::uint8_t dimmed(std::uint8_t c) {
      return std::uint8_t(255 - (255 - c) * 2 / 10);
    }

    void append(ShapeMesh & dst, const ShapeMesh & src, Vec3 offset, float alpha, bool dim) {
      for (const auto * list : { &src.opaque, &src.translucent }) {
        const bool fromTranslucent = list == &src.translucent;
        for (MeshVertex v : *list) {
          v.pos[0] += offset.x;
          v.pos[1] += offset.y;
          v.pos[2] += offset.z;
          if (dim) {
            v.color.r = dimmed(v.color.r);
            v.color.g = dimmed(v.color.g);
            v.color.b = dimmed(v.color.b);
          }
          // the layer dimming must not touch an assembly: no cell
          v.cell[0] = v.cell[1] = v.cell[2] = -1e4f;
          if (alpha < 1.0f)
            v.color.a = std::uint8_t(std::clamp(v.color.a * alpha, 0.0f, 255.0f));
          (fromTranslucent || alpha < 1.0f ? dst.translucent : dst.opaque).push_back(v);
        }
      }
    }
  }

  SceneContent buildShapeScene(const puzzle_c & puz, unsigned int shape, ColorMode colors, VoxelStyle style) {
    SceneContent c;
    if (shape < puz.getNumberOfShapes()) {
      MeshOptions opt;
      opt.style = style;
      opt.colors = colors;
      opt.piece = pieceColor(int(shape));
      opt.palette = paletteOf(puz);
      c.mesh = buildShapeMesh(*puz.getShape(shape), opt);
      // the main view frames a shape on its grid box; so does this
      const Vec3 lo = c.mesh.boundsMin, hi = c.mesh.boundsMax;
      c.centre = (lo + hi) * 0.5f;
      c.radius = std::max(0.5f, length(hi - lo) * 0.5f);
    }
    return c;
  }

  /* legacy showAssembly() + updatePositions() + dimStaticPieces() */
  SceneContent buildAssemblyScene(const problem_c & prob, unsigned int solution, ColorMode colors,
                                  piecePositions_c * positions, bool dimStatic, VoxelStyle style) {
    SceneContent c;
    if (!prob.resultValid() || solution >= prob.getNumberOfSavedSolutions())
      return c;
    const assembly_c * assm = prob.getSavedSolution(solution)->getAssembly();
    if (!assm)
      return c;

    const puzzle_c & puz = prob.getPuzzle();
    const std::vector<Rgb> palette = paletteOf(puz);
    unsigned int piece = 0;

    for (unsigned int p = 0; p < prob.getNumberOfParts(); p++)
      for (unsigned int q = 0; q < prob.getPartMaximum(p); q++, piece++) {
        if (!assm->isPlaced(piece))
          continue;    // legacy gives an unplaced piece alpha 0

        std::unique_ptr<voxel_c> vx(puz.getGridType()->getVoxel(prob.getPartShape(p)));
        if (!vx->transform(assm->getTransformation(piece)))
          continue;

        float x, y, z, alpha = 1.0f;
        bool dim = false;
        if (positions) {
          x = positions->getX(piece);
          y = positions->getY(piece);
          z = positions->getZ(piece);
          alpha = positions->getA(piece);
          dim = dimStatic && !positions->moving(piece);
        } else {
          x = float(assm->getX(piece));
          y = float(assm->getY(piece));
          z = float(assm->getZ(piece));
        }
        if (alpha <= 0.0f)
          continue;

        // setSpacePosition() and the hotspot, both in drawing space
        vx->recalcSpaceCoordinates(&x, &y, &z);
        float hx = float(vx->getHx()), hy = float(vx->getHy()), hz = float(vx->getHz());
        vx->recalcSpaceCoordinates(&hx, &hy, &hz);

        MeshOptions opt;
        opt.style = style;
        opt.colors = colors;
        opt.piece = pieceColor(int(prob.getShapeIdOfPart(p)), int(q));
        opt.palette = palette;
        append(c.mesh, buildShapeMesh(*vx, opt), Vec3{ x - hx, y - hy, z - hz }, alpha, dim);
      }

    frame(c);
    return c;
  }
}
