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

/* template function to get space separated integer values
 * from a string and enter these into an iterator
 * the count of numbers need to exactly fill the range
 * defined by the 2 iterators
 */
template<typename iter>
void getNumbers(std::string str, iter start, iter end, bool neg_allowed) {

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

  xml.endTag("state");
}

state_c::state_c(xmlParser_c & pars, unsigned int pn)
{
  pars.require(xmlParser_c::START_TAG, "state");

#ifndef NDEBUG
  piecenumber = pn;
#endif

  dx = dy = dz = 0;

  try
  {
    do
    {
      int state = pars.nextTag();

      if (state == xmlParser_c::END_TAG) break;
      if (state != xmlParser_c::START_TAG)
        pars.exception("expected new tag but dounf something else");

      if (pars.getName() == "dx")
      {
        dx = new int[pn];
        pars.next();
        getNumbers(pars.getText(), dx, dx+pn, true);
        pars.next();
        pars.require(xmlParser_c::END_TAG, "dx");
      }
      else if (pars.getName() == "dy")
      {
        dy = new int[pn];
        pars.next();
        getNumbers(pars.getText(), dy, dy+pn, true);
        pars.next();
        pars.require(xmlParser_c::END_TAG, "dy");
      }
      else if (pars.getName() == "dz")
      {
        dz = new int[pn];
        pars.next();
        getNumbers(pars.getText(), dz, dz+pn, true);
        pars.next();
        pars.require(xmlParser_c::END_TAG, "dz");
      }
    } while (true);

    if (!dx || !dy || !dz)
      pars.exception("disassembly state needs dx, dy and dz subnode");
  }

  catch (xmlParserException_c & e)
  {
    if (dx) delete [] dx;
    if (dy) delete [] dy;
    if (dz) delete [] dz;
    pars.exception(e.what());
  }

  pars.require(xmlParser_c::END_TAG, "state");
}

state_c::state_c(const state_c * cpy, unsigned int pn)
#ifndef NDEBUG
: piecenumber(pn)
#endif
{
  dx = new int[pn];
  dy = new int[pn];
  dz = new int[pn];

  memcpy(dx, cpy->dx, pn*sizeof(int));
  memcpy(dy, cpy->dy, pn*sizeof(int));
  memcpy(dz, cpy->dz, pn*sizeof(int));
}

state_c::state_c(unsigned int pn)
#ifndef NDEBUG
: piecenumber(pn)
#endif
{
  dx = new int[pn];
  dy = new int[pn];
  dz = new int[pn];
  bt_assert(dx && dy && dz);
}

state_c::~state_c() {
  delete [] dx;
  delete [] dy;
  delete [] dz;
}

void state_c::set(unsigned int piece, int x, int y, int z) {
  bt_assert(piece < piecenumber);
  dx[piece] = x;
  dy[piece] = y;
  dz[piece] = z;
}

bool state_c::pieceRemoved(unsigned int i) const {
  bt_assert(i < piecenumber);
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
  xml.newAttrib("count", (unsigned long)pieces.size());

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
  removed = left = 0;

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
      states.push_back(new state_c(pars, piecenumber));
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
        left = new separation_c(pars, leftPc);
      }
      else if (str == "removed")
      {
        if (removed)
          pars.exception("more than one removed branch in disassembly");
        removed = new separation_c(pars, removedPc);
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

  numSequences = left?left->numSequences:0 + removed?removed->numSequences:0 + 1;
}

separation_c::separation_c(separation_c * r, separation_c * l, const std::vector<unsigned int> & pcs) : removed(r), left(l) {
  pieces = pcs;

  numSequences = l?l->numSequences:0 + r?r->numSequences:0 + 1;
}

separation_c::~separation_c() {
  delete removed;
  delete left;
  for (unsigned int i = 0; i < states.size(); i++)
    delete states[i];
}

unsigned int separation_c::sumMoves(void) const {
  bt_assert(states.size());
  unsigned int erg = states.size() - 1;
  if (removed)
    erg += removed->sumMoves();
  if (left)
    erg += left->sumMoves();

  return erg;
}

void separation_c::addstate(state_c *st) {
  bt_assert(st->getPiecenumber() == pieces.size());
  states.push_front(st);
}

separation_c::separation_c(const separation_c * cpy) {

  pieces = cpy->pieces;

  for (unsigned int i = 0; i < cpy->states.size(); i++)
    states.push_back(new state_c(cpy->states[i], pieces.size()));

  if (cpy->left)
    left = new separation_c(cpy->left);
  else
    left = 0;

  if (cpy->removed)
    removed = new separation_c(cpy->removed);
  else
    removed = 0;

  numSequences = cpy->numSequences;
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

  int len2 = snprintf(txt, len, "%i", values[idx]-1);

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

