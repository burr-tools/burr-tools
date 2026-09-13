#include <catch2/catch_test_macros.hpp>

#include "lib/grouping.h"

/* KNOWN BUG: src/lib/grouping.cpp:136
 *
 *     for (unsigned int p2 = 0; p2 < p; p++)
 *
 * This is inside grouping_c::addPieceToSet's newly-tried-group failure
 * arm. The intent is clearly to undo the pieces already placed into the
 * group by walking p2 from 0 up to (but not including) the OUTER loop
 * variable `p` -- but the increment clause advances `p`, not `p2`. `p2`
 * itself never changes, so once `p > 0` when this line is reached, the
 * condition `p2 < p` never becomes false: the loop increments the outer
 * `for` loop's `p` forever instead. The consequence is an infinite loop:
 * the solver HANGS silently, with no error message, no crash, nothing to
 * grep for in a log.
 *
 * Reachability: src/lib/disassembler_a.cpp:222-226 (subProbGrouping) calls
 * addPieceToSet() once per piece in a subproblem, which routinely puts
 * multiple pieces into one set. This line is reached whenever a puzzle
 * uses part groups with numGroups >= 3 and the newly-tried-group failure
 * branch runs with more than one piece already placed in the current set
 * (i.e. `p >= 1` when the failure is hit).
 *
 * Invariant this file relies on: EVERY case below keeps `p == 0` at this
 * line (either by never entering the newly-tried-group failure arm with
 * more than one piece placed, or by making the very first piece checked
 * the one that fails). That is why this suite does not hang. Anyone
 * adding a case with a multi-piece set that can fail on its SECOND or
 * later piece within a retried group must preserve that invariant, or
 * they will hang CI with no diagnostic. Do not construct such a scenario
 * until grouping.cpp:136 is fixed in production code (out of scope for
 * this test-only PR). Run any new grouping test under a timeout while
 * developing it, just in case.
 */

TEST_CASE("grouping: a group accepts pieces up to its count", "[grouping]") {
  grouping_c g;

  /* piece 0 may appear twice within group 1 */
  g.addPieces(0, 1, 2);

  g.newSet();
  REQUIRE(g.addPieceToSet(0));
  REQUIRE(g.addPieceToSet(0));
}

TEST_CASE("grouping: exhausting a group's count fails the assignment", "[grouping]") {
  grouping_c g;

  /* only one of piece 0 is available in group 1 */
  g.addPieces(0, 1, 1);

  g.newSet();
  REQUIRE(g.addPieceToSet(0));

  /* the second one has nowhere to go: group 1 is spent and there is no
     other group to back off into */
  REQUIRE_FALSE(g.addPieceToSet(0));
}

TEST_CASE("grouping: group 0 means no group and registers nothing", "[grouping]") {
  grouping_c g;

  /* addPieces returns early for group == 0 (grouping.cpp:27-28), so this
     registers NO capacity at all -- not "a group numbered 0", but
     genuinely nothing. To make that observable rather than assumed,
     register piece 0 in group 0 with a generous count AND, separately, in
     group 1 with a count of exactly one. If the group-0 call had silently
     contributed real capacity, more than one placement would succeed
     below; instead exactly ONE succeeds -- drawn entirely from group 1's
     count of one -- and the second fails, which is only possible if the
     group-0 registration added nothing at all. */
  g.addPieces(0, 0, 5);
  g.addPieces(0, 1, 1);

  g.newSet();
  REQUIRE(g.addPieceToSet(0));
  REQUIRE_FALSE(g.addPieceToSet(0));
}

TEST_CASE("grouping: an unregistered piece cannot be placed", "[grouping]") {
  grouping_c g;

  /* nothing was ever registered, so there is no group to place into */
  g.newSet();
  REQUIRE_FALSE(g.addPieceToSet(0));
}

TEST_CASE("grouping: capacity is global, so a new set does not replenish it", "[grouping]") {
  grouping_c g;

  g.addPieces(0, 1, 1);

  g.newSet();
  REQUIRE(g.addPieceToSet(0));

  /* a fresh set draws on the same pool; the single piece is already spent */
  g.newSet();
  REQUIRE_FALSE(g.addPieceToSet(0));
}

TEST_CASE("grouping: reSet does not restore consumed capacity", "[grouping]") {
  grouping_c g;

  g.addPieces(0, 1, 1);

  g.newSet();
  REQUIRE(g.addPieceToSet(0));

  /* reSet clears the sets and the failed flag but leaves pieces[].count
     depleted, so the object cannot in fact be reused. This pins OBSERVED
     behaviour and may well be a defect in grouping.cpp — it is recorded
     here rather than fixed, because this PR changes no production code. */
  g.reSet();
  g.newSet();
  REQUIRE_FALSE(g.addPieceToSet(0));
}

TEST_CASE("grouping: backtracking into a newly-tried group can succeed for the whole set", "[grouping]") {
  grouping_c g;

  /* numGroups must be >= 3 for the backtracking loop to ever try a group
     other than "give up" (grouping.cpp:107's currentGroup >= numGroups
     check needs a third group to still be available after the first
     bump). Piece 0 is available in both group 1 and group 2; piece 1 is
     only available in group 2. */
  g.addPieces(0, 1, 1);
  g.addPieces(0, 2, 1);
  g.addPieces(1, 2, 1); // numGroups becomes 3

  g.newSet();

  /* fast path: piece 0 fits directly into group 1 */
  REQUIRE(g.addPieceToSet(0));

  /* piece 1 does not fit group 1 at all, so the set as a whole must move
     to group 2 (grouping.cpp:104 currentGroup++, then the newly-tried-
     group arm at :121-144, since 2 < numGroups(3) so the give-up branch
     at :107 is not taken). Placing BOTH set members (piece 0 and piece 1)
     into group 2 succeeds (:125-131 with no failure), the for loop
     completes without ever entering the undo branch at :133-141, the do
     loop's `set` reaches sets.size() and the loop exits normally so
     execution falls through to the backtracking-succeeded `return true`
     at grouping.cpp:149 rather than the fast-path return at :84. */
  REQUIRE(g.addPieceToSet(1));
}

TEST_CASE("grouping: a failed newly-tried group retries the next one", "[grouping]") {
  grouping_c g;

  /* Same numGroups >= 3 shape as above, but this time piece 0 is only
     ever available in group 1, and nothing is registered for it in group
     2 — group 2's dummy piece (99) exists purely to push numGroups to 3
     without interacting with this set. */
  g.addPieces(0, 1, 1);
  g.addPieces(99, 2, 1); // numGroups becomes 3, unrelated to pieces 0/1

  g.newSet();

  /* fast path: piece 0 fits group 1 */
  REQUIRE(g.addPieceToSet(0));

  /* piece 1 was never registered anywhere, so the set must move to group
     2 (grouping.cpp:104/:107, entering the newly-tried-group arm at
     :121-144 since 2 < numGroups(3)). There, piece 0 (the first member
     checked, p==0) is not available in group 2 either, so the failure
     branch at :133-141 runs: the "already placed" undo loop at :136-137
     executes zero iterations (p2 < p is 0 < 0, false, so line 137's body
     is never reached — that inner loop has a p2/p mixup bug and would
     spin if it ever ran with p >= 1, so this case deliberately keeps
     p == 0 to stay safe), then :139-140 back off by one and retry. The
     next group increment reaches currentGroup(3) >= numGroups(3), taking
     the give-up branch at :107-112, and since this is the only (bottom)
     set, :110's set==0 check fires and the whole call fails. */
  REQUIRE_FALSE(g.addPieceToSet(1));
}
