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

#include "lib/puzzle.h"
#include "lib/problem.h"
#include "lib/assembler.h"
#include "lib/assembly.h"
#include "lib/disassembler.h"
#include "lib/disassembler_0.h"
#include "lib/disassemblerpool.h"
#include "lib/disassembly.h"
#include "lib/print.h"
#include "lib/rotationtables.h"
#include "lib/voxel.h"
#include "lib/solution.h"
#include "tools/xml.h"
#include "tools/gzstream.h"
#include "tools/envvars.h"
#include "tools/cliargs.h"

#include <stdlib.h>

#include <fstream>
#include <iostream>
#include <atomic>

using namespace std;

bool disassemble;
bool allProblems;
bool printDisassemble;
bool printSolutions;
bool quiet;
bool rotationMoves;

std::unique_ptr<disassembler_c> d;

/* the level line, with the number of rotation moves when there are any */
static void printLevel(const std::string & level, const separation_c * da) {
  unsigned int rotations = da->sumRotations();
  if (rotations)
    printf("level: %s, %u rotation%s\n", level.c_str(), rotations, rotations == 1 ? "" : "s");
  else
    printf("level: %s\n", level.c_str());
}

/* the rotation context for -R, nullptr without it: then no rotation code runs */
static std::shared_ptr<rotationContext_c> rotationContext(const problem_c & problem) {
  if (!rotationMoves)
    return nullptr;
  auto context = rotationContextFor(problem, defaultRotationSettings());
  if (!context)
    cerr << "rotation moves are only available for cube grids, ignoring -R\n";
  return context;
}

/* say so when limits made the search skip rotation moves */
static void reportSkippedRotations(const rotationContext_c * context) {
  if (!context)
    return;
  if (context->getSkipped() & rotationContext_c::SKIP_RADIUS)
    cerr << "note: rotation moves of groups wider than the rotation tables (radius "
         << context->tables().getRadius() << ") may have been missed\n";
  if (context->getSkipped() & rotationContext_c::SKIP_PIECES)
    cerr << "note: rotation moves were not considered in (sub)problems of more than 32 pieces\n";
}

class asm_cb : public assembler_cb {

public:

  std::atomic<int> Assemblies{0};
  std::atomic<int> Solutions{0};
  int pn;
  problem_c * puzzle;
  std::unique_ptr<disassemblerPool_c> pool;

  asm_cb(problem_c * p, unsigned int threads) : pn(p->getNumberOfPieces()), puzzle(p) {
    if (disassemble) {
      pool = std::make_unique<disassemblerPool_c>(
        *p,
        threads,
        [this](uint64_t /*seqNo*/, std::unique_ptr<assembly_c> a, std::unique_ptr<separation_c> da) {
          if (da) {
            Solutions++;

            if (printSolutions)
              print(a.get(), puzzle);

            if (!quiet || allProblems)
            {
              printLevel(da->movesText(), da.get());
            }

            if (printDisassemble)
              print(da.get(), a.get(), puzzle);
          }
        },
        rotationContext(*p)
      );
    }
  }

  bool assembly(std::unique_ptr<assembly_c> a) override {

    Assemblies++;

    if (disassemble) {
      pool->submit(std::move(a));
    } else if (printSolutions)
      print(a.get(), puzzle);

    return true;
  }

  void finish() {
    if (pool)
      pool->finish();
  }
};

/* The usage; with full (-h, --help) the environment variables too, which
 * a wrong command line would only scroll the usage away with. */
void usage(bool full = false) {

  cout << "burrTxt [options] file [options]\n\n";
  cout << "  file: puzzle file with the puzzle definition to solve\n\n";
  cout << "  -h, --help  this help, with the environment variables\n";
  cout << "  -d    try to disassemble and only print solutions that do disassemble\n";
  cout << "  -p    print the disassembly plan\n";
  cout << "  -r    reduce the placements bevore starting to solve the puzzle\n";
  cout << "  -R    with -d or -x: also allow pieces to turn by 90 degrees (cube grids)\n";
  cout << "  -s    print the assemby\n";
  cout << "  -q    be quiet and only print statistics\n";
  cout << "  -n    don't print a newline at the end of the line\n";
  cout << "  -o n  select the problem to solve\n";
  cout << "  -o all solves all problems in file\n";
  cout << "  -t n  set number of worker threads for solver (0 = auto)\n";
  cout << "  -x    only redisassemble the given solutions\n";
  cout << "  -a    ask for information about the current puzzle, the next letters must be:\n";
  cout << "     s0 print solutions with the only the used pieces\n";
  cout << "     s1 print solutions including the assemblies\n";
  cout << "     c  print comment\n";
  if (full)
    btenv::print(cout, { btenv::Scope::Solver });
}

int main(int argv, char* args[]) {

  if (argv < 1) {
    usage();
    return 2;
  }

  if (btcli::wantsHelp(argv, args)) {
    usage(true);
    return 0;
  }

  int state = 0;
  disassemble = false;
  allProblems = false;
  printDisassemble = false;
  printSolutions = false;
  quiet = false;
  rotationMoves = false;
  bool assemble = true;
  unsigned int problem = 0;
  unsigned int firstProblem = 0;
  unsigned int lastProblem = 0;
  int filenumber = 0;
  bool reduce = false;
  bool newline = true;
  bool ask = false;
  unsigned int threads = 0;
  enum {
    W_NUM_SOLUTIONS,
    W_SOLUTION_PIECES,
    W_SOLUTION_ASSM,
    W_COMMENT
  } what = W_COMMENT;

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
      else if (strcmp(args[i], "-R") == 0)
        rotationMoves = true;
      else if (strcmp(args[i], "-n") == 0)
        newline = false;
      else if (strcmp(args[i], "-x") == 0)
        assemble = false;
      else if (strcmp(args[i], "-t") == 0) {
        /* args[argv] is the null terminator, so the bound has to be checked
         * before dereferencing; strtol rather than atoi so that a negative or
         * non-numeric argument is rejected instead of wrapping to a huge
         * unsigned thread count
         */
        if (i+1 >= argv) {
          cout << "-t requires a numeric argument\n";
          return 1;
        }
        char * end = 0;
        long t = strtol(args[i+1], &end, 10);
        if ((end == args[i+1]) || (*end != '\0') || (t < 0)) {
          cout << "-t requires a non-negative number\n";
          return 1;
        }
        threads = (unsigned int)t;
        i++;
      } else if (strcmp(args[i], "-o") == 0) {
        if (strcmp(args[i+1],"all")==0)
          allProblems = true;
        else
          problem = atoi(args[i+1]);
        i++;
      } else if (strcmp(args[i], "-q") == 0) {
        quiet = true;
        printDisassemble = false;
        printSolutions = false;
      } else if (strcmp(args[i], "-a") == 0) {

        ask = true;
        what = W_NUM_SOLUTIONS;

        if (strcmp(args[i+1], "s0") == 0)
          what = W_SOLUTION_PIECES;
        else if (strcmp(args[i+1], "s1") == 0)
          what = W_SOLUTION_ASSM;
        else if (strcmp(args[i+1], "c") == 0)
          what = W_COMMENT;
        else
        {
          usage();
          return 2;
        }

        i++;

      } else
        filenumber = i;

      break;
    }
  }

  if (filenumber == 0) {
    usage();
    return 1;
  }

  auto str = openGzFile(args[filenumber]);
  if (!str) {
    printf("could not open input file \"%s\"\n", args[filenumber]);
    return 2;
  }
  xmlParser_c pars(*str);
  puzzle_c p(pars);

  if (ask) {

    switch (what) {
      case W_COMMENT:
        printf("%s\n", p.getComment().c_str());
        break;
      case W_NUM_SOLUTIONS:
        for (unsigned int i = 0; i < p.getNumberOfProblems(); i++)
          printf("number of solutions for problem %u: %lu\n", i, p.getProblem(i)->getNumSolutions());
        break;
      case W_SOLUTION_PIECES:
      case W_SOLUTION_ASSM:
        for (unsigned int i = 0; i < p.getNumberOfProblems(); i++) {
          printf("problem %u\n", i);
          for (unsigned int s = 0; s < p.getProblem(i)->getNumSolutions(); s++) {

            printf("%03u: ", s+1);
            const assembly_c * a = p.getProblem(i)->getSavedSolution(s)->getAssembly();

            unsigned int pnum = 0;

            for (unsigned int pie = 0; pie < p.getProblem(i)->getNumberOfParts(); pie++) {
              for (unsigned int pp = 0; pp < p.getProblem(i)->getPartMaximum(pie); pp++) {
                if (a->isPlaced(pnum)) {
                  printf("S%u ", p.getProblem(i)->getShapeIdOfPart(pie)+1);
                }
                pnum++;
              }
            }

            printf("\n");
            if (what == W_SOLUTION_ASSM)
              print(a, p.getProblem(i));
          }
        }


        break;

    }

    return 0;
  }

  /* not every space grid has a solver, and fewer still have a disassembler:
   * building one for such a grid fails on the missing movement cache inside
   * a worker, where nothing catches it */
  const unsigned int caps = p.getGridType()->getCapabilities();

  if (assemble && !(caps & gridType_c::CAP_ASSEMBLE)) {
    cout << "Sorry, this space grid doesn't have an assembler (yet)\n";
    return 2;
  }

  if ((disassemble || !assemble) && !(caps & gridType_c::CAP_DISASSEMBLE)) {
    cout << "Sorry, this space grid doesn't have a disassembler (yet)\n";
    if (assemble)
      cout << "drop -d to just assemble the puzzle\n";
    return 2;
  }

  if (allProblems)
    {
      firstProblem = 0;
      lastProblem = p.getNumberOfProblems();
    }
  else
    {
      firstProblem = problem;
      lastProblem = problem+1;
    }
  if (assemble) {

    for (unsigned int i = 0; i < p.getNumberOfShapes(); i++)
      p.getShape(i)->initHotspot();

    if (!quiet) {
      cout << " The puzzle:\n\n";
      print(&p);
    }




    for (unsigned int pr = firstProblem ; pr < lastProblem ; pr++) {

      problem_c * problem = p.getProblem(pr);

      auto assm = p.getGridType()->findAssembler(*problem);

      switch (assm->createMatrix(false, false, false)) {
      case assembler_c::ERR_TOO_MANY_UNITS:
        printf("%i units too many for the result shape\n", assm->getErrorsParam());
        return 0;
      case assembler_c::ERR_TOO_FEW_UNITS:
        printf("%i units too few for the result shape\n", assm->getErrorsParam());
        return 0;
      case assembler_c::ERR_CAN_NOT_PLACE:
        printf("Piece %i can be place nowhere in the result shape\n", assm->getErrorsParam());
        return 0;
      case assembler_c::ERR_NONE:
        /* no error case */
        break;
      case assembler_c::ERR_PUZZLE_UNHANDABLE:
        printf("The puzzles contains features not yet supported by burrTxt\n");
        return 0;
      case assembler_c::ERR_CAN_NOT_RESTORE_VERSION:
      case assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX:
      case assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED:
      case assembler_c::ERR_CAN_NOT_START:
        /* all other errors should not occur (CAN_NOT_START is rejected at
         * run/setAssembler level, never by matrix creation) */
        printf("Oops internal error\n");
        return 0;
      }

      if (reduce) {
        if (!quiet)
          cout << "start reduce\n\n";
        assm->reduce();
        if (!quiet)
          cout << "finished reduce\n\n";
      }

      if (allProblems)
        cout << "problem: " << problem->getName() << endl;

      asm_cb a(problem, threads);

      if (threads > 0)
        assm->setNumThreads(threads);

      assm->assemble(&a);
      a.finish();
      if (a.pool)
        reportSkippedRotations(a.pool->getRotationContext());

      cout << a.Assemblies.load() << " assemblies and " << a.Solutions.load() << " solutions found with " << assm->getIterations() << " iterations ";

      if (newline)
        cout << endl;
    }
  } else {

    for (unsigned int pr = firstProblem ; pr < lastProblem; pr ++) {

      problem_c * problem = p.getProblem(pr);

      auto context = rotationContext(*problem);
      d = std::make_unique<disassembler_0_c>(*problem, context);

      for (unsigned int sol = 0; sol < problem->getNumberOfSavedSolutions(); sol++) {

        if (problem->getSavedSolution(sol)->getAssembly()) {

          auto da = d->disassemble(problem->getSavedSolution(sol)->getAssembly());

          if (da) {
            if (printSolutions)
              print(problem->getSavedSolution(sol)->getAssembly(), problem);

            if (!quiet)
              printLevel(std::to_string(da->getMoves()), da.get());

            if (printDisassemble)
              print(da.get(), problem->getSavedSolution(sol)->getAssembly(),problem);
          }
        }
      }

      d.reset();
      reportSkippedRotations(context.get());
    }
  }

  return 0;
}


