/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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

#include "problem.h"
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
  sign = 1;

  while (*c) {

    if (*c == ' ') {
      if (state == 3) {

        if ((trans != UNPLACED_TRANS) && ((trans < 0) || ((unsigned int)trans >= sym->getNumTransformations())))
          throw load_error("transformations need to be between 0 and NUM_TRANSFORMATIONS", node);

        placements.push_back(placement_c(trans, x, y, z));
        x = y = z = trans = state = 0;
      } else {
        sign = 1;
        state++;
      }

    } else if (*c == 'x' && state == 0) {

      x = y = z = 0;
      trans = UNPLACED_TRANS;
      state = 3;

    } else {

      if ((*c != '-') && ((*c < '0') || (*c > '9')))
        throw load_error("non number character found where number is expected", node);

      switch(state) {
      case 0:
        if (*c == '-') {
          if (x) throw load_error("assembly placements minus in number", node);
          sign = -1;
        } else {
          x *= 10;
          x += sign*(*c - '0');
        }
        break;
      case 1:
        if (*c == '-') {
          if (y) throw load_error("assembly placements minus in number", node);
          sign = -1;
        } else {
          y *= 10;
          y += sign*(*c - '0');
        }
        break;
      case 2:
        if (*c == '-') {
          if (z) throw load_error("assembly placements minus in number", node);
          sign = -1;
        } else {
          z *= 10;
          z += sign*(*c - '0');
        }
        break;
      case 3:
        if (*c == '-') {
          if (trans) throw load_error("assembly placements minus in number", node);
          sign = -1;
        } else {
          trans *= 10;
          trans += sign*(*c - '0');
        }
        break;
      }
    }

    c++;
  }

  if (state != 3)
    throw load_error("not the right number of numbers in assembly", node);

  if ((trans != UNPLACED_TRANS) && ((trans < 0) || ((unsigned int)trans >= sym->getNumTransformations())))
    throw load_error("transformations need to be between 0 and NUM_TRANSFORMATIONS", node);

  placements.push_back(placement_c(trans, x, y, z));

  if (placements.size() != pieces)
    throw load_error("not the right number of placements in assembly", node);
}

assembly_c::assembly_c(const assembly_c * orig) : placements(orig->placements), sym(orig->sym) {
}


xml::node assembly_c::save(void) const {

  xml::node nd("assembly");

  std::string cont;
  char tmp[50];

  for (unsigned int i = 0; i < placements.size(); i++) {

    if (placements[i].getTransformation() != UNPLACED_TRANS)
      snprintf(tmp, 50, "%i %i %i %i", placements[i].getX(), placements[i].getY(), placements[i].getZ(), placements[i].getTransformation());
    else
      snprintf(tmp, 50, "x");

    cont += tmp;

    if (i < placements.size() - 1)
      cont += " ";
  }

  nd.set_content(cont.c_str());

  return nd;
}

void assembly_c::sort(const problem_c * puz) {

  int p = 0;

  for (unsigned int i = 0; i < puz->shapeNumber(); i++) {

    unsigned int cnt = puz->getShapeMax(i);

    /* find out how many pieces are actually placed:
     */
    unsigned int cnt2 = 0;
    while (cnt2 < cnt && isPlaced(p+cnt2)) cnt2++;

    /* now we need to sort pieces to that they are sorted by placement */
    if (cnt2 > 1) {

      /* as we normally only have a few identical pieces that need sorting we use bubble sort
       * with one addition, because many times only a fiew pieces will actually be placed
       * we check, if something was done and bail out, if not
       */
      for (unsigned int a = 0; a < cnt2 - 1; a++) {
        bool swapped = false;

        for (unsigned int b = cnt2-1; b > a; b--)
          if (placements[p+b] < placements[p+b-1]) {

            placement_c tmp(placements[p+b]);
            placements[p+b] = placements[p+b-1];
            placements[p+b-1] = tmp;

            swapped = true;
          }

        if (!swapped)
          break;
      }
    }

    p += cnt;
  }
}

bool assembly_c::transform(unsigned char trans, const problem_c * puz, const mirrorInfo_c * mir) {

  if (trans == 0) return true;

  // if we want to mirror, we need mirroring information for the involved shapes
  bt_assert((trans < sym->getNumTransformations()) || mir);

  int rx, ry, rz;
  if (!puz->getResultShape()->getHotspot(trans, &rx, &ry, &rz)) return false;

  /* the hole idea behind this is:
   *
   * the position of a piece is given as a vector from the origin, now when
   * transforming we can rotate that vector easily but we don't know from where
   * to start it. To find that out we have the hotspot. This hotspot of the
   * result shape is the new point zero from where we start with our rotated
   * vector leading to the final position of the piece
   *
   * this is not all, of course, what we really want to have here is a transformation
   * that is placed in the very same position so that when the result matches itself
   * in this new orientation the rotated assembly is ecatly at the same place.
   * This doesn't work when the result changes place when is corresponding voxel
   * space is rotated, so we need to find out how much it changed place via this
   * rotation. That is done with the bounding box.
   *
   * the 2nd problem is that the 0 vector for piece positions may not be zero
   * after being transformed. This is fixed below
   */

  {
    /* in some grid spaces the 0 coordinate doesn stay in place when transformed
     * because a state needs to change which can only be chieve by changing place
     * (e.g Triangles, when rotating by 60° the orientation of the triangle changes)
     * to accommodate this shift we check, where the hotspot moves to, when transformed
     * and accommodate for this change
     */

    int hx = puz->getResultShape()->getHx();
    int hy = puz->getResultShape()->getHy();
    int hz = puz->getResultShape()->getHz();

    rx += hx;
    ry += hy;
    rz += hz;

    puz->getResultShape()->transformPoint(&hx, &hy, &hz, trans);

    rx -= hx;
    ry -= hy;
    rz -= hz;
  }

  {
    /* when the result shape is not minimized, or in space grids that are not
     * cubes is happens that the rotation of the result shape makes it jump
     * around which makes comparisons of rotated assemblies impossible,
     * this code makes the rotation stay in place. This is possible because
     * this normally only gets called when when the result shape looks
     * identical with the new orientation
     */

    int cx, cy, cz, dx, dy, dz;

    if (!puz->getResultShape()->getBoundingBox(trans, &cx, &cy, &cz)) return false;
    if (!puz->getResultShape()->getBoundingBox(0, &dx, &dy, &dz)) return false;

    rx += dx - cx;
    ry += dy - cy;
    rz += dz - cz;
  }

  unsigned int p = 0;

  for (unsigned int i = 0; i < puz->shapeNumber(); i++) {
    for (unsigned int j = 0; j < puz->getShapeMax(i); j++) {

      // if a piece has a transformation == 255 it is NOT placed so we don't need to do anything
      // we even do leave the shape loop as no more pieces of that shape will be placed..
      if (!isPlaced(p)) {
        p += puz->getShapeMax(i)-j;
        j = puz->getShapeMax(i);
        continue;
      }

      puz->getResultShape()->transformPoint(&placements[p].xpos, &placements[p].ypos, &placements[p].zpos, trans);

      placements[p].xpos += rx;
      placements[p].ypos += ry;
      placements[p].zpos += rz;

      /* add the piece transformations and also find the smallest possible
       * transformation that results in the same piece
       */
      placements[p].transformation = sym->transAdd(placements[p].transformation, trans);
      if (placements[p].transformation == TND) return false;

      unsigned char tr = puz->getShapeShape(i)->normalizeTransformation(placements[p].transformation);

      if (tr != placements[p].transformation) {

        /* Right, the normalized orientation of the piece is different from the calculated one
         * we now need to change the placement of the piece so, that it is at the right position with
         * the normalized position
         * this is the easiest solution but by far the slowest
         */
        int ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz;
        puz->getShapeShape(i)->getHotspot(placements[p].transformation, &ax, &ay, &az);
        puz->getShapeShape(i)->getHotspot(tr, &bx, &by, &bz);

        puz->getShapeShape(i)->getBoundingBox(placements[p].transformation, &cx, &cy, &cz);
        puz->getShapeShape(i)->getBoundingBox(tr, &dx, &dy, &dz);

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

    p = 0;

    for (unsigned int i = 0; i < puz->shapeNumber(); i++) {
      for (unsigned int j = 0; j < puz->getShapeMax(i); j++) {

        // if a piece has a transformation == 255 it is NOT placed so we don't need to do anything
        // we even do leave the shape loop as no more pieces of that shape will be placed..
        if (!isPlaced(p)) {
          p += puz->getShapeMax(i)-j;
          j = puz->getShapeMax(i);
          continue;
        }

        // if a piece is NOT placed so we don't need to do anything
        // only check, if the current piece is mirrored
        if (placements[p].transformation >= sym->getNumTransformations()) {

          unsigned int p2 = 0;
          unsigned char t;

          /* when no mirror piece exists we can stop, this might happen for
           * wne piece ranges are used because then we don't know what pieces
           * are used in the final solution and have to add all known information
           * and then check, if things work out
           */
          if (!mir->getPieceInfo(p, &p2, &t))
            break;

          unsigned int p3;
          unsigned char t_inv;

          bt_assert2(mir->getPieceInfo(p2, &p3, &t_inv));

          bt_assert(p3 == p);

          unsigned int i2 = 0;

          {
            unsigned int ss = 0;

            while (ss+puz->getShapeMax(i2) <= p2) {
              ss += puz->getShapeMax(i2);
              i2++;
            }
          }

          /* when applying the 2 found transformations to the 2 pieces we must return to the
           * original piece. I used to assume the t and t_inv are really inverse transformations
           * but that is not necessarily so because the piece might have symmetries and we might
           * arrive at an orientation that is not 0 but a one of the orientations within the
           * symmetry of the piece. Thats why the normalize operation
           */
          if (sym->transAdd(t, t_inv) == TND || sym->transAdd(t_inv, t) == TND) return false;

          bt_assert(puz->getShapeShape(i)->normalizeTransformation(sym->transAdd(t, t_inv)) == 0);
          bt_assert(puz->getShapeShape(i2)->normalizeTransformation(sym->transAdd(t_inv, t)) == 0);

          /* OK, we found replacement information for piece p,
           * we are supposed to replace it with piece p2
           *
           * so we put the current piece at position p2 and the piece
           * at position p, we need to find out the target orientations
           * of the 2 pieces and we need to transform the new positions
           * with the hotspots
           */

          // when the 2nd piece is not available inside the assembly, we can
          // not swap
          if (isPlaced(p2)) {

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

            /* this step calculates the position of the origin of the bounding
             * boxes of the 2 involved pieced. This coordinate is identical
             * for the mirrored piece and for the replaced pieces, as both pieces
             * have the same shape at the orientation
             */

            puz->getShapeShape(i)->getHotspot(p1t, &hx, &hy, &hz);
            p1x -= hx;
            p1y -= hy;
            p1z -= hz;
            puz->getShapeShape(i)->getBoundingBox(p1t, &hx, &hy, &hz);
            p1x += hx;
            p1y += hy;
            p1z += hz;

            puz->getShapeShape(i2)->getHotspot(p2t, &hx, &hy, &hz);
            p2x -= hx;
            p2y -= hy;
            p2z -= hz;
            puz->getShapeShape(i2)->getBoundingBox(p2t, &hx, &hy, &hz);
            p2x += hx;
            p2y += hy;
            p2z += hz;

            /* calculate the new orientation of both pieces */

            /* the idea is:
             * A is the first shape B the second. A_0 means A in the orientation entered by the user, same for B_0
             * transformation t transforms A_0 -> B_0 (denoted by A_0 -t-> B_0), also B_0 -t_inv->A_0
             * what we search is is an x so that B_x is identical to A_pt1, this is calulated by
             * taking first t_inv and then p1t B_0 -t_inv-> A_0 -p1t-> A_p1t
             * same for the other way around
             */
            if (sym->transAdd(t_inv, p1t) == TND || sym->transAdd(t, p2t) == TND) return false;
            p1t = puz->getShapeShape(i2)->normalizeTransformation(sym->transAdd(t_inv, p1t));
            p2t = puz->getShapeShape(i)->normalizeTransformation(sym->transAdd(t, p2t));

            /* now go back from the origin of the bounding box to the hotspot anchor point */

            puz->getShapeShape(i2)->getHotspot(p1t, &hx, &hy, &hz);
            p1x += hx;
            p1y += hy;
            p1z += hz;
            puz->getShapeShape(i2)->getBoundingBox(p1t, &hx, &hy, &hz);
            p1x -= hx;
            p1y -= hy;
            p1z -= hz;

            puz->getShapeShape(i)->getHotspot(p2t, &hx, &hy, &hz);
            p2x += hx;
            p2y += hy;
            p2z += hz;
            puz->getShapeShape(i)->getBoundingBox(p2t, &hx, &hy, &hz);
            p2x -= hx;
            p2y -= hy;
            p2z -= hz;

            placements[p].xpos = p2x;
            placements[p].ypos = p2y;
            placements[p].zpos = p2z;
            placements[p].transformation = p2t;

            placements[p2].xpos = p1x;
            placements[p2].ypos = p1y;
            placements[p2].zpos = p1z;
            placements[p2].transformation = p1t;

          } else {

            // the 2nd piece is not placed, so after the swapping piece 1 will not be placed
            // this makes things a bit easier


            int p1x = placements[p].xpos;
            int p1y = placements[p].ypos;
            int p1z = placements[p].zpos;
            unsigned char p1t = placements[p].transformation;

            int hx, hy, hz;

            /* this step calculates the position of the origin of the bounding
             * boxes of the 2 involved pieced. This coordinate is identical
             * for the mirrored piece and for the replaced pieces, as both pieces
             * have the same shape at the orientation
             */

            puz->getShapeShape(i)->getHotspot(p1t, &hx, &hy, &hz);
            p1x -= hx;
            p1y -= hy;
            p1z -= hz;
            puz->getShapeShape(i)->getBoundingBox(p1t, &hx, &hy, &hz);
            p1x += hx;
            p1y += hy;
            p1z += hz;

            /* calculate the new orientation of both pieces */

            /* the idea is:
             * A is the first shape B the second. A_0 means A in the orientation entered by the user, same for B_0
             * transformation t transforms A_0 -> B_0 (denoted by A_0 -t-> B_0), also B_0 -t_inv->A_0
             * what we search is is an x so that B_x is identical to A_pt1, this is calulated by
             * taking first t_inv and then p1t B_0 -t_inv-> A_0 -p1t-> A_p1t
             * same for the other way around
             */
            if (sym->transAdd(t_inv, p1t) == TND) return false;
            p1t = puz->getShapeShape(i2)->normalizeTransformation(sym->transAdd(t_inv, p1t));

            /* now go back from the origin of the bounding box to the hotspot anchor point */

            puz->getShapeShape(i2)->getHotspot(p1t, &hx, &hy, &hz);
            p1x += hx;
            p1y += hy;
            p1z += hz;
            puz->getShapeShape(i2)->getBoundingBox(p1t, &hx, &hy, &hz);
            p1x -= hx;
            p1y -= hy;
            p1z -= hz;

            placements[p].xpos = 0;
            placements[p].ypos = 0;
            placements[p].zpos = 0;
            placements[p].transformation = UNPLACED_TRANS;

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

  sort(puz);

  return true;
}

bool assembly_c::compare(const assembly_c & b, unsigned int pivot) const {

  bt_assert(placements.size() == b.placements.size());

  /* we first compare the pivot piece and leave that one out later on
   * we do that because the pivot piece is the one that has reduced
   * placements and may not occur in all possible positions and thus
   * the rotation reduction algorithm may try to select on assembly
   * that doesn't exist
   */
  if (pivot < placements.size()) {
    if (placements[pivot] < b.placements[pivot]) return true;
    if (!(placements[pivot] == b.placements[pivot])) return false;
  }

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

    if (isPlaced(i) && placements[i].transformation >= sym->getNumTransformations())
      return true;
  }

  return false;
}

bool assembly_c::validSolution(const problem_c * puz) const {

  unsigned int pos = 0;

  for (unsigned int i = 0; i < puz->shapeNumber(); i++)
  {
    unsigned int placed = 0;

    while (placed < puz->getShapeMax(i) && isPlaced(pos+placed))
      placed++;

    if (placed < puz->getShapeMin(i))
      return false;

    pos += puz->getShapeMax(i);
  }

  return true;
}

bool assembly_c::smallerRotationExists(const problem_c * puz, unsigned int pivot, const mirrorInfo_c * mir, bool complete) const {

  /* we only need to check for mirrored transformations, if mirrorInfo is given
   * if not we assume that the piece set contains at least one piece that has no
   * mirror symmetries and no mirror pair
   */
  unsigned int endTrans = mir ? sym->getNumTransformationsMirror() : sym->getNumTransformations();

  if (complete)
  {
    for (unsigned char t = 0; t < endTrans; t++)
    {
      assembly_c tmp(this);

      // if we can not create the transformation we can continue to
      // the next orientation
      if (!tmp.transform(t, puz, mir))
      {
        continue;
      }

      // check, if the found transformation is valid, if not
      // we can continue to the next
      if ((t >= sym->getNumTransformations()) &&
          (tmp.containsMirroredPieces() || !tmp.validSolution(puz)))
      {
        continue;
      }

      // ok, we have found the rotated assembly and it exists, we now need to
      // find out if the whole arrangement can be shifted to the lower positions
      // if that is the case we don't keep the stuff

      // now we create a voxel space of the given assembly shape/ and shift that one around
      voxel_c * assm = tmp.createSpace(puz);
      const voxel_c * res = puz->getResultShape();

      for (int x = (int)res->boundX1()-(int)assm->boundX1(); (int)assm->boundX2()+x <= (int)res->boundX2(); x++)
        for (int y = (int)res->boundY1()-(int)assm->boundY1(); (int)assm->boundY2()+y <= (int)res->boundY2(); y++)
          for (int z = (int)res->boundZ1()-(int)assm->boundZ1(); (int)assm->boundZ2()+z <= (int)res->boundZ2(); z++)
          {
            if (assm->onGrid(x, y, z))
            {
              bool fits = true;

              for (int pz = (int)assm->boundZ1(); pz <= (int)assm->boundZ2(); pz++)
                for (int py = (int)assm->boundY1(); py <= (int)assm->boundY2(); py++)
                  for (int px = (int)assm->boundX1(); px <= (int)assm->boundX2(); px++)
                  {
                    if (
                        // the piece can not be place if the result is empty and the piece is filled at a given voxel
                        ((assm->getState(px, py, pz) != voxel_c::VX_EMPTY) &&
                         (res->getState2(x+px, y+py, z+pz) == voxel_c::VX_EMPTY)) ||

                        // the piece can also not be placed when the colour constraints don't fit
                        !puz->placementAllowed(assm->getColor(px, py, pz), res->getColor2(x+px, y+py, z+pz))

                       )
                      fits = false;
                  }

              if (fits)
              {
                // well the assembly fits at the current position, so let us see...
                for (unsigned int i = 0; i < tmp.placements.size(); i++)
                {
                  tmp.placements[i].xpos += x;
                  tmp.placements[i].ypos += y;
                  tmp.placements[i].zpos += z;
                }

                if (tmp.compare(*this, pivot)) {
                  delete assm;
                  return true;
                }

                for (unsigned int i = 0; i < tmp.placements.size(); i++)
                {
                  tmp.placements[i].xpos -= x;
                  tmp.placements[i].ypos -= y;
                  tmp.placements[i].zpos -= z;
                }
              }
            }
          }

      delete assm;
    }
  }
  else
  {
    for (unsigned char t = 0; t < endTrans; t++)
    {
      symmetries_t s = puz->getResultShape()->selfSymmetries();

      if (sym->symmetrieContainsTransformation(s, t))
      {
        assembly_c tmp(this);
        bt_assert2(tmp.transform(t, puz, mir));

        // if the assembly orientation requires mirrored pieces
        // it is invalid, that should be the case for most assemblies
        // when checking for mirrored
        //
        // FIXME: we should check, if we can exchange 2 shapes that are
        // mirrors of one another to see, if we can remove the mirror
        // problem
        //
        // we also need to make sure that the new found assembly uses the right amount
        // of pieces from each shape. Because it is possible that the mirror
        // shape is allowed with a different intervall it is possible that
        // after mirroring the number of instances for some shapes is wrong
        if ((t >= sym->getNumTransformations()) &&
            (tmp.containsMirroredPieces() || !tmp.validSolution(puz)))
        {
          continue;
        }

        if (tmp.compare(*this, pivot)) {
          return true;
        }
      }
    }
  }

  return false;
}

void assembly_c::exchangeShape(unsigned int s1, unsigned int s2) {
  bt_assert(s1 < placements.size());
  bt_assert(s2 < placements.size());

  placement_c p = placements[s1];
  placements[s1] = placements[s2];
  placements[s2] = p;
}

int assembly_c::comparePieces(const assembly_c * b) const {

  // returns 0 if both assemblies use the same pieces
  // 1 if the piece string (AAABBCDE) of this is smaller
  // -1 if the piece string of this is larger

  bt_assert(placements.size() == b->placements.size());

  for (unsigned int piece = 0; piece < placements.size(); piece++) {

    if (placements[piece].transformation == UNPLACED_TRANS &&
        b->placements[piece].transformation != UNPLACED_TRANS)
      return -1;

    if (placements[piece].transformation != UNPLACED_TRANS &&
        b->placements[piece].transformation == UNPLACED_TRANS)
      return 1;
  }

  return 0;
}

voxel_c * assembly_c::createSpace(const problem_c * puz) const {

  std::vector<voxel_c *>pieces;
  pieces.resize(placements.size());

  int maxX = 0;
  int maxY = 0;
  int maxZ = 0;

  // now iterate over all shapes in the assembly and dounf out their  placement
  // to create the proper sized result voxel space
  for (unsigned int i = 0; i < placements.size(); i++)
    if (placements[i].transformation != UNPLACED_TRANS) {

      unsigned int j = puz->pieceToShape(i);

      voxel_c * pc = puz->getGridType()->getVoxel(puz->getShapeShape(j));

      bt_assert(pc->transform(placements[i].transformation));

      int dx = (int)placements[i].xpos - (int)pc->getHx();
      int dy = (int)placements[i].ypos - (int)pc->getHy();
      int dz = (int)placements[i].zpos - (int)pc->getHz();

      if ((int)pc->getX()+dx > maxX) maxX = (int)pc->getX()+dx;
      if ((int)pc->getY()+dy > maxY) maxY = (int)pc->getY()+dy;
      if ((int)pc->getZ()+dz > maxZ) maxZ = (int)pc->getZ()+dz;

      pieces[i] = pc;
    }

  // create a shape identical in size with the result shape of the problem
  voxel_c * res = puz->getGridType()->getVoxel(maxX, maxY, maxZ, 0);
  res->skipRecalcBoundingBox(true);

  // now iterate over all shapes in the assembly and place them into the result
  for (unsigned int i = 0; i < placements.size(); i++)
    if (placements[i].transformation != UNPLACED_TRANS) {

      voxel_c * pc = pieces[i];

      int dx = (int)placements[i].xpos - (int)pc->getHx();
      int dy = (int)placements[i].ypos - (int)pc->getHy();
      int dz = (int)placements[i].zpos - (int)pc->getHz();

      for (unsigned int x = 0; x < pc->getX(); x++)
        for (unsigned int y = 0; y < pc->getY(); y++)
          for (unsigned int z = 0; z < pc->getZ(); z++) {
            if (pc->getState(x, y, z) != voxel_c::VX_EMPTY)
              res->set(x+dx, y+dy, z+dz, pc->get(x, y, z));
          }

      delete pc;
    }

  res->skipRecalcBoundingBox(false);
  return res;
}

