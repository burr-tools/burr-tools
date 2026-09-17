#include <catch2/catch_test_macros.hpp>

#include "lib/gridtype.h"
#include "lib/puzzle.h"
#include "lib/symmetries.h"
#include "lib/voxel.h"
#include "lib/voxeltable.h"

#include "test_helpers.h"

#include <memory>
#include <vector>

using namespace bttest;

/* voxelTable_c is abstract: it stores (index, transformation, hash) triples
   and calls back into findSpace(index) for the actual shape. The library's
   own concrete subclass, voxelTablePuzzle_c, resolves an index through a
   puzzle_c's shape list, so every case below builds a puzzle_c, adds the
   shapes it wants indexed, and drives the table through that subclass.

   Using the shipped subclass rather than a test-local one means findSpace()
   -- the one virtual the header asks a user to supply -- is covered as the
   library actually implements it, and the table is exercised against the
   ownership rules puzzle_c really has (addShape takes the voxel_c*). */

namespace {

/* puzzle_c::addShape takes ownership of the raw pointer, so hand it a
   release()d unique_ptr rather than a borrowed one. Returns the index the
   table will later be asked about. */
unsigned int addShape(puzzle_c & puz, std::unique_ptr<voxel_c> v) {
  return puz.addShape(v.release());
}

/* screw() and someRotationMatches() live in test_helpers.h: the mirror
   cases in test_voxel.cpp need the same chiral fixture and the same
   "is this really chiral?" guard, and a second copy would be a second
   thing to get wrong. */

} // namespace

TEST_CASE("voxelTable: a shape added to the table is found again", "[voxeltable]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  puzzle_c puz(new gridType_c(gridType_c::GT_BRICKS));

  unsigned int idx = addShape(puz, screw(*puz.getGridType()));

  voxelTablePuzzle_c tab(&puz);
  tab.addSpace(idx);

  unsigned int foundIdx = 0xFFFF;
  unsigned char foundTrans = 0xFF;

  REQUIRE(tab.getSpace(puz.getShape(idx), &foundIdx, &foundTrans));
  REQUIRE(foundIdx == idx);

  /* the shape was looked up in the same orientation it was added in, so the
     transformation that maps the stored shape onto the query is the
     identity. Asserting the value -- not merely that something was returned
     -- is what makes this fail if the table ever hands back the wrong node. */
  REQUIRE(foundTrans == 0);
}

TEST_CASE("voxelTable: a shape that was never added is not found", "[voxeltable]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_BRICKS));

  unsigned int stored = addShape(puz, screw(*puz.getGridType()));

  /* a 2x2 square: same bounding box as the L-tromino but a different cell
     count, so it collides in neither hash nor comparison */
  unsigned int absent = addShape(puz, fromLayers(*puz.getGridType(), {{"##",
                                                                      "##"}}));

  voxelTablePuzzle_c tab(&puz);
  tab.addSpace(stored);

  REQUIRE_FALSE(tab.getSpace(puz.getShape(absent)));

  /* and the shape that WAS added is still found -- without this the case
     would also pass against a table whose getSpace() always says no */
  REQUIRE(tab.getSpace(puz.getShape(stored)));
}

TEST_CASE("voxelTable: a rotation of an added shape is found and names the transformation "
          "that produces it", "[voxeltable]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_BRICKS));

  unsigned int idx = addShape(puz, screw(*puz.getGridType()));

  voxelTablePuzzle_c tab(&puz);
  tab.addSpace(idx);

  /* addSpace stores every distinct orientation, so each rotation of the
     original must be found -- and the transformation reported must be one
     that actually maps the stored shape onto the query. Check that by
     applying it and comparing, rather than trusting the number. */
  const symmetries_c * sym = puz.getGridType()->getSymmetries();

  for (unsigned char t = 0; t < sym->getNumTransformations(); t++) {
    std::unique_ptr<voxel_c> rotated = copyVoxel(*puz.getGridType(), *puz.getShape(idx));
    if (!rotated->transform(t)) continue;

    unsigned int foundIdx = 0xFFFF;
    unsigned char foundTrans = 0xFF;

    INFO("transformation " << (unsigned int)t);
    REQUIRE(tab.getSpace(rotated.get(), &foundIdx, &foundTrans));
    REQUIRE(foundIdx == idx);

    std::unique_ptr<voxel_c> replay = copyVoxel(*puz.getGridType(), *puz.getShape(idx));
    REQUIRE(replay->transform(foundTrans));
    REQUIRE(rotated->identicalInBB(replay.get(), false));
  }
}

TEST_CASE("voxelTable: without PAR_MIRROR a chiral shape's mirror is not found", "[voxeltable][mirror]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_BRICKS));

  unsigned int idx = addShape(puz, screw(*puz.getGridType()));

  const symmetries_c * sym = puz.getGridType()->getSymmetries();

  voxelTablePuzzle_c tab(&puz);
  tab.addSpace(idx);   // no PAR_MIRROR: only the 24 proper rotations are stored

  /* the first mirror transformation. getNumTransformations() is where the
     proper rotations end and the mirrored ones begin. */
  std::unique_ptr<voxel_c> mirrored = copyVoxel(*puz.getGridType(), *puz.getShape(idx));
  REQUIRE(mirrored->transform(sym->getNumTransformations()));

  /* the screw is chiral, so the mirrored copy is genuinely a shape this
     table does not hold. Guard that premise explicitly: were the fixture
     achiral its mirror would equal some stored rotation, the lookup below
     would succeed for a reason having nothing to do with PAR_MIRROR, and
     the case would be asserting nothing. */
  REQUIRE_FALSE(someRotationMatches(*puz.getGridType(), *puz.getShape(idx), *mirrored));

  REQUIRE_FALSE(tab.getSpace(mirrored.get(), nullptr, nullptr, voxelTable_c::PAR_MIRROR));
}

TEST_CASE("voxelTable: a mirrored node is hidden from a query that did not ask for mirrors",
          "[voxeltable][mirror]") {
  /* The configuration the filter exists for, and the only one in which it
     can change an answer: the table HOLDS mirrored orientations, and the
     caller does NOT want them.

     The two cases either side of this one both leave the guard inert. When
     the table is built without PAR_MIRROR every stored node already has a
     proper-rotation transformation, so the filter's second term is true
     whatever it is asked; when both sides pass the flag the first term
     short-circuits it. Delete the filter outright and neither notices.

     The GUI does exactly this: statuswindow.cpp fills the table with
     PAR_MIRROR and then queries with no flags to compute its "identical
     shape, ignoring mirror" column. Without the guard that column reports
     mirror images as identical. */
  puzzle_c puz(new gridType_c(gridType_c::GT_BRICKS));

  unsigned int idx = addShape(puz, screw(*puz.getGridType()));

  const symmetries_c * sym = puz.getGridType()->getSymmetries();

  voxelTablePuzzle_c tab(&puz);
  tab.addSpace(idx, voxelTable_c::PAR_MIRROR);   // mirrored orientations ARE stored

  std::unique_ptr<voxel_c> mirrored = copyVoxel(*puz.getGridType(), *puz.getShape(idx));
  REQUIRE(mirrored->transform(sym->getNumTransformations()));

  /* the premise: no proper rotation reproduces this, so the only node that
     could match it is a mirrored one -- exactly what the query must refuse */
  REQUIRE_FALSE(someRotationMatches(*puz.getGridType(), *puz.getShape(idx), *mirrored));

  REQUIRE_FALSE(tab.getSpace(mirrored.get(), nullptr, nullptr, 0));

  /* and the shape itself is still found, so the refusal above is the filter
     at work rather than the table having lost the entry */
  REQUIRE(tab.getSpace(puz.getShape(idx), nullptr, nullptr, 0));
}

TEST_CASE("voxelTable: with PAR_MIRROR on both sides a chiral shape's mirror is found", "[voxeltable][mirror]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_BRICKS));

  unsigned int idx = addShape(puz, screw(*puz.getGridType()));

  const symmetries_c * sym = puz.getGridType()->getSymmetries();

  voxelTablePuzzle_c tab(&puz);
  tab.addSpace(idx, voxelTable_c::PAR_MIRROR);

  std::unique_ptr<voxel_c> mirrored = copyVoxel(*puz.getGridType(), *puz.getShape(idx));
  REQUIRE(mirrored->transform(sym->getNumTransformations()));

  /* as above: without chirality this case would prove nothing */
  REQUIRE_FALSE(someRotationMatches(*puz.getGridType(), *puz.getShape(idx), *mirrored));

  unsigned int foundIdx = 0xFFFF;
  unsigned char foundTrans = 0xFF;

  REQUIRE(tab.getSpace(mirrored.get(), &foundIdx, &foundTrans, voxelTable_c::PAR_MIRROR));
  REQUIRE(foundIdx == idx);

  /* the transformation reported must be a mirrored one -- no proper
     rotation can produce a chiral shape's mirror, so a value below
     getNumTransformations() would mean the table matched the wrong node */
  REQUIRE(foundTrans >= sym->getNumTransformations());

  /* and it must be the transformation that actually does the job */
  std::unique_ptr<voxel_c> replay = copyVoxel(*puz.getGridType(), *puz.getShape(idx));
  REQUIRE(replay->transform(foundTrans));
  REQUIRE(mirrored->identicalInBB(replay.get(), false));
}

TEST_CASE("voxelTable: PAR_COLOUR distinguishes shapes that differ only in colour", "[voxeltable][colour]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_BRICKS));

  puz.addColor(255, 0, 0);
  puz.addColor(0, 255, 0);

  unsigned int plain = addShape(puz, screw(*puz.getGridType()));

  /* the same geometry, but every filled cell carries colour 1 */
  unsigned int tinted = addShape(puz, screw(*puz.getGridType()));
  voxel_c * t = puz.getShape(tinted);
  for (unsigned int i = 0; i < t->getXYZ(); i++)
    if (t->getState(i) != voxel_c::VX_EMPTY)
      t->setColor(i, 1);

  /* PAR_COLOUR selects which hash function the table uses, so it is not a
     query-time filter over a single stored set: it must be passed
     identically to addSpace and getSpace or the query hashes into a
     different bucket than the one the entry was filed under and misses for
     a reason unrelated to colour. That is why this needs two tables rather
     than two queries against one -- and it is what the header means by
     "if you want to be able to search for all combinations you must call
     the function twice". */

  voxelTablePuzzle_c blind(&puz);
  blind.addSpace(plain);

  voxelTablePuzzle_c aware(&puz);
  aware.addSpace(plain, voxelTable_c::PAR_COLOUR);

  /* colour-blind: the two shapes have identical geometry, so the tinted one
     matches the stored plain one */
  REQUIRE(blind.getSpace(puz.getShape(tinted)));

  /* colour-aware: now they are different shapes */
  REQUIRE_FALSE(aware.getSpace(puz.getShape(tinted), nullptr, nullptr, voxelTable_c::PAR_COLOUR));

  /* ...and the plain shape is still found in the colour-aware table, which
     is what rules out "PAR_COLOUR simply never matches anything" -- without
     this the REQUIRE_FALSE above would hold against a table that had stored
     nothing at all */
  unsigned int foundIdx = 0xFFFF;
  REQUIRE(aware.getSpace(puz.getShape(plain), &foundIdx, nullptr, voxelTable_c::PAR_COLOUR));
  REQUIRE(foundIdx == plain);
}

TEST_CASE("voxelTable: getSpace tolerates null index and transformation pointers", "[voxeltable]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_BRICKS));

  unsigned int idx = addShape(puz, screw(*puz.getGridType()));

  voxelTablePuzzle_c tab(&puz);
  tab.addSpace(idx);

  /* the header documents both out-parameters as optional; exercise the
     branch that skips writing them */
  REQUIRE(tab.getSpace(puz.getShape(idx), nullptr, nullptr));
}

TEST_CASE("voxelTable: the table rehashes past its initial capacity without losing entries",
          "[voxeltable]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_BRICKS));

  /* The table starts at 101 buckets and rehashes to 3n+1 once tableEntries
     reaches tableSize (voxeltable.cpp:122). Crossing that threshold takes
     more care than it looks:

     addSpace stores only the orientations isTransformationUnique() reports
     as distinct, so a SYMMETRIC shape contributes far fewer than 24 nodes.
     An earlier version of this case used bars of growing length; a bar has
     just three distinct orientations, so eleven of them filed 33 entries,
     the rehash never ran, and the case passed identically against a build
     with the rehash deleted -- and against two builds with the rehash
     actively corrupted. It was covering lookup, not the branch it named.

     Screws with an arm of growing length are what actually drives it. They
     are near-asymmetric, so each files 12 to 24 orientations rather than
     three, and eleven of them clear 101 with room to spare -- the case
     asserts the summed total below rather than trusting that arithmetic.
     The growing arm also makes the cell counts pairwise distinct, so no two
     shapes can be rotations of one another and a lookup returning the wrong
     index is a detectable error rather than an ambiguity.

     The mutation check to repeat if this case is ever edited: re-bucket the
     rehash with the old table size, and separately make it skip a bucket.
     Both must turn this case red. (Verified: before the fixture was fixed
     neither did.)

     What this case deliberately does NOT claim is that the rehash happens.
     Deleting the rehash block outright leaves every assertion here green,
     and that is correct rather than a gap: the table chains on collision,
     so a table that never grows still answers every query correctly and
     merely gets slower. Growth is a performance property with no
     observable behaviour behind it, and the only "test" for it would be one
     that asserts on timing or reaches into private state. What is covered
     is that the rehash, when it runs, moves every entry intact. */
  std::vector<unsigned int> indices;
  unsigned int entries = 0;

  for (unsigned int n = 2; n <= 12; n++) {
    std::vector<std::string> bottom;
    bottom.push_back(std::string(n, '#'));                     // the arm
    bottom.push_back(std::string(n - 1, '.') + "#");           // the turn

    std::vector<std::string> top;
    top.push_back(std::string(n, '.'));
    top.push_back(std::string(n - 1, '.') + "#");              // the lift

    std::unique_ptr<voxel_c> v = fromLayers(*puz.getGridType(), {bottom, top});

    /* The premise the entry count rests on: no self-symmetry, so all 24
       orientations are distinct and every one of them gets filed. Counted
       with isTransformationUnique -- the very predicate addSpace filters on
       (voxeltable.cpp:145) -- so this measures the number of entries the
       shape will really contribute rather than a proxy for it. */
    const symmetries_c * sm = puz.getGridType()->getSymmetries();
    symmetries_t self = v->selfSymmetries();

    unsigned int distinct = 0;
    for (unsigned char t = 0; t < sm->getNumTransformations(); t++)
      if (sm->isTransformationUnique(self, t)) distinct++;

    entries += distinct;

    indices.push_back(addShape(puz, std::move(v)));
  }

  /* The premise the whole case rests on, asserted rather than assumed: the
     shapes together file more entries than the initial 101 buckets, so
     addSpace really does take the rehash branch. Note the count is summed
     from isTransformationUnique rather than assumed to be 24 per shape --
     the short screw has a two-fold axis and contributes only 12, which is
     exactly the sort of thing that would silently leave the total under the
     threshold and quietly turn this back into a lookup test. */
  REQUIRE(entries > 101);

  voxelTablePuzzle_c tab(&puz);
  for (unsigned int idx : indices)
    tab.addSpace(idx);

  /* every shape must still be findable, must report its own index, and the
     transformation handed back must genuinely map the stored shape onto the
     query -- a rehash that dropped a chain shows up as a miss, one that
     re-bucketed wrongly as a miss, and one that crossed chains as the wrong
     index */
  for (unsigned int idx : indices) {
    unsigned int foundIdx = 0xFFFF;
    unsigned char foundTrans = 0xFF;

    INFO("shape index " << idx);
    REQUIRE(tab.getSpace(puz.getShape(idx), &foundIdx, &foundTrans));
    REQUIRE(foundIdx == idx);
    REQUIRE(foundTrans == 0);
  }

  /* and every rotation of every shape too: this is the part that depends on
     the bulk of the entries surviving the move, not just one per shape */
  const symmetries_c * sym = puz.getGridType()->getSymmetries();

  for (unsigned int idx : indices)
    for (unsigned char t = 0; t < sym->getNumTransformations(); t++) {
      std::unique_ptr<voxel_c> rotated = copyVoxel(*puz.getGridType(), *puz.getShape(idx));
      if (!rotated->transform(t)) continue;

      unsigned int foundIdx = 0xFFFF;
      INFO("shape index " << idx << ", transformation " << (unsigned int)t);
      REQUIRE(tab.getSpace(rotated.get(), &foundIdx));
      REQUIRE(foundIdx == idx);
    }
}

TEST_CASE("voxelTable: a shape added twice under different indices reports one of them -- "
          "and always a genuine match", "[voxeltable]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_BRICKS));

  unsigned int first  = addShape(puz, screw(*puz.getGridType()));
  unsigned int second = addShape(puz, screw(*puz.getGridType()));

  voxelTablePuzzle_c tab(&puz);
  tab.addSpace(first);
  tab.addSpace(second);

  unsigned int foundIdx = 0xFFFF;
  REQUIRE(tab.getSpace(puz.getShape(first), &foundIdx));

  /* the table is a multimap in effect: both indices hash to the same chain
     and either is a correct answer. Pin the contract that is actually
     guaranteed -- the returned index names a shape equal to the query --
     rather than a particular resolution order, which is an implementation
     detail of the chain and would be re-recorded rather than investigated
     if insertion order ever changed. */
  REQUIRE((foundIdx == first || foundIdx == second));
  REQUIRE(puz.getShape(foundIdx)->identicalInBB(puz.getShape(first), false));
}
