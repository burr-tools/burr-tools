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

  /* addPieces returns early for group 0, so this registers no capacity
     at all and leaves numGroups at zero */
  g.addPieces(0, 0, 5);

  g.newSet();
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
