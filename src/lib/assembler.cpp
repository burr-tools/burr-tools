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
#include "assembler.h"

#include "problem.h"
#include "puzzle.h"
#include "voxel.h"

#include "../tools/xml.h"

#include <algorithm>

placementFinder_c::placementFinder_c(const problem_c & problem, const voxel_c * result) :
  prob(problem), res(result)
{
  colorTableWidth = problem.getPuzzle().colorNumber() + 1;

  if (colorTableWidth > 1) {
    colorAllowed.resize(colorTableWidth * colorTableWidth);
    for (unsigned int p = 0; p < colorTableWidth; p++)
      for (unsigned int r = 0; r < colorTableWidth; r++)
        colorAllowed[p * colorTableWidth + r] = problem.placementAllowed(p, r) ? 1 : 0;
  }

  /* all non empty voxels of the result, these are the only voxels a filled
   * piece voxel may end up on, so they are the only candidate positions for
   * the anchor voxel of a piece
   */
  unsigned int idx = 0;
  for (unsigned int z = 0; z < result->getZ(); z++)
    for (unsigned int y = 0; y < result->getY(); y++)
      for (unsigned int x = 0; x < result->getX(); x++, idx++)
        if (result->getState(idx) != voxel_c::VX_EMPTY) {
          resultVoxel_s e;
          e.x = x;
          e.y = y;
          e.z = z;
          e.index = idx;
          e.color = result->getColor(idx);
          resultVoxels.push_back(e);
        }
}

void placementFinder_c::find(const voxel_c * rotation,
    std::vector<long> & voxelOffsets,
    std::vector<int> & placements) const
{
  voxelOffsets.clear();
  placements.clear();

  /* the placement window: the same translations the brute force scan tried,
   * the bounding box of the piece stays within the bounding box of the result
   */
  const int wx1 = (int)res->boundX1()-(int)rotation->boundX1();
  const int wx2 = (int)res->boundX2()-(int)rotation->boundX2();
  const int wy1 = (int)res->boundY1()-(int)rotation->boundY1();
  const int wy2 = (int)res->boundY2()-(int)rotation->boundY2();
  const int wz1 = (int)res->boundZ1()-(int)rotation->boundZ1();
  const int wz2 = (int)res->boundZ2()-(int)rotation->boundZ2();

  if ((wx2 < wx1) || (wy2 < wy1) || (wz2 < wz1))
    return;

  const long sx = res->getX();
  const long sy = res->getY();

  /* the filled voxels of the piece as result space index offsets relative to
   * the translation, in the order the matrix nodes are emitted
   */
  std::vector<unsigned char> voxelColor;
  std::vector<long> checkOffsets;         // colour-only checks for cells that are not filled
  std::vector<unsigned char> checkColor;

  /* the anchor is the first filled voxel of the piece. Note that its
   * coordinates can not be recovered from its index offset because the piece
   * voxel space may be bigger than the result voxel space, so they are
   * recorded here directly */
  int ax = 0, ay = 0, az = 0;

  for (unsigned int pz = rotation->boundZ1(); pz <= rotation->boundZ2(); pz++)
    for (unsigned int py = rotation->boundY1(); py <= rotation->boundY2(); py++)
      for (unsigned int px = rotation->boundX1(); px <= rotation->boundX2(); px++) {
        if (rotation->getState(px, py, pz) == voxel_c::VX_FILLED) {
          if (voxelOffsets.empty()) {
            ax = px;
            ay = py;
            az = pz;
          }
          voxelOffsets.push_back(px + sx * (py + sy * pz));
          voxelColor.push_back(rotation->getColor(px, py, pz));
        } else if ((colorTableWidth > 1) && (rotation->getColor(px, py, pz) != 0)) {
          /* a coloured cell that is not filled can not occupy a result voxel
           * but it still takes part in the colour constraint check */
          checkOffsets.push_back(px + sx * (py + sy * pz));
          checkColor.push_back(rotation->getColor(px, py, pz));
        }
      }

  if (voxelOffsets.empty())
    return;

  const long anchorOff = voxelOffsets[0];

  const unsigned int voxels = voxelOffsets.size();
  const unsigned int checks = checkOffsets.size();

  /* checks whether the piece fits at the translation with the given result
   * space base index, the state of the anchor voxel has already been tested
   * by the caller */
  auto fits = [&](long base, unsigned char anchorResColor) -> bool {

    if (colorTableWidth > 1 && !colorAllowed[voxelColor[0] * colorTableWidth + anchorResColor])
      return false;

    for (unsigned int v = 1; v < voxels; v++) {
      const unsigned int ri = base + voxelOffsets[v];
      if (res->getState(ri) == voxel_c::VX_EMPTY)
        return false;
      if (colorTableWidth > 1 && !colorAllowed[voxelColor[v] * colorTableWidth + res->getColor(ri)])
        return false;
    }

    for (unsigned int c = 0; c < checks; c++)
      if (!colorAllowed[checkColor[c] * colorTableWidth + res->getColor(base + checkOffsets[c])])
        return false;

    return true;
  };

  const unsigned long windowVol =
    (unsigned long)(wx2-wx1+1) * (wy2-wy1+1) * (wz2-wz1+1);

  if (resultVoxels.size() < windowVol) {

    /* the result is sparse within its bounding box: only try translations
     * that put the anchor onto one of the non empty result voxels */
    for (unsigned int i = 0; i < resultVoxels.size(); i++) {

      const resultVoxel_s & rv = resultVoxels[i];

      const int x = rv.x - ax;
      const int y = rv.y - ay;
      const int z = rv.z - az;

      if ((x < wx1) || (x > wx2) || (y < wy1) || (y > wy2) || (z < wz1) || (z > wz2))
        continue;

      if (!rotation->onGrid(x, y, z))
        continue;

      if (fits(rv.index - anchorOff, rv.color)) {
        placements.push_back(x);
        placements.push_back(y);
        placements.push_back(z);
      }
    }

    /* the result voxels are traversed x fastest, the brute force scan tried x
     * slowest, restore that order so the matrix is built up identically
     */
    struct triple_s { int x, y, z; };
    triple_s * t = (triple_s*)placements.data();
    std::sort(t, t + placements.size()/3, [](const triple_s & a, const triple_s & b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
        });

  } else {

    /* the result is mostly filled: iterating the translation window directly
     * is cheaper than going through the voxel list and needs no sorting, the
     * anchor voxel read rejects most translations with a single access */
    for (int x = wx1; x <= wx2; x++)
      for (int y = wy1; y <= wy2; y++)
        for (int z = wz1; z <= wz2; z++) {

          const long base = x + sx * (y + sy * (long)z);
          const long ri = base + anchorOff;

          if (res->getState(ri) == voxel_c::VX_EMPTY)
            continue;

          if (!rotation->onGrid(x, y, z))
            continue;

          if (fits(base, res->getColor(ri))) {
            placements.push_back(x);
            placements.push_back(y);
            placements.push_back(z);
          }
        }
  }
}

assembler_c::errState assembler_c::createMatrix(bool /*keepMirror*/, bool /*keepRotations*/, bool /*complete*/)
{
  return ERR_NONE;
}

assembler_c::errState assembler_c::setPosition(const char * /*string*/, const char * /*version*/)
{
  return ERR_CAN_NOT_RESTORE_VERSION;
}

void assembler_c::save(xmlWriter_c & xml) const
{
  xml.newTag("assembler");
  xml.endTag("assembler");
}

