/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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
#include "lib/disassembler_3.h"
#include "lib/print.h"

#include <fstream>

#include <xmlwrapp/xmlwrapp.h>

using namespace std;

bool disassemble;
bool printDisassemble;
bool printSolutions;

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

    if ((Assemblies & 0x3f) == 0)
      cout << Assemblies << " Assembies found so far\n";

    if (disassemble) {

      assemblyVoxel_c * assm = a->getVoxelSpace(puzzle, prob);

      disassembler_3_c d(assm, pn);
  
      separation_c * da = d.disassemble();
  
      if (da) {
        Solutions++;
        if (printSolutions)
          print(assm);
        printf("level: %i\n", da->getMoves());
        if (printDisassemble)
          print(da, assm);
        delete da;
      }

      delete assm;
  
    } else {

      if (printSolutions) {
        assemblyVoxel_c * assm = a->getVoxelSpace(puzzle, prob);
        print(assm);
        delete assm;
      }
    }

    return true;
  }
};

void usage(void) {

  cout << "burrTxt [options] file [options]\n\n";
  cout << "  file: puzzle file with the puzzle definition to solve\n\n";
  cout << "  -d try to disassemble and only print solutions that do disassemble\n";
  cout << "  -p print the disassembly plan\n";
  cout << "  -r reduce the placements bevore starting to solve the puzzle\n";
  cout << "  -s print the assemby\n";

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
  int filenumber = 0;
  bool reduce = false;

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
      else
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

  cout << " The puzzle:\n\n";

  print(&p);

  assembler_0_c *assm = new assm_0_frontend_0_c();

  switch (assm->createMatrix(&p, 0)) {
  case assm_0_frontend_0_c::ERR_TOO_MANY_UNITS:
    printf("%i units too many for the result shape\n", assm->getErrorsParam());
    return 0;
  case assm_0_frontend_0_c::ERR_TOO_FEW_UNITS:
    printf("%i units too few for the result shape\n", assm->getErrorsParam());
    return 0;
  case assm_0_frontend_0_c::ERR_CAN_NOT_PLACE:
    printf("Piece %i can be place nowhere in the result shape\n", assm->getErrorsParam());
    return 0;
  }

  if (reduce) {
    cout << "start reduce\n\n";
    assm->reduce();
    cout << "finished reduce\n\n";
  }

  asm_cb a(&p, 0);

  assm->assemble(&a);

  cout << a.Assemblies << " assemblies and " << a.Solutions << " solutions found with " << assm->getIterations() << " iterations\n";
  delete assm;

  return 0;
}


