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
#include "print.h"

#include "voxel.h"
#include "puzzle.h"
#include "problem.h"
#include "disassembly.h"
#include "assembly.h"
#include "assert.h"

#include <vector>
#include <memory>

void print(const voxel_c * v, char base) {
  for (unsigned int z = 0; z < v->getZ(); z++) {
    printf(" +");
    for (unsigned int x = 0; x < v->getX(); x++)
      printf("-");
    printf("+");
  }
  printf(" bx %u-%u by %u-%u bz %u-%u h: %i %i %i \n", v->boundX1(), v->boundX2(), v->boundY1(), v->boundY2(), v->boundZ1(), v->boundZ2(), v->getHx(), v->getHy(), v->getHz());

  for (unsigned int y = 0; y < v->getY(); y++) {
    for (unsigned int z = 0; z < v->getZ(); z++) {
      printf(" !");
      for (unsigned int x = 0; x < v->getX(); x++)
        if (v->get(x, y, z) != 0)
          printf("%c", base + v->get(x, y, z)-1);
        else
          printf(" ");
      printf("!");
    }
    printf("\n");
  }

  { for (unsigned int z = 0; z < v->getZ(); z++) {
      printf(" +");
      for (unsigned int x = 0; x < v->getX(); x++)
        printf("-");
      printf("+");
    }
  }
  printf("\n");
}

void print(const puzzle_c * p) {

  for (unsigned int s = 0; s < p->getNumberOfShapes(); s++) {
    printf("shape %u:\n", s);
    print(p->getShape(s));
  }

  printf("=======================================================\n");

  for (unsigned int pr = 0; pr < p->getNumberOfProblems(); pr++) {

    const problem_c * prob = p->getProblem(pr);

    printf("problem %u (%s):\n", pr, prob->getName().c_str());
    if (!prob->resultValid())
      printf(" result shape: not defined\n");
    else
      printf(" result shape: %u\n", prob->getResultId());

    for (unsigned int sh = 0; sh < prob->getNumberOfParts(); sh++)
      if (prob->getPartMinimum(sh) != prob->getPartMaximum(sh))
        printf(" piece shape: %u-%u times shape number %u\n", prob->getPartMinimum(sh), prob->getPartMaximum(sh), prob->getShapeIdOfPart(sh));
      else if (prob->getPartMinimum(sh) != 1)
        printf(" piece shape: %u times shape number %u\n", prob->getPartMinimum(sh), prob->getShapeIdOfPart(sh));
      else
        printf(" piece shape: %u\n", prob->getShapeIdOfPart(sh));

    printf("-------------------------------------------------------\n");
  }
}

static void print_rec(const separation_c * s, const std::vector<std::unique_ptr<voxel_c>> & pieces, int sx, int sy, int sz, const std::vector<unsigned int> & pieceNum) {

  for (unsigned int i = 0; i <= s->getMoves(); i++) {

    const state_c * st = s->getState(i);

    for (int z = -sz/2; z < sz+sz/2; z++) {
      printf(" +");
      for (int x = -sx/2; x < sx+sx/2; x++)
        printf("-");
      printf("+");
    }
    printf("\n");

    for (int y = -sy/2; y < sy+sy/2; y++) {
      for (int z = -sz/2; z < sz+sz/2; z++) {
        printf(" !");
        for (int x = -sx/2; x < sx+sx/2; x++) {
          char c = ' ';

          for (unsigned int pc = 0; pc < s->getPieceNumber(); pc++)
            if (pieces[pc]->isFilled2(x - (st->getX(pc) - pieces[pc]->getHx()),
                                      y - (st->getY(pc) - pieces[pc]->getHy()),
                                      z - (st->getZ(pc) - pieces[pc]->getHz()))) {
              c = 'a' + pieceNum[pc];
              break;
            }

          printf("%c", c);
        }
        printf("!");
      }
      printf("\n");
    }

    { for (int z = -sz/2; z < sz+sz/2; z++) {
        printf(" +");
        for (int x = -sx/2; x < sx+sx/2; x++)
          printf("-");
        printf("+");
      }
    }
    printf("\n");
  }

  printf(" puzzle separates into  2 parts\n");

  if (s->getRemoved()) {

    std::vector<unsigned int> pieceNum2;

    for (unsigned int i = 0; i < s->getPieceNumber(); i++)
      if (s->getState(s->getMoves())->pieceRemoved(i))
        pieceNum2.push_back(pieceNum[i]);

    print_rec(s->getRemoved(), pieces, sx, sy, sz, pieceNum2);
  }
  if (s->getLeft()) {

    std::vector<unsigned int> pieceNum2;

    for (unsigned int i = 0; i < s->getPieceNumber(); i++)
      if (!(s->getState(s->getMoves())->pieceRemoved(i)))
        pieceNum2.push_back(pieceNum[i]);

    print_rec(s->getLeft(), pieces, sx, sy, sz, pieceNum2);
  }
}

void print(const separation_c * s, const assembly_c * a, const problem_c * p) {

  bt_assert(p->resultValid());

  const voxel_c * res = getResultShape(*p);

  std::vector<std::unique_ptr<voxel_c>> pieces(a->placementCount());

  unsigned int pc = 0;

  for (unsigned int i = 0; i < p->getNumberOfParts(); i++)
    for (unsigned int j = 0; j < p->getPartMaximum(i); j++) {

      pieces[pc] = std::unique_ptr<voxel_c>(p->getPuzzle().getGridType()->getVoxel(p->getPartShape(i)));
      bt_assert2(pieces[pc]->transform(a->getTransformation(pc)));
      pc++;
    }

  std::vector<unsigned int> pieceNum(a->placementCount());
  for (unsigned int i = 0; i < a->placementCount(); i++)
    pieceNum[i] = i;

  print_rec(s, pieces, res->getX(), res->getY(), res->getZ(), pieceNum);
}

void print(const assembly_c * a, const problem_c * p) {

  bt_assert(p->resultValid());

  const voxel_c * res = getResultShape(*p);

  std::vector<std::unique_ptr<voxel_c>> pieces(a->placementCount());

  unsigned int pc = 0;

  for (unsigned int i = 0; i < p->getNumberOfParts(); i++)
    for (unsigned int j = 0; j < p->getPartMaximum(i); j++) {

      if (a->isPlaced(pc)) {
        pieces[pc] = std::unique_ptr<voxel_c>(p->getPuzzle().getGridType()->getVoxel(p->getPartShape(i)));
        bt_assert2(pieces[pc]->transform(a->getTransformation(pc)));
      }

      pc++;
    }

  for (unsigned int z = 0; z < res->getZ(); z++) {
    printf(" +");
    for (unsigned int x = 0; x < res->getX(); x++)
      printf("-");
    printf("+");
  }
  printf("\n");

  for (unsigned int y = 0; y < res->getY(); y++) {
    for (unsigned int z = 0; z < res->getZ(); z++) {
      printf(" !");
      for (unsigned int x = 0; x < res->getX(); x++) {
        char c = ' ';

        unsigned int pc = 0;

        for (unsigned int p = 0; p < a->placementCount(); p++)

          if (a->isPlaced(p)) {

            if (pieces[p]->isFilled2(x - (a->getX(p) - pieces[p]->getHx()),
                                     y - (a->getY(p) - pieces[p]->getHy()),
                                     z - (a->getZ(p) - pieces[p]->getHz()))) {
              c = 'a' + pc;
              break;
            }

            pc++;
          }

        printf("%c", c);
      }
      printf("!");
    }
    printf("\n");
  }

  { for (unsigned int z = 0; z < res->getZ(); z++) {
      printf(" +");
      for (unsigned int x = 0; x < res->getX(); x++)
        printf("-");
      printf("+");
    }
  }
  printf("\n");
}
