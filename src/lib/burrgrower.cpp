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
#include "burrgrower.h"

#if 0

#include "disassembler_0.h"
#include "disassembly.h"
#include "assembler_0.h"
#include "puzzle.h"
#include "voxel.h"
#include "gridtype.h"
#include "stdlib.h"

#include <fstream>

using namespace std;

puzzleSol_c::puzzleSol_c(puzzle_c * p, unsigned int prob) {

  puzzle = p;
  this->prob = prob;

  assembler_0_c *assm = new assembler_0_c(p->getGridType()->getAssemblerFrontend());
  if (assm->createMatrix(p, prob, false, false) == assembler_0_c::ERR_NONE) {

    solutions = 0;

    maxLevel = maxMoves = 0;
    minLevel = minMoves = (unsigned long)-1;

    assm->assemble(this);
  }

//  printf("maxLevel %i, maxMoves %i\n", maxLevel, maxMoves);

  delete assm;
}

puzzleSol_c::puzzleSol_c(const puzzleSol_c * p) {

  puzzle = new puzzle_c(p->puzzle);
  prob = p->prob;
  maxLevel = p->maxLevel;
  minLevel = p->minLevel;
  maxMoves = p->maxMoves;
  minMoves = p->minMoves;
  solutions = p->solutions;
}

bool puzzleSol_c::assembly(assembly_c* a) {


  disassembler_c * d = new disassembler_0_c(puzzle, prob);
  separation_c * da = d->disassemble(a);

  if (da) {

    solutions++;
    if ((solutions % 1000) == 0)
      printf("%li\n", solutions);

    unsigned int l = da->getMoves();

    if (l > maxLevel) maxLevel = l;
    if (l < minLevel) minLevel = l;

    l = da->sumMoves();

    if (l > maxMoves) maxMoves = l;
    if (l < minMoves) minMoves = l;

    delete da;
  }

  delete d;

  return false;
}

puzzleSol_c::~puzzleSol_c(void) {
  delete puzzle;
}

double puzzleSol_c::fitness(void) {

  double erg;

  erg = 200/solutions;

  erg += maxMoves * 100;
  erg += minMoves * 200;

  erg += maxLevel * 500;
  erg += minLevel * 300;

//  printf("sol %i mam %i mim %i mal %i mil %i erg %f", solutions, maxMoves, minMoves, maxLevel, minLevel, erg);

  return erg;
}

void burrGrower_c::grow(std::vector<puzzleSol_c*> currentSet) {

  std::vector<puzzleSol_c*> newSet;

  if (currentSet.size() == 0) {

    // create a new puzzle, and add it to the puzzle set
    puzzle_c *n = new puzzle_c(base);

    // copy the pieces, here we make all variable shapes empty
    for (unsigned int p = 0; p < n->probShapeNumber(problem); p++) {
      for (unsigned int ii = 0; ii < n->probGetShapeShape(problem, p)->getXYZ(); ii++)
        if (n->probGetShapeShape(problem, p)->getState(ii) == voxel_c::VX_VARIABLE)
          n->probGetShapeShape(problem, p)->setState(ii, voxel_c::VX_EMPTY);
    }

    puzzleSol_c * ps = new puzzleSol_c(n, problem);

    // if the base already has no solution we don't need to search any longer
    if (ps->nosol()) {
      delete ps;
      printf(" basis has no solution\n");
      return;
    }

    currentSet.push_back(ps);
  }

  while (true) {

    for (unsigned int p = 0; p < currentSet.size(); p++) {

      printf("%u / %u puzzlews grown\n", p, currentSet.size());

      for (unsigned int i = 0; i < base->probShapeNumber(problem); i++)
        for (unsigned int z = 0; z < base->probGetShapeShape(problem, i)->getXYZ(); z++) {
          if ((base->probGetShapeShape(problem, i)->getState(z) == voxel_c::VX_VARIABLE) &&
              (currentSet[p]->getPuzzle()->probGetShapeShape(problem, i)->getState(z) == voxel_c::VX_EMPTY) &&
               currentSet[p]->getPuzzle()->probGetShapeShape(problem, i)->neighbour(z, voxel_c::VX_FILLED)
             ) {

            puzzle_c *n = new puzzle_c(currentSet[p]->getPuzzle());
            n->probGetShapeShape(problem, i)->setState(z, voxel_c::VX_FILLED);

            puzzleSol_c * ps = new puzzleSol_c(n, problem);

            if (ps->nosol()) {
              delete ps;
            } else {
              addToLists(ps);
              newSet.push_back(ps);
            }
          }
        }
    }

    // merge current_puzzles into new_puzzles;
    for (unsigned int i = 0; i < currentSet.size(); i++)
      newSet.push_back(currentSet[i]);


    // save best x puzzles;
    // this needs to be some kind of genetic algorithm, where we
    // keep good ones, but some times also bad ones with a low chance
    printf("selecting ot of %i puzzles...\n", newSet.size());
    while (newSet.size() > maxSetSize) {

      // current alg: select one puzzle randomly, check the fitness, the higher
      // the less probable we will remove it.
      int pos = (rand() % newSet.size());

      double f = 80.0/newSet[pos]->fitness();

//      printf(" f %f ", f);

#ifdef _WIN32
      if (1 < f)
#else
      if (drand48() < f)
#endif
      {
//        printf(" removed");
        delete newSet[pos];
        std::vector<puzzleSol_c*>::iterator i = newSet.begin();
        while (pos) {
          i++;
          pos--;
        }

        newSet.erase(i);
      }
//      printf("\n");
    }

    printf(" left puzzles:\n");

    for (unsigned int i = 0; i < newSet.size(); i++) {
      double f = 80.0/newSet[i]->fitness();
      printf(" f %f\n", f);
    }


    // from time to time select one piece from one puzzle and replace it by the
    // simplified version in the base and add this new puzzle
    printf("simplifying\n");

    int pos = (rand() % newSet.size());
    int piece = (rand() % base->probShapeNumber(problem));

    puzzle_c * ps = new puzzle_c(newSet[pos]->getPuzzle());

    for (unsigned int ii = 0; ii < base->probGetShapeShape(problem, piece)->getXYZ(); ii++)
      if (base->probGetShapeShape(problem, piece)->getState(ii) == voxel_c::VX_VARIABLE)
        ps->probGetShapeShape(problem, piece)->setState(ii, voxel_c::VX_EMPTY);

    newSet.push_back(new puzzleSol_c(ps, problem));

    currentSet.clear();
    for (unsigned int i = 0; i < newSet.size(); i++)
      currentSet.push_back(newSet[i]);

    newSet.clear();
  }
}

void burrGrower_c::addToLists(puzzleSol_c * pz) {
  bool found;
  bool print = false;

  static unsigned int num = 0;

  if (pz->numSolutions() == 1) {
    // check into unqie list

    found = false;
    for (std::vector<puzzleSol_c*>::iterator i = unique.begin(); i < unique.end(); i++)
      if (((*i)->numLevel() < pz->numLevel()) ||
          (((*i)->numLevel() == pz->numLevel()) && ((*i)->numMoves() < pz->numMoves()))) {
        unique.insert(i, new puzzleSol_c(pz));
        found = true;
        if (i == unique.begin()) print = true;
        break;
      }

    if (!found) {
      unique.push_back(new puzzleSol_c(pz));
      print = unique.size() < 20;
    }

    if (unique.size() > 20) {
      std::vector<puzzleSol_c*>::iterator i = unique.end();
      i--;
      delete (*i);
      unique.erase(i);
    }
  }

  found = false;
  for (std::vector<puzzleSol_c*>::iterator i = highLevel.begin(); i < highLevel.end(); i++)
    if (((*i)->numLevel() < pz->numLevel()) ||
        (((*i)->numLevel() == pz->numLevel()) && ((*i)->numSolutions() > pz->numSolutions()))) {
      highLevel.insert(i, new puzzleSol_c(pz));
      if (i == highLevel.begin()) print = true;
      found = true;
      break;
    }
  if (!found) {
    highLevel.push_back(new puzzleSol_c(pz));
    print = highLevel.size() < 20;
  }

  if (highLevel.size() > 20) {
    std::vector<puzzleSol_c*>::iterator i = highLevel.end();
    i--;
    delete (*i);
    highLevel.erase(i);
  }

  found = false;
  for (std::vector<puzzleSol_c*>::iterator i = highMoves.begin(); i < highMoves.end(); i++)
    if (((*i)->numMoves() < pz->numMoves()) ||
        (((*i)->numMoves() == pz->numMoves()) && ((*i)->numSolutions() > pz->numSolutions()))) {
      highMoves.insert(i, new puzzleSol_c(pz));
      if (i == highMoves.begin()) print = true;
      found = true;
      break;
    }
  if (!found) {
    highMoves.push_back(new puzzleSol_c(pz));
    print = highMoves.size() < 20;
  }

  if (highMoves.size() > 20) {
    std::vector<puzzleSol_c*>::iterator i = highMoves.end();
    i--;
    delete (*i);
    highMoves.erase(i);
  }

  if (print) {

    char fname[100];
    snprintf(fname, 100, "grow/pz%06i_l%li_m%li.xmpuzzle", num++, pz->numLevel(), pz->numMoves());
    ofstream file(fname);
    file << pz->getPuzzle()->save();

    printf("unique: ");
    if (unique.size() != 0)
      printf("%li ", unique[0]->numLevel());
    else
      printf("-- ");

    printf("level: ");
    if (highLevel.size() != 0)
      printf("%li(%li) ", highLevel[0]->numLevel(), highLevel[0]->numSolutions());
    else
      printf("-- ");

    printf("moves: ");
    if (highMoves.size() != 0)
      printf("%li(%li) ", highMoves[0]->numMoves(), highMoves[0]->numSolutions());
    else
      printf("-- ");

    printf("\n");
  }
}

#endif
