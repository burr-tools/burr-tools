/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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
#include "assembly.h"

#include "puzzle.h"
#include "bt_assert.h"
#include "voxel.h"


void mirrorInfo_c::addPieces(unsigned int p1, unsigned int p2, unsigned char trans) {
  entry e;
  e.pc1 = p1;
  e.pc2 = p2;
  e.trans = trans;
  entries.push_back(e);
}

bool mirrorInfo_c::getPieceInfo(unsigned int p, unsigned int * p_out, unsigned char * trans) const {

  for (unsigned int i = 0; i < entries.size(); i++)
    if (entries[i].pc1 == p) {
      *p_out = entries[i].pc2;
      *trans = entries[i].trans;
      return true;
    }

  return false;
}


assembly_c::assembly_c(const xml::node & node, unsigned int pieces, const gridType_c * gt) : sym(gt->getSymmetries()) {

  // we must have a real node and the following attributes
  if ((node.get_type() != xml::node::type_element) ||
      (strcmp(node.get_name(), "assembly") != 0))
    throw load_error("not the right node for assembly", node);

  const char * c = node.get_content();

  int x, y, z, trans, state, sign;

  x = y = z = trans = state = 0;
  sign = 0;

  while (*c) {

    if (*c == ' ') {
      if (state == 3) {

        if ((trans != 255) && ((trans < 0) || ((unsigned int)trans >= sym->getNumTransformations())))
          throw load_error("transformations need to be either 255 or between 0 and NUM_TRANSFORMATIONS", node);

        placements.push_back(placement_c(trans, x, y, z));
        x = y = z = trans = state = 0;
      } else
        state++;

    } else {

      if ((*c != '-') && ((*c < '0') || (*c > '9')))
        throw load_error("nun number character found where number is expected", node);

      switch(state) {
      case 0:
        if (*c == '-')
          sign = 1;
        else {
          x *= 10;
          x += *c - '0';
          if (x && sign) {
            sign = 0;
            x = -x;
          }
        }
        break;
      case 1:
        if (*c == '-')
          sign = 1;
        else {
          y *= 10;
          y += *c - '0';
          if (y && sign) {
            sign = 0;
            y = -y;
          }
        }
        break;
      case 2:
        if (*c == '-')
          sign = 1;
        else {
          z *= 10;
          z += *c - '0';
          if (z && sign) {
            sign = 0;
            z = -z;
          }
        }
        break;
      case 3:
        if (*c == '-')
          sign = 1;
        else {
          trans *= 10;
          trans += *c - '0';
          if (trans && sign) {
            sign = 0;
            trans = -trans;
          }
        }
        break;
      }
    }

    c++;
  }

  if (state != 3)
    throw load_error("not the right number of numbers in assembly", node);

  if ((trans != 255) && ((trans < 0) || ((unsigned int)trans >= sym->getNumTransformations())))
    throw load_error("transformations need to be either 255 or between 0 and NUM_TRANSFORMATIONS", node);

  placements.push_back(placement_c(trans, x, y, z));

  if (placements.size() != pieces)
    throw load_error("not the right number of placements in assembly", node);
}

assembly_c::assembly_c(const assembly_c * orig) : sym(orig->sym) {

  for (unsigned int i = 0; i < orig->placements.size(); i++)
    placements.push_back(placement_c(orig->getTransformation(i), orig->getX(i), orig->getY(i), orig->getZ(i)));

}

assembly_c::assembly_c(const assembly_c * orig, unsigned char trans, const puzzle_c * puz, unsigned int prob, const mirrorInfo_c * mir) : sym(orig->sym) {
  for (unsigned int i = 0; i < orig->placements.size(); i++)
    placements.push_back(placement_c(orig->getTransformation(i), orig->getX(i), orig->getY(i), orig->getZ(i)));

  transform(trans, puz, prob, mir);

}

xml::node assembly_c::save(void) const {

  xml::node nd("assembly");

  std::string cont;
  char tmp[50];

  for (unsigned int i = 0; i < placements.size(); i++) {
    snprintf(tmp, 50, "%i %i %i %i", placements[i].getX(), placements[i].getY(), placements[i].getZ(), placements[i].getTransformation());

    cont += tmp;

    if (i < placements.size() - 1)
      cont += " ";
  }

  nd.set_content(cont.c_str());

  return nd;
}

void assembly_c::sort(const puzzle_c * puz, unsigned int prob) {

  int p = 0;

  for (unsigned int i = 0; i < puz->probShapeNumber(prob); i++) {

    unsigned int cnt = puz->probGetShapeCount(prob, i);

    /* now we need to sort pieces to that they are sorted by placement */
    if (cnt > 1) {

      /* as we normally only have a few identical pieces that need sorting we use bubble sort */
      for (unsigned int a = 0; a < cnt - 1; a++)
        for (unsigned int b = a + 1; b < cnt; b++)
          if (placements[p+b] < placements[p+a]) {

            placement_c tmp(placements[p+b]);
            placements[p+b] = placements[p+a];
            placements[p+a] = tmp;
          }
    }

    p += cnt;
  }
}

void assembly_c::transform(unsigned char trans, const puzzle_c * puz, unsigned int prob, const mirrorInfo_c * mir) {

  if (trans == 0) return;

  int rx, ry, rz;
  puz->probGetResultShape(prob)->getHotspot(trans, &rx, &ry, &rz);

  {
    /* when the result shape is not minimized, or in space grids that are not
     * cubes is happens that the rotation of the result shape makes it jump
     * around which makes comparisons of rotated assemblies impossible,
     * this code makes the rotation stay in place. This is possible because
     * this normally only gets called when when the result shape looks
     * identical with the new orientation
     */

    int cx, cy, cz, dx, dy, dz;

    puz->probGetResultShape(prob)->getBoundingBox(trans, &cx, &cy, &cz);
    puz->probGetResultShape(prob)->getBoundingBox(0, &dx, &dy, &dz);

    rx += dx - cx;
    ry += dy - cy;
    rz += dz - cz;
  }

  int p = 0;

  for (unsigned int i = 0; i < puz->probShapeNumber(prob); i++) {
    for (unsigned int j = 0; j < puz->probGetShapeCount(prob, i); j++) {

      puz->probGetResultShape(prob)->transformPoint(&placements[p].xpos, &placements[p].ypos, &placements[p].zpos, trans);

      placements[p].xpos += rx;
      placements[p].ypos += ry;
      placements[p].zpos += rz;

      /* add the piece transformations and also find the smallest possible
       * transformation that results in the same piece
       */
      placements[p].transformation = sym->transAdd(placements[p].transformation, trans);

      unsigned char tr = puz->probGetShapeShape(prob, i)->normalizeTransformation(placements[p].transformation);

      if (tr != placements[p].transformation) {

        /* alright, the normalized orientation of the piece is different from the calculated one
         * we now neet to change the placement of the piece so, that it is at the right position with
         * the normalized position
         * this is the easiest solution but by far the slowest
         */
        int ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz;
        puz->probGetShapeShape(prob, i)->getHotspot(placements[p].transformation, &ax, &ay, &az);
        puz->probGetShapeShape(prob, i)->getHotspot(tr, &bx, &by, &bz);

        puz->probGetShapeShape(prob, i)->getBoundingBox(placements[p].transformation, &cx, &cy, &cz);
        puz->probGetShapeShape(prob, i)->getBoundingBox(tr, &dx, &dy, &dz);

        placements[p].xpos += bx-ax + cx-dx;
        placements[p].ypos += by-ay + cy-dy;
        placements[p].zpos += bz-az + cz-dz;

        placements[p].transformation = tr;
      }

      p++;
    }
  }

  // finally we need to check, if we can exchange mirrored pieces, so that
  // they no longer require mirrored orientation

  if (mir) {

    unsigned int p = 0;

    for (unsigned int i = 0; i < puz->probShapeNumber(prob); i++) {
      for (unsigned int j = 0; j < puz->probGetShapeCount(prob, i); j++) {

        // only check, if the current piece is mirrored
        if (placements[p].transformation >= sym->getNumTransformations()) {

          unsigned int p2 = 0;
          unsigned char t;

          if (mir->getPieceInfo(p, &p2, &t)) {

            unsigned int p3;
            unsigned char t_inv;

            mir->getPieceInfo(p2, &p3, &t_inv);

            bt_assert(p3 == p);

            unsigned int i2 = 0;

            {
              unsigned int ss = 0;

              while (ss+puz->probGetShapeCount(prob, i2) < p2) {
                ss += puz->probGetShapeCount(prob, i2);
                i2++;
              }
            }

            /* ok, we found replacement information for piece p,
             * we are supposed to replace it with piece p2
             *
             * so we put the current piece at position p2 and the piece
             * at position p, we need to find out the target orientations
             * of the 2 pieces and we need to transform the new positions
             * with the hotspots
             */

            int p1x = placements[p].xpos;
            int p1y = placements[p].ypos;
            int p1z = placements[p].zpos;
            unsigned char p1t = placements[p].transformation;

            int p2x = placements[p2].xpos;
            int p2y = placements[p2].ypos;
            int p2z = placements[p2].zpos;
            unsigned char p2t = placements[p2].transformation;

            /* the 2nd piece must also be mirrored */
            bt_assert(p2t >= sym->getNumTransformations());

            int hx, hy, hz;

            puz->probGetShapeShape(prob, i)->getHotspot(p1t, &hx, &hy, &hz);
            p1x -= hx;
            p1y -= hy;
            p1z -= hz;

            puz->probGetShapeShape(prob, i2)->getHotspot(p2t, &hx, &hy, &hz);
            p2x -= hx;
            p2y -= hy;
            p2z -= hz;

            p1t = puz->probGetShapeShape(prob, i)->normalizeTransformation(sym->transAdd(t, p1t));
            p2t = puz->probGetShapeShape(prob, i2)->normalizeTransformation(sym->transAdd(t_inv, p2t));

            puz->probGetShapeShape(prob, i)->getHotspot(p1t, &hx, &hy, &hz);
            p1x += hx;
            p1y += hy;
            p1z += hz;

            puz->probGetShapeShape(prob, i2)->getHotspot(p2t, &hx, &hy, &hz);
            p2x += hx;
            p2y += hy;
            p2z += hz;

            placements[p].xpos = p2x;
            placements[p].ypos = p2y;
            placements[p].zpos = p2z;
            placements[p].transformation = p2t;

            placements[p2].xpos = p1x;
            placements[p2].ypos = p1y;
            placements[p2].zpos = p1z;
            placements[p2].transformation = p1t;
          }
        }

        p++;
      }
    }
  }

  sort(puz, prob);
}

bool assembly_c::compare(const assembly_c & b, unsigned int pivot) const {

  bt_assert(placements.size() == b.placements.size());
  bt_assert(pivot < placements.size());

  /* we first compare the pivot piece and leave that one out later on
   * we do that because the pivot piece is the one that has reduced
   * placements and may not occure in all possible positions and thus
   * the rotation reduction algorithm may try to select on assembly
   * that doesn't exist
   */
  if (placements[pivot] < b.placements[pivot]) return true;
  if (!(placements[pivot] == b.placements[pivot])) return false;

  for (unsigned int i = 0; i < placements.size(); i++) {
    if (i != pivot) {
      if (placements[i] < b.placements[i]) return true;
      /* here it can only be larger or equal, so if it is not
       * equal it must be larger so return false
       */
      if (!(placements[i] == b.placements[i])) return false;
    }
  }

  return false;
}

bool assembly_c::containsMirroredPieces(void) const {

  for (unsigned int i = 0; i < placements.size(); i++) {

    if (placements[i].transformation >= sym->getNumTransformations())
      return true;
  }

  return false;
}

bool assembly_c::smallerRotationExists(const puzzle_c * puz, unsigned int prob, unsigned int pivot, const mirrorInfo_c * mir) const {

  symmetries_t s = puz->probGetResultShape(prob)->selfSymmetries();

  /* we only need to check for mirrored transformations, if mirrorInfo is given
   * if not we assume that the pieceset contains at least one piece that has no
   * mirror symmetries and no mirror pair
   */
  unsigned int endTrans = mir ? sym->getNumTransformationsMirror() : sym->getNumTransformations();

  for (unsigned char t = 1; t < endTrans; t++) {

    if (sym->symmetrieContainsTransformation(s, t)) {

      assembly_c tmp(this, t, puz, prob, mir);

      // if the assembly orientation requires mirrored pieces
      // it is invalid, that should be the case for most assemblies
      // when checking for mirrored
      //
      // FIXME: we should check, if we can exchange 2 shapes that are
      // mirrors of one another to see, if we can remove the mirror
      // problem
      if ((t >= sym->getNumTransformations()) && tmp.containsMirroredPieces()) {
        continue;
      }

      if (tmp.compare(*this, pivot)) {
        return true;
      }
    }
  }

  return false;
}

void assembly_c::shiftPiece(unsigned int pc, int dx, int dy, int dz) {
  bt_assert(pc < placements.size());
  placements[pc].xpos += dx;
  placements[pc].ypos += dy;
  placements[pc].zpos += dz;
}

void assembly_c::exchangeShape(unsigned int s1, unsigned int s2) {
  bt_assert(s1 < placements.size());
  bt_assert(s2 < placements.size());

  placement_c p = placements[s1];
  placements[s1] = placements[s2];
  placements[s2] = p;
}
