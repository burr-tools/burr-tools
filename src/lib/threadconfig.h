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
#ifndef __THREADCONFIG_H__
#define __THREADCONFIG_H__

/* The single place that decides how many worker threads each solver stage
 * uses. Every front end - burrTxt, burrTxt2, the GUI and the Python module -
 * goes through here, so the contract is identical everywhere.
 *
 * The two stages are configured separately because they run *concurrently*:
 * the disassembly pool is fed while the assembler is still searching, so it is
 * the sum of the two counts, not either one alone, that decides how much of
 * the machine is busy. See design/2026-09-21-thread-configuration.md.
 *
 * Resolution order, applied per stage (first match wins):
 *
 *   1. the explicitly requested count, when non-zero
 *   2. the stage's own environment variable
 *      (BURRTOOLS_ASSEMBLER_THREADS / BURRTOOLS_DISASSEMBLER_THREADS)
 *   3. BURRTOOLS_THREADS, which applies to both stages
 *   4. the stage default - see defaultAssemblerThreads/defaultDisassemblerThreads
 *
 * Every path is clamped into [1, MAX_THREADS], so a resolved count is always
 * directly usable to size a thread vector.
 */
namespace threadConfig {

  /* Upper bound on worker threads for one stage. Counts arrive from command
   * line flags, environment variables and Problem.solve(threads=...), none of
   * which is otherwise validated, and reach std::thread creation directly.
   */
  constexpr unsigned int MAX_THREADS = 256;

  /* Hardware threads of this machine, at least 1, clamped to MAX_THREADS.
   * std::thread::hardware_concurrency() is the portable query on all three
   * supported platforms; it may return 0 when it cannot tell, hence the floor.
   */
  unsigned int maxThreads(void);

  /* 60% of the machine, rounded down, at least 1.
   *
   * Not all of it: the assembler runs alongside the disassembly stage and,
   * in the GUI, alongside an interactive main thread. Leaving headroom keeps
   * the machine usable while solving.
   */
  unsigned int defaultAssemblerThreads(void);

  /* 1, which means inline: disassemblerPool_c spawns no threads at all and
   * disassembles on the assembler's callback thread. The pool only pays off
   * for puzzles with many assemblies, and it costs one disassembler_0_c -
   * with its own movement cache - per worker, so it is opt-in.
   */
  unsigned int defaultDisassemblerThreads(void);

  /* Apply the resolution order described above. `requested` of 0 means
   * "not specified", which is what every front end passes when the user did
   * not choose a value.
   */
  unsigned int resolveAssembler(unsigned int requested);
  unsigned int resolveDisassembler(unsigned int requested);

  /* Threads the disassembly stage actually spawns for a given count: 0 when
   * the count is 1, because 1 means inline - no worker and no merger thread
   * is created. This is why an (N, 1) pair fits a machine with N cores.
   */
  unsigned int disassemblerThreadCost(unsigned int disassemblerThreads);

  /* True when the two stages together would oversubscribe the machine, i.e.
   * assemblerThreads + disassemblerThreadCost(disassemblerThreads) exceeds
   * maxThreads(). Both counts are expected to be already resolved (>= 1).
   */
  bool exceedsBudget(unsigned int assemblerThreads, unsigned int disassemblerThreads);

  /* Shrink the pair until it fits within maxThreads(). The disassembly pool
   * gives way first - the assembler runs for the whole solve, and dropping
   * the pool back to inline costs no threads at all. A pool of exactly 1 is
   * meaningless, so when fewer than two threads are spare the disassembler
   * goes inline rather than keep a single useless worker.
   *
   * Neither value drops below 1, so on a single core machine the pair stays
   * (1, 1) - which costs nothing: serial assembly plus inline disassembly.
   */
  void fitToBudget(unsigned int * assemblerThreads, unsigned int * disassemblerThreads);

  /* Shared parser for a command line thread count, so every front end rejects
   * the same inputs with the same rules: decimal, no trailing characters, not
   * negative. Returns false and leaves *out untouched on bad input.
   */
  bool parseThreadArg(const char * text, unsigned int * out);
}

#endif
