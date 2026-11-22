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
#include "drawcolours.h"

#include "../lib/problem.h"
#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <iterator>

namespace btui {

  namespace {
    bool isUserColour(const puzzle_c & p, int id) {
      return id >= 1 && unsigned(id) <= p.colorNumber();
    }

    // legacy mainWindow_c::changeColor: a shape with any voxel of the colour
    bool shapeHasColour(const voxel_c & v, unsigned int id) {
      for (unsigned int j = 0; j < v.getXYZ(); j++)
        if (v.getColor(j) == id)
          return true;
      return false;
    }

    template <class F> void forProblemsUsingColour(const puzzle_c & p, unsigned int id, F && f) {
      for (unsigned int s = 0; s < p.getNumberOfShapes(); s++)
        if (shapeHasColour(*p.getShape(s), id))
          for (unsigned int k = 0; k < p.getNumberOfProblems(); k++)
            if (p.getProblem(k)->usesShape(s))
              f(k);
    }
  }

  std::string colourName(int id) {
    return id <= 0 ? std::string("Default") : "C" + std::to_string(id);
  }

  int colourCount(const puzzle_c & p) {
    return int(p.colorNumber()) + 1;
  }

  bool canAddColour(const puzzle_c & p) {
    return int(p.colorNumber()) < maxColours;
  }

  Rgb8 colourRgb(const puzzle_c & p, int id) {
    if (!isUserColour(p, id))
      return {};
    unsigned char r, g, b;
    p.getColor(unsigned(id) - 1, &r, &g, &b);
    return { r, g, b };
  }

  Rgb8 presetColour(const puzzle_c & p) {
    // the mock's five (red, cyan, pink, lime, orange), then violet, yellow,
    // teal, blue, brown
    static constexpr Rgb8 presets[] = {
      { 0xef, 0x44, 0x44 }, { 0x06, 0xb6, 0xd4 }, { 0xec, 0x48, 0x99 }, { 0x84, 0xcc, 0x16 },
      { 0xf9, 0x73, 0x16 }, { 0x8b, 0x5c, 0xf6 }, { 0xea, 0xb3, 0x08 }, { 0x14, 0xb8, 0xa6 },
      { 0x3b, 0x82, 0xf6 }, { 0xa1, 0x62, 0x07 },
    };
    for (const Rgb8 & c : presets) {
      bool used = false;
      for (int id = 1; id < colourCount(p) && !used; id++)
        used = colourRgb(p, id) == c;
      if (!used)
        return c;
    }
    return presets[p.colorNumber() % std::size(presets)];
  }

  int addColour(puzzle_c & p, Rgb8 c) {
    if (!canAddColour(p))
      return -1;
    p.addColor(c.r, c.g, c.b);
    // pieces of the new colour may go into cells of it (legacy cb_AddColor)
    const unsigned int id = p.colorNumber();
    for (unsigned int k = 0; k < p.getNumberOfProblems(); k++)
      p.getProblem(k)->allowPlacement(id, id);
    return int(id);
  }

  bool editColour(puzzle_c & p, int id, Rgb8 c) {
    if (!isUserColour(p, id) || colourRgb(p, id) == c)
      return false;
    p.changeColor(unsigned(id) - 1, c.r, c.g, c.b);
    return true;
  }

  bool removeColourLosesSolutions(const puzzle_c & p, int id) {
    if (!isUserColour(p, id))
      return false;
    bool loses = false;
    forProblemsUsingColour(p, unsigned(id), [&](unsigned int k) {
      const problem_c * pr = p.getProblem(k);
      const solveState_e st = pr->getSolveState();
      if (pr->getNumberOfSavedSolutions() > 0 || st == SS_SOLVING || st == SS_SOLVED)
        loses = true;
    });
    return loses;
  }

  bool removeColour(puzzle_c & p, int id) {
    if (!isUserColour(p, id))
      return false;
    forProblemsUsingColour(p, unsigned(id), [&](unsigned int k) { p.getProblem(k)->removeAllSolutions(); });
    p.removeColor(unsigned(id));
    return true;
  }
}
