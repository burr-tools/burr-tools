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


#include "lib/puzzle.h"
#include "lib/assm_0_frontend_0.h"
#include "lib/disassembler_0.h"

#include "lib/burrgrower.h"
#include "lib/piecegenerator.h"

#include "lib/print.h"

#include <time.h>

#include <fstream>
#include <iostream>

#include <xmlwrapp/xmlwrapp.h>

using namespace std;

class asm_cb : public assembler_cb {

public:

  int count;
  int pn;

  asm_cb(int pnum) : count(0), pn(pnum) {}

  bool assembly(assembly_c * a) {

#if 0
    count++;

    if ((count & 0x3f) == 0)
      printf("%i\n", count);

//    assm->print('a');

    return true;

    fflush(stdout);


    disassembler_3_c d(assm, pn);

    clock_t start = clock();
    separation_c * da = d.disassemble();
    printf(" timing = %f\n", ((double)(clock() - start))/CLOCKS_PER_SEC);

    if (da) {
//      assm->print();
      printf("level: %i\n", da->getMoves());
//      da->print();
      delete da;
    }

    return true;
#endif
  }
};

#if 0

void solve(int argv, char* args[]) {
  ifstream str(args[1]);

  if (!str) {
    cout << "oope file not opened\n";
    return;
  }

  puzzle_c p(new gridType_c()); /* FIXME &str*/

//  p.print();

  assembler_0_c * assm = new assm_0_frontend_0_c();
  assm->createMatrix(&p, 0);

  if (assm->createMatrix(&p, 0) != assm_0_frontend_0_c::ERR_NONE) {
    printf("errors\n");
    delete assm;
    return;
  }

  printf("start reduce\n");
  assm->reduce();
  printf("finished reduce\n");

  asm_cb a(p.probPieceNumber(0));

  a.count = 0;

  assm->assemble(&a);

  printf("%i sol found with %i iterations\n", a.count, assm->getIterations());

  delete assm;

  return;
}

#endif
void calcSymmetries(char * fname) {

  xml::tree_parser parser(fname);
  puzzle_c p(parser.get_document().get_root_node());

  for (unsigned int i = 0; i < p.shapeNumber(); i++) {
    p.getShape(i)->selfSymmetries();
  }
}

void hotspotCheck(char * fname) {

  xml::tree_parser parser(fname);
  puzzle_c p(parser.get_document().get_root_node());

  for (unsigned int i = 1; i < p.shapeNumber(); i++) {
    for (unsigned int t = 0; t < p.getGridType()->getSymmetries()->getNumTransformationsMirror(); t++) {
      voxel_c * vv = p.getGridType()->getVoxel(p.getShape(i), t);
      int x, y, z;
      p.getShape(i)->getHotspot(t, &x, &y, &z);
      if (x != vv->getHx()) {
        printf("hx for trans %i  %i %i\n", t, x, vv->getHx());
      }
      if (y != vv->getHy()) {
        printf("hy for trans %i  %i %i\n", t, y, vv->getHy());
      }
      if (z != vv->getHz()) {
        printf("hz for trans %i  %i %i\n", t, z, vv->getHz());
      }
      delete vv;
    }
  }
}
#if 0

void grow(int argv, char* args[]) {

//  srand48(time(0));
  srand(time(0));

  ifstream str(args[1]);

  if (!str) {
    cout << "oope file not opened\n";
    return;
  }

  xml::tree_parser parser(args[1]);
  puzzle_c p(parser.get_document().get_root_node());

  cout << " The puzzle:\n\n";

  print(&p);

  burrGrower_c grow(&p, 10, atoi(args[2]));

  std::vector<puzzleSol_c*> v;

  printf("start\n");
  grow.grow(v);
  printf("finished\n");
}

#endif


void comparison(voxel_c * a, voxel_c * b) {

  printf("sx: %i %i \n", a->getX(), b->getX());
  printf("sy: %i %i \n", a->getY(), b->getY());
  printf("sz: %i %i \n", a->getZ(), b->getZ());

  printf("bx: %i %i %i %i \n", a->boundX1(), a->boundX2(), b->boundX1(), b->boundX2());
  printf("by: %i %i %i %i \n", a->boundY1(), a->boundY2(), b->boundY1(), b->boundY2());
  printf("bz: %i %i %i %i \n", a->boundZ1(), a->boundZ2(), b->boundZ1(), b->boundZ2());

  print(a);
  print(b);
}

#if 0
void testNewRots(void) {


  for (int i = 0; i < 1000; i++) {

    voxel_c *a = new voxel_c(16, 8, 4);

    for (int p = 0; p < 10; p++)
      a->set(rand() % 0x1FF, 1);

    voxel_c *b0 = new voxel_c(a);
    voxel_c *b1 = new voxel_c(a);
    voxel_c *b2 = new voxel_c(a);
    voxel_c *b3 = new voxel_c(a);
    voxel_c *b4 = new voxel_c(a);
    b0->rotatex(0);
    b1->rotatex(1);
    b2->rotatex(2);
    b3->rotatex(3);
    b4->rotatex(4);

    if (!(*a == *b0)
        || (a->boundX1() != b0->boundX1())
        || (a->boundX2() != b0->boundX2())
        || (a->boundY1() != b0->boundY1())
        || (a->boundY2() != b0->boundY2())
        || (a->boundZ1() != b0->boundZ1())
        || (a->boundZ2() != b0->boundZ2())
       ) comparison(a, b0);

    a->rotatex(1);

    if (!(*a == *b1)
        || (a->boundX1() != b1->boundX1())
        || (a->boundX2() != b1->boundX2())
        || (a->boundY1() != b1->boundY1())
        || (a->boundY2() != b1->boundY2())
        || (a->boundZ1() != b1->boundZ1())
        || (a->boundZ2() != b1->boundZ2())
       ) comparison(a, b1);

    a->rotatex(1);

    if (!(*a == *b2)
        || (a->boundX1() != b2->boundX1())
        || (a->boundX2() != b2->boundX2())
        || (a->boundY1() != b2->boundY1())
        || (a->boundY2() != b2->boundY2())
        || (a->boundZ1() != b2->boundZ1())
        || (a->boundZ2() != b2->boundZ2())
       ) comparison(a, b2);

    a->rotatex(1);

    if (!(*a == *b3)
        || (a->boundX1() != b3->boundX1())
        || (a->boundX2() != b3->boundX2())
        || (a->boundY1() != b3->boundY1())
        || (a->boundY2() != b3->boundY2())
        || (a->boundZ1() != b3->boundZ1())
        || (a->boundZ2() != b3->boundZ2())
       ) comparison(a, b3);

    a->rotatex(1);

    if (!(*a == *b4)
        || (a->boundX1() != b4->boundX1())
        || (a->boundX2() != b4->boundX2())
        || (a->boundY1() != b4->boundY1())
        || (a->boundY2() != b4->boundY2())
        || (a->boundZ1() != b4->boundZ1())
        || (a->boundZ2() != b4->boundZ2())
       ) comparison(a, b4);

  }

  for (int i = 0; i < 1000; i++) {

    voxel_c *a = new voxel_c(16, 8, 4);

    for (int p = 0; p < 10; p++)
      a->set(rand() % 0x1FF, 1);

    voxel_c *b0 = new voxel_c(a);
    voxel_c *b1 = new voxel_c(a);
    voxel_c *b2 = new voxel_c(a);
    voxel_c *b3 = new voxel_c(a);
    voxel_c *b4 = new voxel_c(a);
    b0->rotatey(0);
    b1->rotatey(1);
    b2->rotatey(2);
    b3->rotatey(3);
    b4->rotatey(4);

    printf("0\n");

    if (!(*a == *b0)
        || (a->boundX1() != b0->boundX1())
        || (a->boundX2() != b0->boundX2())
        || (a->boundY1() != b0->boundY1())
        || (a->boundY2() != b0->boundY2())
        || (a->boundZ1() != b0->boundZ1())
        || (a->boundZ2() != b0->boundZ2())
       ) comparison(a, b0);

    a->rotatey(1);

    printf("1\n");
    if (!(*a == *b1)
        || (a->boundX1() != b1->boundX1())
        || (a->boundX2() != b1->boundX2())
        || (a->boundY1() != b1->boundY1())
        || (a->boundY2() != b1->boundY2())
        || (a->boundZ1() != b1->boundZ1())
        || (a->boundZ2() != b1->boundZ2())
       ) comparison(a, b1);

    a->rotatey(1);

    printf("2\n");
    if (!(*a == *b2)
        || (a->boundX1() != b2->boundX1())
        || (a->boundX2() != b2->boundX2())
        || (a->boundY1() != b2->boundY1())
        || (a->boundY2() != b2->boundY2())
        || (a->boundZ1() != b2->boundZ1())
        || (a->boundZ2() != b2->boundZ2())
       ) comparison(a, b2);

    a->rotatey(1);

    printf("3\n");
    if (!(*a == *b3)
        || (a->boundX1() != b3->boundX1())
        || (a->boundX2() != b3->boundX2())
        || (a->boundY1() != b3->boundY1())
        || (a->boundY2() != b3->boundY2())
        || (a->boundZ1() != b3->boundZ1())
        || (a->boundZ2() != b3->boundZ2())
       ) comparison(a, b3);

    a->rotatey(1);

    printf("4\n");
    if (!(*a == *b4)
        || (a->boundX1() != b4->boundX1())
        || (a->boundX2() != b4->boundX2())
        || (a->boundY1() != b4->boundY1())
        || (a->boundY2() != b4->boundY2())
        || (a->boundZ1() != b4->boundZ1())
        || (a->boundZ2() != b4->boundZ2())
       ) comparison(a, b4);

  }


  for (int i = 0; i < 1000; i++) {

    voxel_c *a = new voxel_c(16, 8, 4);

    for (int p = 0; p < 10; p++)
      a->set(rand() % 0x1FF, 1);

    voxel_c *b0 = new voxel_c(a);
    voxel_c *b1 = new voxel_c(a);
    voxel_c *b2 = new voxel_c(a);
    voxel_c *b3 = new voxel_c(a);
    voxel_c *b4 = new voxel_c(a);
    b0->rotatez(0);
    b1->rotatez(1);
    b2->rotatez(2);
    b3->rotatez(3);
    b4->rotatez(4);

    printf("0\n");

    if (!(*a == *b0)
        || (a->boundX1() != b0->boundX1())
        || (a->boundX2() != b0->boundX2())
        || (a->boundY1() != b0->boundY1())
        || (a->boundY2() != b0->boundY2())
        || (a->boundZ1() != b0->boundZ1())
        || (a->boundZ2() != b0->boundZ2())
       ) comparison(a, b0);

    a->rotatez(1);

    printf("1\n");
    if (!(*a == *b1)
        || (a->boundX1() != b1->boundX1())
        || (a->boundX2() != b1->boundX2())
        || (a->boundY1() != b1->boundY1())
        || (a->boundY2() != b1->boundY2())
        || (a->boundZ1() != b1->boundZ1())
        || (a->boundZ2() != b1->boundZ2())
       ) comparison(a, b1);

    a->rotatez(1);

    printf("2\n");
    if (!(*a == *b2)
        || (a->boundX1() != b2->boundX1())
        || (a->boundX2() != b2->boundX2())
        || (a->boundY1() != b2->boundY1())
        || (a->boundY2() != b2->boundY2())
        || (a->boundZ1() != b2->boundZ1())
        || (a->boundZ2() != b2->boundZ2())
       ) comparison(a, b2);

    a->rotatez(1);

    printf("3\n");
    if (!(*a == *b3)
        || (a->boundX1() != b3->boundX1())
        || (a->boundX2() != b3->boundX2())
        || (a->boundY1() != b3->boundY1())
        || (a->boundY2() != b3->boundY2())
        || (a->boundZ1() != b3->boundZ1())
        || (a->boundZ2() != b3->boundZ2())
       ) comparison(a, b3);

    a->rotatez(1);

    printf("4\n");
    if (!(*a == *b4)
        || (a->boundX1() != b4->boundX1())
        || (a->boundX2() != b4->boundX2())
        || (a->boundY1() != b4->boundY1())
        || (a->boundY2() != b4->boundY2())
        || (a->boundZ1() != b4->boundZ1())
        || (a->boundZ2() != b4->boundZ2())
       ) comparison(a, b4);

  }
}

/* returns a number the corresponds to the opposite angle color */
int opposite(int i) {
  if (i == 1) return 1;
  return i + 10;
}

/* this function is there to calculate all possible variations of a parallelepiped
 * and its symmetries, of which the cube is only one variation
 */
void epipedize(void) {

  voxel_c v(4, 4, 4);

  v.setAll(voxel_c::VX_FILLED);

  for (int l1 = 1; l1 < 4; l1++)
    for (int l2 = 1; l2 < 4; l2++)
      for (int l3 = 1; l3 < 4; l3++)
        for (int a1 = 1; a1 < 5; a1++)
          for (int a2 = 1; a2 < 5; a2++)
            for (int a3 = 1; a3 < 5; a3++) {

              /* setup of the cube */

              // l1 = x length
              v.setColor(1, 0, 0, l1); v.setColor(2, 0, 0, l1);
              v.setColor(1, 3, 0, l1); v.setColor(2, 3, 0, l1);
              v.setColor(1, 0, 3, l1); v.setColor(2, 0, 3, l1);
              v.setColor(1, 3, 3, l1); v.setColor(2, 3, 3, l1);

              // l2 = y length
              v.setColor(0, 1, 0, l2); v.setColor(0, 2, 0, l2);
              v.setColor(3, 1, 0, l2); v.setColor(3, 2, 0, l2);
              v.setColor(0, 1, 3, l2); v.setColor(0, 2, 3, l2);
              v.setColor(3, 1, 3, l2); v.setColor(3, 2, 3, l2);

              // l3 = z length
              v.setColor(0, 0, 1, l3); v.setColor(0, 0, 2, l3);
              v.setColor(3, 0, 1, l3); v.setColor(3, 0, 2, l3);
              v.setColor(0, 3, 1, l3); v.setColor(0, 3, 2, l3);
              v.setColor(3, 3, 1, l3); v.setColor(3, 3, 2, l3);

              // a1 = angle between x and y
              v.setColor(1, 1, 0, a1); v.setColor(2, 2, 0, a1);
              v.setColor(1, 1, 3, a1); v.setColor(2, 2, 3, a1);
              v.setColor(1, 2, 0, opposite(a1)); v.setColor(2, 1, 0, opposite(a1));
              v.setColor(1, 2, 3, opposite(a1)); v.setColor(2, 1, 3, opposite(a1));

              // a2 = angle between x and z
              v.setColor(1, 0, 1, a2); v.setColor(2, 0, 2, a2);
              v.setColor(1, 3, 1, a2); v.setColor(2, 3, 2, a2);
              v.setColor(1, 0, 2, opposite(a2)); v.setColor(2, 0, 1, opposite(a2));
              v.setColor(1, 3, 2, opposite(a2)); v.setColor(2, 3, 1, opposite(a2));

              // a3 = angle between y and z
              v.setColor(0, 1, 1, a3); v.setColor(0, 2, 2, a3);
              v.setColor(3, 1, 1, a3); v.setColor(3, 2, 2, a3);
              v.setColor(0, 1, 2, opposite(a3)); v.setColor(0, 2, 1, opposite(a3));
              v.setColor(3, 1, 2, opposite(a3)); v.setColor(3, 2, 1, opposite(a3));

              // now printout
              printf("sym %5i for l %i %i %i a %i %i %i\n", v.selfSymmetries(), l1, l2, l3, a1, a2, a3);
            }

}
#endif
int main(int argv, char* args[]) {

//  grow(argv, args);
//  solve(argv, agrs);

//  testNewRots();

//  epipedize();

//    calcSymmetries(args[1]);
    hotspotCheck(args[1]);
}

