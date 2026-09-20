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
#include "threadconfig.h"

#include <algorithm>
#include <cstdlib>
#include <thread>

namespace threadConfig {

namespace {

  unsigned int clampCount(unsigned int v) {
    return std::clamp(v, 1u, MAX_THREADS);
  }

  /* 0 when the variable is unset, empty, non-numeric or not positive, which
   * is the same "not specified" signal the requested counts use
   */
  unsigned int fromEnv(const char * name) {
    const char * text = getenv(name);
    if (!text || !*text) return 0;

    char * end = 0;
    long v = strtol(text, &end, 10);
    if ((end == text) || (*end != '\0') || (v <= 0)) return 0;

    return clampCount(static_cast<unsigned int>(v));
  }

  /* the resolution order is identical for both stages; only the stage's own
   * variable and its default differ
   */
  unsigned int resolve(unsigned int requested, const char * stageEnv, unsigned int fallback) {
    if (requested > 0)
      return clampCount(requested);

    if (unsigned int v = fromEnv(stageEnv))
      return v;

    if (unsigned int v = fromEnv("BURRTOOLS_THREADS"))
      return v;

    return clampCount(fallback);
  }
}

unsigned int maxThreads(void) {
  unsigned int hw = std::thread::hardware_concurrency();
  return hw ? std::min(hw, MAX_THREADS) : 1u;
}

unsigned int defaultAssemblerThreads(void) {
  return std::max(1u, maxThreads() * 6 / 10);
}

unsigned int defaultDisassemblerThreads(void) {
  return 1u;
}

unsigned int resolveAssembler(unsigned int requested) {
  return resolve(requested, "BURRTOOLS_ASSEMBLER_THREADS", defaultAssemblerThreads());
}

unsigned int resolveDisassembler(unsigned int requested) {
  return resolve(requested, "BURRTOOLS_DISASSEMBLER_THREADS", defaultDisassemblerThreads());
}

unsigned int disassemblerThreadCost(unsigned int disassemblerThreads) {
  /* 1 is inline: disassemblerPool_c creates neither workers nor a merger and
   * runs on the calling thread, so it adds nothing to the machine's load.
   *
   * Above that the pool starts N workers *and* a merger thread, so it costs
   * N + 1. The merger is near idle - it only reorders finished results by
   * sequence number - but it is a real thread and leaving it out of the
   * budget let (7, 7) through on a 14 thread machine, which runs 15.
   */
  return (disassemblerThreads > 1) ? (disassemblerThreads + 1) : 0u;
}

bool exceedsBudget(unsigned int assemblerThreads, unsigned int disassemblerThreads) {
  return assemblerThreads + disassemblerThreadCost(disassemblerThreads) > maxThreads();
}

unsigned int maxDisassemblerFor(unsigned int assemblerThreads) {
  const unsigned int budget = maxThreads();
  const unsigned int a = clampCount(assemblerThreads);

  /* spare has to cover the merger as well as the workers, so the largest
   * usable pool is spare - 1; and a pool below 2 workers is just inline with
   * an extra merger bolted on, so fall back to inline instead
   */
  const unsigned int spare = (budget > a) ? (budget - a) : 0u;
  return (spare >= 3) ? (spare - 1) : 1u;
}

unsigned int maxAssemblerFor(unsigned int disassemblerThreads) {
  const unsigned int budget = maxThreads();
  const unsigned int cost = disassemblerThreadCost(clampCount(disassemblerThreads));
  return (budget > cost) ? (budget - cost) : 1u;
}

void fitToBudget(unsigned int * assemblerThreads, unsigned int * disassemblerThreads) {

  if (!assemblerThreads || !disassemblerThreads) return;

  const unsigned int budget = maxThreads();

  unsigned int a = clampCount(*assemblerThreads);
  unsigned int d = clampCount(*disassemblerThreads);

  /* The assembler has preference: it is busy for the whole solve, so the
   * disassembly pool is what gives way. Dropping the pool back to inline
   * costs no threads at all.
   *
   * `spare` threads have to cover the workers *and* the merger, so the
   * largest usable pool is spare - 1. A pool needs at least 2 workers to be
   * worth having (1 worker is what inline already does, only with an extra
   * merger thread on top), so below that go inline.
   */
  if (a + disassemblerThreadCost(d) > budget)
    d = maxDisassemblerFor(a);

  /* only reachable when the assembler alone is bigger than the machine */
  if (a > budget)
    a = budget;

  *assemblerThreads = a;
  *disassemblerThreads = d;
}

bool parseThreadArg(const char * text, unsigned int * out) {

  if (!text || !*text || !out) return false;

  char * end = 0;
  long v = strtol(text, &end, 10);

  /* reject trailing characters and negatives rather than letting them wrap
   * into a huge unsigned count; 0 stays legal and means "auto"
   */
  if ((end == text) || (*end != '\0') || (v < 0)) return false;

  *out = (v > static_cast<long>(MAX_THREADS)) ? MAX_THREADS : static_cast<unsigned int>(v);
  return true;
}

}
