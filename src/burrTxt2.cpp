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
#include "lib/solvethread.h"
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef _WIN32
#include <winsock.h>
#endif

using namespace std;

/* Where the messages go: stdout, unless the puzzle came through stdin. Its
 * result then goes to stdout, for a redirection or the next program in the
 * pipe, and every message to stderr, as not one may land in the result.
 * solve() picks it, main()'s report of an internal error follows it.
 */
static std::ostream * messageStream = &cout;

/* The usage; with full (-h, --help) the environment variables too, which
 * a wrong command line would only scroll the usage away with. */
void usage(std::ostream & out, bool full = false) {

  out << "burrTxt2 [options] [file] [options]\n\n";
  out << "  file: puzzle file with the puzzle definition to solve, compressed or\n";
  out << "        not; \"-\" or no file at all reads it from standard input\n";
  out << "        The result goes into the file's name with \"ttt\" added and input on\n";
  out << "        standard input stops the solver. For a puzzle read from standard\n";
  out << "        input the result goes to standard output, or nothing does when the\n";
  out << "        run fails; everything else goes to standard error, and only killing\n";
  out << "        the program stops it.\n\n";
  out << "  -h, --help  this help, with the environment variables\n";
  out << "  -R    restart and throw away all found solutions, otherwise continue\n";
  out << "  -d    try to disassemble and only keep solutions that do disassemble\n";
  out << "  -c    just count solutions\n";
  out << "  -m    keep mirror solutions\n";
  out << "  -r    keep rotated solutions\n";
  out << "  -p    drop disassemblies and replace by information about disassembly\n";
  out << "  -b    selecte problem, else 0\n";
  out << "  -t n  set number of worker threads for solver (without -t or with 0: BURRTOOLS_THREADS, else all cores)\n";
  if (full)
    btenv::print(out, { btenv::Scope::Solver });
}


/* Waits a second for the solver, and says whether the user asked to stop it by
 * giving input. A stdin the puzzle came through can not say that: it stays at
 * its end, which reads as ready for ever. */
bool checkInput(bool fromStdin)
{
  if (fromStdin) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return false;
  }

  /* Initialize the file descriptor set. */
  fd_set set;
  FD_ZERO (&set);
  FD_SET (STDIN_FILENO, &set);

  /* Initialize the timeout data structure. */
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  /* select returns 0 if timeout, 1 if input available, -1 if error. */
  return select(FD_SETSIZE, &set, NULL, NULL, &timeout);
}


static int solve(int argv, char* args[]) {

  if (argv < 1) {
    usage(cout);
    return 2;
  }

  if (btcli::wantsHelp(argv, args)) {
    usage(cout, true);
    return 0;
  }

  int par = solveThread_c::PAR_REDUCE;
  bool restart = false;
  int filenumber = 0;
  int firstProblem = 0;
  int lastProblem = 1;
  /* what the command line chooses, the rest from the environment */
  solverConfig_t config;

  /* only told once the file is known, and with it where messages go */
  const char * argError = nullptr;

  for(int i = 1; i < argv; i++) {

    if (strcmp(args[i], "-R") == 0)
      restart = true;
    else if (strcmp(args[i], "-d") == 0)
      par |= solveThread_c::PAR_DISASSM;
    else if (strcmp(args[i], "-c") == 0)
      par |= solveThread_c::PAR_JUST_COUNT;
    else if (strcmp(args[i], "-m") == 0)
      par |= solveThread_c::PAR_KEEP_MIRROR;
    else if (strcmp(args[i], "-r") == 0)
      par |= solveThread_c::PAR_KEEP_ROTATIONS;
    else if (strcmp(args[i], "-p") == 0)
      par |= solveThread_c::PAR_DROP_DISASSEMBLIES;
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
        if (!argError)
          argError = "-t requires a numeric argument\n";
        continue;
      }
      char * end = 0;
      long t = strtol(args[i+1], &end, 10);
      if ((end == args[i+1]) || (*end != '\0') || (t < 0) || (t > (long)solverConfig_t::maxThreads)) {
        if (!argError)
          argError = "-t requires a number from 0 to 4096\n";
      }
      /* 0 chooses nothing, as threads=0 in Python and the GUI */
      else if (t > 0)
        config.threads = (unsigned int)t;
      i++;
    }
    else
      filenumber = i;
  }

  const char * file = filenumber ? args[filenumber] : nullptr;
  const bool fromStdin = btcli::solverReadsStdin(file);

  if (fromStdin)
    messageStream = &cerr;

  std::ostream & log = *messageStream;

  if (argError) {
    log << argError;
    return 1;
  }

  if (!file && !fromStdin) {
    usage(log);
    return 1;
  }

  solverSettings_t settings;
  try {
    settings = config.resolve();
  } catch (const std::invalid_argument & e) {
    cerr << e.what() << "\n";
    return 1;
  }

  std::string stdinError;
  bool damaged = false;
  auto str = fromStdin ? btcli::openStdin(stdinError, damaged) : openGzFile(file);
  if (!str) {
    if (damaged) {
      log << "could not load the puzzle: " << stdinError << "\n";
      return 2;
    }
    if (fromStdin) {
      log << stdinError << "\n\n";
      usage(log);
      return 1;
    }
    log << "could not open input file \"" << file << "\"\n";
    return 2;
  }

  std::unique_ptr<puzzle_c> loaded;

  try {
    xmlParser_c pars(*str);
    loaded = std::make_unique<puzzle_c>(pars);
  }
  catch (const xmlParserException_c & e) {
    log << "could not load the puzzle: " << e.what() << "\n";
    return 2;
  }

  puzzle_c & p = *loaded;

  if (firstProblem < 0 || (unsigned int)firstProblem >= p.getNumberOfProblems()) {
    log << "the file has " << p.getNumberOfProblems() << " problem(s), there is no problem "
         << firstProblem << "\n";
    return 2;
  }

  /* not every space grid has a solver, and fewer still have a disassembler.
   * Check before we build one: the disassembler asserts on the movement cache
   * a grid without CAP_DISASSEMBLE does not provide, and that assert happens
   * while constructing the solver thread, where nothing catches it.
   */
  const unsigned int caps = p.getGridType()->getCapabilities();

  if (!(caps & gridType_c::CAP_ASSEMBLE)) {
    log << "Sorry, this space grid doesn't have an assembler (yet)\n";
    return 2;
  }

  if ((par & solveThread_c::PAR_DISASSM) && !(caps & gridType_c::CAP_DISASSEMBLE)) {
    log << "Sorry, this space grid doesn't have a disassembler (yet)\n";
    log << "drop -d to just assemble the puzzle\n";
    return 2;
  }

  ofstream ostr;

  if (!fromStdin) {
    const std::string outname = std::string(file) + "ttt";
    log << "outputting into " << outname << std::endl;

    ostr.open(outname.c_str());
    if (!ostr)
    {
      log << "Can not open output file, aborting\n";
      return 2;
    }
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
     * nothing to continue from, and setAssembler's precondition rejects one.
     */
    if (!problem->canStartSolving()) {
      log << "problem " << pr << " (" << problem->getName() << ") ";
      if (problem->getSolveState() == SS_SOLVED)
        log << "is already solved: " << problem->getNumAssemblies() << " assemblies, "
             << problem->getNumSolutions() << " solutions\n";
      else
        log << "has no state to continue from\n";
      log << "use -R to solve it again\n";
      continue;
    }

    solveThread_c assmThread(*problem, par, settings);

    if (!assmThread.start(false)) {
      log << "Could not start Solver\n";
      continue;
    }

    while (!assmThread.stopped()) {

      if (checkInput(fromStdin)) {
        log << "abborting \n";
        assmThread.stop();

        while (!assmThread.stopped())
#ifdef WIN32
          Sleep(1);
#else
          sleep(1);
#endif

        break;
      }

      float finished = (problem->getAssembler())
        ? problem->getAssembler()->getFinished()
        : 0;

      switch (assmThread.currentAction()) {
        case solveThread_c::ACT_PREPARATION:
          log << "\rpreparing piece " << assmThread.currentActionParameter()+1;
          break;
        case solveThread_c::ACT_REDUCE:
          log << "\rreducing piece " << assmThread.currentActionParameter()+1;
          break;
        case solveThread_c::ACT_ASSEMBLING:
          log << "\rassembling " << finished*100 << "% done";
          break;
        case solveThread_c::ACT_DISASSEMBLING:
          log << "\rdisassembling " << finished*100 << "% done";
          break;
        case solveThread_c::ACT_WAIT_TO_STOP:
          log << "\rwaiting for solver to stop";
          break;
      }

      log.flush();

    }

    /* report how the run ended. Each of these states means the worker has left
     * run() for good, so each needs an answer here; a state handled nowhere
     * leaves the loop above waiting on a thread that has already exited.
     */
    switch (assmThread.currentAction()) {
      case solveThread_c::ACT_FINISHED:
        log << "\rdone\n";
        break;
      case solveThread_c::ACT_PAUSING:
        log << "\rstopped, the puzzle is not completely solved\n";
        break;
      case solveThread_c::ACT_ASSERT:
        log << "\rException in Solver\n";
        log << " file      : " << assmThread.getAssertException().file << "\n";
        log << " function  : " << assmThread.getAssertException().function << "\n";
        log << " line      : " << assmThread.getAssertException().line << "\n";
        log << " expression: " << assmThread.getAssertException().expr << "\n";
        exitCode = 1;
        break;
      case solveThread_c::ACT_ERROR:
        log << "\rerror: ";
        switch (assmThread.getErrorState()) {
          case assembler_c::ERR_TOO_MANY_UNITS:
            log << "Pieces contain " << assmThread.getErrorParam() << " units too many\n";
            break;
          case assembler_c::ERR_TOO_FEW_UNITS:
            log << "Pieces contain " << assmThread.getErrorParam() << " units less than required\n";
            break;
          case assembler_c::ERR_CAN_NOT_PLACE:
            log << "Piece " << assmThread.getErrorParam()+1 << " can be placed nowhere within the result\n";
            break;
          case assembler_c::ERR_CAN_NOT_RESTORE_VERSION:
            log << "Impossible to restore the saved state because the internal format changed.\n";
            log << "You either have to start from the beginning or finish with the old version of BurrTools, sorry\n";
            break;
          case assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX:
            log << "Impossible to restore the saved state because something with the data is wrong.\n";
            log << "You have to start from the beginning, sorry\n";
            break;
          case assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED:
            log << "The saved search was stopped before it had any search state to save.\n";
            log << "That can not be continued, so it has been reset -- please start it again\n";
            break;
          case assembler_c::ERR_PUZZLE_UNHANDABLE:
            log << "Something went wrong the program can not solve your puzzle definitions.\n";
            log << "You should send the puzzle file to the programmer!\n";
            break;
          case assembler_c::ERR_CAN_NOT_START:
            log << "The problem can not be started in its current state, use -R to solve it again\n";
            break;
          case assembler_c::ERR_NONE:
            log << "unknown\n";
            break;
        }
        exitCode = 1;
        break;
    }

    log.flush();
  }

  /* what a pipe gets is a complete puzzle or nothing, so a run that failed
   * leaves stdout empty; one on a named file still writes its file */
  if (fromStdin && exitCode != 0)
    return exitCode;

  std::ostream & result = fromStdin ? cout : ostr;

  {
    xmlWriter_c xml(result);
    p.save(xml);
  }

  // a full disk or a closed pipe shows only here
  if (!result.flush()) {
    log << "could not write the result\n";
    return 2;
  }

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
    std::ostream & log = *messageStream;
    log << "\nInternal error in BurrTools\n";
    log << " file      : " << (e.file ? e.file : "?") << "\n";
    log << " function  : " << (e.function ? e.function : "?") << "\n";
    log << " line      : " << e.line << "\n";
    log << " expression: " << (e.expr ? e.expr : "?") << "\n";
    log << "You should send the puzzle file to the programmer!\n";
    return 1;
  }
}


