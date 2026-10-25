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
#include "disassembly.h"

#include "voxel.h"

#include "../tools/xml.h"

#include <string.h>
#include <stdlib.h>
#include <numbers>
#include <sstream>
#include <stdexcept>

/* template function to get space separated integer values
 * from a string and enter these into an iterator
 * the count of numbers need to exactly fill the range
 * defined by the 2 iterators
 */
template<typename iter>
void getNumbers(const std::string & str, iter start, iter end, bool neg_allowed) {

  int val = 0;
  bool gotNum = false;
  bool negative = false;

  for (unsigned int pos = 0; pos < str.length(); pos++)
  {
    char c = str[pos];

    if (c == '-' && neg_allowed) {

      // only one - and not number must have been there
      if (negative || gotNum)
        throw xmlParserException_c("too many '-' signs in disassembly number");

      negative = true;

    } else if ((c >= '0') && (c <= '9')) {

      val = val * 10 + c - '0';
      gotNum = true;

    } else if (c == ' ') {

      if (gotNum) {

        if (start == end)
          throw xmlParserException_c("too many numbers in disassembly");

        if (negative) val = -val;
        *start = val;
        start++;
        val = 0;
        gotNum = negative = false;
      }

      if (negative)
        throw xmlParserException_c("only '-' encountered in disassembly");

    } else {

      throw xmlParserException_c("not allowed character in disassembly");

    }
  }

  /* if we have got a last number, enter it
   * this happens, when there is no space at the end of
   * the list of numbers
   */
  if (gotNum) {

    if (start == end)
      throw xmlParserException_c("too many numbers in disassembly");

    if (negative) val = -val;
    *start = val;
    start++;
  }

  // check, if we filled the range
  if (start != end)
    throw xmlParserException_c("too few number in disassembly");
}

/************************************************************************
 * State
 ************************************************************************/

void state_c::save(xmlWriter_c & xml, unsigned int piecenumber) const
{
  xml.newTag("state");

  xml.newTag("dx");
  {
    std::ostream & str = xml.addContent();
    for (unsigned int ii = 0; ii < piecenumber; ii++)
    {
      str << dx[ii];
      if (ii < piecenumber-1)
        str << " ";
    }
  }
  xml.endTag("dx");

  xml.newTag("dy");
  {
    std::ostream & str = xml.addContent();
    for (unsigned int ii = 0; ii < piecenumber; ii++)
    {
      str << dy[ii];
      if (ii < piecenumber-1)
        str << " ";
    }
  }
  xml.endTag("dy");

  xml.newTag("dz");
  {
    std::ostream & str = xml.addContent();
    for (unsigned int ii = 0; ii < piecenumber; ii++)
    {
      str << dz[ii];
      if (ii < piecenumber-1)
        str << " ";
    }
  }
  xml.endTag("dz");

  if (!dt.empty())
  {
    xml.newTag("dt");
    std::ostream & str = xml.addContent();
    for (unsigned int ii = 0; ii < piecenumber; ii++)
    {
      str << (unsigned int)dt[ii];
      if (ii < piecenumber-1)
        str << " ";
    }
    xml.endTag("dt");
  }

  if (rotation)
  {
    static const char * kinds[] = { "pure", "wiggle", "slide", "planned" };
    static const char * axes[] = { "x", "y", "z" };
    xml.newTag("rotation");
    xml.newAttrib("axis", std::string(axes[rotation->axis]));
    xml.newAttrib("pu", rotation->pivotU);
    xml.newAttrib("pv", rotation->pivotV);
    xml.newAttrib("path", kinds[rotation->path.kind]);
    xml.newAttrib("angle", std::string(rotation->path.sense == 0 ? "90" : "-90"));
    {
      /* the file names the pieces that follow the path. Inverted, those are
       * the others, which include piece 0: a reader tells an inverted turn
       * by piece 0 being among them. */
      std::vector<unsigned int> group;
      for (unsigned int k = 0; k < piecenumber; k++)
        if (!pieceRemoved(k) && rotation->turns(k) != rotation->inverse)
          group.push_back(k);
      std::ostringstream pcs;
      for (size_t i = 0; i < group.size(); i++)
        pcs << (i ? " " : "") << group[i];
      xml.newAttrib("pieces", pcs.str());
    }
    if (rotation->path.kind == rotationGeometry::PATH_PLANNED)
    {
      /* every digit: the loaded polyline is the one that was certified */
      std::ostringstream wp;
      wp.precision(17);
      for (size_t i = 0; i < rotation->path.waypoints.size(); i++) {
        const rotationGeometry::pose_t & q = rotation->path.waypoints[i];
        wp << (i ? " " : "") << q.theta << " " << q.du << " " << q.dv;
      }
      xml.newAttrib("waypoints", wp.str());
    }
    else if (rotation->path.kind != rotationGeometry::PATH_PURE)
    {
      xml.newAttrib("du", rotation->path.dirU);
      xml.newAttrib("dv", rotation->path.dirV);
      std::ostringstream amount;
      amount << rotation->path.amount;
      xml.newAttrib("amount", amount.str());
    }
    xml.endTag("rotation");
  }

  xml.endTag("state");
}

namespace {

stateRotation_t loadRotation(xmlParser_c & pars)
{
  stateRotation_t r = {};
  try
  {
    /* x, y or z; anything else is caught as an invalid axis below */
    const std::string axis = pars.getAttributeValue("axis");
    r.axis = (axis == "x") ? 0 : (axis == "y") ? 1 : (axis == "z") ? 2 : 3;
    r.pivotU = std::stoi(pars.getAttributeValue("pu"));
    r.pivotV = std::stoi(pars.getAttributeValue("pv"));
    /* degrees about the axis, right-handed: 90 or -90 (sense 0 or 1) */
    const std::string angle = pars.getAttributeValue("angle");
    r.path.sense = (angle == "90") ? 0 : (angle == "-90") ? 1 : 2;
    {
      std::istringstream pcs(pars.getAttributeValue("pieces"));
      unsigned int p;
      while (pcs >> p)
        r.pieces.push_back(p);
    }
    std::string kind = pars.getAttributeValue("path");
    if (kind == "pure")
      r.path.kind = rotationGeometry::PATH_PURE;
    else if (kind == "wiggle" || kind == "slide")
    {
      r.path.kind = (kind == "wiggle") ? rotationGeometry::PATH_WIGGLE : rotationGeometry::PATH_SLIDE;
      r.path.dirU = std::stoi(pars.getAttributeValue("du"));
      r.path.dirV = std::stoi(pars.getAttributeValue("dv"));
      r.path.amount = std::stod(pars.getAttributeValue("amount"));
    }
    else if (kind == "planned")
    {
      r.path.kind = rotationGeometry::PATH_PLANNED;
      std::istringstream wp(pars.getAttributeValue("waypoints"));
      std::vector<double> v;
      double x;
      while (wp >> x)
        v.push_back(x);
      if (!wp.eof() || v.size() % 3)
        pars.exception("rotation waypoints must be numbers, three per pose");
      for (size_t i = 0; i < v.size(); i += 3)
        r.path.waypoints.push_back({ v[i], v[i + 1], v[i + 2] });
    }
    else
      pars.exception("unknown rotation path kind");
  }
  catch (const std::logic_error &)
  {
    /* stoi and friends on a missing or malformed attribute */
    pars.exception("rotation needs numeric pu, pv (and du, dv, amount for shifted paths)");
  }

  if (r.axis > 2 || r.path.sense > 1 ||
      std::abs(r.path.dirU) > 1 || std::abs(r.path.dirV) > 1 ||
      ((r.path.kind == rotationGeometry::PATH_WIGGLE || r.path.kind == rotationGeometry::PATH_SLIDE) &&
       ((r.path.dirU == 0 && r.path.dirV == 0) || !(r.path.amount > 0))))
    pars.exception("rotation with an invalid axis, angle or path");

  if (r.path.kind == rotationGeometry::PATH_PLANNED) {
    /* makePath's requirements: from the identity to the quarter turn */
    const double end = (r.path.sense == 0 ? 0.5 : -0.5) * std::numbers::pi;
    if (r.path.waypoints.size() < 2 || r.path.waypoints.front().theta != 0 ||
        r.path.waypoints.front().du != 0 || r.path.waypoints.front().dv != 0 ||
        r.path.waypoints.back().theta != end || r.path.waypoints.back().du != 0 || r.path.waypoints.back().dv != 0)
      pars.exception("a planned rotation path runs from the identity to the quarter turn");
  }

  return r;
}

}

state_c::state_c(xmlParser_c & pars, unsigned int pn)
{
  pars.require(xmlParser_c::START_TAG, "state");

  bool has_dx = false, has_dy = false, has_dz = false;

  do
  {
    int state = pars.nextTag();

    if (state == xmlParser_c::END_TAG) break;
    if (state != xmlParser_c::START_TAG)
      pars.exception("expected new tag but found something else");

    if (pars.getName() == "dx")
    {
      dx.resize(pn);
      pars.next();
      getNumbers(pars.getText(), dx.begin(), dx.end(), true);
      pars.next();
      pars.require(xmlParser_c::END_TAG, "dx");
      has_dx = true;
    }
    else if (pars.getName() == "dy")
    {
      dy.resize(pn);
      pars.next();
      getNumbers(pars.getText(), dy.begin(), dy.end(), true);
      pars.next();
      pars.require(xmlParser_c::END_TAG, "dy");
      has_dy = true;
    }
    else if (pars.getName() == "dz")
    {
      dz.resize(pn);
      pars.next();
      getNumbers(pars.getText(), dz.begin(), dz.end(), true);
      pars.next();
      pars.require(xmlParser_c::END_TAG, "dz");
      has_dz = true;
    }
    else if (pars.getName() == "dt")
    {
      std::vector<unsigned int> t(pn);
      pars.next();
      getNumbers(pars.getText(), t.begin(), t.end(), false);
      pars.next();
      pars.require(xmlParser_c::END_TAG, "dt");
      /* the grid's range is checked with the solution (solution_c) */
      for (unsigned int v : t)
        if (v > 255)
          pars.exception("piece orientation out of range");
      dt.assign(t.begin(), t.end());
    }
    else if (pars.getName() == "rotation")
    {
      rotation = loadRotation(pars);
      if (rotation->pieces.empty())
        pars.exception("rotation without turning pieces");
      for (unsigned int p : rotation->pieces)
        if (p >= pn)
          pars.exception("rotation turns a piece that does not exist");
      pars.skipSubTree();
    }
    else
      pars.skipSubTree();
  } while (true);

  if (!has_dx || !has_dy || !has_dz)
    pars.exception("disassembly state needs dx, dy and dz subnode");

  /* the listed pieces follow the path; piece 0 stays in place in every
   * state, so when it is among them the others turned instead */
  if (rotation && std::find(rotation->pieces.begin(), rotation->pieces.end(), 0u) != rotation->pieces.end()) {
    std::vector<unsigned int> others;
    for (unsigned int k = 0; k < pn; k++)
      if (!pieceRemoved(k) && std::find(rotation->pieces.begin(), rotation->pieces.end(), k) == rotation->pieces.end())
        others.push_back(k);
    if (others.empty())
      pars.exception("rotation turns every piece");
    rotation->pieces = others;
    rotation->inverse = true;
  }

  pars.require(xmlParser_c::END_TAG, "state");
}

state_c::state_c(const state_c * cpy, unsigned int pn)
  : dx(cpy->dx), dy(cpy->dy), dz(cpy->dz), dt(cpy->dt), rotation(cpy->rotation)
{
  (void)pn;
  bt_assert(dx.size() == pn && dy.size() == pn && dz.size() == pn);
}

state_c::state_c(unsigned int pn)
  : dx(pn, 0), dy(pn, 0), dz(pn, 0)
{
}

state_c::~state_c() = default;

void state_c::set(unsigned int piece, int x, int y, int z) {
  bt_assert(piece < dx.size());
  dx[piece] = x;
  dy[piece] = y;
  dz[piece] = z;
}

void state_c::setTrans(unsigned int piece, unsigned int trans) {
  bt_assert(piece < dx.size() && trans < 256);
  if (dt.empty())
    dt.resize(dx.size(), 0);
  dt[piece] = (unsigned char)trans;
}

bool state_c::pieceRemoved(unsigned int i) const {
  bt_assert(i < dx.size());
  return (abs(dx[i]) > 10000) || (abs(dy[i]) > 10000) || (abs(dz[i]) > 10000);
}

/************************************************************************
 * Disassembly
 ************************************************************************/

int disassembly_c::compare(const disassembly_c * s2) const
{
  unsigned int numSeq = std::max(getNumSequences(), s2->getNumSequences());

  for (unsigned int i = 0; i < numSeq; i++) {
    if (getSequenceLength(i) < s2->getSequenceLength(i)) return -1;
    if (getSequenceLength(i) > s2->getSequenceLength(i)) return 1;
  }

  return 0;
}


/************************************************************************
 * Separation
 ************************************************************************/

void separation_c::save(xmlWriter_c & xml, int type) const
{
  xml.newTag("separation");

  switch (type)
  {
    case 0: break;
    case 1: xml.newAttrib("type", "left"); break;
    case 2: xml.newAttrib("type", "removed"); break;
  }

  // first save the pieces array
  xml.newTag("pieces");
  xml.newAttrib("count", (int)pieces.size());

  for (unsigned int ii = 0; ii < pieces.size(); ii++) {
    xml.addContent(pieces[ii]);
    if (ii < pieces.size()-1)
      xml.addContent(" ");
  }

  xml.endTag("pieces");

  // now add all the states
  for (unsigned int jj = 0; jj < states.size(); jj++)
    states[jj]->save(xml, pieces.size());

  // finally save the removed and left over part
  // we add an attribute to the node of the subseparations to later distinguish
  // between the removed and the left over separation
  if (removed) removed->save(xml, 2);
  if (left)    left->save(xml, 1);

  xml.endTag("separation");
}

separation_c::separation_c(xmlParser_c & pars, unsigned int pieceCnt)
{
  pars.require(xmlParser_c::START_TAG, "separation");

  unsigned int piecenumber = 0;
  std::string str;
  unsigned int removedPc = 0, leftPc = 0;

  do
  {
    int state = pars.nextTag();

    if (state == xmlParser_c::END_TAG) break;
    pars.require(xmlParser_c::START_TAG, "");

    if (pars.getName() == "pieces")
    {
      // load the pieces array
      str = pars.getAttributeValue("count");
      if (!str.length())
        pars.exception("pieces node needs a 'count' attribute");

      piecenumber = atoi(str.c_str());

      if (piecenumber != pieceCnt)
        pars.exception("the number of pieces in the count array is not as expected");

      pieces.resize(piecenumber);

      pars.next();
      pars.require(xmlParser_c::TEXT, "");
      getNumbers(pars.getText(), pieces.begin(), pieces.begin()+piecenumber, false);
      pars.next();
      pars.require(xmlParser_c::END_TAG, "pieces");
    }
    else if (pars.getName() == "state")
    {
      if (piecenumber == 0)
        pars.exception("there must be a pieces array before the states");

      if (removed || left)
        pars.exception("there are states behind the sub separations");

      // get the states
      states.push_back(std::make_unique<state_c>(pars, piecenumber));
    }
    else if (pars.getName() == "separation")
    {
      if (removedPc == 0)
      {
        for (unsigned int i = 0; i < pieceCnt; i++)
          if (states[states.size()-1]->pieceRemoved(i))
            removedPc++;
          else
            leftPc++;

        if ((removedPc == 0) || (leftPc == 0))
          pars.exception("there need to be pieces in both parts of the tree");
      }

      // get the left and removed subseparation

      str = pars.getAttributeValue("type");
      if (!str.length())
        pars.exception("sub-sepatations need to have a type field");

      if (str == "left")
      {
        if (left)
          pars.exception("more than one left branch in disassembly");
        left = std::make_unique<separation_c>(pars, leftPc);
      }
      else if (str == "removed")
      {
        if (removed)
          pars.exception("more than one removed branch in disassembly");
        removed = std::make_unique<separation_c>(pars, removedPc);
      }
      else
        pars.exception("subnodes must have either left or removed type");
    }
    else
      pars.skipSubTree();

  } while(true);

  if (states.size() == 0)
    pars.exception("there are no state nodes in the separation");

  pars.require(xmlParser_c::END_TAG, "separation");

  numSequences = (left?left->numSequences:0) + (removed?removed->numSequences:0) + 1;
}

separation_c::separation_c(separation_c * r, separation_c * l, const std::vector<unsigned int> & pcs)
  : pieces(pcs), removed(r), left(l) {
  numSequences = (l?l->numSequences:0) + (r?r->numSequences:0) + 1;
}

separation_c::separation_c(std::unique_ptr<separation_c> r, std::unique_ptr<separation_c> l, const std::vector<unsigned int> & pcs)
  : pieces(pcs), removed(std::move(r)), left(std::move(l)) {
  numSequences = (left?left->numSequences:0) + (removed?removed->numSequences:0) + 1;
}

separation_c::~separation_c() = default;

unsigned int separation_c::sumMoves(void) const {
  bt_assert(states.size());
  unsigned int erg = states.size() - 1;
  if (removed)
    erg += removed->sumMoves();
  if (left)
    erg += left->sumMoves();

  return erg;
}

void separation_c::clearTrans(void) {
  bt_assert(sumRotations() == 0);
  for (auto & st : states)
    st->clearTrans();
  if (removed)
    removed->clearTrans();
  if (left)
    left->clearTrans();
}

void separation_c::shiftPiece(unsigned int name, int dx, int dy, int dz) {
  for (unsigned int i = 0; i < pieces.size(); i++)
    if (pieces[i] == name)
      for (auto & st : states)
        st->set(i, st->getX(i) + dx, st->getY(i) + dy, st->getZ(i) + dz);
  if (removed)
    removed->shiftPiece(name, dx, dy, dz);
  if (left)
    left->shiftPiece(name, dx, dy, dz);
}

unsigned int separation_c::sumRotations(void) const {
  unsigned int erg = 0;
  for (const auto & s : states)
    if (s->getRotation())
      erg++;
  if (removed)
    erg += removed->sumRotations();
  if (left)
    erg += left->sumRotations();

  return erg;
}

void separation_c::addstate(std::unique_ptr<state_c> st) {
  bt_assert(st->getPiecenumber() == pieces.size());
  states.push_front(std::move(st));
}

void separation_c::addstate(state_c *st) {
  addstate(std::unique_ptr<state_c>(st));
}

separation_c::separation_c(const separation_c * cpy)
  : pieces(cpy->pieces), numSequences(cpy->numSequences) {

  for (unsigned int i = 0; i < cpy->states.size(); i++)
    states.push_back(std::make_unique<state_c>(cpy->states[i].get(), pieces.size()));

  if (cpy->left)
    left = std::make_unique<separation_c>(cpy->left.get());

  if (cpy->removed)
    removed = std::make_unique<separation_c>(cpy->removed.get());
}


bool separation_c::containsMultiMoves(void) {
  return (states.size() > 2) ||
    (left && left->containsMultiMoves()) ||
    (removed && removed->containsMultiMoves());
}

int separation_c::movesText2(char * txt, int len) const {

  bt_assert(states.size() > 0);

  int len2 = snprintf(txt, len, "%zu", states.size()-1);

  if (len2+5 > len)
    return len2;

  if (left && left->containsMultiMoves()) {
    snprintf(txt+len2, len-len2, ".");
    len2++;
    len2 += left->movesText2(txt+len2, len-len2);
  }

  if (len2+5 > len)
    return len2;

  if (removed && removed->containsMultiMoves()) {
    snprintf(txt+len2, len-len2, ".");
    len2++;
    len2 += removed->movesText2(txt+len2, len-len2);
  }

  return len2;
}

std::string separation_c::movesText(void) const {
  char buf[256];
  movesText2(buf, sizeof(buf));
  return std::string(buf);
}

void separation_c::exchangeShape(unsigned int s1, unsigned int s2) {

  for (unsigned int i = 0; i < pieces.size(); i++)
    if (pieces[i] == s1) pieces[i] = s2;
    else if (pieces[i] == s2) pieces[i] = s1;

  if (removed)
    removed->exchangeShape(s1, s2);

  if (left)
    left->exchangeShape(s1, s2);
}

unsigned int separation_c::getSequenceLength(unsigned int x) const
{
  if (x == 0)
    return states.size();

  x--;

  if (left)
  {
    if (x < left->numSequences)
      return left->getSequenceLength(x);

    x -= left->numSequences;
  }

  if (removed)
  {
    if (x < removed->numSequences)
      return removed->getSequenceLength(x);
  }

  return 0;
}

unsigned int separation_c::getNumSequences(void) const
{
  return numSequences;
}

void separation_c::removePieces(unsigned int from, unsigned int cnt)
{
  /* for the moment we assume, that none of the removed pieces is actually used
   * in this disassembly, because otherwise the whole solution should have been
   * deleted, so all that is left is to decrease the piece counters
   */

  for (unsigned int i = 0; i < pieces.size(); i++)
  {
    bt_assert(pieces[i] < from || pieces[i] >= from+cnt);

    if (pieces[i] >= from+cnt)
      pieces[i] -= cnt;
  }

  if (left) left->removePieces(from, cnt);
  if (removed) removed->removePieces(from, cnt);

}

void separation_c::addNonPlacedPieces(unsigned int from, unsigned int cnt)
{
  /* increase piece numbers accordingly */
  for (unsigned int i = 0; i < pieces.size(); i++)
    if (pieces[i] >= from)
      pieces[i] += cnt;

  if (left) left->addNonPlacedPieces(from, cnt);
  if (removed) removed->addNonPlacedPieces(from, cnt);
}



/************************************************************************
 * SeparationInfo
 ************************************************************************/

separationInfo_c::separationInfo_c(xmlParser_c & pars)
{
  pars.require(xmlParser_c::START_TAG, "separationInfo");

  pars.next();
  pars.require(xmlParser_c::TEXT, "");

  std::string str = pars.getText();

  unsigned int pos = 0;
  unsigned int num = 0;

  while (pos < str.length()) {

    if (str[pos] == ' ') {
      values.push_back(num);
      num = 0;
    }

    if (str[pos] >= '0' && (str[pos] <= '9'))
      num = 10*num + str[pos] - '0';

    pos++;
  }

  values.push_back(num);

  pars.next();
  pars.require(xmlParser_c::END_TAG, "separationInfo");

  // check consistency of the tree (does it end?)

  /* the idea here with the branches counter is used again and again blow but I will
   * explain only here.
   * branches counts the number of _open_ ends that still need to be looked at, so
   * when we encounter a node with a number in it, it _must_ have 2 children and
   * so the number of open branches increases by one. If we come to an and the
   * number of open ends decreases because this branch just ended
   */
  unsigned int branches = 1;
  pos = 0;

  while (branches) {
    if (pos >= values.size())
      pars.exception("the tree in the disassemblyInformation tag is incomplete");

    if (values[pos])
      branches++;
    else
      branches--;

    pos++;
  }
}

/* this is a simple recursive function to get the separation tree into pre-order */
void separationInfo_c::recursiveConstruction(const separation_c * sep) {
  values.push_back(sep->getMoves()+1);

  if (sep->getLeft())
    recursiveConstruction(sep->getLeft());
  else
    values.push_back(0);

  if (sep->getRemoved())
    recursiveConstruction(sep->getRemoved());
  else
    values.push_back(0);
}

separationInfo_c::separationInfo_c(const separation_c * sep) {

  recursiveConstruction(sep);
}

void separationInfo_c::save(xmlWriter_c & xml) const
{
  xml.newTag("separationInfo");

  for (unsigned int i = 0; i < values.size(); i++)
  {
    xml.addContent(values[i]);
    if (i < values.size()-1)
      xml.addContent(" ");
  }

  xml.endTag("separationInfo");
}

unsigned int separationInfo_c::sumMoves(void) const {

  unsigned int erg = 0;

  for (unsigned int i = 0; i < values.size(); i++)
    if (values[i])
      erg += (values[i] - 1);

  return erg;
}

int separationInfo_c::movesText2(char * txt, int len, unsigned int idx) const {

  int len2 = snprintf(txt, len, "%u", values[idx]-1);

  if (len2+5 > len)
    return len2;

  idx++;

  if (values[idx] && containsMultiMoves(idx)) {
    snprintf(txt+len2, len-len2, ".");
    len2++;
    len2 += movesText2(txt+len2, len-len2, idx);
  }

  if (len2+5 > len)
    return len2;

  /* skip the left tree the idea is described in the xml node constructor above */
  unsigned int branches = 1;

  while (branches) {
    if (values[idx])
      branches++;
    else
      branches--;
    idx++;
  }

  if (values[idx] && containsMultiMoves(idx)) {
    snprintf(txt+len2, len-len2, ".");
    len2++;
    len2 += movesText2(txt+len2, len-len2, idx);
  }

  return len2;
}

std::string separationInfo_c::movesText(void) const {
  char buf[256];
  movesText2(buf, sizeof(buf), 0);
  return std::string(buf);
}

bool separationInfo_c::containsMultiMoves(unsigned int idx) const {

  unsigned int branches = 1;

  while (branches) {
    if (values[idx] > 2) return true;

    /* see xml::node constructor above for the idea behind this */
    if (values[idx])
      branches++;
    else
      branches--;

    idx++;
  }

  return false;
}

unsigned int separationInfo_c::getSequenceLength(unsigned int x) const
{
  if (x < values.size())
    return values[x];
  else
    return 0;
}

unsigned int separationInfo_c::getNumSequences(void) const
{
  return values.size();
}

