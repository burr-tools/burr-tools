/*
 * Copyright (C) 2003-2009  Andreas Röver
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
#include "problem.h"

#include "bt_assert.h"
#include "voxel.h"
#include "assembly.h"
#include "disassembly.h"
#include "puzzle.h"
#include "solution.h"

#include "../tools/xml.h"

#include <algorithm>

#include <stdio.h>

/** internal class of problem storing the grouping information of a shape */
class group_c {

  public:

    group_c(unsigned short gr, unsigned short cnt) : group(gr), count(cnt) {}

    unsigned short group;
    unsigned short count;
};

/** internal class of problem storing the information attatched to a piece in a problem */
class part_c {

  public:

    part_c(unsigned short id, unsigned short mn, unsigned short mx, unsigned short grp) : shapeId(id), min(mn), max(mx) {
      if (grp)
        groups.push_back(group_c(grp, max));
    }

    part_c(unsigned short id, unsigned short mn, unsigned short mx) : shapeId(id), min(mn), max(mx) { }

    part_c(part_c * orig) : shapeId(orig->shapeId), min(orig->min), max(orig->max), groups(orig->groups) { }

    void addGroup(unsigned short grp, unsigned short cnt) {
      groups.push_back(group_c(grp, cnt));
    }

    unsigned short shapeId;
    unsigned short min;
    unsigned short max;

    std::vector<group_c> groups;
};

problem_c::problem_c(puzzle_c & puz) :
  puzzle(puz), result(0xFFFFFFFF),
  assm(0),solveState(SS_UNSOLVED), numAssemblies(0),
  numSolutions(0), usedTime(0), maxHoles(0xFFFFFFFF)
{}

problem_c::~problem_c(void) {
  for (unsigned int i = 0; i < solutions.size(); i++)
    delete solutions[i];

  for (unsigned int i = 0; i < parts.size(); i++)
    delete parts[i];

  if (assm)
    delete assm;
}

problem_c::problem_c(const problem_c * orig, puzzle_c & puz) :
  puzzle(puz), result(orig->result),
  solveState(SS_UNSOLVED), numAssemblies(0), numSolutions(0), usedTime(0)
{
  assm = 0;

  for (std::set<uint32_t>::iterator i = orig->colorConstraints.begin(); i != orig->colorConstraints.end(); i++)
    colorConstraints.insert(*i);

  for (unsigned int i = 0; i < orig->parts.size(); i++)
    parts.push_back(new part_c(orig->parts[i]));

  maxHoles = orig->maxHoles;

  // solutions are NOT copied including the fields assm, solve state is set to unsolved
  // number of solutions and time to find them are unknown...

  // the name is also left intentionally empty because the user will
  // likely give a new name any way
}

void problem_c::save(xmlWriter_c & xml) const
{
  xml.newTag("problem");

  if (name.length() > 0)
    xml.newAttrib("name", name);

  xml.newAttrib("state", solveState);

  if (solveState != SS_UNSOLVED)
  {
    xml.newAttrib("assemblies", numAssemblies);
    xml.newAttrib("solutions", numSolutions);
    xml.newAttrib("time", usedTime);
  }

  if (maxHoles != 0xFFFFFFFF)
    xml.newAttrib("maxHoles", maxHoles);

  xml.newTag("shapes"); // for historical reasons, parts are saved in an XML-tag called shapes

  for (unsigned int i = 0; i < parts.size(); i++) {
    xml.newTag("shape");

    xml.newAttrib("id", parts[i]->shapeId);

    if (parts[i]->min == parts[i]->max)
    {
      xml.newAttrib("count", parts[i]->min);
    }
    else
    {
      xml.newAttrib("min", parts[i]->min);
      xml.newAttrib("max", parts[i]->max);
    }

    if (parts[i]->groups.size() == 0)
    {
      // do nothing, we don't need to save anything in this case
    }
    else if ((parts[i]->groups.size() == 1) &&
             (parts[i]->groups[0].count == parts[i]->max))
    {
      // this is the case when all pieces are in the same group
      // we only need to save the group, if it is not 0,
      // the loader takes 0 as default anyway
      if (parts[i]->groups[0].group != 0)
        xml.newAttrib("group", parts[i]->groups[0].group);
    }
    else
    {
      for (unsigned int j = 0; j < parts[i]->groups.size(); j++)
        if (parts[i]->groups[j].group != 0)
        {
          xml.newTag("group");
          xml.newAttrib("group", parts[i]->groups[j].group);
          xml.newAttrib("count", parts[i]->groups[j].count);
          xml.endTag("group");
        }
    }
    xml.endTag("shape");
  }

  xml.endTag("shapes");

  xml.newTag("result");
  xml.newAttrib("id", result);
  xml.endTag("result");

  xml.newTag("bitmap");
  for (std::set<uint32_t>::iterator i = colorConstraints.begin(); i != colorConstraints.end(); i++)
  {
    xml.newTag("pair");
    xml.newAttrib("piece", *i >> 16);
    xml.newAttrib("result", *i & 0xFFFF);
    xml.endTag("pair");
  }
  xml.endTag("bitmap");

  if (solveState == SS_SOLVING)
  {
    if (assm)
    {
      assm->save(xml);
    }
    else if (assemblerState != "" && assemblerVersion != "")
    {
      xml.newTag("assembler");
      xml.newAttrib("version", assemblerVersion);
      xml.addContent(assemblerState);
      xml.endTag("assembler");
    }
  }

  if (solutions.size()) {
    xml.newTag("solutions");
    for (unsigned int i = 0; i < solutions.size(); i++)
      solutions[i]->save(xml);
    xml.endTag("solutions");
  }

  xml.endTag("problem");
}

problem_c::problem_c(puzzle_c & puz, xmlParser_c & pars) : puzzle(puz), result(0xFFFFFFFF), assm(0)
{
  pars.require(xmlParser_c::START_TAG, "problem");

  name = pars.getAttributeValue("name");
  solveState = SS_UNSOLVED;
  numAssemblies = numSolutions = usedTime = 0;
  maxHoles = 0xFFFFFFFF;

  std::string str = pars.getAttributeValue("maxHoles");
  if (str.length())
    maxHoles = atoi(str.c_str());

  str = pars.getAttributeValue("state");
  if (str.length())
    solveState = (solveState_e)atoi(str.c_str());

  if (solveState != SS_UNSOLVED)
  {
    str = pars.getAttributeValue("assemblies");
    if (str.length())
      numAssemblies = atoi(str.c_str());

    str = pars.getAttributeValue("solutions");
    if (str.length())
      numSolutions = atoi(str.c_str());

    str = pars.getAttributeValue("time");
    if (str.length())
      usedTime = atoi(str.c_str());
  }

  unsigned int pieces = 0;

  do
  {
    int state = pars.nextTag();

    if (state == xmlParser_c::END_TAG) break;
    pars.require(xmlParser_c::START_TAG, "");

    if (pars.getName() == "shapes")  // parts are saved in this tag for historical reasons
    {
      do
      {
        int state = pars.nextTag();

        if (state == xmlParser_c::END_TAG) break;
        pars.require(xmlParser_c::START_TAG, "");

        if (pars.getName() == "shape")
        {
          unsigned short id, min, max, grp;

          str = pars.getAttributeValue("id");
          if (str.length() == 0)
            pars.exception("a shape node must have an 'idt' attribute");

          id = atoi(str.c_str());

          str = pars.getAttributeValue("count");

          if (str.length())
            min = max = atoi(str.c_str());
          else if (pars.getAttributeValue("min").length() &&
                   pars.getAttributeValue("max").length()) {
            min = atoi(pars.getAttributeValue("min").c_str());
            max = atoi(pars.getAttributeValue("max").c_str());
            if (min > max)
              pars.exception("min of shape count must by <= max");
          } else
            min = max = 1;

          str = pars.getAttributeValue("group");
          grp = atoi(str.c_str());

          pieces += max;

          if (id >= puzzle.shapeNumber())
            pars.exception("the shape ids must be for valid shapes");

          if (grp)
          {
            parts.push_back(new part_c(id, min, max, grp));
            pars.skipSubTree();
          }
          else
          {
            /* OK we have 2 ways to specify groups for pieces, either
             * a group attribute in the tag. Then all pieces are
             * in the given group, or you specify a list of group
             * tags inside the tag. Each of the group tag gives a
             * group and a count
             */
            parts.push_back(new part_c(id, min, max));

            do
            {
              int state = pars.nextTag();

              if (state == xmlParser_c::END_TAG) break;
              pars.require(xmlParser_c::START_TAG, "");

              if (pars.getName() == "group")
              {
                str = pars.getAttributeValue("group");
                if (!str.length())
                  pars.exception("a group node must have a valid group attribute");
                grp = atoi(str.c_str());

                str = pars.getAttributeValue("count");
                if (!str.length())
                  pars.exception("a group node must have a valid count attribute");
                unsigned int cnt = atoi(str.c_str());

                (*parts.rbegin())->addGroup(grp, cnt);
              }

            } while (true);
          }
        }
        else
          pars.skipSubTree();

	pars.require(xmlParser_c::END_TAG, "shape");

      } while (true);

      pars.require(xmlParser_c::END_TAG, "shapes");
    }
    else if (pars.getName() == "result")
    {
      str = pars.getAttributeValue("id");
      if (!str.length())
        pars.exception("the result node must have an 'id' attribute with content");
      result = atoi(str.c_str());
      pars.skipSubTree();
    }
    else if (pars.getName() == "solutions")
    {
      do
      {
        int state = pars.nextTag();

        if (state == xmlParser_c::END_TAG) break;
        pars.require(xmlParser_c::START_TAG, "");

        if (pars.getName() == "solution")
          solutions.push_back(new solution_c(pars, pieces, puzzle.getGridType()));
        else
          pars.skipSubTree();

        pars.require(xmlParser_c::END_TAG, "solution");

      } while (true);

      pars.require(xmlParser_c::END_TAG, "solutions");
    }
    else if (pars.getName() == "bitmap")
    {
      do
      {
        int state = pars.nextTag();

        if (state == xmlParser_c::END_TAG) break;
        pars.require(xmlParser_c::START_TAG, "");

        if (pars.getName() == "pair")
        {
          str = pars.getAttributeValue("piece");
          if (str.length() == 0)
            pars.exception("a pair node must have a piece attribute");
          unsigned int piece = atoi(str.c_str());

          str = pars.getAttributeValue("result");
          if (str.length() == 0)
            pars.exception("a pair node must have a result attribute");
          unsigned int result = atoi(str.c_str());

          colorConstraints.insert(piece << 16 | result);
        }

        pars.skipSubTree();

      } while (true);

      pars.require(xmlParser_c::END_TAG, "bitmap");
    }
    else if (pars.getName() == "assembler")
    {
      str = pars.getAttributeValue("version");
      if (str.length())
      {
        assemblerVersion = str;
        pars.next();
        assemblerState = pars.getText();
        pars.next();
        pars.require(xmlParser_c::END_TAG, "assembler");
      }
      else
        pars.skipSubTree();

      pars.require(xmlParser_c::END_TAG, "assembler");
    }
    else
    {
      pars.skipSubTree();
    }

  } while (true);

  // some post processing after the loading of values

  // if, for whatever reasons the shape is was not right, we reset it to an empty shape
  if (result >= puzzle.shapeNumber())
    result = 0xFFFFFFFF;

  pars.require(xmlParser_c::END_TAG, "problem");
}

/************** PROBLEM ****************/

void problem_c::shapeRemoved(unsigned short idx) {

  if (result == idx)
    result = 0xFFFFFFFF;

  unsigned int i = 0;
  while (i < parts.size()) {
    if (parts[i]->shapeId == idx) {
      delete parts[i];
      parts.erase(parts.begin()+i);
    } else {
      i++;
    }
  }

  /* now check all shapes, and the result, if their id is larger
   * than the deleted shape, if so decrement to update the number
   */
  for (unsigned int i = 0; i < parts.size(); i++)
    if (parts[i]->shapeId > idx) parts[i]->shapeId--;

  if (result > idx)
    result--;
}

void problem_c::exchangeShapeId(unsigned int s1, unsigned int s2) {

  if (result == s1) result = s2;
  else if (result == s2) result = s1;

  for (unsigned int i = 0; i < parts.size(); i++)
    if (parts[i]->shapeId == s1) parts[i]->shapeId = s2;
    else if (parts[i]->shapeId == s2) parts[i]->shapeId = s1;
}

void problem_c::allowPlacement(unsigned int pc, unsigned int res) {
  bt_assert(pc <= puzzle.colorNumber());
  bt_assert(res <= puzzle.colorNumber());

  if ((pc == 0) || (res == 0))
    return;

  colorConstraints.insert((pc-1) << 16 | (res-1));
}

void problem_c::disallowPlacement(unsigned int pc, unsigned int res) {
  bt_assert(pc <= puzzle.colorNumber());
  bt_assert(res <= puzzle.colorNumber());

  if ((pc == 0) || (res == 0))
    return;

  std::set<uint32_t>::iterator i = colorConstraints.find((pc-1) << 16 | (res-1));
  if (i != colorConstraints.end())
    colorConstraints.erase(i);
}

bool problem_c::placementAllowed(unsigned int pc, unsigned int res) const {
  bt_assert(pc <= puzzle.colorNumber());
  bt_assert(res <= puzzle.colorNumber());

  if (puzzle.colorNumber() == 0)
    return true;

  return (pc == 0) || (res == 0) || (colorConstraints.find((pc-1) << 16 | (res-1)) != colorConstraints.end());
}


void problem_c::exchangeShape(unsigned int s1, unsigned int s2) {
  bt_assert(s1 < parts.size());
  bt_assert(s2 < parts.size());

  if (s1 == s2) return;

  if (s1 > s2) {
    unsigned int s = s1;
    s1 = s2;
    s2 = s;
  }

  unsigned int p1Start;
  unsigned int p2Start;
  unsigned int p1Count;
  unsigned int p2Count;

  p1Start = p2Start = 0;

  for (unsigned int i = 0; i < s1; i++)
    p1Start += parts[i]->max;

  for (unsigned int i = 0; i < s2; i++)
    p2Start += parts[i]->max;

  p1Count = parts[s1]->max;
  p2Count = parts[s2]->max;

  bt_assert(p1Start+p1Count == p2Start);

  part_c * s = parts[s1];
  parts[s1] = parts[s2];
  parts[s2] = s;

  /* this vector holds the target position of all the involved piece
   * as long as its not in the order 0, 1, 2, ... some pieces must be exchanged
   */
  std::vector<unsigned int>pos;

  pos.resize(p1Count+p2Count);

  for (unsigned int i = 0; i < p1Count; i++)
    pos[i] = p2Count+i;
  for (unsigned int i = 0; i < p2Count; i++)
    pos[p1Count+i] = i;

  for (unsigned int i = 0; i < p1Count+p2Count-1; i++)
    if (pos[i] != i) {

      // search for i
      unsigned int j = i+1;
      while ((j < p1Count+p2Count) && (pos[j] != i)) j++;

      bt_assert(j < p1Count+p2Count);

      for (unsigned int s = 0; s < solutions.size(); s++)
        solutions[s]->exchangeShape(p1Start+i, p1Start+j);

      pos[j] = pos[i];
      // normally we would also need pos[i] = i; but as we don't touch that field any more let's save that operation
    }
}

void problem_c::setResultId(unsigned int shape)
{
  bt_assert(shape < puzzle.shapeNumber());

  if (shape != result)
  {
    removeAllSolutions();
    result = shape;
  }
}

unsigned int problem_c::getResultId(void) const {
  bt_assert(result < puzzle.shapeNumber());
  return result;
}
bool problem_c::resultValid(void) const {
  return result < puzzle.shapeNumber();
}

/* get the result shape voxel space */
const voxel_c * problem_c::getResultShape(void) const {
  bt_assert(result < puzzle.shapeNumber());
  return puzzle.getShape(result);
}

voxel_c * problem_c::getResultShape(void) {
  bt_assert(result < puzzle.shapeNumber());
  return puzzle.getShape(result);
}

void problem_c::setShapeCountMin(unsigned int shape, unsigned int count) {
  bt_assert(shape < puzzle.shapeNumber());

  for (unsigned int id = 0; id < parts.size(); id++) {
    if (parts[id]->shapeId == shape) {

      parts[id]->min = count;
      if (parts[id]->min > parts[id]->max)
        parts[id]->max = count;

      return;
    }
  }

  // when we get here there is no piece with the required puzzle shape, so add it
  if (count)
    parts.push_back(new part_c(shape, count, count, 0));
}

void problem_c::setShapeCountMax(unsigned int shape, unsigned int count) {
  bt_assert(shape < puzzle.shapeNumber());

  for (unsigned int id = 0; id < parts.size(); id++) {
    if (parts[id]->shapeId == shape) {

      if (count == 0) {
        parts.erase(parts.begin()+id);
        return;
      }

      parts[id]->max = count;
      if (parts[id]->max < parts[id]->min)
        parts[id]->min = count;

      return;
    }
  }

  if (count)
    parts.push_back(new part_c(shape, 0, count, 0));
}

unsigned int problem_c::getShapeCountMin(unsigned int shape) const {
  bt_assert(shape < puzzle.shapeNumber());

  for (unsigned int id = 0; id < parts.size(); id++)
    if (parts[id]->shapeId == shape)
      return parts[id]->min;

  return 0;
}

unsigned int problem_c::getShapeCountMax(unsigned int shape) const {
  bt_assert(shape < puzzle.shapeNumber());

  for (unsigned int id = 0; id < parts.size(); id++)
    if (parts[id]->shapeId == shape)
      return parts[id]->max;

  return 0;
}

/* return the number of pieces in the problem (sum of all counts of all shapes */
unsigned int problem_c::pieceNumber(void) const {
  unsigned int result = 0;

  for (unsigned int i = 0; i < parts.size(); i++)
    result += parts[i]->max;

  return result;
}

/* return the shape id of the given shape (index into the shape array of the puzzle */
unsigned int problem_c::getShape(unsigned int shapeID) const {
  bt_assert(shapeID < parts.size());

  return parts[shapeID]->shapeId;
}

unsigned int problem_c::getShapeId(unsigned int shape) const {
  bt_assert(shape < puzzle.shapeNumber());

  for (unsigned int i = 0; i < parts.size(); i++)
    if (parts[i]->shapeId == shape)
      return i;

  bt_assert(0);
  return 0;
}

bool problem_c::containsShape(unsigned int shape) const {
  if (result == shape)
    return true;

  for (unsigned int i = 0; i < parts.size(); i++)
    if (parts[i]->shapeId == shape)
      return true;

  return false;
}

const voxel_c * problem_c::getShapeShape(unsigned int shapeID) const {
  bt_assert(shapeID < parts.size());
  return puzzle.getShape(parts[shapeID]->shapeId);
}

voxel_c * problem_c::getShapeShape(unsigned int shapeID) {
  bt_assert(shapeID < parts.size());
  return puzzle.getShape(parts[shapeID]->shapeId);
}

/* return the instance count for one shape of the problem */
unsigned int problem_c::getShapeMin(unsigned int shapeID) const {
  bt_assert(shapeID < parts.size());

  return parts[shapeID]->min;
}
unsigned int problem_c::getShapeMax(unsigned int shapeID) const {
  bt_assert(shapeID < parts.size());

  return parts[shapeID]->max;
}

void problem_c::addSolution(assembly_c * assm) {
  bt_assert(assm);
  bt_assert(solveState == SS_SOLVING);

  solutions.push_back(new solution_c(assm, numAssemblies));
}

void problem_c::addSolution(assembly_c * assm, separation_c * disasm, unsigned int pos) {
  bt_assert(assm);
  bt_assert(solveState == SS_SOLVING);

  // if the given index is behind the number of solutions add at the end
  if (pos < solutions.size())
    solutions.insert(solutions.begin()+pos, new solution_c(assm, numAssemblies, disasm, numSolutions));
  else
    solutions.push_back(new solution_c(assm, numAssemblies, disasm, numSolutions));
}

void problem_c::addSolution(assembly_c * assm, separationInfo_c * disasm, unsigned int pos) {
  bt_assert(assm);
  bt_assert(solveState == SS_SOLVING);

  // if the given index is behind the number of solutions add at the end
  if (pos < solutions.size())
    solutions.insert(solutions.begin()+pos, new solution_c(assm, numAssemblies, disasm, numSolutions));
  else
    solutions.push_back(new solution_c(assm, numAssemblies, disasm, numSolutions));
}

void problem_c::removeAllSolutions(void) {
  for (unsigned int i = 0; i < solutions.size(); i++)
    delete solutions[i];
  solutions.clear();
  delete assm;
  assm = 0;
  assemblerState = "";
  solveState = SS_UNSOLVED;
  numAssemblies = 0;
  numSolutions = 0;
  usedTime = 0;
}

void problem_c::removeSolution(unsigned int sol) {
  bt_assert(sol < solutions.size());
  delete solutions[sol];
  solutions.erase(solutions.begin()+sol);
}

assembler_c::errState problem_c::setAssembler(assembler_c * assm) {


  if (assemblerState.length()) {

    bt_assert(solveState == SS_SOLVING);

    // if we have some assembler position data, try to load that
    assembler_c::errState err = assm->setPosition(assemblerState.c_str(), assemblerVersion.c_str());

    // when we could not load, return with error and reset to unsolved
    if (err != assembler_c::ERR_NONE) {
      solveState = SS_UNSOLVED;
      return err;
    }

    // and remove that data
    assemblerState = "";

  } else {

    bt_assert(solveState == SS_UNSOLVED);

    // if no data is available for assembler restoration reset counters
    bt_assert(numAssemblies == 0);
    bt_assert(numSolutions == 0);

    solveState = SS_SOLVING;
  }

  this->assm = assm;
  return assembler_c::ERR_NONE;
}

void problem_c::setShapeGroup(unsigned int shapeID, unsigned short group, unsigned short count) {
  bt_assert(shapeID < parts.size());

  // not first look, if we already have this group number in our list
  for (unsigned int i = 0; i < parts[shapeID]->groups.size(); i++)
    if (parts[shapeID]->groups[i].group == group) {

      /* if we change count, change it, if we set count to 0 remove that entry */
      if (count)
        parts[shapeID]->groups[i].count = count;
      else
        parts[shapeID]->groups.erase(parts[shapeID]->groups.begin()+i);
      return;
    }

  // not found add, but only groups not equal to 0
  if (group && count)
    parts[shapeID]->addGroup(group, count);
}

unsigned short problem_c::getShapeGroupNumber(unsigned int shapeID) const {
  bt_assert(shapeID < parts.size());

  return parts[shapeID]->groups.size();
}

unsigned short problem_c::getShapeGroup(unsigned int shapeID, unsigned int groupID) const {
  bt_assert(shapeID < parts.size());
  bt_assert(groupID < parts[shapeID]->groups.size());

  return parts[shapeID]->groups[groupID].group;
}

unsigned short problem_c::getShapeGroupCount(unsigned int shapeID, unsigned int groupID) const {
  bt_assert(shapeID < parts.size());
  bt_assert(groupID < parts[shapeID]->groups.size());

  return parts[shapeID]->groups[groupID].count;
}

unsigned int problem_c::pieceToShape(unsigned int piece) const {

  unsigned int shape = 0;

  bt_assert(shape < parts.size());

  while (piece >= parts[shape]->max) {
    piece -= parts[shape]->max;
    shape++;
    bt_assert(shape < parts.size());
  }

  return shape;
}

unsigned int problem_c::pieceToSubShape(unsigned int piece) const {

  unsigned int shape = 0;

  bt_assert(shape < parts.size());

  while (piece >= parts[shape]->max) {
    piece -= parts[shape]->max;
    shape++;
    bt_assert(shape < parts.size());
  }

  return piece;
}

const gridType_c * problem_c::getGridType(void) const { return puzzle.getGridType(); }
gridType_c * problem_c::getGridType(void) { return puzzle.getGridType(); }


static bool comp_0_assembly(const solution_c * s1, const solution_c * s2)
{
  return s1->getAssemblyNumber() < s2->getAssemblyNumber();
}

static bool comp_1_level(solution_c * s1, solution_c * s2)
{
  return s1->getDisassemblyInfo() && s2->getDisassemblyInfo() &&
      (s1->getDisassemblyInfo()->compare(s2->getDisassemblyInfo()) < 0);
}

static bool comp_2_moves(solution_c * s1, solution_c * s2)
{
  return s1->getDisassemblyInfo() && s2->getDisassemblyInfo() &&
      (s1->getDisassemblyInfo()->sumMoves() < s2->getDisassemblyInfo()->sumMoves());
}

static bool comp_3_pieces(const solution_c * s1, const solution_c * s2)
{
  return s1->getAssembly()->comparePieces(s2->getAssembly()) > 0;
}

void problem_c::sortSolutions(int by) {
  switch (by) {
    case 0: stable_sort(solutions.begin(), solutions.end(), comp_0_assembly); break;
    case 1: stable_sort(solutions.begin(), solutions.end(), comp_1_level   ); break;
    case 2: stable_sort(solutions.begin(), solutions.end(), comp_2_moves   ); break;
    case 3: stable_sort(solutions.begin(), solutions.end(), comp_3_pieces  ); break;
  }
}

