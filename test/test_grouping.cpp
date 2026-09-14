#include <catch2/catch_test_macros.hpp>

#include "lib/grouping.h"

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
     executes zero iterations (p2 < p is 0 < 0, false), then :139-140 back
     off by one and retry. The next group increment reaches
     currentGroup(3) >= numGroups(3), taking the give-up branch at
     :107-112, and since this is the only (bottom) set, :110's set==0
     check fires and the whole call fails. */
  REQUIRE_FALSE(g.addPieceToSet(1));
}

TEST_CASE("grouping: undoing a multi-piece placement in a retried group lets a later group succeed", "[grouping]") {
  grouping_c g;

  /* This is the scenario the p2/p mixup at grouping.cpp:136 used to hang
     on: a set with three pieces, driven into a newly-tried group (group
     2) where the SECOND piece checked (p==1) fails after the first
     (p==0) already placed successfully. That forces the undo loop at
     :136-137 to run with p >= 1, walking p2 from 0 up to (not including)
     p to give back exactly the one piece already placed in that group.
     numGroups must be >= 4 so that after group 2 fails outright there is
     still a group 3 left to retry into.

     Piece 0 is available in groups 1, 2 and 3.
     Piece 1 is available in groups 1 and 3, but NOT group 2.
     Piece 2 is available in group 3 only. */
  g.addPieces(0, 1, 1);
  g.addPieces(1, 1, 1);
  g.addPieces(0, 2, 1);
  g.addPieces(0, 3, 1);
  g.addPieces(1, 3, 1);
  g.addPieces(2, 3, 1); // numGroups becomes 4

  g.newSet();

  /* fast path: pieces 0 and 1 both fit directly into group 1 */
  REQUIRE(g.addPieceToSet(0));
  REQUIRE(g.addPieceToSet(1));

  /* piece 2 does not fit group 1 at all (grouping.cpp:81), so the whole
     set backs out of group 1 (:88-89) and the backtracking do-loop takes
     over. It bumps to group 2 (:104) and tries to place all three set
     members there (:125-143): piece 0 (p==0) fits and is placed, but
     piece 1 (p==1) does not exist in group 2 at all, so the failure arm
     at :133-141 runs with p==1 -- the undo loop at :136-137 must give
     back piece 0's group-2 placement, not spin forever. The do-loop then
     bumps to group 3 (:104), where all three pieces (0, 1 and 2) fit, so
     the for loop at :125-143 completes with no failure, `set` reaches
     sets.size() and the call succeeds via :149.

     Before the fix (`p++` instead of `p2++` at :136) this call hung
     forever instead of returning.

     NOTE: this case exercises the undo path and checks the right answer,
     but it CANNOT detect a reintroduction of the p++/p2++ mistake at the
     optimization levels this project actually builds with (-O2 locally,
     -O3 in CI). The undone loop body is a plain non-volatile write with
     no I/O or atomics, so C++'s forward-progress rule lets the optimizer
     assume it terminates -- the UB is exploited away rather than
     manifesting as a hang, and the test passes either way. What would
     actually catch a regression here is review of that line, or a build
     that doesn't assume loop termination (e.g. Clang's
     -fno-finite-loops), not more test engineering against -O2. */
  REQUIRE(g.addPieceToSet(2));
}
