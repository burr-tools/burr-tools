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
#include "lib/problemcollector.h"
#include "lib/solver/solversession.h"
#include "lib/bt_assert.h"
#include "lib/gridtype.h"
#include "lib/voxel.h"
#include "tools/xml.h"
#include "tools/gzstream.h"
#include "tools/envvars.h"
#include "tools/cliargs.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef _WIN32
#include <winsock.h>
#endif

using namespace std;

/* The usage; with full (-h, --help) the environment variables too, which
 * a wrong command line would only scroll the usage away with. */
void usage(bool full = false) {

  cout << "burrTxt2 [options] file [options]\n\n";
  cout << "  file: puzzle file with the puzzle definition to solve\n\n";
  cout << "  -h, --help  this help, with the environment variables\n";
  cout << "  -R    restart and throw away all found solutions, otherwise continue\n";
  cout << "  -d    try to disassemble and only keep solutions that do disassemble\n";
  cout << "  -c    just count solutions\n";
  cout << "  -m    (deprecated, ignored)\n";
  cout << "  -r    keep rotated solutions\n";
  cout << "  -p    drop disassemblies and replace by information about disassembly\n";
  cout << "  -b    selecte problem, else 0\n";
  cout << "  -t n  set number of worker threads for solver (0 = auto, default: BURRTOOLS_THREADS, else auto)\n";
  if (full)
    btenv::print(cout, { btenv::Scope::Solver });
}


bool checkInput(void)
{
  /* Initialize the file descriptor set. */
  fd_set set;
  FD_ZERO (&set);
  FD_SET (STDIN_FILENO, &set);

  /* Non-blocking check so the consumer loop keeps draining the bounded
   * result queue promptly while polling stdin for an interactive stop. */
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  /* select returns 0 if timeout, 1 if input available, -1 if error. */
  return select(FD_SETSIZE, &set, NULL, NULL, &timeout) > 0;
}

static void printError(assembler_c::errState err, int param) {
  cout << "\rerror: ";
  switch (err) {
    case assembler_c::ERR_TOO_MANY_UNITS:
      cout << "Pieces contain " << param << " units too many\n";
      break;
    case assembler_c::ERR_TOO_FEW_UNITS:
      cout << "Pieces contain " << param << " units less than required\n";
      break;
    case assembler_c::ERR_CAN_NOT_PLACE:
      cout << "Piece " << param+1 << " can be placed nowhere within the result\n";
      break;
    case assembler_c::ERR_CAN_NOT_RESTORE_VERSION:
      cout << "Impossible to restore the saved state because the internal format changed.\n";
      cout << "You either have to start from the beginning or finish with the old version of BurrTools, sorry\n";
      break;
    case assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX:
      cout << "Impossible to restore the saved state because something with the data is wrong.\n";
      cout << "You have to start from the beginning, sorry\n";
      break;
    case assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED:
      cout << "The saved search was stopped before it had any search state to save.\n";
      cout << "That can not be continued, so it has been reset -- please start it again\n";
      break;
    case assembler_c::ERR_PUZZLE_UNHANDABLE:
      cout << "Something went wrong the program can not solve your puzzle definitions.\n";
      cout << "You should send the puzzle file to the programmer!\n";
      break;
    case assembler_c::ERR_CAN_NOT_START:
      cout << "The problem can not be started in its current state, use -R to solve it again\n";
      break;
    case assembler_c::ERR_NONE:
      cout << "unknown\n";
      break;
  }
}

static int solve(int argv, char* args[]) {

  if (argv < 1) {
    usage();
    return 2;
  }

  if (btcli::wantsHelp(argv, args)) {
    usage(true);
    return 0;
  }

  solverOptions_t opts;
  opts.reduce = true;
  problemCollectorOptions_t collectorOpts;

  bool restart = false;
  int filenumber = 0;
  int firstProblem = 0;
  int lastProblem = 1;
  /* what the command line chooses, the rest from the environment */
  solverConfig_t config;

  for(int i = 1; i < argv; i++) {

    if (strcmp(args[i], "-R") == 0)
      restart = true;
    else if (strcmp(args[i], "-d") == 0) {
      opts.disassemble = true;
      collectorOpts.disassemble = true;
    }
    else if (strcmp(args[i], "-c") == 0)
      collectorOpts.countOnly = true;
    else if (strcmp(args[i], "-m") == 0)
      cerr << "-m is deprecated and ignored: mirror duplicates are always removed\n";
    else if (strcmp(args[i], "-r") == 0)
      opts.keepRotations = true;
    else if (strcmp(args[i], "-p") == 0)
      collectorOpts.dropDisassemblies = true;
    else if (strcmp(args[i], "-b") == 0) {
      firstProblem = atoi(args[i+1]);
      lastProblem = firstProblem + 1;
      i++;
    }
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
      config.threads = (unsigned int)t;
      i++;
    }
    else
      filenumber = i;
  }

  if (filenumber == 0) {
    usage();
    return 1;
  }

  solverSettings_t settings;
  try {
    settings = config.resolve();
  } catch (const std::invalid_argument & e) {
    cerr << e.what() << "\n";
    return 1;
  }
  opts.settings = settings;
  /* -c with -d still hands every disassembled assembly to the collector,
   * which counts them and keeps single-piece assemblies as solutions, as the
   * solve thread always did; only without -d is nothing enqueued at all */
  opts.countOnly = collectorOpts.countOnly && !opts.disassemble;

  auto str = openGzFile(args[filenumber]);
  if (!str) {
    cout << "could not open input file \"" << args[filenumber] << "\"\n";
    return 2;
  }
  xmlParser_c pars(*str);
  std::unique_ptr<puzzle_c> loaded = puzzle_c::read(pars);
  puzzle_c & p = *loaded;

  if (firstProblem < 0 || (unsigned int)firstProblem >= p.getNumberOfProblems()) {
    cout << "the file has " << p.getNumberOfProblems() << " problem(s), there is no problem "
         << firstProblem << "\n";
    return 2;
  }

  const unsigned int caps = p.getGridType()->getCapabilities();

  if (!(caps & gridType_c::CAP_ASSEMBLE)) {
    cout << "Sorry, this space grid doesn't have an assembler (yet)\n";
    return 2;
  }

  if (opts.disassemble && !(caps & gridType_c::CAP_DISASSEMBLE)) {
    cout << "Sorry, this space grid doesn't have a disassembler (yet)\n";
    cout << "drop -d to just assemble the puzzle\n";
    return 2;
  }

  std::string outname = args[filenumber];
  outname += "ttt";
  cout << "outputting into " << outname << std::endl;

  ofstream ostr(outname.c_str());
  if (!ostr)
  {
    cout << "Can not open output file, aborting\n";
    return 2;
  }

  for (unsigned int i = 0; i < p.getNumberOfShapes(); i++)
    p.getShape(i)->initHotspot();


  int exitCode = 0;

  for (int pr = firstProblem ; pr < lastProblem ; pr++) {

    problem_c * problem = p.getProblem(pr);

    if (restart)
      problem->removeAllSolutions();

    /* without -R we continue where an earlier run left off. A problem that is
     * already finished, or whose information was invalidated by editing, has
     * nothing to continue from.
     */
    if (!problem->canStartSolving()) {
      cout << "problem " << pr << " (" << problem->getName() << ") ";
      if (problem->getSolveState() == SS_SOLVED)
        cout << "is already solved: " << problem->getNumAssemblies() << " assemblies, "
             << problem->getNumSolutions() << " solutions\n";
      else
        cout << "has no state to continue from\n";
      cout << "use -R to solve it again\n";
      continue;
    }

    const time_t startTime = time(0);
    solverSession_c session(makeSolverSpec(*problem), opts);
    problemCollector_c collector(*problem, collectorOpts);
    collector.resetDropMultiplicator();

    bool usedSavedResume = false;
    if (problem->hasResumeData()) {
      usedSavedResume = true;
      resumeData_t res = problem->cloneResumeData();
      const unsigned int pieces = problem->getNumberOfPieces();
      std::erase_if(res.pending, [pieces](const std::unique_ptr<assembly_c> & a) {
        return !a || a->placementCount() != pieces;
      });
      session.setResume(std::move(res));
    } else {
      session.setPreparedHook([problem]() { problem->beginSolving(); });
    }

    if (!session.start(false)) {
      if (session.exception()) {
        cout << "\rException in Solver: " << session.exceptionMessage() << "\n";
        exitCode = 1;
      } else if (session.currentAction() == solverSession_c::ACT_ASSERT) {
        const auto & ex = session.getAssertException();
        cout << "\rException in Solver\n";
        cout << " file      : " << ex.file << "\n";
        cout << " function  : " << ex.function << "\n";
        cout << " line      : " << ex.line << "\n";
        cout << " expression: " << ex.expr << "\n";
        exitCode = 1;
      } else {
        auto info = session.runInfo();
        const assembler_c::errState err = info ? info->error : assembler_c::ERR_NONE;
        const int param = info ? info->errorParam : 0;
        if (usedSavedResume) {
          if (err == assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED) {
            problem->removeAllSolutions();
          } else if (err == assembler_c::ERR_CAN_NOT_RESTORE_VERSION ||
                     err == assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX) {
            problem->discardResumeEngineState();
          }
        }
        printError(err, param);
        exitCode = 1;
      }
      cout.flush();
      continue;
    }

    if (usedSavedResume)
      problem->clearResumeData();

    auto lastPrint = std::chrono::steady_clock::now() - std::chrono::seconds(1);

    while (session.sessionState() == solverState_e::Running) {
      if (checkInput()) {
        cout << "abborting \n";
        cout.flush();
        session.pause();
        break;
      }

      if (opts.countOnly) {
        if (problem->getSolveState() == SS_SOLVING)
          collector.syncCountOnly(session.assembliesFound(), session.solutionsFound());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      } else {
        if (auto item = session.popFor(std::chrono::milliseconds(50))) {
          collector.consume(std::move(*item));
          collector.drainFrom(session);
        }
      }

      const auto now = std::chrono::steady_clock::now();
      if (now - lastPrint >= std::chrono::milliseconds(500)) {
        lastPrint = now;
        const float finished = session.getProgress();
        switch (session.currentAction()) {
          case solverSession_c::ACT_PREPARATION:
            cout << "\rpreparing piece " << session.currentActionParameter()+1;
            break;
          case solverSession_c::ACT_REDUCE:
            cout << "\rreducing piece " << session.currentActionParameter()+1;
            break;
          case solverSession_c::ACT_ASSEMBLING:
            cout << "\rassembling " << finished*100 << "% done";
            break;
          case solverSession_c::ACT_DISASSEMBLING:
            cout << "\rdisassembling " << finished*100 << "% done";
            break;
          case solverSession_c::ACT_WAIT_TO_STOP:
            cout << "\rwaiting for solver to stop";
            break;
        }
        cout.flush();
      }
    }

    if (opts.countOnly) {
      session.join();
      if (problem->getSolveState() == SS_SOLVING)
        collector.syncCountOnly(session.assembliesFound(), session.solutionsFound());
    } else {
      collector.consumeAll(session);
      session.join();
    }

    switch (session.sessionState()) {
      case solverState_e::Finished:
        if (problem->getSolveState() == SS_UNSOLVED)
          problem->beginSolving();
        problem->addTime(time(0) - startTime);
        problem->finishedSolving();
        cout << "\rdone\n";
        break;
      case solverState_e::Paused:
        if (problem->getSolveState() == SS_UNSOLVED)
          problem->beginSolving();
        problem->setResumeData(session.state());
        problem->addTime(time(0) - startTime);
        cout << "\rstopped, the puzzle is not completely solved\n";
        break;
      case solverState_e::Error:
        if (session.exception()) {
          if (problem->getSolveState() == SS_SOLVING)
            problem->removeAllSolutions();
          cout << "\rException in Solver: " << session.exceptionMessage() << "\n";
          exitCode = 1;
        } else if (session.currentAction() == solverSession_c::ACT_ASSERT) {
          const auto & ex = session.getAssertException();
          if (problem->getSolveState() == SS_SOLVING)
            problem->removeAllSolutions();
          cout << "\rException in Solver\n";
          cout << " file      : " << ex.file << "\n";
          cout << " function  : " << ex.function << "\n";
          cout << " line      : " << ex.line << "\n";
          cout << " expression: " << ex.expr << "\n";
          exitCode = 1;
        } else {
          auto info = session.runInfo();
          printError(info ? info->error : assembler_c::ERR_NONE,
                     info ? info->errorParam : 0);
          exitCode = 1;
        }
        break;
      case solverState_e::Idle:
      case solverState_e::Running:
      case solverState_e::Stopped:
        cout << "\rstopped, the puzzle is not completely solved\n";
        break;
    }

    cout.flush();
  }

  xmlWriter_c xml(ostr);
  p.save(xml);

  return exitCode;
}

/* asserts reach here from everything main does outside the solver thread -
 * loading the file, building the solver. Letting one escape aborts the process
 * with nothing but the expression text, so report it the way the solver thread's
 * asserts are reported and exit cleanly.
 */
int main(int argv, char* args[]) {

  try {
    return solve(argv, args);
  }

  catch (const assert_exception & e) {
    cout << "\nInternal error in BurrTools\n";
    cout << " file      : " << (e.file ? e.file : "?") << "\n";
    cout << " function  : " << (e.function ? e.function : "?") << "\n";
    cout << " line      : " << e.line << "\n";
    cout << " expression: " << (e.expr ? e.expr : "?") << "\n";
    cout << "You should send the puzzle file to the programmer!\n";
    return 1;
  }
}
