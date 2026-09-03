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
#include "lib/disassembly.h"
#include "lib/print.h"
#include "lib/voxel.h"
#include "lib/solution.h"
#include "tools/xml.h"
#include "tools/gzstream.h"

#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

#include <fstream>
#include <iostream>

using namespace std;

bool disassemble;
bool allProblems;
bool printDisassemble;
bool printSolutions;
bool quiet;
bool jsonOutput;

disassembler_c * d;

#ifndef _WIN32
/** Silence library diagnostics on stderr during batch solves. */
class stderr_redirect_c {
  int saved_;

public:

  explicit stderr_redirect_c(bool enable) : saved_(-1) {
    if (!enable)
      return;
    fflush(stderr);
    saved_ = dup(STDERR_FILENO);
    if (saved_ < 0)
      return;
    int nullfd = open("/dev/null", O_WRONLY);
    if (nullfd >= 0) {
      dup2(nullfd, STDERR_FILENO);
      close(nullfd);
    }
  }

  ~stderr_redirect_c(void) {
    if (saved_ >= 0) {
      fflush(stderr);
      dup2(saved_, STDERR_FILENO);
      close(saved_);
    }
  }

private:

  stderr_redirect_c(const stderr_redirect_c&);
  void operator=(const stderr_redirect_c&);
};
#endif

/** Parse dot-separated move text the same way icsearch/process.py does. */
static bool parse_dotlevel(const char * dotlevel, int * level, int * totalmoves) {

  int parts[32];
  int count = 0;
  int current = 0;
  bool in_number = false;

  for (const char * p = dotlevel; ; p++) {
    if (*p >= '0' && *p <= '9') {
      in_number = true;
      current = current * 10 + (*p - '0');
    } else if (*p == '.' || *p == 0) {
      if (!in_number)
        return false;
      if (count >= 32)
        return false;
      parts[count++] = current;
      current = 0;
      in_number = false;
      if (*p == 0)
        break;
    } else {
      return false;
    }
  }

  if (count == 0)
    return false;

  *level = parts[0];
  int total = 0;
  for (int i = 0; i < count; i++)
    total += parts[i];
  *totalmoves = total;
  return true;
}

static bool level_is_better(int level, int totalmoves, int bestLevel, int bestTotalmoves) {
  if (level > bestLevel)
    return true;
  if (level == bestLevel && totalmoves > bestTotalmoves)
    return true;
  return false;
}

class asm_cb : public assembler_cb {

public:

  int Assemblies;
  int Solutions;
  int pn;
  problem_c * puzzle;

  bool hasBest;
  char bestDotlevel[200];
  int bestLevel;
  int bestTotalmoves;

  asm_cb(problem_c * p) :
    Assemblies(0), Solutions(0), pn(p->getNumberOfPieces()), puzzle(p),
    hasBest(false), bestLevel(0), bestTotalmoves(0)
  {
    bestDotlevel[0] = 0;
  }

  void considerLevel(separation_c * da) {

    char lev[200];
    da->movesText(lev, 200);

    int level = 0;
    int totalmoves = 0;
    if (!parse_dotlevel(lev, &level, &totalmoves))
      return;

    if (!hasBest || level_is_better(level, totalmoves, bestLevel, bestTotalmoves)) {
      hasBest = true;
      strncpy(bestDotlevel, lev, sizeof(bestDotlevel));
      bestDotlevel[sizeof(bestDotlevel) - 1] = 0;
      bestLevel = level;
      bestTotalmoves = totalmoves;
    }
  }

  bool assembly(assembly_c * a) {

    Assemblies++;

    if (disassemble) {

      separation_c * da = d->disassemble(a);

      if (da) {
        Solutions++;

        if (jsonOutput) {
          considerLevel(da);
        } else {
          if (printSolutions)
            print(a, puzzle);

          if (!quiet || allProblems)
          {
            char lev[200];
            da->movesText(lev,200);
            printf("level: %s\n", lev);
          }

          if (printDisassemble)
            print(da, a, puzzle);
        }

        delete da;
      }

    } else if (printSolutions)
      print(a, puzzle);

    delete a;

    return true;
  }
};

struct json_result_c {

  int assemblies;
  int solutions;
  bool hasBest;
  char bestDotlevel[200];
  int bestLevel;
  int bestTotalmoves;

  json_result_c(void) :
    assemblies(0), solutions(0), hasBest(false), bestLevel(0), bestTotalmoves(0)
  {
    bestDotlevel[0] = 0;
  }

  void merge(const asm_cb & a) {

    assemblies += a.Assemblies;
    solutions += a.Solutions;

    if (a.hasBest && (!hasBest ||
          level_is_better(a.bestLevel, a.bestTotalmoves, bestLevel, bestTotalmoves))) {
      hasBest = true;
      strncpy(bestDotlevel, a.bestDotlevel, sizeof(bestDotlevel));
      bestDotlevel[sizeof(bestDotlevel) - 1] = 0;
      bestLevel = a.bestLevel;
      bestTotalmoves = a.bestTotalmoves;
    }
  }
};

static void print_json_result(const json_result_c & stats) {

  printf("{\"assemblies\":%d,\"solutions\":%d",
      stats.assemblies, stats.solutions);

  if (stats.hasBest)
    printf(",\"dotlevel\":\"%s\",\"level\":%d,\"totalmoves\":%d",
        stats.bestDotlevel, stats.bestLevel, stats.bestTotalmoves);
  else
    printf(",\"dotlevel\":null,\"level\":null,\"totalmoves\":null");

  printf("}\n");
}

void usage(void) {

  cout << "burrTxt [options] file [options]\n\n";
  cout << "  file: puzzle file with the puzzle definition to solve\n\n";
  cout << "  --json  machine-readable result for batch tools (implies -d -q -r;\n";
  cout << "          prints one JSON object with the highest disassembly level)\n";
  cout << "  Short options may be combined (e.g. -dq, -rq).\n";
  cout << "  -d    try to disassemble and only print solutions that do disassemble\n";
  cout << "  -p    print the disassembly plan\n";
  cout << "  -r    reduce the placements before starting to solve the puzzle\n";
  cout << "  -s    print the assembly\n";
  cout << "  -q    be quiet and only print statistics\n";
  cout << "  -n    don't print a newline at the end of the line\n";
  cout << "  -o n  select the problem to solve\n";
  cout << "  -o all solves all problems in file\n";
  cout << "  -x    only redisassemble the given solutions\n";
  cout << "  -a    ask for information about the current puzzle, the next letters must be:\n";
  cout << "     s0 print solutions with only the used pieces\n";
  cout << "     s1 print solutions including the assemblies\n";
  cout << "     c  print comment\n";
}

int main(int argv, char* args[]) {

  if (argv < 1) {
    usage();
    return 2;
  }

  int state = 0;
  disassemble = false;
  allProblems = false;
  printDisassemble = false;
  printSolutions = false;
  quiet = false;
  bool assemble = true;
  unsigned int problem = 0;
  unsigned int firstProblem = 0;
  unsigned int lastProblem = 0;
  int filenumber = 0;
  bool reduce = false;
  bool newline = true;
  bool ask = false;
  enum {
    W_NUM_SOLUTIONS,
    W_SOLUTION_PIECES,
    W_SOLUTION_ASSM,
    W_COMMENT
  } what = W_COMMENT;

  for(int i = 1; i < argv; i++) {

    switch (state) {

    case 0:

      if (strcmp(args[i], "--json") == 0) {
        jsonOutput = true;
        disassemble = true;
        quiet = true;
        reduce = true;
      } else if (strcmp(args[i], "-o") == 0) {
        if (i + 1 >= argv) {
          usage();
          return 2;
        }
        if (strcmp(args[i+1],"all")==0)
          allProblems = true;
        else
          problem = atoi(args[i+1]);
        i++;
      } else if (strcmp(args[i], "-a") == 0) {

        if (i + 1 >= argv) {
          usage();
          return 2;
        }

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

      } else if (args[i][0] == '-' && args[i][1] != '-' && args[i][1] != 0) {

        for (int j = 1; args[i][j]; j++) {
          switch (args[i][j]) {
          case 'd':
            disassemble = true;
            break;
          case 'p':
            printDisassemble = true;
            break;
          case 's':
            printSolutions = true;
            break;
          case 'r':
            reduce = true;
            break;
          case 'n':
            newline = false;
            break;
          case 'x':
            assemble = false;
            break;
          case 'q':
            quiet = true;
            printDisassemble = false;
            printSolutions = false;
            break;
          case 'o':
          case 'a':
            fprintf(stderr, "burrTxt: -%c cannot be clustered; give it as its own argument\n", args[i][j]);
            return 2;
          default:
            fprintf(stderr, "burrTxt: unknown option -%c\n", args[i][j]);
            usage();
            return 2;
          }
        }

      } else
        filenumber = i;

      break;
    }
  }

  if (filenumber == 0) {
    usage();
    return 1;
  }

  if (jsonOutput && (ask || !assemble || allProblems || printDisassemble || printSolutions)) {
    fprintf(stderr, "burrTxt: --json cannot be combined with -a, -x, -o all, -p, or -s\n");
    return 2;
  }

  std::istream * str = openGzFile(args[filenumber]);
  xmlParser_c pars(*str);
  puzzle_c p(pars);
  delete str;

  if (ask) {

    switch (what) {
      case W_COMMENT:
        printf("%s\n", p.getComment().c_str());
        break;
      case W_NUM_SOLUTIONS:
        for (unsigned int i = 0; i < p.getNumberOfProblems(); i++)
          printf("number of solutions for problem %i: %li\n", i, p.getProblem(i)->getNumSolutions());
        break;
      case W_SOLUTION_PIECES:
      case W_SOLUTION_ASSM:
        for (unsigned int i = 0; i < p.getNumberOfProblems(); i++) {
          printf("problem %i\n", i);
          for (unsigned int s = 0; s < p.getProblem(i)->getNumSolutions(); s++) {

            printf("%03i: ", s+1);
            const assembly_c * a = p.getProblem(i)->getSavedSolution(s)->getAssembly();

            unsigned int pnum = 0;

            for (unsigned int pie = 0; pie < p.getProblem(i)->getNumberOfParts(); pie++) {
              for (unsigned int pp = 0; pp < p.getProblem(i)->getPartMaximum(pie); pp++) {
                if (a->isPlaced(pnum)) {
                  printf("S%i ", p.getProblem(i)->getShapeIdOfPart(pie)+1);
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

    if (!quiet && !jsonOutput) {
      cout << " The puzzle:\n\n";
      print(&p);
    }

    json_result_c jsonStats;

#ifndef _WIN32
    stderr_redirect_c stderrQuiet(jsonOutput);
#endif

    for (unsigned int pr = firstProblem ; pr < lastProblem ; pr++) {

      problem_c * problem = p.getProblem(pr);

      assembler_c *assm = p.getGridType()->findAssembler(*problem);

      switch (assm->createMatrix(false, false, false)) {
      case assembler_c::ERR_TOO_MANY_UNITS:
        if (jsonOutput)
          fprintf(stderr, "%i units too many for the result shape\n", assm->getErrorsParam());
        else
          printf("%i units too many for the result shape\n", assm->getErrorsParam());
        return jsonOutput ? 1 : 0;
      case assembler_c::ERR_TOO_FEW_UNITS:
        if (jsonOutput)
          fprintf(stderr, "%i units too few for the result shape\n", assm->getErrorsParam());
        else
          printf("%i units too few for the result shape\n", assm->getErrorsParam());
        return jsonOutput ? 1 : 0;
      case assembler_c::ERR_CAN_NOT_PLACE:
        if (jsonOutput)
          fprintf(stderr, "Piece %i can be place nowhere in the result shape\n", assm->getErrorsParam());
        else
          printf("Piece %i can be place nowhere in the result shape\n", assm->getErrorsParam());
        return jsonOutput ? 1 : 0;
      case assembler_c::ERR_NONE:
        /* no error case */
        break;
      case assembler_c::ERR_PUZZLE_UNHANDABLE:
        if (jsonOutput)
          fprintf(stderr, "The puzzles contains features not yet supported by burrTxt\n");
        else
          printf("The puzzles contains features not yet supported by burrTxt\n");
        return jsonOutput ? 1 : 0;
      case assembler_c::ERR_CAN_NOT_RESTORE_VERSION:
      case assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX:
        /* all other errors should not occur */
        if (jsonOutput)
          fprintf(stderr, "Oops internal error\n");
        else
          printf("Oops internal error\n");
        return jsonOutput ? 1 : 0;
      }

      if (reduce) {
        if (!quiet && !jsonOutput)
          cout << "start reduce\n\n";
        assm->reduce();
        if (!quiet && !jsonOutput)
          cout << "finished reduce\n\n";
      }

      if (allProblems && !jsonOutput)
        cout << "problem: " << problem->getName() << endl;

      asm_cb a(problem);

      d = 0;
      if (disassemble)
        d = new disassembler_0_c(*problem);

      assm->assemble(&a);

      if (jsonOutput) {
        jsonStats.merge(a);
      } else {
        cout << a.Assemblies << " assemblies and " << a.Solutions << " solutions found with " << assm->getIterations() << " iterations ";

        if (newline)
          cout << endl;
      }

      delete assm;
      delete d;
      d = 0;
      assm = 0;
    }

    if (jsonOutput)
      print_json_result(jsonStats);
  } else {

    for (unsigned int pr = firstProblem ; pr < lastProblem; pr ++) {

      problem_c * problem = p.getProblem(pr);

      d = new disassembler_0_c(*problem);

      for (unsigned int sol = 0; sol < problem->getNumberOfSavedSolutions(); sol++) {

        if (problem->getSavedSolution(sol)->getAssembly()) {

          separation_c * da = d->disassemble(problem->getSavedSolution(sol)->getAssembly());

          if (da) {
            if (printSolutions)
              print(problem->getSavedSolution(sol)->getAssembly(), problem);

            if (!quiet)
              printf("level: %i\n", da->getMoves());

            if (printDisassemble)
              print(da, problem->getSavedSolution(sol)->getAssembly(),problem);
            delete da;
          }
        }
      }

      delete d;
    }
  }

  return 0;
}


