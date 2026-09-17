#include <catch2/catch_test_macros.hpp>

#include "lib/disassembly.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/solution.h"
#include "tools/gzstream.h"
#include "tools/xml.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

/* The write side of disassembly.cpp: state_c::save(), separation_c::save()
   and separationInfo_c::save(). #61 covered the disassembly engine's
   internals and deliberately left serialising a tree back out to XML to a
   serialization-focused package; this is it.

   Nothing here runs a disassembler. examples/PelikanBurr.xmpuzzle ships a
   saved solution carrying a full <separation> tree, which parses straight
   from the file -- repo data, so it cannot shift underneath these cases the
   way solver output could, and it costs nothing.

   The cases are written as roundtrips and fixpoints rather than as
   comparisons against recorded XML. A golden-file test of this output would
   be re-recorded rather than investigated the first time it failed, which
   is exactly the regression it would exist to catch. */

namespace {

/* the bundled Pelikan burr, parsed once and kept for the lifetime of the
   test binary. Every case below only reads from it. */
const puzzle_c & pelikan() {
  static const std::unique_ptr<puzzle_c> puz = [] {
    std::unique_ptr<std::istream> str(openGzFile("examples/PelikanBurr.xmpuzzle"));
    REQUIRE(str != nullptr);
    xmlParser_c pars(*str);
    return std::make_unique<puzzle_c>(pars);
  }();
  return *puz;
}

/** the separation tree the bundled puzzle's saved solution carries */
const separation_c & pelikanSeparation() {
  const problem_c * p = pelikan().getProblem(0);
  REQUIRE(p != nullptr);
  REQUIRE(p->getNumberOfSavedSolutions() > 0);

  const separation_c * sep = p->getSavedSolution(0)->getDisassembly();
  REQUIRE(sep != nullptr);
  return *sep;
}

/** serialise a separation tree to a standalone XML document */
std::string writeSeparation(const separation_c & sep) {
  std::ostringstream out;
  {
    xmlWriter_c xml(out);
    sep.save(xml);
  }
  return out.str();
}

/** parse a document written by writeSeparation back into a tree */
std::unique_ptr<separation_c> readSeparation(const std::string & doc, unsigned int pieces) {
  std::istringstream in(doc);
  xmlParser_c pars(in);
  pars.nextTag();
  return std::make_unique<separation_c>(pars, pieces);
}

/* Structural equality of two separation trees, over everything the save
   format persists: the piece list, every state's per-piece position, and
   both children, recursively.

   Returns a bool rather than asserting so a caller can put the whole
   comparison inside one REQUIRE. */
bool sameTree(const separation_c & a, const separation_c & b) {
  if (a.getPieceNumber() != b.getPieceNumber()) return false;

  for (unsigned int i = 0; i < a.getPieceNumber(); i++)
    if (a.getPieceName(i) != b.getPieceName(i)) return false;

  if (a.getMoves() != b.getMoves()) return false;

  for (unsigned int s = 0; s <= a.getMoves(); s++) {
    const state_c * sa = a.getState(s);
    const state_c * sb = b.getState(s);
    if (sa->getPiecenumber() != sb->getPiecenumber()) return false;

    for (unsigned int i = 0; i < sa->getPiecenumber(); i++) {
      /* a removed piece's coordinates are a sentinel rather than a
         position, so compare removedness first and only compare
         coordinates for pieces that have one */
      if (sa->pieceRemoved(i) != sb->pieceRemoved(i)) return false;
      if (sa->pieceRemoved(i)) continue;

      if (sa->getX(i) != sb->getX(i)) return false;
      if (sa->getY(i) != sb->getY(i)) return false;
      if (sa->getZ(i) != sb->getZ(i)) return false;
    }
  }

  if ((a.getLeft() == nullptr) != (b.getLeft() == nullptr)) return false;
  if ((a.getRemoved() == nullptr) != (b.getRemoved() == nullptr)) return false;

  if (a.getLeft() && !sameTree(*a.getLeft(), *b.getLeft())) return false;
  if (a.getRemoved() && !sameTree(*a.getRemoved(), *b.getRemoved())) return false;

  return true;
}

/** how many separation nodes the tree holds, counting both children */
unsigned int countNodes(const separation_c & s) {
  unsigned int n = 1;
  if (s.getLeft())    n += countNodes(*s.getLeft());
  if (s.getRemoved()) n += countNodes(*s.getRemoved());
  return n;
}

} // namespace

TEST_CASE("separation save: the tree survives a write and read unchanged", "[disasm][save]") {
  const separation_c & orig = pelikanSeparation();

  /* the fixture has to be worth roundtripping: a single-node tree with one
     state would pass this against a save() that emitted nothing but the
     root, so assert the shape before relying on it */
  REQUIRE(orig.getPieceNumber() > 1);
  REQUIRE(countNodes(orig) > 1);
  REQUIRE(orig.getMoves() > 0);

  std::string doc = writeSeparation(orig);
  std::unique_ptr<separation_c> back = readSeparation(doc, orig.getPieceNumber());

  REQUIRE(sameTree(orig, *back));
}

TEST_CASE("separation save: writing what was read reproduces the same document byte for byte",
          "[disasm][save]") {
  const separation_c & orig = pelikanSeparation();

  std::string first = writeSeparation(orig);
  std::unique_ptr<separation_c> back = readSeparation(first, orig.getPieceNumber());
  std::string second = writeSeparation(*back);

  /* A fixpoint, not a golden file: nothing here records what the output
     should look like, only that writing and reading are inverse. That
     catches a field the writer emits and the parser drops, or one the
     parser defaults differently on the way back, neither of which the
     structural comparison above would necessarily see -- it only knows
     about the accessors it was taught. */
  REQUIRE(first == second);
}

TEST_CASE("separation save: the child separations are tagged so the parser can tell removed "
          "from left", "[disasm][save]") {
  const separation_c & orig = pelikanSeparation();

  /* the premise: this tree really does branch, so there is something for
     the type attribute to disambiguate */
  REQUIRE(orig.getLeft() != nullptr);
  REQUIRE(orig.getRemoved() != nullptr);

  std::string doc = writeSeparation(orig);

  REQUIRE(doc.find("type=\"removed\"") != std::string::npos);
  REQUIRE(doc.find("type=\"left\"") != std::string::npos);

  /* The root carries no type attribute -- save()'s case 0. Checked by
     counting: a tree with one branching node emits exactly one "left" and
     one "removed" per branch, so if the root were tagged too the totals
     would not match the branch count. */
  unsigned int branches = 0;
  std::vector<const separation_c *> stack{&orig};
  while (!stack.empty()) {
    const separation_c * s = stack.back();
    stack.pop_back();
    if (s->getLeft())    { branches++; stack.push_back(s->getLeft()); }
    if (s->getRemoved()) { stack.push_back(s->getRemoved()); }
  }

  unsigned int leftTags = 0;
  for (size_t at = doc.find("type=\"left\""); at != std::string::npos;
       at = doc.find("type=\"left\"", at + 1))
    leftTags++;

  REQUIRE(leftTags == branches);
}

TEST_CASE("separation save: swapping which shapes two pieces name is reflected in what is written",
          "[disasm][save]") {
  const separation_c & orig = pelikanSeparation();

  /* exchangeShape renames pieces within the tree. Working on a copy, since
     the shared fixture is read-only for every other case in this file. */
  separation_c copy(&orig);

  REQUIRE(copy.getPieceNumber() >= 2);
  unsigned int first = copy.getPieceName(0);
  unsigned int second = copy.getPieceName(1);

  /* the two names must genuinely differ or the swap is unobservable */
  REQUIRE(first != second);

  /* Find a child node that also names one of the two, and remember where.

     The root's pieces array is the identity [0 1 2 ...], so at the root a
     piece's NAME equals its INDEX -- and a positional swap of slots 0 and 1
     produces exactly the same array as the rename that exchangeShape
     actually performs. Looking only at the root cannot tell the two apart,
     and cannot see that the rename is supposed to recurse at all. A child
     node holds a subset in its own order, so both properties become
     visible there. */
  const separation_c * child = copy.getRemoved() ? copy.getRemoved() : copy.getLeft();
  REQUIRE(child != nullptr);

  std::vector<unsigned int> childBefore;
  for (unsigned int i = 0; i < child->getPieceNumber(); i++)
    childBefore.push_back(child->getPieceName(i));
  REQUIRE(childBefore.size() >= 2);

  copy.exchangeShape(first, second);

  REQUIRE(copy.getPieceName(0) == second);
  REQUIRE(copy.getPieceName(1) == first);

  /* the child followed the rename: every occurrence of the two names is
     swapped in place, and every other name is untouched */
  {
    const separation_c * c = copy.getRemoved() ? copy.getRemoved() : copy.getLeft();
    REQUIRE(c != nullptr);
    REQUIRE(c->getPieceNumber() == childBefore.size());

    for (unsigned int i = 0; i < c->getPieceNumber(); i++) {
      INFO("child slot " << i);
      const unsigned int was = childBefore[i];
      const unsigned int want = (was == first) ? second : (was == second) ? first : was;
      REQUIRE(c->getPieceName(i) == want);
    }
  }

  /* and the change reaches the serialised form rather than living only in
     memory */
  std::unique_ptr<separation_c> back =
      readSeparation(writeSeparation(copy), copy.getPieceNumber());

  REQUIRE(back->getPieceName(0) == second);
  REQUIRE(back->getPieceName(1) == first);
}

TEST_CASE("separationInfo save: the summary survives a write and read unchanged",
          "[disasm][save]") {
  const separation_c & orig = pelikanSeparation();

  /* separationInfo_c is the compact form: it keeps the shape of the
     disassembly without the per-state positions. Built here from the full
     tree, which is the conversion the application itself performs when it
     drops a disassembly but keeps its complexity. */
  separationInfo_c info(&orig);

  REQUIRE(info.getNumSequences() > 0);

  std::ostringstream out;
  {
    xmlWriter_c xml(out);
    info.save(xml);
  }

  std::istringstream in(out.str());
  xmlParser_c pars(in);
  pars.nextTag();
  separationInfo_c back(pars);

  REQUIRE(back.getNumSequences() == info.getNumSequences());
  REQUIRE(back.sumMoves() == info.sumMoves());
  REQUIRE(back.movesText() == info.movesText());

  for (unsigned int i = 0; i < info.getNumSequences(); i++) {
    INFO("sequence " << i);
    REQUIRE(back.getSequenceLength(i) == info.getSequenceLength(i));
  }
}

TEST_CASE("separationInfo: the flattened form lists the tree's sequence lengths in order with a "
          "zero marking each absent child", "[disasm][save]") {
  const separation_c & orig = pelikanSeparation();
  separationInfo_c info(&orig);

  /* separation_c and separationInfo_c both implement disassembly_c's
     getNumSequences()/getSequenceLength() pair, and they do NOT agree on
     what the index means. This is the relationship between them, pinned
     because it is not obvious and because nothing else records it:

       separation_c indexes separation NODES. getNumSequences() is the
       number of nodes in the tree -- 6 for this fixture.

       separationInfo_c indexes its flattened values array, which stores
       each node's move count in pre-order and pushes a 0 for every ABSENT
       child. getNumSequences() is that array's length -- 13 here, being 6
       nodes plus the 7 empty child slots a six-node binary tree has.

     So the two report different totals for the same disassembly, and an
     index means different things to each. What holds is that the NON-ZERO
     entries of the flattened form are exactly the tree's sequence lengths,
     in the same order. That is the invariant worth depending on, and it is
     what makes the compact form a faithful summary despite the differing
     counts.

     The divergence is recorded, not fixed: two subclasses returning
     different quantities for one virtual is at least surprising, but
     deciding which one is wrong changes the meaning of stored data and is a
     maintainer's call, not a coverage change's. Raised separately. */
  REQUIRE(info.getNumSequences() != orig.getNumSequences());

  std::vector<unsigned int> nonZero;
  for (unsigned int i = 0; i < info.getNumSequences(); i++)
    if (info.getSequenceLength(i) != 0)
      nonZero.push_back(info.getSequenceLength(i));

  REQUIRE(nonZero.size() == orig.getNumSequences());

  for (unsigned int i = 0; i < orig.getNumSequences(); i++) {
    INFO("sequence " << i);
    REQUIRE(nonZero[i] == orig.getSequenceLength(i));
  }
}

TEST_CASE("separationInfo: the summary reports the same move totals as the tree it was built from",
          "[disasm][save]") {
  const separation_c & orig = pelikanSeparation();
  separationInfo_c info(&orig);

  /* sumMoves skips the zero placeholders, so despite the differing sequence
     counts above the two classes agree on the figure a user actually sees.
     movesText agrees for the same reason. These are the properties that
     make separationInfo_c safe to store in place of the full tree. */
  REQUIRE(info.sumMoves() == orig.sumMoves());
  REQUIRE(info.movesText() == orig.movesText());
}
