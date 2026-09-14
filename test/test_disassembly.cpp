#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "lib/disassemblernode.h"
#include "lib/disassemblerhashes.h"

#include "lib/puzzle.h"
#include "lib/problem.h"
#include "lib/assembler.h"
#include "lib/assembly.h"
#include "lib/disassembler.h"
#include "lib/disassembler_0.h"
#include "lib/disassembly.h"
#include "lib/disasmtomoves.h"
#include "lib/gridtype.h"
#include "lib/movementcache.h"
#include "lib/movementcache_0.h"
#include "lib/movementcache_1.h"
#include "lib/solution.h"
#include "tools/xml.h"
#include "tools/gzstream.h"

#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

/* DECLARATION-ORDER RULE, referenced by several fixture groups below (the
   come-from node tests and the nodeHash/countingNodeHash tests): a number of
   fixtures here are disassemblerNode_c objects with a manual, raw-pointer
   refcount (see decRefCount() in lib/disassemblernode.h), and other
   fixtures -- a come-from child node, or a hash table -- hold raw pointers
   into them and read/decrement that refcount at their own destruction. C++
   destroys automatic (stack) variables in the reverse of their declaration
   order, so anything that still references a fixture at scope exit must be
   declared AFTER that fixture, so it is destroyed BEFORE it. Getting this
   backwards does not fail an assertion -- it aborts the whole test binary,
   via decRefCount()'s bt_assert(refcount > 0) firing on already-freed (or
   otherwise corrupted) memory. */

/* a root node with three pieces at the given offsets */
std::unique_ptr<disassemblerNode_c> node3(int dx, int dy, int dz) {
  std::unique_ptr<disassemblerNode_c> n(new disassemblerNode_c(3));
  n->set(0, 0 + dx, 0 + dy, 0 + dz, 0);
  n->set(1, 2 + dx, 0 + dy, 0 + dz, 0);
  n->set(2, 0 + dx, 3 + dy, 1 + dz, 0);
  return n;
}

/* a root node with three pieces whose RELATIVE arrangement is keyed on idx,
   so distinct idx values are guaranteed to produce non-equal nodes, while a
   given idx together with any (dx, dy, dz) shift produces equal nodes -- the
   same shift trick as node3(), just parameterised so we can mint many
   distinct arrangements for growth/scan tests. */
std::unique_ptr<disassemblerNode_c> nodeDistinct(int idx, int dx = 0, int dy = 0, int dz = 0) {
  std::unique_ptr<disassemblerNode_c> n(new disassemblerNode_c(3));
  n->set(0, 0 + dx, 0 + dy, 0 + dz, 0);
  n->set(1, 2 + idx + dx, 0 + dy, 0 + dz, 0);
  n->set(2, 0 + dx, 3 + dy, 1 + dz, 0);
  return n;
}

} // namespace

TEST_CASE("disassembler node: positions and orientation are stored per piece", "[disasm][node]") {
  std::unique_ptr<disassemblerNode_c> n(new disassemblerNode_c(2));
  n->set(0, 1, 2, 3, 0);
  n->set(1, -4, 5, -6, 7);

  REQUIRE(n->getPiecenumber() == 2);

  REQUIRE(n->getX(0) == 1);
  REQUIRE(n->getY(0) == 2);
  REQUIRE(n->getZ(0) == 3);
  REQUIRE(n->getTrans(0) == 0);

  REQUIRE(n->getX(1) == -4);
  REQUIRE(n->getY(1) == 5);
  REQUIRE(n->getZ(1) == -6);
  REQUIRE(n->getTrans(1) == 7);
}

TEST_CASE("disassembler node: equality ignores a shift applied to every piece", "[disasm][node]") {
  /* operator== normalises so piece 0 sits at the origin, so displacing the
     whole assembly must not change which node this is */
  std::unique_ptr<disassemblerNode_c> a = node3(0, 0, 0);
  std::unique_ptr<disassemblerNode_c> b = node3(5, -3, 2);

  REQUIRE(*a == *b);
}

TEST_CASE("disassembler node: equality notices a shift applied to one piece", "[disasm][node]") {
  /* the counterpart to the case above: moving a single piece changes the
     relative arrangement, which is what the node identifies */
  std::unique_ptr<disassemblerNode_c> a = node3(0, 0, 0);

  std::unique_ptr<disassemblerNode_c> b(new disassemblerNode_c(3));
  b->set(0, 0, 0, 0, 0);
  b->set(1, 2, 0, 0, 0);
  b->set(2, 0, 3, 2, 0);   /* z differs by one */

  REQUIRE_FALSE(*a == *b);
}

TEST_CASE("disassembler node: equality notices a changed orientation", "[disasm][node]") {
  std::unique_ptr<disassemblerNode_c> a = node3(0, 0, 0);

  std::unique_ptr<disassemblerNode_c> b = node3(0, 0, 0);
  b->set(1, 2, 0, 0, 1);   /* same place, different transformation */

  REQUIRE_FALSE(*a == *b);
}

TEST_CASE("disassembler node: equal nodes hash equal", "[disasm][node][hash]") {
  /* the header states this contract explicitly: nodes equal under operator==
     return the same hash. A hash table built on the pair breaks silently if
     the two ever disagree. */
  std::unique_ptr<disassemblerNode_c> a = node3(0, 0, 0);
  std::unique_ptr<disassemblerNode_c> b = node3(5, -3, 2);

  REQUIRE(*a == *b);
  REQUIRE(a->hash() == b->hash());
}

TEST_CASE("disassembler node: a removed piece is marked and detected", "[disasm][node]") {
  std::unique_ptr<disassemblerNode_c> n(new disassemblerNode_c(2));
  n->set(0, 0, 0, 0, 0);
  n->set(1, 1, 0, 0, 0);

  REQUIRE_FALSE(n->is_piece_removed(0));
  REQUIRE_FALSE(n->is_piece_removed(1));
  REQUIRE_FALSE(n->is_separation());

  n->setRemove(1, 10, 0, 0);

  REQUIRE_FALSE(n->is_piece_removed(0));
  REQUIRE(n->is_piece_removed(1));
  REQUIRE(n->is_separation());
}

TEST_CASE("disassembler node: come-from constructor accumulates relative moves", "[disasm][node]") {
  /* the come-from constructor increments the parent's reference count, and
     the 4-argument set() overload offsets from the come-from node's stored
     position rather than taking an absolute coordinate. `root` is declared
     before `child` per the declaration-order rule above: `child` holds a raw
     comefrom pointer to `root`, so it must be destroyed first, dropping
     root's count from 2 back to 1 without freeing it, before root's own
     unique_ptr deletes it. */
  std::unique_ptr<disassemblerNode_c> root(new disassemblerNode_c(2));
  root->set(0, 0, 0, 0, 0);
  root->set(1, 5, 0, 0, 0);

  std::unique_ptr<disassemblerNode_c> child(
      new disassemblerNode_c(2, root.get(), /* dir */ 0, /* amount */ 3, /* step */ 1));
  child->set(0, 3, 0, 0);   /* relative: root's (0,0,0) + (3,0,0) */
  child->set(1, 0, 4, 0);   /* relative: root's (5,0,0) + (0,4,0) */

  REQUIRE(child->getX(0) == 3);
  REQUIRE(child->getY(0) == 0);
  REQUIRE(child->getZ(0) == 0);

  REQUIRE(child->getX(1) == 5);
  REQUIRE(child->getY(1) == 4);
  REQUIRE(child->getZ(1) == 0);

  REQUIRE(child->getComefrom() == root.get());
  REQUIRE(child->getAmount() == 3);
  REQUIRE(child->getDirection() == 0u);
  REQUIRE(child->getWaylength() == root->getWaylength() + 1);
}

/* nodeHash and countingNodeHash do not take ownership of inserted nodes in
   the unique_ptr sense: the header for nodeHash says so explicitly ("The
   nodes will not become owned by the hashtable, but the table will use the
   reference counting system of the node"), and both tables' clear() only
   calls decRefCount() on nodes they themselves incRefCount()'d at insertion
   time, deleting a node only if that brings its count to zero. Every fixture
   below is a freshly constructed root node, which starts at refcount 1 (see
   the node tests above) representing this test's own ownership via
   unique_ptr. A successful insert takes that to 2; clear()/the table's own
   destructor takes it back to 1 and stops -- the table never reaches zero on
   a node this test also owns, so it is safe to keep these in unique_ptr and
   let the table run clear() or go out of scope. Inserting an already-present
   duplicate never touches the duplicate's refcount at all: the table
   discards it without storing it.

   Ownership being shared this way means the declaration-order rule stated
   above (near the top of the file) applies: in every case below, the table
   is declared LAST, after every fixture it might still be holding at scope
   exit, so it is destroyed FIRST. (A test that calls table.clear() itself
   before the fixture it just removed goes out of scope does not need this
   care for that fixture, since the table no longer references it either
   way.) */

TEST_CASE("nodeHash: an inserted node is found by contains", "[disasm][hash]") {
  std::unique_ptr<disassemblerNode_c> a = node3(1, 2, 3);
  nodeHash table;

  REQUIRE(table.insert(a.get()) == nullptr);
  REQUIRE(table.contains(a.get()));
}

TEST_CASE("nodeHash: contains is false for a node that was never inserted", "[disasm][hash]") {
  std::unique_ptr<disassemblerNode_c> a = node3(0, 0, 0);
  std::unique_ptr<disassemblerNode_c> b = nodeDistinct(1);
  nodeHash table;

  REQUIRE(table.insert(a.get()) == nullptr);
  REQUIRE_FALSE(table.contains(b.get()));
}

TEST_CASE("nodeHash: inserting a node equal to one already present returns the existing node", "[disasm][hash]") {
  /* this is the case the table exists for: dedup nodes that represent the
     same state reached at different absolute offsets. */
  std::unique_ptr<disassemblerNode_c> a = node3(0, 0, 0);
  std::unique_ptr<disassemblerNode_c> b = node3(5, -3, 2);   // same arrangement, shifted
  nodeHash table;

  REQUIRE(*a == *b);

  REQUIRE(table.insert(a.get()) == nullptr);        // a is new, gets added

  const disassemblerNode_c * found = table.insert(b.get());

  REQUIRE(found != nullptr);
  REQUIRE(found == a.get());          // the table hands back the ORIGINAL node, not b
  REQUIRE(table.contains(b.get()));   // b's arrangement is present in the table (as a)
}

TEST_CASE("nodeHash: clear empties the table", "[disasm][hash]") {
  std::unique_ptr<disassemblerNode_c> a = node3(0, 0, 0);
  nodeHash table;

  REQUIRE(table.insert(a.get()) == nullptr);
  REQUIRE(table.contains(a.get()));

  table.clear();   // table no longer references a from this point on

  REQUIRE_FALSE(table.contains(a.get()));
}

TEST_CASE("nodeHash: growth rehashes without losing or duplicating entries", "[disasm][hash]") {
  /* the table starts at size 11 (tab_size in the constructor) and rehashes
     once tab_entries exceeds tab_size, growing to tab_size*4+1 = 45. Thirty
     distinct entries force exactly one rehash partway through the loop, so
     this exercises both pre- and post-rehash lookups. */
  const int count = 30;
  std::vector<std::unique_ptr<disassemblerNode_c>> nodes;
  nodes.reserve(count);
  std::unique_ptr<disassemblerNode_c> absent = nodeDistinct(count);
  std::unique_ptr<disassemblerNode_c> dup = nodeDistinct(0, 100, -100, 50);
  nodeHash table;

  for (int i = 0; i < count; i++) {
    nodes.push_back(nodeDistinct(i));
    REQUIRE(table.insert(nodes.back().get()) == nullptr);
  }

  for (int i = 0; i < count; i++)
    REQUIRE(table.contains(nodes[i].get()));

  // a node with an arrangement that was never inserted must still be absent
  REQUIRE_FALSE(table.contains(absent.get()));

  // and a shifted duplicate of an entry inserted before the rehash must
  // still be found afterwards
  REQUIRE(table.contains(dup.get()));
}

TEST_CASE("countingNodeHash: insert reports new vs. already-present nodes", "[disasm][hash][counting]") {
  std::unique_ptr<disassemblerNode_c> a = node3(0, 0, 0);
  std::unique_ptr<disassemblerNode_c> b = node3(5, -3, 2);   // equal to a, shifted
  std::unique_ptr<disassemblerNode_c> c = nodeDistinct(1);   // genuinely different
  countingNodeHash table;

  REQUIRE_FALSE(table.insert(a.get()));   // new node -> false
  REQUIRE(table.insert(b.get()));          // equal to a, already present -> true
  REQUIRE_FALSE(table.insert(c.get()));    // new node -> false
}

TEST_CASE("countingNodeHash: clear empties the table", "[disasm][hash][counting]") {
  std::unique_ptr<disassemblerNode_c> a = node3(0, 0, 0);
  std::unique_ptr<disassemblerNode_c> b = node3(5, -3, 2);
  countingNodeHash table;

  REQUIRE_FALSE(table.insert(a.get()));

  table.clear();   // table no longer references a from this point on

  // after clear(), an arrangement equal to the old entry must be reported
  // as new again, not as already present
  REQUIRE_FALSE(table.insert(b.get()));
}

TEST_CASE("countingNodeHash: initScan/nextScan visits every inserted node exactly once", "[disasm][hash][counting]") {
  // countingNodeHash's constructor starts tab_size at 100 and rehashes once
  // tab_entries exceeds it, growing to tab_size*4+1 = 401 (same scheme as
  // nodeHash, just a different starting size) -- count must exceed 100 for
  // this case to exercise that growth path at all, the way the nodeHash
  // growth test above already exercises it for the other table.
  const int count = 110;
  std::vector<std::unique_ptr<disassemblerNode_c>> nodes;
  nodes.reserve(count);
  // a duplicate of an already-inserted arrangement, tried below -- it is
  // never actually stored, so its position relative to `table` is moot, but
  // it is declared here for locality with the other fixtures
  std::unique_ptr<disassemblerNode_c> dup = nodeDistinct(0, 9, 9, 9);
  countingNodeHash table;

  for (int i = 0; i < count; i++) {
    nodes.push_back(nodeDistinct(i));
    REQUIRE_FALSE(table.insert(nodes.back().get()));
  }

  // a duplicate of an already-inserted arrangement must not add a second
  // scan entry
  REQUIRE(table.insert(dup.get()));

  table.initScan();

  std::set<const disassemblerNode_c *> seen;
  int scanned = 0;
  const disassemblerNode_c * n = table.nextScan();
  while (n != nullptr) {
    REQUIRE(seen.insert(n).second);   // never reported twice
    scanned++;
    n = table.nextScan();
  }

  REQUIRE(scanned == count);
  REQUIRE(seen.size() == static_cast<size_t>(count));

  for (int i = 0; i < count; i++)
    REQUIRE(seen.count(nodes[i].get()) == 1);

  // the duplicate itself was never stored, so it must not appear
  REQUIRE(seen.count(dup.get()) == 0);
}

namespace {

/* Solves examples/CubeInCage.xmpuzzle exactly once and hands out the
   resulting separation tree to every case below.

   CubeInCage is the smallest bundled puzzle that both supports disassembly
   and produces a tree with more than one level. At 6 pieces it ties
   DiagonalCube, BrokenSticks and PermutatedThirdStellation for fewest
   pieces among the bundled examples, but those three don't report
   CAP_DISASSEMBLE for this grid/problem. Puzzles with fewer pieces that DO
   disassemble give only a single flat split: FourPieceTetrahedron (4
   pieces) has no disassembler at all, and DemoMirrorParadox's first
   problem (4 pieces) disassembles its whole tree in one move with a root
   of only 2 pieces. Pelikan Burr -- already used by test_solver.cpp -- is
   the next multi-level example up at 7 pieces and costs roughly 85ms to
   solve; CubeInCage does the same job, with a comparably deep tree, in
   about 6ms.

   Disassembly is the single most expensive thing this file does (this is
   the first place in test_disassembly.cpp that runs a solver), so the
   puzzle is solved a single time here, via a function-local static, and
   the resulting separation_c handed out by const reference to every
   TEST_CASE that needs it. Every disasmToMoves_c and separation-tree case
   below also draws on this same tree, so paying the solve cost once here
   rather than per-case matters even more than it would in isolation.

   Ownership: disassembler_c::disassemble() returns a heap-allocated
   separation_c that the caller owns (test_solver.cpp's solvePuzzle follows
   the same contract and explicitly deletes it). Here that pointer is
   handed straight into a static std::unique_ptr, so it is owned exactly
   once, for the lifetime of the test binary, and every TEST_CASE below
   only ever sees a const reference into it -- nothing here ever calls
   delete on it directly. separation_c is a self-contained tree of piece
   indices and states; it does not reference the assembler or disassembler
   that built it, so it safely outlives both. */
const separation_c & cubeInCageSeparation() {
  static const std::unique_ptr<separation_c> sep = [] {
    std::unique_ptr<std::istream> str(openGzFile("examples/CubeInCage.xmpuzzle"));
    REQUIRE(str != nullptr);

    xmlParser_c pars(*str);
    puzzle_c p(pars);

    problem_c * problem = p.getProblem(0);
    REQUIRE(problem != nullptr);

    const gridType_c * gt = problem->getPuzzle().getGridType();
    REQUIRE(gt != nullptr);
    REQUIRE((gt->getCapabilities() & gridType_c::CAP_DISASSEMBLE));

    std::unique_ptr<assembler_c> assm(gt->findAssembler(*problem));
    REQUIRE(assm != nullptr);
    REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);

    disassembler_0_c disasm(*problem);

    struct StopAtFirstSolution : public assembler_cb {
      disassembler_0_c * disasm;
      std::unique_ptr<separation_c> result;

      explicit StopAtFirstSolution(disassembler_0_c * d) : disasm(d) {}

      bool assembly(std::unique_ptr<assembly_c> a) override {
        std::unique_ptr<separation_c> da = disasm->disassemble(a.get());
        if (da) {
          result = std::move(da);
          return false;   // this puzzle has exactly one disassemblable
                           // solution -- once found, there is no reason
                           // to keep enumerating assemblies
        }
        return true;
      }
    } cb(&disasm);

    assm->assemble(&cb);
    REQUIRE(cb.result != nullptr);

    return std::move(cb.result);
  }();

  return *sep;
}

} // namespace

TEST_CASE("CubeInCage separation tree: getMoves() matches the sequence actually taken", "[disasm][separation]") {
  const separation_c & root = cubeInCageSeparation();

  // the known shape of this puzzle's tree: root -> left -> left, 3 levels
  REQUIRE(root.getRemoved() == nullptr);
  REQUIRE(root.getLeft() != nullptr);
  const separation_c & mid = *root.getLeft();

  REQUIRE(mid.getRemoved() == nullptr);
  REQUIRE(mid.getLeft() != nullptr);
  const separation_c & leaf = *mid.getLeft();

  REQUIRE(leaf.getLeft() == nullptr);
  REQUIRE(leaf.getRemoved() == nullptr);

  // every level here takes more than one move, so movesText must report
  // all three of them, dot separated, in root/mid/leaf order
  CHECK(root.getMoves() > 1);
  CHECK(mid.getMoves() > 1);
  CHECK(leaf.getMoves() > 1);

  const std::string expected = std::to_string(root.getMoves()) + "." +
                                std::to_string(mid.getMoves()) + "." +
                                std::to_string(leaf.getMoves());
  CHECK(root.movesText() == expected);
}

TEST_CASE("CubeInCage separation tree: getNumSequences()/getSequenceLength() are the API disassembly_c::compare() actually uses", "[disasm][separation]") {
  // sumMoves(), pinned and exercised extensively above and below, is a
  // different, simpler total -- it is not the sequence API. compare()
  // (disassembly.cpp, defined once on the disassembly_c base both
  // separation_c and separationInfo_c share) is what solution sorting is
  // built on, and it calls getNumSequences()/getSequenceLength() directly,
  // never sumMoves().
  const separation_c & root = cubeInCageSeparation();
  const separation_c & mid = *root.getLeft();
  const separation_c & leaf = *mid.getLeft();

  // one sequence per level of this puzzle's pure getLeft() spine: root,
  // mid, leaf
  REQUIRE(root.getNumSequences() == 3u);

  // getSequenceLength(x) is NOT a move count: disassembly.cpp's
  // separation_c::getSequenceLength returns states.size() once x selects
  // this node's own entry, i.e. moves + 1 (state 0 through state
  // getMoves() inclusive), not getMoves() itself. Cross-check that
  // non-obvious "+1" dynamically against the tree before pinning the
  // literal lengths below.
  CHECK(root.getSequenceLength(0) == root.getMoves() + 1);
  CHECK(root.getSequenceLength(1) == mid.getMoves() + 1);
  CHECK(root.getSequenceLength(2) == leaf.getMoves() + 1);

  const unsigned int expectedLengths[3] = {5, 10, 4};
  for (unsigned int i = 0; i < 3; i++) {
    INFO("sequence " << i);
    CHECK(root.getSequenceLength(i) == expectedLengths[i]);
  }

  // compare() itself: identical trees compare equal, and since
  // getSequenceLength(0) strictly orders mid (10) above root (5), mid
  // compares greater than root and root compares less than mid
  CHECK(root.compare(&root) == 0);
  CHECK(mid.compare(&root) == 1);
  CHECK(root.compare(&mid) == -1);
}

TEST_CASE("CubeInCage separation tree: getPieceNumber()/getPieceName() account for every piece", "[disasm][separation]") {
  const separation_c & root = cubeInCageSeparation();
  const separation_c & mid = *root.getLeft();
  const separation_c & leaf = *mid.getLeft();

  const unsigned int totalPieces = 6;   // examples/CubeInCage.xmpuzzle, problem 0

  REQUIRE(root.getPieceNumber() == totalPieces);

  std::set<unsigned int> rootNames;
  for (unsigned int i = 0; i < root.getPieceNumber(); i++) {
    unsigned int name = root.getPieceName(i);
    CHECK(name < totalPieces);
    CHECK(rootNames.insert(name).second);   // every name appears exactly once
  }
  CHECK(rootNames.size() == totalPieces);   // ... and every piece is present

  // each level down peels off at least one piece as an (implicit) single,
  // so piece counts strictly decrease and each level's names are a subset
  // of its parent's
  std::set<unsigned int> midNames;
  for (unsigned int i = 0; i < mid.getPieceNumber(); i++) {
    unsigned int name = mid.getPieceName(i);
    CHECK(name < totalPieces);
    CHECK(midNames.insert(name).second);
  }
  CHECK(mid.getPieceNumber() < root.getPieceNumber());
  for (unsigned int name : midNames)
    CHECK(rootNames.count(name) == 1);

  std::set<unsigned int> leafNames;
  for (unsigned int i = 0; i < leaf.getPieceNumber(); i++) {
    unsigned int name = leaf.getPieceName(i);
    CHECK(name < totalPieces);
    CHECK(leafNames.insert(name).second);
  }
  CHECK(leaf.getPieceNumber() < mid.getPieceNumber());
  for (unsigned int name : leafNames)
    CHECK(midNames.count(name) == 1);
}

TEST_CASE("CubeInCage separation tree: navigation terminates and never revisits a node", "[disasm][separation]") {
  const separation_c & root = cubeInCageSeparation();

  std::set<const separation_c *> visited;
  std::vector<const separation_c *> pending{&root};
  int nodeCount = 0;

  // an explicit worklist rather than recursion: `pending` only grows by
  // pushing a node's own children, so if the tree really is a DAG-free
  // tree this drains to empty in finitely many iterations. The
  // insert().second check below turns any cycle (a node reachable from
  // itself) into an immediate, explicit failure instead of a hang.
  while (!pending.empty()) {
    const separation_c * node = pending.back();
    pending.pop_back();

    REQUIRE(visited.insert(node).second);   // never visited before
    nodeCount++;

    unsigned int childPieces = 0;

    if (node->getLeft()) {
      CHECK(node->getLeft()->getPieceNumber() < node->getPieceNumber());
      childPieces += node->getLeft()->getPieceNumber();
      pending.push_back(node->getLeft());
    }
    if (node->getRemoved()) {
      CHECK(node->getRemoved()->getPieceNumber() < node->getPieceNumber());
      childPieces += node->getRemoved()->getPieceNumber();
      pending.push_back(node->getRemoved());
    }

    // pieces recorded in child nodes never exceed the pieces recorded at
    // this node -- the difference is pieces peeled off as an (implicit)
    // single at this step, which get no separation_c node of their own
    CHECK(childPieces <= node->getPieceNumber());
  }

  // the walk terminated (loop exited) having covered the whole known tree,
  // not a truncated part of it and not more than exists
  CHECK(nodeCount == 3);
}

/* disasmToMoves_c turns a separation_c tree into animated piece positions.
   getX/Y/Z do NOT return a displacement that starts at (0,0,0) for every
   piece -- despite disassembly.h's state_c comment describing state 0 as
   "the assembled puzzle with all values 0" -- they return the coordinates
   stored directly in the tree's states, which for CubeInCage's root are the
   pieces' real (and mutually distinct) assembled grid positions. So "the
   assembled position" below means "whatever root.getState(0) says", cross
   -checked once against that call and then pinned as literal numbers so a
   regression shows up as a value change, not a silent divergence from a
   helper that duplicates the same lookup.

   size (the constructor's 2nd argument) is computed ONCE per tree node, from
   a single representative removed piece's last state -- whichever axis that
   one piece was pushed more than 10000 units along is the axis the sentinel
   marks, and size replaces the sentinel with a real, controllable offset
   along that axis. The same offset is then applied to every piece removed
   at that node, not computed independently per piece (disasmtomoves.cpp's
   doRecursive picks pc once via a single scan, then reuses dx/dy/dz for the
   whole loop over removed pieces). CubeInCage never removes more than one
   piece per node, so this test can't tell the two apart, but the wording
   here shouldn't imply it could. maxPiece
   (3rd argument) just sizes the internal arrays: it must exceed the largest
   piece name the tree uses, but need not equal the tree's own piece count,
   since piece names can have gaps.

   The three-level shape of CubeInCage's tree (established in the
   separation-tree tests above: root, 6 pieces --left--> mid, 5 pieces
   --left--> leaf, 4 pieces, with every getRemoved() null) was used to hand
   -derive every expected number below directly from getState()/getMoves(),
   the same way doRecursive itself computes them, and then cross-checked by
   running disasmToMoves_c and comparing. That walk also turned up which
   piece is removed at each level: piece 3 at the root, piece 4 at mid,
   piece 5 at the leaf, leaving pieces 0/1/2 as the never-separated-further
   remainder placed by leaf's getLeft()==nullptr branch. */

namespace {

/* sz is chosen large (1000) and distinct from every real coordinate in the
   tree (all of which are small, since they are raw grid positions/deltas)
   so that a piece being pushed out reads unambiguously in test output and
   can't be confused with an unrelated small coordinate. */
constexpr float kPushSize = 1000.0f;
constexpr unsigned int kMaxPiece = 6;   // CubeInCage problem 0 has exactly 6 pieces, named 0..5

} // namespace

TEST_CASE("disasmToMoves_c: step 0 places every piece at its assembled position -- fully visible", "[disasm][moves]") {
  const separation_c & root = cubeInCageSeparation();
  disasmToMoves_c mv(&root, kPushSize, kMaxPiece);

  mv.setStep(0);

  // cross-check once against the tree's own state 0, dynamically, then pin
  // literal numbers below so the two can never silently drift apart without
  // a test failure pointing at exactly which piece changed
  for (unsigned int p = 0; p < kMaxPiece; p++) {
    CHECK(mv.getX(p) == Catch::Approx(root.getState(0)->getX(p)).margin(1e-4));
    CHECK(mv.getY(p) == Catch::Approx(root.getState(0)->getY(p)).margin(1e-4));
    CHECK(mv.getZ(p) == Catch::Approx(root.getState(0)->getZ(p)).margin(1e-4));
  }

  const float expected[kMaxPiece][3] = {
    {1, 6, 3},   // piece 0
    {6, 3, 1},   // piece 1
    {3, 1, 0},   // piece 2
    {2, 2, 3},   // piece 3
    {2, 6, 0},   // piece 4
    {1, 5, 6},   // piece 5
  };

  for (unsigned int p = 0; p < kMaxPiece; p++) {
    INFO("piece " << p);
    CHECK(mv.getX(p) == Catch::Approx(expected[p][0]).margin(1e-4));
    CHECK(mv.getY(p) == Catch::Approx(expected[p][1]).margin(1e-4));
    CHECK(mv.getZ(p) == Catch::Approx(expected[p][2]).margin(1e-4));
    CHECK(mv.getA(p) == Catch::Approx(1.0f).margin(1e-4));   // assembled: fully opaque
  }
}

TEST_CASE("disasmToMoves_c: maxPiece may exceed the tree's own piece count to cover name gaps", "[disasm][moves]") {
  const separation_c & root = cubeInCageSeparation();

  // maxPiece = 8 even though the tree only names pieces 0..5: the header
  // documents maxPiece as "NOT identical with pieceNumber of tree because
  // there might be gaps". Names 6 and 7 are never written by doRecursive,
  // so they must stay at the zero-initialised default for every query.
  disasmToMoves_c mv(&root, kPushSize, 8);

  mv.setStep(0);
  for (unsigned int p : {6u, 7u}) {
    INFO("piece " << p);
    CHECK(mv.getX(p) == Catch::Approx(0.0f).margin(1e-4));
    CHECK(mv.getY(p) == Catch::Approx(0.0f).margin(1e-4));
    CHECK(mv.getZ(p) == Catch::Approx(0.0f).margin(1e-4));
    CHECK(mv.getA(p) == Catch::Approx(0.0f).margin(1e-4));
    CHECK_FALSE(mv.moving(p));
  }
}

TEST_CASE("disasmToMoves_c: a fractional step linearly interpolates the two bracketing keyframes", "[disasm][moves]") {
  const separation_c & root = cubeInCageSeparation();
  disasmToMoves_c mv(&root, kPushSize, kMaxPiece);

  SECTION("within a single node's own move sequence") {
    // piece 2 moves from root.getState(0) (3,1,0) to root.getState(1)
    // (2,1,0) -- only x changes, so the midpoint must be the exact average
    mv.setStep(0.5f);
    CHECK(mv.getX(2) == Catch::Approx(2.5f).margin(1e-4));
    CHECK(mv.getY(2) == Catch::Approx(1.0f).margin(1e-4));
    CHECK(mv.getZ(2) == Catch::Approx(0.0f).margin(1e-4));
    CHECK(mv.getA(2) == Catch::Approx(1.0f).margin(1e-4));   // still fully assembled, no fade yet
  }

  SECTION("across the boundary where a piece is peeled off and starts fading") {
    // piece 3 is root's own peeled piece: root.getState(3) has it at
    // (3,2,3); once step reaches root.getMoves()==4, doRecursive's "after
    // this node" path places it at (3, size+2, 3) = (3,1002,3) with size
    // =1000 (the push is along y: the last state's y is the one that
    // crosses the 10000 sentinel). setStep(3.5) interpolates exactly
    // halfway between those two keyframes, including alpha fading from 1
    // (still part of the assembled group) toward 0 (removed and invisible)
    mv.setStep(3.5f);
    CHECK(mv.getX(3) == Catch::Approx(3.0f).margin(1e-4));
    CHECK(mv.getY(3) == Catch::Approx(502.0f).margin(1e-4));   // (2 + 1002) / 2
    CHECK(mv.getZ(3) == Catch::Approx(3.0f).margin(1e-4));
    CHECK(mv.getA(3) == Catch::Approx(0.5f).margin(1e-4));
    CHECK(mv.moving(3));
  }
}

TEST_CASE("disasmToMoves_c: moving() is true exactly for the pieces whose position actually changes over the next step", "[disasm][moves]") {
  const separation_c & root = cubeInCageSeparation();
  disasmToMoves_c mv(&root, kPushSize, kMaxPiece);

  const unsigned int total = root.sumMoves();
  REQUIRE(total == 16u);   // 4 (root) + 9 (mid) + 3 (leaf), matches the movesText test above

  // walk every integer step boundary; independently determine, from getX/Y/Z
  // alone, whether each piece's position differs between the two ends of the
  // boundary, and check moving() (queried at the boundary's start) agrees --
  // this exercises the *contract*, not disasmToMoves_c's own mv[] bookkeeping
  for (unsigned int k = 0; k < total; k++) {
    mv.setStep((float)k);
    bool reportedMoving[kMaxPiece];
    float x0[kMaxPiece], y0[kMaxPiece], z0[kMaxPiece];
    for (unsigned int p = 0; p < kMaxPiece; p++) {
      reportedMoving[p] = mv.moving(p);
      x0[p] = mv.getX(p);
      y0[p] = mv.getY(p);
      z0[p] = mv.getZ(p);
    }

    mv.setStep((float)(k + 1));
    for (unsigned int p = 0; p < kMaxPiece; p++) {
      bool actuallyChanged = std::fabs(mv.getX(p) - x0[p]) > 1e-4 ||
                              std::fabs(mv.getY(p) - y0[p]) > 1e-4 ||
                              std::fabs(mv.getZ(p) - z0[p]) > 1e-4;
      INFO("step " << k << " -> " << (k + 1) << ", piece " << p);
      CHECK(reportedMoving[p] == actuallyChanged);
    }
  }
}

TEST_CASE("disasmToMoves_c: setStep is an exact affine blend of its two bracketing keyframes", "[disasm][moves]") {
  /* setStep(step) is, by construction, s=int(step), frac=step-s, and then
     a pure affine blend of two FIXED endpoints: A = doRecursive(tree,s) and
     B = doRecursive(tree,s+1), returned as (1-frac)*A + frac*B. An affine
     blend between two fixed endpoints cannot reverse direction, whatever A
     and B are -- so a sign-based "did it ever reverse" sweep is close to
     tautological: it would also pass a broken "snap" implementation that
     ignores frac and jumps from A to B partway through the bracket, since
     that jump is still only one sign change per bracket.

     What's actually worth checking -- and what subsumes monotonicity,
     magnitude and proportionality all at once -- is the blend formula
     itself: for every bracket, every interior frac, every piece and every
     one of x/y/z/alpha, the interpolated value must equal the arithmetic
     blend of the two keyframes exactly (up to float rounding). */
  const separation_c & root = cubeInCageSeparation();
  disasmToMoves_c mv(&root, kPushSize, kMaxPiece);

  const unsigned int total = root.sumMoves();

  // every keyframe (an exact, frac=0 query) is captured ONCE, in a single
  // pass over steps 0..total inclusive (total+1 distinct keyframes).
  // Bracket k's B keyframe is bracket k+1's A keyframe, so capturing both
  // independently per bracket would query every interior keyframe twice for
  // no extra coverage.
  using PieceKeyframes = std::array<std::array<float, 4>, kMaxPiece>;
  std::vector<PieceKeyframes> keyframes(total + 1);
  for (unsigned int k = 0; k <= total; k++) {
    mv.setStep((float)k);
    for (unsigned int p = 0; p < kMaxPiece; p++)
      keyframes[k][p] = { mv.getX(p), mv.getY(p), mv.getZ(p), mv.getA(p) };
  }

  // every alpha write in doRecursive contributes literal 0 or literal 1
  // (see the "vanish"/"left over"/"still assembled" branches) -- alpha is
  // therefore bounded to [0,1] BY CONSTRUCTION at every keyframe, and any
  // blend of two such values is bounded too. Check the construction fact
  // directly, once per keyframe, rather than re-deriving the bound at
  // hundreds of sample points that can't fail once this holds.
  for (unsigned int k = 0; k <= total; k++)
    for (unsigned int p = 0; p < kMaxPiece; p++) {
      INFO("keyframe " << k << ", piece " << p);
      const float a = keyframes[k][p][3];
      CHECK((a == Catch::Approx(0.0f).margin(1e-4) || a == Catch::Approx(1.0f).margin(1e-4)));
    }

  for (unsigned int k = 0; k < total; k++) {
    const PieceKeyframes & A = keyframes[k];
    const PieceKeyframes & B = keyframes[k + 1];

    // now check the actual contract: at a handful of interior fractions,
    // every component of every piece must equal the exact affine blend of
    // A and B. This is what a "snap" stub (return A for frac<0.5, B
    // otherwise) fails: at frac=0.25 it would return A instead of the
    // blend 0.75*A+0.25*B, and at frac=0.75 it would return B instead of
    // 0.25*A+0.75*B -- caught below at both points.
    for (float frac : {0.25f, 0.5f, 0.75f}) {
      mv.setStep((float)k + frac);
      for (unsigned int p = 0; p < kMaxPiece; p++) {
        INFO("bracket " << k << " + " << frac << ", piece " << p);
        CHECK(mv.getX(p) == Catch::Approx((1 - frac) * A[p][0] + frac * B[p][0]).margin(1e-3));
        CHECK(mv.getY(p) == Catch::Approx((1 - frac) * A[p][1] + frac * B[p][1]).margin(1e-3));
        CHECK(mv.getZ(p) == Catch::Approx((1 - frac) * A[p][2] + frac * B[p][2]).margin(1e-3));
        CHECK(mv.getA(p) == Catch::Approx((1 - frac) * A[p][3] + frac * B[p][3]).margin(1e-4));
      }
    }
  }
}

TEST_CASE("disasmToMoves_c: piece 2's own reversal across the disassembly is real -- not sampling noise", "[disasm][moves]") {
  // The affine-blend test above deliberately only checks behaviour WITHIN
  // one bracket -- disasmToMoves_c's own contract -- and says nothing about
  // the sequence of keyframes across brackets, which comes from the solver,
  // not this class. This pins down, as a real value check rather than
  // prose, that those keyframes genuinely reverse for this puzzle: piece
  // 2's x goes 3 (step 0) -> 2 (step 1) -> 3 (step 5) -> 2 (step 14). A
  // real disassembly sequence can require backing a piece off before it
  // clears another piece on a later move; disasmToMoves_c faithfully
  // replays that, and doesn't (and shouldn't) try to smooth it away.
  const separation_c & root = cubeInCageSeparation();
  disasmToMoves_c mv(&root, kPushSize, kMaxPiece);

  mv.setStep(0);
  CHECK(mv.getX(2) == Catch::Approx(3.0f).margin(1e-4));
  mv.setStep(1);
  CHECK(mv.getX(2) == Catch::Approx(2.0f).margin(1e-4));   // moved -1
  mv.setStep(5);
  CHECK(mv.getX(2) == Catch::Approx(3.0f).margin(1e-4));   // moved back +1
  mv.setStep(14);
  CHECK(mv.getX(2) == Catch::Approx(2.0f).margin(1e-4));   // moved -1 again
}

TEST_CASE("disasmToMoves_c: the final step matches the tree's fully-separated state", "[disasm][moves]") {
  const separation_c & root = cubeInCageSeparation();
  const separation_c & mid = *root.getLeft();
  const separation_c & leaf = *mid.getLeft();
  disasmToMoves_c mv(&root, kPushSize, kMaxPiece);

  const unsigned int total = root.sumMoves();
  REQUIRE(total == 16u);

  mv.setStep((float)total);

  // pieces 0/1/2 are the leaf's never-further-separated remainder, placed
  // by getLeft()==nullptr straight from leaf.getState(leaf.getMoves()) --
  // cross-check dynamically against that call FIRST (the same pattern the
  // step-0 case above uses against root.getState(0)), so a regression in
  // either doRecursive or the literal table below shows up as a value
  // mismatch rather than the two silently drifting apart together. leaf's
  // local piece indices 0/1/2 are also named 0/1/2 (checked directly here,
  // rather than assumed), so no getPieceName() indirection is needed.
  const state_c * leafFinal = leaf.getState(leaf.getMoves());
  for (unsigned int p = 0; p < 3; p++) {
    INFO("piece " << p);
    REQUIRE(leaf.getPieceName(p) == p);
    CHECK(mv.getX(p) == Catch::Approx(leafFinal->getX(p)).margin(1e-4));
    CHECK(mv.getY(p) == Catch::Approx(leafFinal->getY(p)).margin(1e-4));
    CHECK(mv.getZ(p) == Catch::Approx(leafFinal->getZ(p)).margin(1e-4));
  }

  // pieces 3/4/5 are each peeled off at a different tree level (root, mid,
  // leaf respectively) and placed by the "vanish" branch using size=1000
  // to replace whichever axis crossed the >10000/-10000 sentinel
  const float expected[kMaxPiece][3] = {
    {1, 6, 3},        // piece 0
    {6, 2, 1},        // piece 1
    {2, 1, 0},        // piece 2
    {3, 1002, 3},     // piece 3: root's push is along y (+size)
    {-999, 7, -2},    // piece 4: mid's push is along x (-size)
    {2, 6, -994},     // piece 5: leaf's push is along z (-size)
  };

  for (unsigned int p = 0; p < kMaxPiece; p++) {
    INFO("piece " << p);
    CHECK(mv.getX(p) == Catch::Approx(expected[p][0]).margin(1e-4));
    CHECK(mv.getY(p) == Catch::Approx(expected[p][1]).margin(1e-4));
    CHECK(mv.getZ(p) == Catch::Approx(expected[p][2]).margin(1e-4));
    // fully disassembled: every piece has passed its own removal branch,
    // and every such branch adds alpha 0 (see doRecursive) -- so once the
    // whole tree is done, every piece is invisible by this class's design,
    // not just the ones that were literally "removed"
    CHECK(mv.getA(p) == Catch::Approx(0.0f).margin(1e-4));
    CHECK_FALSE(mv.moving(p));
  }

  // steady state: stepping further doesn't move anything, because nothing
  // in doRecursive depends on *how far* past a node's own moves the step is
  mv.setStep((float)total + 4);
  for (unsigned int p = 0; p < kMaxPiece; p++) {
    INFO("piece " << p);
    CHECK(mv.getX(p) == Catch::Approx(expected[p][0]).margin(1e-4));
    CHECK(mv.getY(p) == Catch::Approx(expected[p][1]).margin(1e-4));
    CHECK(mv.getZ(p) == Catch::Approx(expected[p][2]).margin(1e-4));
    CHECK(mv.getA(p) == Catch::Approx(0.0f).margin(1e-4));
  }
}

TEST_CASE("disasmToMoves_c: the constructor's size controls exactly how far a peeled piece is pushed", "[disasm][moves]") {
  const separation_c & root = cubeInCageSeparation();

  disasmToMoves_c small(&root, 1000, kMaxPiece);
  disasmToMoves_c big(&root, 3000, kMaxPiece);

  const unsigned int total = root.sumMoves();
  small.setStep((float)total);
  big.setStep((float)total);

  // the 2000-unit difference in size shows up ONLY on the one axis each
  // piece is pushed along, and nowhere else
  INFO("piece 3, pushed along y");
  CHECK(big.getY(3) - small.getY(3) == Catch::Approx(2000.0f).margin(1e-3));
  CHECK(big.getX(3) == Catch::Approx(small.getX(3)).margin(1e-4));
  CHECK(big.getZ(3) == Catch::Approx(small.getZ(3)).margin(1e-4));

  INFO("piece 4, pushed along x (negative direction)");
  CHECK(big.getX(4) - small.getX(4) == Catch::Approx(-2000.0f).margin(1e-3));
  CHECK(big.getY(4) == Catch::Approx(small.getY(4)).margin(1e-4));
  CHECK(big.getZ(4) == Catch::Approx(small.getZ(4)).margin(1e-4));

  INFO("piece 5, pushed along z (negative direction)");
  CHECK(big.getZ(5) - small.getZ(5) == Catch::Approx(-2000.0f).margin(1e-3));
  CHECK(big.getX(5) == Catch::Approx(small.getX(5)).margin(1e-4));
  CHECK(big.getY(5) == Catch::Approx(small.getY(5)).margin(1e-4));

  // pieces 0/1/2 are never pushed at all -- size must not affect them
  for (unsigned int p : {0u, 1u, 2u}) {
    INFO("piece " << p << " is never pushed");
    CHECK(big.getX(p) == Catch::Approx(small.getX(p)).margin(1e-4));
    CHECK(big.getY(p) == Catch::Approx(small.getY(p)).margin(1e-4));
    CHECK(big.getZ(p) == Catch::Approx(small.getZ(p)).margin(1e-4));
  }
}

namespace {

/* Loads examples/CubeInCage.xmpuzzle a second time (via a function-local
   static, so still only once) to get at a problem_c that outlives this
   function. cubeInCageSeparation() above cannot supply one: its puzzle_c and
   problem_c are local to its lambda and are destroyed once the
   self-contained separation_c tree is built from them -- that tree never
   references the puzzle or problem again, but a movementCache_c is
   constructed directly from a live problem_c&, and problem_c::getPuzzle()
   returns a reference into the puzzle_c that owns it. So the puzzle can't be
   discarded here the way cubeInCageSeparation() discards it, and a second
   load is genuinely needed to get a problem_c with the right lifetime. */
problem_c & cubeInCageProblem() {
  static const std::unique_ptr<puzzle_c> puzzle = [] {
    std::unique_ptr<std::istream> str(openGzFile("examples/CubeInCage.xmpuzzle"));
    REQUIRE(str != nullptr);
    xmlParser_c pars(*str);
    return std::make_unique<puzzle_c>(pars);
  }();
  return *puzzle->getProblem(0);
}

/* gridType_c::getMovementCache() returns a movementCache_1_c only for
   GT_TRIANGULAR_PRISM, a grid CubeInCage does not use (it is GT_BRICKS). So
   covering movementCache_1_c needs a puzzle on that other grid --
   examples/Prisgon.xmpuzzle is one such bundled example puzzle (so is
   examples/Bermuda.xmpuzzle, but Prisgon is the one loaded here). This is a
   third load overall (a different puzzle entirely), which the CubeInCage
   helper could never have supplied however it were reshaped.

   The file is opened once, via a function-local static, and both problem 0
   (used below for movementCache_1_c coverage) and problem 1 (used by
   prisgonBranchingTree() further down for a branching separation tree) are
   handed out of this single load. */
puzzle_c & prisgonPuzzle() {
  static const std::unique_ptr<puzzle_c> puzzle = [] {
    std::unique_ptr<std::istream> str(openGzFile("examples/Prisgon.xmpuzzle"));
    REQUIRE(str != nullptr);
    xmlParser_c pars(*str);
    return std::make_unique<puzzle_c>(pars);
  }();
  return *puzzle;
}

problem_c & prisgonProblem() {
  return *prisgonPuzzle().getProblem(0);
}

/* CubeInCage's separation tree (used by every "CubeInCage separation tree"
   and "disasmToMoves_c" case above) is a pure getLeft() spine: every node's
   getRemoved() is null. That leaves the two-child case completely
   untouched by this file -- in the tree walk, in piece reconciliation, in
   movesText2's "removed &&" branch, and in doRecursive's entire
   if(tree->getRemoved()) placement path (disasmtomoves.cpp). The tree
   below covers the first two of those four; see the synthetic-tree case at
   the end of this file for movesText2, and the note there for why
   doRecursive's branch remains uncovered.

   examples/Prisgon.xmpuzzle problem 1's saved solution carries a fully
   -saved BRANCHING separation tree, reachable via
   getSavedSolution(0)->getDisassembly() with NO SOLVE AT ALL: 8 nodes,
   branching at depth 2 (the node two levels below the root has both
   getLeft() and getRemoved() non-null), movesText "9.5.1.1.2.2.2", sumMoves
   23. A loaded tree like this one is also LESS brittle than a solved one
   -- it is repo data, not solver output, so it can't shift out from under
   these tests the way a solved tree could if the solver's search order
   ever changed. */
const separation_c & prisgonBranchingTree() {
  problem_c * problem = prisgonPuzzle().getProblem(1);
  REQUIRE(problem != nullptr);
  REQUIRE(problem->getNumberOfSavedSolutions() > 0);

  const solution_c * sol = problem->getSavedSolution(0);
  const separation_c * tree = sol->getDisassembly();
  REQUIRE(tree != nullptr);

  return *tree;
}

/* getMoValue()'s dx/dy/dz/t1/t2 describe piece p2's placement relative to
   piece p1 -- but not for an arbitrary pair of placements: movementCache_1_c
   (src/lib/movementcache_1.cpp) bt_asserts that the two pieces, so placed,
   do not overlap. A problem's saved solution is a real assembled state, so
   every pair of placed pieces within it is guaranteed non-overlapping by
   construction; pulling dx/dy/dz/t1/t2 out of it (the same way
   movementAnalysator_c::prepare() pulls them out of a disassembly search
   node, src/lib/movementanalysator.cpp) gives queries the cache can actually
   answer instead of tripping that assertion. */
struct MoQuery { int dx, dy, dz; unsigned char t1, t2; unsigned int p1, p2; };

MoQuery queryFromSolution(const problem_c & problem, unsigned int p1, unsigned int p2) {
  REQUIRE(problem.getNumberOfSavedSolutions() > 0);
  const solution_c * sol = problem.getSavedSolution(0);
  const assembly_c * assm = sol->getAssembly();
  REQUIRE(assm->isPlaced(p1));
  REQUIRE(assm->isPlaced(p2));

  return MoQuery{
    assm->getX(p2) - assm->getX(p1),
    assm->getY(p2) - assm->getY(p1),
    assm->getZ(p2) - assm->getZ(p1),
    assm->getTransformation(p1),
    assm->getTransformation(p2),
    p1, p2
  };
}

} // namespace

TEST_CASE("movement cache: the cube grid yields a movementCache_0_c -- and repeating a query returns the identical value", "[disasm][movementcache]") {
  problem_c & problem = cubeInCageProblem();
  const gridType_c * gt = problem.getPuzzle().getGridType();
  REQUIRE(gt->getType() == gridType_c::GT_BRICKS);

  std::unique_ptr<movementCache_c> cache(gt->getMovementCache(problem));
  REQUIRE(cache != nullptr);
  REQUIRE(dynamic_cast<movementCache_0_c *>(cache.get()) != nullptr);
  REQUIRE(cache->numDirections() == 3);

  // getMoValue()'s 3-entry move[] is indexed by direction, and getDirection()
  // is what actually assigns each index its axis (movementCache_0_c::getDirection,
  // src/lib/movementcache_0.cpp): index 0 is +x, 1 is +y, 2 is +z. This is the
  // fact that gives the pinned {0, 32000, 0} triple below its meaning -- "free
  // in y" means specifically move[1], because getDirection(1) is the y axis.
  int dx, dy, dz;
  cache->getDirection(0, &dx, &dy, &dz);
  CHECK((dx == 1 && dy == 0 && dz == 0));
  cache->getDirection(1, &dx, &dy, &dz);
  CHECK((dx == 0 && dy == 1 && dz == 0));
  cache->getDirection(2, &dx, &dy, &dz);
  CHECK((dx == 0 && dy == 0 && dz == 1));

  // piece 0 relative to piece 1, taken from CubeInCage's saved solution: a
  // real, non-overlapping placement. The values below were established by
  // running this exact query against the cache: piece 1 is free to move
  // without limit in y (the 32000 sentinel, see moCalcValues()'s initial mx =
  // my = mz = 32000) but is blocked at the very first step in x and z.
  MoQuery q = queryFromSolution(problem, 0, 1);

  unsigned int first[3];
  cache->getMoValue(q.dx, q.dy, q.dz, q.t1, q.t2, q.p1, q.p2, first);
  CHECK(first[0] == 0u);
  CHECK(first[1] == 32000u);
  CHECK(first[2] == 0u);

  // repeating the identical query must return the identical answer -- the
  // memoisation is transparent to the caller
  unsigned int second[3];
  cache->getMoValue(q.dx, q.dy, q.dz, q.t1, q.t2, q.p1, q.p2, second);
  CHECK(second[0] == first[0]);
  CHECK(second[1] == first[1]);
  CHECK(second[2] == first[2]);
}

TEST_CASE("movement cache: cube grid queries for a set of piece pairs do not depend on the order they are asked in", "[disasm][movementcache]") {
  // a cache whose hash table leaked state between entries (e.g. a rehash
  // that dropped or corrupted an existing bucket) would answer at least one
  // of these differently depending on what had already been inserted before
  // it, so the pairs are populated into two FRESH cache instances, one in
  // forward order and one in reverse, and the two are compared -- querying
  // twice against the same instance would just hit the memoisation checked
  // above and never insert anything between the two passes.
  problem_c & problem = cubeInCageProblem();
  const gridType_c * gt = problem.getPuzzle().getGridType();

  struct Expected { unsigned int p1, p2; unsigned int move[3]; };
  const Expected cases[] = {
    { 0, 1, { 0, 32000, 0 } },
    { 2, 3, { 32000, 0, 1 } },
    { 4, 5, { 0, 0, 0 } },
  };

  std::unique_ptr<movementCache_c> forwardCache(gt->getMovementCache(problem));
  REQUIRE(forwardCache != nullptr);
  unsigned int forward[3][3];
  for (unsigned int k = 0; k < 3; k++) {
    MoQuery q = queryFromSolution(problem, cases[k].p1, cases[k].p2);
    forwardCache->getMoValue(q.dx, q.dy, q.dz, q.t1, q.t2, q.p1, q.p2, forward[k]);
  }

  std::unique_ptr<movementCache_c> backwardCache(gt->getMovementCache(problem));
  REQUIRE(backwardCache != nullptr);
  unsigned int backward[3][3];
  for (unsigned int k = 3; k-- > 0; ) {
    MoQuery q = queryFromSolution(problem, cases[k].p1, cases[k].p2);
    backwardCache->getMoValue(q.dx, q.dy, q.dz, q.t1, q.t2, q.p1, q.p2, backward[k]);
  }

  for (unsigned int k = 0; k < 3; k++) {
    INFO("pair (" << cases[k].p1 << "," << cases[k].p2 << ")");
    for (unsigned int d = 0; d < 3; d++) {
      CHECK(forward[k][d] == cases[k].move[d]);
      CHECK(backward[k][d] == cases[k].move[d]);
    }
  }
}

TEST_CASE("movement cache: the triangular-prism grid yields a movementCache_1_c -- and repeating a query returns the identical value", "[disasm][movementcache]") {
  // GT_TRIANGULAR_PRISM is the only other grid type gridType_c::getMovementCache()
  // implements (see src/lib/gridtype.cpp); examples/Prisgon.xmpuzzle is a
  // bundled example on that grid (examples/Bermuda.xmpuzzle is another).
  problem_c & problem = prisgonProblem();
  const gridType_c * gt = problem.getPuzzle().getGridType();
  REQUIRE(gt->getType() == gridType_c::GT_TRIANGULAR_PRISM);

  std::unique_ptr<movementCache_c> cache(gt->getMovementCache(problem));
  REQUIRE(cache != nullptr);
  REQUIRE(dynamic_cast<movementCache_1_c *>(cache.get()) != nullptr);
  REQUIRE(cache->numDirections() == 4);

  // piece 6 relative to piece 0, taken from Prisgon's saved solution. As
  // above, the expected values were established by running this exact query.
  MoQuery q = queryFromSolution(problem, 0, 6);

  unsigned int first[4];
  cache->getMoValue(q.dx, q.dy, q.dz, q.t1, q.t2, q.p1, q.p2, first);
  CHECK(first[0] == 1u);
  CHECK(first[1] == 1u);
  CHECK(first[2] == 2u);
  CHECK(first[3] == 0u);

  unsigned int second[4];
  cache->getMoValue(q.dx, q.dy, q.dz, q.t1, q.t2, q.p1, q.p2, second);
  CHECK(second[0] == first[0]);
  CHECK(second[1] == first[1]);
  CHECK(second[2] == first[2]);
  CHECK(second[3] == first[3]);
}

TEST_CASE("movement cache: triangular-prism grid queries for a set of piece pairs do not depend on the order they are asked in", "[disasm][movementcache]") {
  // as with the cube-grid case above: two FRESH cache instances, one
  // populated forward and one in reverse, so a leak between entries during
  // insertion would actually have a chance to show up.
  problem_c & problem = prisgonProblem();
  const gridType_c * gt = problem.getPuzzle().getGridType();

  struct Expected { unsigned int p1, p2; unsigned int move[4]; };
  const Expected cases[] = {
    { 0, 6, { 1, 1, 2, 0 } },
    { 0, 7, { 1, 1, 1, 0 } },
    { 5, 7, { 0, 32000, 1, 1 } },
  };

  std::unique_ptr<movementCache_c> forwardCache(gt->getMovementCache(problem));
  REQUIRE(forwardCache != nullptr);
  unsigned int forward[3][4];
  for (unsigned int k = 0; k < 3; k++) {
    MoQuery q = queryFromSolution(problem, cases[k].p1, cases[k].p2);
    forwardCache->getMoValue(q.dx, q.dy, q.dz, q.t1, q.t2, q.p1, q.p2, forward[k]);
  }

  std::unique_ptr<movementCache_c> backwardCache(gt->getMovementCache(problem));
  REQUIRE(backwardCache != nullptr);
  unsigned int backward[3][4];
  for (unsigned int k = 3; k-- > 0; ) {
    MoQuery q = queryFromSolution(problem, cases[k].p1, cases[k].p2);
    backwardCache->getMoValue(q.dx, q.dy, q.dz, q.t1, q.t2, q.p1, q.p2, backward[k]);
  }

  for (unsigned int k = 0; k < 3; k++) {
    INFO("pair (" << cases[k].p1 << "," << cases[k].p2 << ")");
    for (unsigned int d = 0; d < 4; d++) {
      CHECK(forward[k][d] == cases[k].move[d]);
      CHECK(backward[k][d] == cases[k].move[d]);
    }
  }
}

TEST_CASE("movement cache: grid types without an implementation yield no cache at all", "[disasm][movementcache]") {
  // gridType_c::getMovementCache() only has cases for GT_BRICKS and
  // GT_TRIANGULAR_PRISM (src/lib/gridtype.cpp); every other grid type falls
  // through to `default: return 0`. That default branch never touches the
  // problem_c argument, so it is safe to probe it with any already-loaded
  // problem, even though these grid types are not the puzzle's own.
  problem_c & problem = cubeInCageProblem();

  for (gridType_c::gridType t : { gridType_c::GT_SPHERES, gridType_c::GT_RHOMBIC, gridType_c::GT_TETRA_OCTA }) {
    INFO("grid type " << t);
    gridType_c gt(t);
    std::unique_ptr<movementCache_c> cache(gt.getMovementCache(problem));
    CHECK(cache == nullptr);
  }
}

TEST_CASE("Prisgon problem 1 branching separation tree: getPieceNumber()/getPieceName() account for every piece -- including at a branch", "[disasm][separation]") {
  // the branching counterpart to the CubeInCage piece-reconciliation case
  // above: CubeInCage's tree never has two children at once, so it never
  // exercises reconciling BOTH a getLeft() and a getRemoved() child against
  // the same parent's piece names
  const separation_c & root = prisgonBranchingTree();
  const unsigned int totalPieces = root.getPieceNumber();   // 9, examples/Prisgon.xmpuzzle problem 1

  REQUIRE(totalPieces == 9u);

  std::set<unsigned int> rootNames;
  for (unsigned int i = 0; i < root.getPieceNumber(); i++) {
    unsigned int name = root.getPieceName(i);
    CHECK(name < totalPieces);
    CHECK(rootNames.insert(name).second);
  }
  CHECK(rootNames.size() == totalPieces);

  // walk down to the branching node: root --left--> depth1 --left--> branch,
  // where branch has BOTH a getLeft() and a getRemoved() child
  REQUIRE(root.getLeft() != nullptr);
  const separation_c & depth1 = *root.getLeft();
  REQUIRE(depth1.getLeft() != nullptr);
  const separation_c & branch = *depth1.getLeft();
  REQUIRE(branch.getLeft() != nullptr);
  REQUIRE(branch.getRemoved() != nullptr);

  std::set<unsigned int> branchNames;
  for (unsigned int i = 0; i < branch.getPieceNumber(); i++) {
    unsigned int name = branch.getPieceName(i);
    CHECK(name < totalPieces);
    CHECK(branchNames.insert(name).second);
  }
  CHECK(branch.getPieceNumber() < depth1.getPieceNumber());

  // both children's names must each be a subset of the branching node's own
  // names, and each child must have fewer pieces than its parent
  for (const separation_c * child : { branch.getLeft(), branch.getRemoved() }) {
    std::set<unsigned int> childNames;
    for (unsigned int i = 0; i < child->getPieceNumber(); i++) {
      unsigned int name = child->getPieceName(i);
      CHECK(name < totalPieces);
      CHECK(childNames.insert(name).second);
    }
    CHECK(child->getPieceNumber() < branch.getPieceNumber());
    for (unsigned int name : childNames)
      CHECK(branchNames.count(name) == 1);
  }
}

TEST_CASE("Prisgon problem 1 branching separation tree: navigation terminates and never revisits a node -- taking both children", "[disasm][separation]") {
  // the branching counterpart to the CubeInCage tree-walk case above:
  // CubeInCage's tree is a pure getLeft() spine, so its walk's
  // node->getRemoved() branch is never actually taken there
  const separation_c & root = prisgonBranchingTree();

  std::set<const separation_c *> visited;
  std::vector<const separation_c *> pending{&root};
  int nodeCount = 0;
  bool tookRemovedBranch = false;

  while (!pending.empty()) {
    const separation_c * node = pending.back();
    pending.pop_back();

    REQUIRE(visited.insert(node).second);   // never visited before
    nodeCount++;

    unsigned int childPieces = 0;

    if (node->getLeft()) {
      CHECK(node->getLeft()->getPieceNumber() < node->getPieceNumber());
      childPieces += node->getLeft()->getPieceNumber();
      pending.push_back(node->getLeft());
    }
    if (node->getRemoved()) {
      CHECK(node->getRemoved()->getPieceNumber() < node->getPieceNumber());
      childPieces += node->getRemoved()->getPieceNumber();
      pending.push_back(node->getRemoved());
      tookRemovedBranch = true;
    }

    CHECK(childPieces <= node->getPieceNumber());
  }

  CHECK(nodeCount == 8);
  CHECK(tookRemovedBranch);   // confirms this walk actually visited via getRemoved()
}


/* movesText2 appends a dot and recurses for the removed child only when
   `removed && removed->containsMultiMoves()` (disassembly.cpp:481). No
   bundled example puzzle can reach that body. Scanning every saved
   disassembly tree that ships in examples/ -- six of them across the five
   puzzles that save one -- exactly two branch at all: PelikanBurr problem 0
   (two branch nodes) and Prisgon problem 1 (one, the tree
   prisgonBranchingTree() returns). In both, every removed child is a single
   one-move separation, so states.size() is 2, containsMultiMoves() is false
   and the body is skipped. Prisgon's movesText is "9.5.1.1.2.2.2": seven
   numbers for the seven nodes down its getLeft() spine, with nothing
   contributed by the removed child hanging off depth 2.

   So the only way to reach it is to build a tree directly, through
   separation_c's public (removed, left, pieces) constructor and addstate().
   That is a genuine unit test of separation_c's own API rather than a
   reach-through into solver output, and it pins the one thing a loaded tree
   cannot show: that the removed child's numbers are appended AFTER the left
   child's, not instead of them.

   separation_c's destructor deletes both children and every state, so the
   root's unique_ptr owns the whole structure -- the children are handed to
   it by move, so nothing is left owning a raw pointer at any point;
   addstate() bt_asserts that a
   state's piece count matches its node's, hence the per-node piece lists
   below. States carry no positions because movesText looks only at how many
   there are. */
namespace {

std::unique_ptr<separation_c> makeSeparation(std::unique_ptr<separation_c> removed,
                                             std::unique_ptr<separation_c> left,
                                             const std::vector<unsigned int> & pieces,
                                             unsigned int moves) {
  std::unique_ptr<separation_c> node(new separation_c(removed.release(), left.release(), pieces));
  // getMoves() is states.size()-1, so a node of n moves needs n+1 states
  for (unsigned int i = 0; i <= moves; i++)
    node->addstate(new state_c(static_cast<unsigned int>(pieces.size())));
  return node;
}

} // namespace

TEST_CASE("separation tree: movesText appends the removed child's moves after the left child's", "[disasm][separation]") {

  SECTION("both children report -- the removed child's numbers come last") {
    // root(2 moves) with left(2 moves) and removed(3 moves); both children
    // exceed one move, so both clear containsMultiMoves()
    std::unique_ptr<separation_c> left    = makeSeparation(nullptr, nullptr, {1}, 2);
    std::unique_ptr<separation_c> removed = makeSeparation(nullptr, nullptr, {2}, 3);
    std::unique_ptr<separation_c> root = makeSeparation(std::move(removed), std::move(left), {0, 1, 2}, 2);

    REQUIRE(root->getLeft() != nullptr);
    REQUIRE(root->getRemoved() != nullptr);
    REQUIRE(root->getMoves() == 2u);
    REQUIRE(root->getLeft()->getMoves() == 2u);
    REQUIRE(root->getRemoved()->getMoves() == 3u);

    const std::string got = root->movesText();

    // "2.2.3", not "2.2" -- the trailing 3 is the removed child, and it is
    // the last component, which is what distinguishes the removed branch
    // from the left one
    CHECK(got == "2.2.3");
    CHECK(root->sumMoves() == 7u);
    CHECK(root->getNumSequences() == 3u);
  }

  SECTION("only the removed child reports -- its dot is its own, not the left child's") {
    // same shape, except the left child takes a single move, so
    // containsMultiMoves() is false for it and movesText skips it entirely.
    // Without this section, "2.2.3" above could not tell a removed-branch
    // dot apart from a second left-branch dot.
    std::unique_ptr<separation_c> left    = makeSeparation(nullptr, nullptr, {1}, 1);
    std::unique_ptr<separation_c> removed = makeSeparation(nullptr, nullptr, {2}, 3);
    std::unique_ptr<separation_c> root = makeSeparation(std::move(removed), std::move(left), {0, 1, 2}, 2);

    const std::string got = root->movesText();

    CHECK(got == "2.3");
  }

  SECTION("a removed child of a single move contributes nothing -- the loaded-tree case") {
    // this is the shape every branching tree in examples/ actually has, and
    // the reason none of them reaches the branch the sections above cover
    std::unique_ptr<separation_c> left    = makeSeparation(nullptr, nullptr, {1}, 2);
    std::unique_ptr<separation_c> removed = makeSeparation(nullptr, nullptr, {2}, 1);
    std::unique_ptr<separation_c> root = makeSeparation(std::move(removed), std::move(left), {0, 1, 2}, 2);

    const std::string got = root->movesText();

    CHECK(got == "2.2");
  }
}

/* movesText() formats into a fixed 256-byte stack buffer and guards every
   append with `len2+5 > len` (disassembly.cpp), so a tree deep enough to
   outrun that buffer must stop cleanly rather than run off the end.
   movesText() takes no length parameter, so the only way to reach that guard
   is a tree deep enough to hit it: each nested two-move node contributes "2."
   to the string, so about 130 of them fill the buffer. */
TEST_CASE("separation tree: movesText stops at its internal buffer instead of overrunning it", "[disasm][separation]") {
  // 200 levels is comfortably past the 256-byte buffer, so the guard has to
  // fire partway down rather than at the last level
  std::unique_ptr<separation_c> tree = makeSeparation(nullptr, nullptr, {199}, 2);
  for (unsigned int i = 199; i-- > 0; )
    tree = makeSeparation(nullptr, std::move(tree), {i}, 2);

  const std::string got = tree->movesText();

  // It stopped just short of the 256-byte buffer rather than at the end of
  // the tree, which would have taken 399 characters. Pinning the exact length
  // rather than an upper bound means a recursion that bailed out early --
  // returning "2", say -- fails here instead of satisfying "< 256" by
  // accident. The string is still well formed: only digits and separators.
  CHECK(got.size() == 253);
  CHECK(got.find_first_not_of("0123456789.") == std::string::npos);

  // and the tree really is the deep one, so 399 characters is what the
  // formatter was asked for
  CHECK(tree->sumMoves() == 400u);
}
