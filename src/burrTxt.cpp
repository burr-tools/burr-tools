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
#include <cstdlib>
#include <string>
#include <optional>
#include "lib/assembler.h"
#include "lib/assembly.h"
#include "lib/disassembler.h"
#include "lib/disassembler_0.h"
#include "lib/disassembly.h"
#include "lib/print.h"
#include "lib/rotationtables.h"
#include "lib/solver/solversession.h"
#include "lib/solver/solverspec.h"
#include "lib/voxel.h"
#include "lib/solution.h"
#include "lib/converter.h"
#include <memory>
#include <stdexcept>
#include "tools/xml.h"
#include "tools/gzstream.h"
#include "tools/envvars.h"
#include "tools/cliargs.h"

#include <stdio.h>
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
bool reportPlay;
/* what the command line chooses; resolved against the environment and the
 * defaults once the arguments are read */
solverConfig_t solverConfig;
/* -m: the model of the pieces, in place of what the puzzle file stores */
std::optional<rotationGeometry::model_t> cliModel;
/* -w: the file the solved puzzle is written to, nullptr without it */
const char * writeFile = nullptr;
solverSettings_t settings;

std::unique_ptr<disassembler_c> d;

/* with -P, the play a disassembly with turns needs; this also replaces
 * the turns' paths by those with the most room (see optimizeTurns). Slow:
 * only when the play is printed or stored (used) */
static std::optional<turnPlay_t> measurePlay(separation_c * da, const assembly_c * a, const solverSpec_t & problem, bool used) {
  if (!reportPlay || !used || !da->sumRotations())
    return std::nullopt;
  return optimizeTurns(da, a, problem, settings.rotation);
}

/* the level line, with the number of rotation moves when there are any,
 * and the play measurePlay found */
static void printLevel(const std::string & level, const separation_c * da, const std::optional<turnPlay_t> & play) {
  unsigned int rotations = da->sumRotations();
  if (!rotations) {
    printf("level: %s\n", level.c_str());
    return;
  }
  printf("level: %s, %u rotation%s", level.c_str(), rotations, rotations == 1 ? "" : "s");
  if (play) {
    if (play->failed)
      printf(", a turn needs more play than the planner tries");
    else
      printf(", works at play %.4f, not below %.4f", play->upper, play->lower);
    if (play->bevelFailed)
      printf("; at this play no bevel makes every turn");
    else
      printf("; at this play it works with bevel %.4f, not below %.4f", play->bevelUpper, play->bevelLower);
  }
  printf("\n");
}

/* -w: keep the measured play with the solution, at the problem's bevel,
 * and the bevel at its play */
static void keepPlay(solution_c * s, const std::optional<turnPlay_t> & play, const solverSpec_t & problem) {
  if (play && !play->failed) {
    const rotationGeometry::model_t model = rotationSettingsFor(problem, settings.rotation).model;
    solutionPlay_t p{ play->upper, play->lower, model.chamfer };
    if (!play->bevelFailed) {
      p.hasBevel = true;
      p.bevelUpper = play->bevelUpper;
      p.bevelLower = play->bevelLower;
      p.bevelPlay = model.play;
    }
    s->setPlay(p);
  }
}

/* the rotation context for -R, nullptr without it: then no rotation code runs */
static std::shared_ptr<rotationContext_c> rotationContext(std::shared_ptr<const solverSpec_t> problem) {
  if (!rotationMoves)
    return nullptr;
  auto context = rotationContextFor(problem, settings.rotation);
  if (!context && settings.rotation.mode != ROTATION_OFF)
    cerr << "rotation moves are only available for cube grids, ignoring -R\n";
  return context;
}

/* An internal assertion or another exception ended the solve: say so on
 * stderr (the caller exits non-zero). True when the session failed that way. */
static bool reportSolverFailure(const solverSession_c & session) {
  if (session.sessionState() != solverState_e::Error)
    return false;
  if (session.currentAction() == solverSession_c::ACT_ASSERT) {
    const assert_exception & e = session.getAssertException();
    cerr << "\nInternal error in BurrTools\n";
    cerr << " file      : " << (e.file ? e.file : "?") << "\n";
    cerr << " function  : " << (e.function ? e.function : "?") << "\n";
    cerr << " line      : " << e.line << "\n";
    cerr << " expression: " << (e.expr ? e.expr : "?") << "\n";
    cerr << "You should send the puzzle file to the programmer!\n";
    return true;
  }
  if (session.exception()) {
    cerr << "\nInternal error in BurrTools: " << session.exceptionMessage() << "\n";
    return true;
  }
  return false;
}

/* say so when limits made the search skip rotation moves */
static void reportSkippedRotations(const rotationContext_c * context) {
  if (!context)
    return;
  if (context->getSkipped() & rotationContext_c::SKIP_RADIUS)
    cerr << "note: rotation moves of groups wider than the rotation tables (radius "
         << context->tables().getRadius() << ") may have been missed\n";
  if (context->getSkipped() & rotationContext_c::SKIP_PLANNER)
    cerr << "note: the planner left some turns undecided, which may have been missed\n";
  if (context->getSkipped() & rotationContext_c::SKIP_PIECES)
    cerr << "note: rotation moves were not considered in (sub)problems of more than 32 pieces\n";
  if (context->getSkipped() & rotationContext_c::SKIP_GROUPS)
    cerr << "note: with very many loosely blocked pieces, turns of several groups together were not tried\n";

  /* BURRTOOLS_ROT_STATS=1: how the turns were decided (benchmarking) */
  if (settings.rotation.stats) {
    const rotationContext_c::stats_t st = context->getStats();
    fprintf(stderr, "rotation stats: candidates %llu family %llu gate %llu planned %llu possible %llu impossible %llu "
            "undecided %llu planner %.3f s impossible %.3f s worst %.3f s\n",
            (unsigned long long)st.candidates, (unsigned long long)st.family, (unsigned long long)st.gate,
            (unsigned long long)st.planned, (unsigned long long)st.possible, (unsigned long long)st.impossible,
            (unsigned long long)st.undecided, st.plannerSeconds, st.impossibleSeconds, st.worstSeconds);
  }
}

/* The usage; with full (-h, --help) the environment variables too, which
 * a wrong command line would only scroll the usage away with. */
void usage(bool full = false) {

  cout << "burrTxt [options] file [options]\n\n";
  cout << "  file: puzzle file with the puzzle definition to solve\n\n";
  cout << "  -h, --help  this help, with the environment variables\n";
  cout << "  -d    try to disassemble and only print solutions that do disassemble\n";
  cout << "  -p    print the disassembly plan\n";
  cout << "  -r    reduce the placements bevore starting to solve the puzzle\n";
  cout << "  -R [fast|balanced|slow]  with -d or -x: also allow pieces to turn by 90 degrees\n";
  cout << "        (cube grids). Every mode takes only turns proven at the model's play:\n";
  cout << "          fast      the fixed paths only\n";
  cout << "          balanced  (without a mode, unless BURRTOOLS_ROT_MODE says otherwise) also\n";
  cout << "                    plans turns the fixed paths make with more play\n";
  cout << "          slow      also plans every other turn whose end pose is free\n";
  cout << "  -m play,bevel  physical model for -R, in cube sizes, in place of the one the file\n";
  cout << "        stores (else 0.002,0.03, the STL export's, made for slides; turns usually\n";
  cout << "        need more, see -P): gap between cubes, length a 45 degree bevel cuts\n";
  cout << "        off each face; from puzzlecad's $burr_inset i, $burr_bevel b, $burr_scale s:\n";
  cout << "        play 2 i / s, bevel b / (1.414 s)\n";
  cout << "  -P    with -R: print the play each disassembly needs, and the bevel at the model's play,\n";
  cout << "        with optimized turn paths: it works at the upper value, no motion the planner\n";
  cout << "        tries works below the lower\n";
  cout << "  -w file  write the puzzle to file: the solutions found (or with -x, those\n";
  cout << "        disassembled again), the model they are for and with -P the measured play\n";
  cout << "  -D    solve the puzzle converted to the described grid (BurrTools NG's grid as data)\n";
  cout << "        its grid corresponds to: bricks, triangular prisms, rhombic, tetra-octa (cross-checks)\n";
  cout << "  --native-grids  keep triangular prism, rhombic and tetra-octa puzzles on their native grids\n";
  cout << "        does; without it they are read onto NG's grids, which -D then leaves as they are\n";
  cout << "  -s    print the assemby\n";
  cout << "  -q    be quiet and only print statistics\n";
  cout << "  -n    don't print a newline at the end of the line\n";
  cout << "  -o n  select the problem to solve\n";
  cout << "  -o all solves all problems in file\n";
  cout << "  -t n  set number of worker threads for solver (0 = auto, default: BURRTOOLS_THREADS, else auto)\n";
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
  reportPlay = false;
  bool turnOptions = false;   // -m or -P, which act on rotation moves
  bool assemble = true;
  unsigned int problem = 0;
  unsigned int firstProblem = 0;
  unsigned int lastProblem = 0;
  int filenumber = 0;
  bool reduce = false;
  bool newline = true;
  bool ask = false;
  bool describedGrid = false;  // -D
  bool nativeGrids = false;    // --native-grids
  enum {
    W_NUM_SOLUTIONS,
    W_SOLUTION_PIECES,
    W_SOLUTION_ASSM,
    W_COMMENT
  } what = W_COMMENT;

  for(int i = 1; i < argv; i++) {

    switch (state) {

    case 0:

      if (strcmp(args[i], "-D") == 0)
        describedGrid = true;
      else if (strcmp(args[i], "--native-grids") == 0)
        nativeGrids = true;
      else if (strcmp(args[i], "-d") == 0)
        disassemble = true;
      else if (strcmp(args[i], "-p") == 0)
        printDisassemble = true;
      else if (strcmp(args[i], "-s") == 0)
        printSolutions = true;
      else if (strcmp(args[i], "-r") == 0)
        reduce = true;
      else if (strcmp(args[i], "-R") == 0) {
        rotationMoves = true;
        /* an optional mode; without one BURRTOOLS_ROT_MODE, else balanced */
        if (i+1 < argv) {
          const std::optional<rotationMode_e> mode = rotationModeFromName(args[i+1]);
          if (mode) {
            solverConfig.rotation.mode = *mode;
            i++;
          }
        }
      }
      else if (strcmp(args[i], "-P") == 0)
        reportPlay = turnOptions = true;
      else if (strcmp(args[i], "-m") == 0) {
        double play, chamfer;
        char rest;
        if (i+1 >= argv || sscanf(args[i+1], "%lf,%lf%c", &play, &chamfer, &rest) != 2 ||
            !puzzle_c::validRotationModel({ play, chamfer })) {
          cout << "-m requires play,bevel with 0 < play < 1 and the bevel below half a cube\n";
          return 1;
        }
        cliModel = rotationGeometry::model_t{ play, chamfer };
        turnOptions = true;
        i++;
      }
      else if (strcmp(args[i], "-n") == 0)
        newline = false;
      else if (strcmp(args[i], "-w") == 0) {
        if (i+1 >= argv) {
          cout << "-w requires a file name\n";
          return 1;
        }
        writeFile = args[++i];
      }
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
        solverConfig.threads = (unsigned int)t;
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

  if (turnOptions && !(rotationMoves && (disassemble || !assemble)))
    cerr << "note: -m and -P only act with -R and -d or -x, ignoring them\n";

  try {
    settings = solverConfig.resolve();
  } catch (const std::invalid_argument & e) {
    cerr << e.what() << "\n";
    return 1;
  }

  auto str = openGzFile(args[filenumber]);
  if (!str) {
    printf("could not open input file \"%s\"\n", args[filenumber]);
    return 2;
  }
  xmlParser_c pars(*str);
  std::unique_ptr<puzzle_c> loaded = puzzle_c::read(pars, nativeGrids ? cutCubeGrids_e::nativeAll : cutCubeGrids_e::ng);
  /* -D: the puzzle on the described grid its grid converts to; one read
   * onto a described grid already stays */
  if (describedGrid && !loaded->getGridType()->isDescribed()) {
    puzzle_c * converted = convertToDescribed(loaded.get());
    if (!converted) {
      cerr << "-D: this puzzle's grid has no described counterpart\n";
      return 1;
    }
    loaded.reset(converted);
  }
  puzzle_c & p = *loaded;
  /* -m: the pieces as they are, the puzzle's model, replacing the file's */
  if (cliModel)
    p.setRotationModel(*cliModel);

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

  /* once, for every problem that checks turns (-d or -x) */
  if (rotationMoves && settings.rotation.mode != ROTATION_OFF && (disassemble || !assemble) && !p.getRotationModel())
    cerr << "note: turns are checked for the default play 0.002 and bevel 0.03, made for slides;"
            " turns usually need more: set them with -m\n";

  if (assemble) {

    for (unsigned int i = 0; i < p.getNumberOfShapes(); i++)
      p.getShape(i)->initHotspot();

    if (!quiet) {
      cout << " The puzzle:\n\n";
      print(&p);
    }




    for (unsigned int pr = firstProblem ; pr < lastProblem ; pr++) {

      problem_c * problem = p.getProblem(pr);

      if (writeFile) {
        /* the file says which pieces its disassemblies are for */
        if (rotationMoves)
          storeSolveRotationModel(p, settings.rotation);
        problem->removeAllSolutions();
      }

      const std::shared_ptr<const solverSpec_t> spec = makeSolverSpec(*problem);

      solverOptions_t opts;
      opts.reduce            = reduce;
      opts.disassemble       = disassemble;
      opts.keepRotations     = false;
      opts.completeRotations = false;
      opts.rotationMoves     = rotationMoves && settings.rotation.mode != ROTATION_OFF;
      /* with -d the solutions counted are the assemblies that come apart,
       * so the results are looked at (single-piece assemblies count as
       * assemblies only, as they always did here) */
      opts.countOnly         = !printSolutions && !printDisassemble && !writeFile && !disassemble;
      opts.settings          = settings;

      solverSession_c session(spec, opts);
      if (opts.disassemble && opts.rotationMoves && !session.rotationContext())
        cerr << "rotation moves are only available for cube grids, ignoring -R\n";

      if (reduce && !quiet)
        cout << "start reduce\n\n";

      session.start(/*stopAfterPreparation=*/true);
      session.join();

      if (reportSolverFailure(session))
        return 1;

      if (session.sessionState() == solverState_e::Error) {
        auto info = session.runInfo();
        const assembler_c::errState err = info ? info->error : assembler_c::ERR_PUZZLE_UNHANDABLE;
        const int param = info ? info->errorParam : 0;
        switch (err) {
        case assembler_c::ERR_TOO_MANY_UNITS:
          printf("%i units too many for the result shape\n", param);
          return 0;
        case assembler_c::ERR_TOO_FEW_UNITS:
          printf("%i units too few for the result shape\n", param);
          return 0;
        case assembler_c::ERR_CAN_NOT_PLACE:
          printf("Piece %i can be place nowhere in the result shape\n", param);
          return 0;
        case assembler_c::ERR_NONE:
          break;
        case assembler_c::ERR_PUZZLE_UNHANDABLE:
          printf("The puzzles contains features not yet supported by burrTxt\n");
          return 0;
        case assembler_c::ERR_CAN_NOT_RESTORE_VERSION:
        case assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX:
        case assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED:
        case assembler_c::ERR_CAN_NOT_START:
          printf("Oops internal error\n");
          return 0;
        }
      }

      if (reduce && !quiet)
        cout << "finished reduce\n\n";

      if (allProblems)
        cout << "problem: " << problem->getName() << endl;

      if (writeFile)
        problem->beginSolving();

      if (!session.resume()) {
        if (!reportSolverFailure(session))
          cerr << "the solver did not start\n";
        return 1;
      }

      uint64_t solutions = 0;
      if (opts.countOnly) {
        session.join();
      } else {
        while (auto r = session.pop()) {
          if (disassemble) {
            if (r->separation) {
              solutions++;
              const std::optional<turnPlay_t> play =
                measurePlay(r->separation.get(), r->assembly.get(), *spec, !quiet || allProblems || writeFile);

              if (printSolutions)
                print(r->assembly.get(), problem);

              if (!quiet || allProblems)
                printLevel(r->separation->movesText(), r->separation.get(), play);

              if (printDisassemble)
                print(r->separation.get(), r->assembly.get(), problem);

              if (writeFile) {
                auto guard = problem->lockSolutions();
                problem->addSolution(r->assembly.release(), r->separation.release());
                keepPlay(problem->getSavedSolution(problem->getNumberOfSavedSolutions() - 1), play, *spec);
                problem->incNumSolutions();
              }
            }
            if (writeFile)
              problem->incNumAssemblies();
          } else {
            if (printSolutions)
              print(r->assembly.get(), problem);
            if (writeFile) {
              problem->addSolution(r->assembly.release());
              problem->incNumAssemblies();
            }
          }
        }
        session.join();
      }

      if (session.sessionState() != solverState_e::Finished) {
        if (!reportSolverFailure(session))
          cerr << "the solver stopped before the search was complete\n";
        return 1;
      }

      reportSkippedRotations(session.rotationContext());

      cout << session.assembliesFound() << " assemblies and "
           << solutions << " solutions found with "
           << session.getIterations() << " iterations ";
      if (writeFile)
        problem->finishedSolving();

      if (newline)
        cout << endl;
    }
  } else {

    for (unsigned int pr = firstProblem ; pr < lastProblem; pr ++) {

      problem_c * problem = p.getProblem(pr);
      /* as when assembling: with -w the file says which pieces its
       * disassemblies are for */
      if (rotationMoves && writeFile)
        storeSolveRotationModel(p, settings.rotation);

      const std::shared_ptr<const solverSpec_t> spec = makeSolverSpec(*problem);
      auto context = rotationContext(spec);
      d = std::make_unique<disassembler_0_c>(spec, settings, context);

      for (unsigned int sol = 0; sol < problem->getNumberOfSavedSolutions(); sol++) {

        solution_c * solution = problem->getSavedSolution(sol);
        if (solution->getAssembly()) {

          auto da = d->disassemble(solution->getAssembly());

          /* no longer comes apart: the old disassembly was for other pieces
           * or other moves, and goes */
          if (!da && writeFile)
            solution->setDisassembly(std::unique_ptr<separation_c>());

          if (da) {
            const std::optional<turnPlay_t> play = measurePlay(da.get(), solution->getAssembly(), *spec, !quiet || writeFile);

            if (printSolutions)
              print(solution->getAssembly(), problem);

            if (!quiet)
              printLevel(std::to_string(da->getMoves()), da.get(), play);

            if (printDisassemble)
              print(da.get(), solution->getAssembly(), problem);

            if (writeFile) {
              solution->setDisassembly(std::move(da));
              keepPlay(solution, play, *spec);
            }
          }
        }
      }

      d.reset();
      reportSkippedRotations(context.get());
    }
  }

  if (writeFile) {
    try {
      p.save(writeFile);
    } catch (const std::runtime_error & e) {
      cerr << e.what() << "\n";
      return 2;
    }
  }

  return 0;
}


