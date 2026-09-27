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
#ifndef __SOLVEPROGRESSCACHE_H__
#define __SOLVEPROGRESSCACHE_H__

#include <map>
#include <vector>

/* What the solve tab last displayed for one problem. */
struct solveSnapshot_c {

  /* whole-solve progress in [0,1], as solveThread_c::getProgress() reported it */
  float progress;

  /* seconds still to go, as the tab computed them from progress and elapsed */
  unsigned int timeLeft;

  /* whether timeLeft means anything. At zero progress the estimate divides by
   * zero, and the tab shows "unknown" instead of a number; the flag carries
   * that rather than a sentinel value in timeLeft.
   */
  bool timeLeftKnown;
};

/* The last values the solve tab painted, kept per problem.
 *
 * They have no other source once the solve ends. solveThread_c is destroyed as
 * soon as it reports ACT_PAUSING, and the assembler that outlives it reports
 * progress from live search state that the abort has already unwound -- so
 * after Stop the bar would fall to 0% and the estimate would disappear, even
 * though both were on screen a moment earlier.
 *
 * Problems are keyed by address, and an address is only unique among the
 * problems that currently exist: deleting a problem can hand its address to
 * the next one allocated. keepOnly() closes that, and the tab calls it with
 * the puzzle's live problems on every update, so a stale entry can never
 * outlive the problem it describes by more than one refresh.
 *
 * Deliberately free of both FLTK and the lib -- the key is opaque and is never
 * dereferenced -- so that it can be built into the test binary, which links
 * the core library only.
 */
class solveProgressCache_c {

  private:

    std::map<const void *, solveSnapshot_c> entries;

  public:

    /* record what is on screen now for this problem, replacing any earlier
     * snapshot of it
     */
    void remember(const void * problem, const solveSnapshot_c & snapshot);

    /* what was last on screen for this problem, or nullptr if nothing was.
     * The pointer is invalidated by the next call that modifies the cache.
     */
    const solveSnapshot_c * recall(const void * problem) const;

    /* drop this problem's snapshot; harmless if it has none */
    void forget(const void * problem);

    /* drop every snapshot whose problem is not in `live`. Problems in `live`
     * that have no snapshot stay without one.
     */
    void keepOnly(const std::vector<const void *> & live);

    /* drop every snapshot */
    void clear(void);
};

#endif
