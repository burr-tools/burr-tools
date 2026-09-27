/* Tests for the GUI's last-painted solve progress cache.
 *
 * The cache exists because the values the solve tab shows have no source once
 * the solve thread is gone: solveThread_c is destroyed as soon as it reports
 * ACT_PAUSING, and the assembler's own getFinished() reads live search state
 * that the abort has already unwound. So the GUI remembers what it last
 * painted, per problem, and shows that instead.
 *
 * The cache keys on the problem's address. That is only sound because entries
 * are pruned against the puzzle's live problem list -- a deleted problem can
 * hand its address to a new one, and the new one must not inherit the old
 * one's progress. keepOnly() is what makes that safe, so it is tested here
 * rather than left to the GUI wiring, which has no test harness.
 */

#include <catch2/catch_test_macros.hpp>

#include "../src/gui/solveprogresscache.h"

namespace {

/* Distinct addresses to key on. The cache never dereferences a key, so these
 * never need to be real problems -- which is the point of the opaque key type:
 * it keeps this file free of the lib and of FLTK.
 */
char problemA, problemB, problemC;

const void * A = &problemA;
const void * B = &problemB;
const void * C = &problemC;

} // namespace

TEST_CASE("solve progress cache starts empty", "[gui][progresscache]") {
  solveProgressCache_c cache;

  CHECK(cache.recall(A) == nullptr);
}

TEST_CASE("solve progress cache returns what was remembered", "[gui][progresscache]") {
  solveProgressCache_c cache;

  cache.remember(A, solveSnapshot_c{0.25f, 90, true});

  const solveSnapshot_c * s = cache.recall(A);
  REQUIRE(s != nullptr);
  CHECK(s->progress == 0.25f);
  CHECK(s->timeLeft == 90);
  CHECK(s->timeLeftKnown);
}

/* The GUI writes a snapshot on every poll while the solve runs, so the last
 * write is the one that has to survive.
 */
TEST_CASE("solve progress cache overwrites an existing entry", "[gui][progresscache]") {
  solveProgressCache_c cache;

  cache.remember(A, solveSnapshot_c{0.25f, 90, true});
  cache.remember(A, solveSnapshot_c{0.5f, 40, true});

  const solveSnapshot_c * s = cache.recall(A);
  REQUIRE(s != nullptr);
  CHECK(s->progress == 0.5f);
  CHECK(s->timeLeft == 40);
}

/* Progress of 0 carries no usable estimate: the GUI divides by it. The flag,
 * not a sentinel in timeLeft, is what says so.
 */
TEST_CASE("solve progress cache keeps an unknown estimate unknown", "[gui][progresscache]") {
  solveProgressCache_c cache;

  cache.remember(A, solveSnapshot_c{0.0f, 0, false});

  const solveSnapshot_c * s = cache.recall(A);
  REQUIRE(s != nullptr);
  CHECK(s->progress == 0.0f);
  CHECK_FALSE(s->timeLeftKnown);
}

TEST_CASE("solve progress cache keeps problems apart", "[gui][progresscache]") {
  solveProgressCache_c cache;

  cache.remember(A, solveSnapshot_c{0.25f, 90, true});
  cache.remember(B, solveSnapshot_c{0.75f, 10, true});

  REQUIRE(cache.recall(A) != nullptr);
  REQUIRE(cache.recall(B) != nullptr);
  CHECK(cache.recall(A)->progress == 0.25f);
  CHECK(cache.recall(B)->progress == 0.75f);
  CHECK(cache.recall(C) == nullptr);
}

/* Editing a problem resets it to SS_UNKNOWN and throws away its assembler;
 * the remembered progress describes a search that no longer exists.
 */
TEST_CASE("solve progress cache forgets one problem", "[gui][progresscache]") {
  solveProgressCache_c cache;

  cache.remember(A, solveSnapshot_c{0.25f, 90, true});
  cache.remember(B, solveSnapshot_c{0.75f, 10, true});

  cache.forget(A);

  CHECK(cache.recall(A) == nullptr);
  REQUIRE(cache.recall(B) != nullptr);
  CHECK(cache.recall(B)->progress == 0.75f);
}

TEST_CASE("solve progress cache forgetting an absent problem is harmless", "[gui][progresscache]") {
  solveProgressCache_c cache;

  cache.remember(A, solveSnapshot_c{0.25f, 90, true});

  cache.forget(C);

  REQUIRE(cache.recall(A) != nullptr);
  CHECK(cache.recall(A)->progress == 0.25f);
}

/* Loading another puzzle invalidates every problem at once. */
TEST_CASE("solve progress cache clears everything", "[gui][progresscache]") {
  solveProgressCache_c cache;

  cache.remember(A, solveSnapshot_c{0.25f, 90, true});
  cache.remember(B, solveSnapshot_c{0.75f, 10, true});

  cache.clear();

  CHECK(cache.recall(A) == nullptr);
  CHECK(cache.recall(B) == nullptr);
}

/* The reused-address guard. A problem that is no longer in the puzzle must not
 * leave an entry behind for whatever is allocated at its address next.
 */
TEST_CASE("solve progress cache prunes problems that are gone", "[gui][progresscache]") {
  solveProgressCache_c cache;

  cache.remember(A, solveSnapshot_c{0.25f, 90, true});
  cache.remember(B, solveSnapshot_c{0.75f, 10, true});
  cache.remember(C, solveSnapshot_c{0.5f, 50, true});

  const std::vector<const void *> live{A, C};
  cache.keepOnly(live);

  REQUIRE(cache.recall(A) != nullptr);
  CHECK(cache.recall(A)->progress == 0.25f);
  CHECK(cache.recall(B) == nullptr);
  REQUIRE(cache.recall(C) != nullptr);
  CHECK(cache.recall(C)->progress == 0.5f);
}

TEST_CASE("solve progress cache pruning against nothing empties it", "[gui][progresscache]") {
  solveProgressCache_c cache;

  cache.remember(A, solveSnapshot_c{0.25f, 90, true});

  cache.keepOnly(std::vector<const void *>{});

  CHECK(cache.recall(A) == nullptr);
}

/* keepOnly runs on every interface update, so it has to cope with a live list
 * naming problems it has never seen without inventing entries for them.
 */
TEST_CASE("solve progress cache pruning does not invent entries", "[gui][progresscache]") {
  solveProgressCache_c cache;

  cache.remember(A, solveSnapshot_c{0.25f, 90, true});

  const std::vector<const void *> live{A, B, C};
  cache.keepOnly(live);

  REQUIRE(cache.recall(A) != nullptr);
  CHECK(cache.recall(B) == nullptr);
  CHECK(cache.recall(C) == nullptr);
}
