/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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
#include "lib/assembler_0.h"
#include "lib/assembly.h"
#include "lib/disassembler.h"
#include "lib/disassembly.h"
#include "lib/print.h"

#include <fstream>
#include <iostream>

#include <xmlwrapp/xmlwrapp.h>

using namespace std;

bool disassemble;
bool printDisassemble;
bool printSolutions;
bool quiet;

disassembler_c * d;

class asm_cb : public assembler_cb {

public:

  int Assemblies;
  int Solutions;
  int pn;
  puzzle_c * puzzle;
  int prob;

  asm_cb(puzzle_c * p, int pr) : Assemblies(0), Solutions(0), pn(p->probPieceNumber(pr)), puzzle(p), prob(pr) {}

  bool assembly(assembly_c * a) {


    Assemblies++;

    if (!quiet && ((Assemblies & 0x3f) == 0))
      cout << Assemblies << " Assembies found so far\n";

    if (disassemble) {

      separation_c * da = d->disassemble(a);

      if (da) {
        Solutions++;

        if (printSolutions)
          print(a, puzzle, prob);

        if (!quiet)
          printf("level: %i\n", da->getMoves());

        if (printDisassemble)
          print(da, a, puzzle, prob);
        delete da;
      }

    } else if (printSolutions)
      print(a, puzzle, prob);

    delete a;

    return true;
  }
};

void usage(void) {

  cout << "burrTxt [options] file [options]\n\n";
  cout << "  file: puzzle file with the puzzle definition to solve\n\n";
  cout << "  -d    try to disassemble and only print solutions that do disassemble\n";
  cout << "  -p    print the disassembly plan\n";
  cout << "  -r    reduce the placements bevore starting to solve the puzzle\n";
  cout << "  -s    print the assemby\n";
  cout << "  -q    be quiet and only print statistics\n";
  cout << "  -n    don't print a newline at the end of the line\n";
  cout << "  -o n  select the problem to solve\n";
  cout << "  -x    only redisassemble the given solutions\n";
}

int main(int argv, char* args[]) {

  if (argv < 1) {
    usage();
    return 2;
  }

  int state = 0;
  disassemble = false;
  printDisassemble = false;
  printSolutions = false;
  quiet = false;
  bool assemble = true;
  unsigned int problem = 0;
  int filenumber = 0;
  bool reduce = false;
  bool newline = true;

  for(int i = 1; i < argv; i++) {

    switch (state) {

    case 0:

      if (strcmp(args[i], "-d") == 0)
        disassemble = true;
      else if (strcmp(args[i], "-p") == 0)
        printDisassemble = true;
      else if (strcmp(args[i], "-s") == 0)
        printSolutions = true;
      else if (strcmp(args[i], "-r") == 0)
        reduce = true;
      else if (strcmp(args[i], "-n") == 0)
        newline = false;
      else if (strcmp(args[i], "-x") == 0)
        assemble = false;
      else if (strcmp(args[i], "-o") == 0) {
        problem = atoi(args[i+1]);
        i++;
      } else if (strcmp(args[i], "-q") == 0) {
        quiet = true;
        printDisassemble = false;
        printSolutions = false;
      } else
        filenumber = i;

      break;
    }
  }

  if (filenumber == 0) {
    usage();
    return 1;
  }

  xml::tree_parser parser(args[filenumber]);
  puzzle_c p(parser.get_document().get_root_node());

  if (assemble) {

    for (unsigned int i = 0; i < p.shapeNumber(); i++)
      p.getShape(i)->initHotspot();

    if (!quiet) {
      cout << " The puzzle:\n\n";
      print(&p);
    }

    assembler_0_c *assm = new assembler_0_c(p.getGridType()->getAssemblerFrontend());

    switch (assm->createMatrix(&p, problem)) {
      case assembler_0_c::ERR_TOO_MANY_UNITS:
        printf("%i units too many for the result shape\n", assm->getErrorsParam());
        return 0;
      case assembler_0_c::ERR_TOO_FEW_UNITS:
        printf("%i units too few for the result shape\n", assm->getErrorsParam());
        return 0;
      case assembler_0_c::ERR_CAN_NOT_PLACE:
        printf("Piece %i can be place nowhere in the result shape\n", assm->getErrorsParam());
        return 0;
    }

    if (reduce) {
      if (!quiet)
        cout << "start reduce\n\n";
      assm->reduce();
      if (!quiet)
        cout << "finished reduce\n\n";
    }

    asm_cb a(&p, problem);

    d = p.getGridType()->getDisassembler(&p, problem);

    assm->assemble(&a);

    cout << a.Assemblies << " assemblies and " << a.Solutions << " solutions found with " << assm->getIterations() << " iterations ";

    if (newline)
      cout << endl;

    delete assm;
    delete d;
    d = 0;
    assm = 0;

  } else {

    d = p.getGridType()->getDisassembler(&p, problem);

    for (int sol = 0; sol < p.probSolutionNumber(problem); sol++) {

      if (p.probGetAssembly(problem, sol)) {

        separation_c * da = d->disassemble(p.probGetAssembly(problem, sol));

        if (da) {
          if (printSolutions)
            print(p.probGetAssembly(problem, sol), &p, problem);

          if (!quiet)
            printf("level: %i\n", da->getMoves());

          if (printDisassemble)
            print(da, p.probGetAssembly(problem, sol), &p, problem);
          delete da;
        }
      }
    }

    delete d;
  }

  return 0;
}


