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
#include "xml.h"

#include <algorithm>

#include <stdio.h>

/* ********* SOLUTION ************** */

class solution_c {

public:

  solution_c(assembly_c * assm, unsigned int assmNum, separation_c * t, unsigned int solNum) :
    assembly(assm), tree(t), treeInfo(0), assemblyNum(assmNum), solutionNum(solNum) {}

  solution_c(assembly_c * assm, unsigned int assmNum, separationInfo_c * ti, unsigned int solNum) :
    assembly(assm), tree(0), treeInfo(ti), assemblyNum(assmNum), solutionNum(solNum) {}

  solution_c(assembly_c * assm, unsigned int assmNum) :
    assembly(assm), tree(0), treeInfo(0), assemblyNum(assmNum), solutionNum(0) {}

  solution_c(xmlParser_c & pars, unsigned int pieces, const gridType_c * gt);

  ~solution_c(void);

  /* the assembly contains the pieces so that they
   * do assemble into the result shape */
  assembly_c * assembly;

  /* the disassembly tree, only not NULL, if we
   * have disassembled the puzzle
   */
  separation_c * tree;

  /* if no separation is given, maybe we have a separationInfo
   * that contains some of the information in that
   */
  separationInfo_c * treeInfo;

  /* as it is now possible to not save all solutions
   * it might be useful to know the exact number and sequence
   * how solutions were found
   *
   * solutionNum is 0, when tree is 0
   */
  unsigned int assemblyNum;
  unsigned int solutionNum;

  void save(xmlWriter_c & xml) const;

  void exchangeShape(unsigned int s1, unsigned int s2) {
    if (assembly)
      assembly->exchangeShape(s1, s2);
    if (tree)
      tree->exchangeShape(s1, s2);
  }
};

solution_c::solution_c(xmlParser_c & pars, unsigned int pieces, const gridType_c * gt) :
  tree(0), treeInfo(0), assemblyNum(0), solutionNum(0)
{
  pars.require(xmlParser_c::START_TAG, "solution");

  std::string str;

  str = pars.getAttributeValue("asmNum");
  if (str.length())
    assemblyNum = atoi(str.c_str());

  str = pars.getAttributeValue("solNum");
  if (str.length())
    solutionNum = atoi(str.c_str());

  do {
    int state = pars.nextTag();

    if (state == xmlParser_c::END_TAG) break;
    pars.require(xmlParser_c::START_TAG, "");

    if (pars.getName() == "assembly")
    {
      assembly = new assembly_c(pars, pieces, gt);
      pars.require(xmlParser_c::END_TAG, "assembly");
    }
    else if (pars.getName() == "separation")
    {
      if (!assembly)
        pars.exception("an assembly must always be before a separation in a solution");

      // find the number of really placed pieces
      unsigned int pl = 0;
      for (unsigned int i = 0; i < assembly->placementCount(); i++)
        if (assembly->isPlaced(i))
          pl++;
      tree = new separation_c(pars, pl);

      pars.require(xmlParser_c::END_TAG, "separation");
    }
    else if (pars.getName() == "separationInfo")
    {
      treeInfo = new separationInfo_c(pars);
      pars.require(xmlParser_c::END_TAG, "separationInfo");
    }
    else
      pars.skipSubTree();

    pars.require(xmlParser_c::END_TAG, "");

  } while (true);

  pars.require(xmlParser_c::END_TAG, "solution");

  if (!assembly)
    pars.exception("no assembly in solution");

  if (tree && treeInfo)
  {
    delete treeInfo;
    treeInfo = 0;
  }
}

void solution_c::save(xmlWriter_c & xml) const
{
  xml.newTag("solution");

  if (assemblyNum) {
    xml.newAttrib("asmNum", assemblyNum);
  }

  if ((tree || treeInfo) && solutionNum)
    xml.newAttrib("solNum", solutionNum);

  assembly->save(xml);

  if (tree) {            tree->save(xml);
  } else if (treeInfo) { treeInfo->save(xml);
  }

  xml.endTag("solution");
}

solution_c::~solution_c(void) {
  if (tree)
    delete tree;

  if (assembly)
    delete assembly;

  if (treeInfo)
    delete treeInfo;
}


/******** ORIG PROBLEM ************/

class group_c {

  public:

    group_c(unsigned short gr, unsigned short cnt) : group(gr), count(cnt) {}

    unsigned short group;
    unsigned short count;
};

class shape_c {

  public:

    shape_c(unsigned short id, unsigned short mn, unsigned short mx, unsigned short grp) : shapeId(id), min(mn), max(mx) {
      if (grp)
        groups.push_back(group_c(grp, max));
    }

    shape_c(unsigned short id, unsigned short mn, unsigned short mx) : shapeId(id), min(mn), max(mx) { }

    shape_c(shape_c * orig) : shapeId(orig->shapeId), min(orig->min), max(orig->max), groups(orig->groups) { }

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

  for (unsigned int i = 0; i < shapes.size(); i++)
    delete shapes[i];

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

  for (unsigned int i = 0; i < orig->shapes.size(); i++)
    shapes.push_back(new shape_c(orig->shapes[i]));

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

  xml.newTag("shapes");

  for (unsigned int i = 0; i < shapes.size(); i++) {
    xml.newTag("shape");

    xml.newAttrib("id", shapes[i]->shapeId);

    if (shapes[i]->min == shapes[i]->max)
    {
      xml.newAttrib("count", shapes[i]->min);
    }
    else
    {
      xml.newAttrib("min", shapes[i]->min);
      xml.newAttrib("max", shapes[i]->max);
    }

    if (shapes[i]->groups.size() == 0)
    {
      // do nothing, we don't need to save anything in this case
    }
    else if ((shapes[i]->groups.size() == 1) &&
             (shapes[i]->groups[0].count == shapes[i]->max))
    {
      // this is the case when all pieces are in the same group
      // we only need to save the group, if it is not 0,
      // the loader takes 0 as default anyway
      if (shapes[i]->groups[0].group != 0)
        xml.newAttrib("group", shapes[i]->groups[0].group);
    }
    else
    {
      for (unsigned int j = 0; j < shapes[i]->groups.size(); j++)
        if (shapes[i]->groups[j].group != 0)
        {
          xml.newTag("group");
          xml.newAttrib("group", shapes[i]->groups[j].group);
          xml.newAttrib("count", shapes[i]->groups[j].count);
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

    if (pars.getName() == "shapes")
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
            shapes.push_back(new shape_c(id, min, max, grp));
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
            shapes.push_back(new shape_c(id, min, max));

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

                (*shapes.rbegin())->addGroup(grp, cnt);
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

void problem_c::shapeIdRemoved(unsigned short idx) {

  if (result == idx)
    result = 0xFFFFFFFF;

  unsigned int i = 0;
  while (i < shapes.size()) {
    if (shapes[i]->shapeId == idx) {
      delete shapes[i];
      shapes.erase(shapes.begin()+i);
    } else {
      i++;
    }
  }

  /* now check all shapes, and the result, if their id is larger
   * than the deleted shape, if so decrement to update the number
   */
  for (unsigned int i = 0; i < shapes.size(); i++)
    if (shapes[i]->shapeId > idx) shapes[i]->shapeId--;

  if (result > idx)
    result--;
}

void problem_c::exchangeShapeId(unsigned int s1, unsigned int s2) {

  if (result == s1) result = s2;
  else if (result == s2) result = s1;

  for (unsigned int i = 0; i < shapes.size(); i++)
    if (shapes[i]->shapeId == s1) shapes[i]->shapeId = s2;
    else if (shapes[i]->shapeId == s2) shapes[i]->shapeId = s1;
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
  bt_assert(s1 < shapes.size());
  bt_assert(s2 < shapes.size());

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
    p1Start += shapes[i]->max;

  for (unsigned int i = 0; i < s2; i++)
    p2Start += shapes[i]->max;

  p1Count = shapes[s1]->max;
  p2Count = shapes[s2]->max;

  bt_assert(p1Start+p1Count == p2Start);

  shape_c * s = shapes[s1];
  shapes[s1] = shapes[s2];
  shapes[s2] = s;

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

void problem_c::setResult(unsigned int shape) {
  bt_assert(shape < puzzle.shapeNumber());
  result = shape;
}

unsigned int problem_c::getResult(void) const {
  bt_assert(result < puzzle.shapeNumber());
  return result;
}
bool problem_c::resultInvalid(void) const {
  return result >= puzzle.shapeNumber();
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

  for (unsigned int id = 0; id < shapes.size(); id++) {
    if (shapes[id]->shapeId == shape) {

      shapes[id]->min = count;
      if (shapes[id]->min > shapes[id]->max)
        shapes[id]->max = count;

      return;
    }
  }

  // when we get here there is no piece with the required puzzle shape, so add it
  if (count)
    shapes.push_back(new shape_c(shape, count, count, 0));
}

void problem_c::setShapeCountMax(unsigned int shape, unsigned int count) {
  bt_assert(shape < puzzle.shapeNumber());

  for (unsigned int id = 0; id < shapes.size(); id++) {
    if (shapes[id]->shapeId == shape) {

      if (count == 0) {
        shapes.erase(shapes.begin()+id);
        return;
      }

      shapes[id]->max = count;
      if (shapes[id]->max < shapes[id]->min)
        shapes[id]->min = count;

      return;
    }
  }

  if (count)
    shapes.push_back(new shape_c(shape, 0, count, 0));
}

unsigned int problem_c::getShapeCountMin(unsigned int shape) const {
  bt_assert(shape < puzzle.shapeNumber());

  for (unsigned int id = 0; id < shapes.size(); id++)
    if (shapes[id]->shapeId == shape)
      return shapes[id]->min;

  return 0;
}

unsigned int problem_c::getShapeCountMax(unsigned int shape) const {
  bt_assert(shape < puzzle.shapeNumber());

  for (unsigned int id = 0; id < shapes.size(); id++)
    if (shapes[id]->shapeId == shape)
      return shapes[id]->max;

  return 0;
}

/* return the number of pieces in the problem (sum of all counts of all shapes */
unsigned int problem_c::pieceNumber(void) const {
  unsigned int result = 0;

  for (unsigned int i = 0; i < shapes.size(); i++)
    result += shapes[i]->max;

  return result;
}

/* return the shape id of the given shape (index into the shape array of the puzzle */
unsigned int problem_c::getShape(unsigned int shapeID) const {
  bt_assert(shapeID < shapes.size());

  return shapes[shapeID]->shapeId;
}

unsigned int problem_c::getShapeId(unsigned int shape) const {
  bt_assert(shape < puzzle.shapeNumber());

  for (unsigned int i = 0; i < shapes.size(); i++)
    if (shapes[i]->shapeId == shape)
      return i;

  bt_assert(0);
  return 0;
}

bool problem_c::containsShape(unsigned int shape) const {
  if (result == shape)
    return true;

  for (unsigned int i = 0; i < shapes.size(); i++)
    if (shapes[i]->shapeId == shape)
      return true;

  return false;
}

const voxel_c * problem_c::getShapeShape(unsigned int shapeID) const {
  bt_assert(shapeID < shapes.size());
  return puzzle.getShape(shapes[shapeID]->shapeId);
}

voxel_c * problem_c::getShapeShape(unsigned int shapeID) {
  bt_assert(shapeID < shapes.size());
  return puzzle.getShape(shapes[shapeID]->shapeId);
}

/* return the instance count for one shape of the problem */
unsigned int problem_c::getShapeMin(unsigned int shapeID) const {
  bt_assert(shapeID < shapes.size());

  return shapes[shapeID]->min;
}
unsigned int problem_c::getShapeMax(unsigned int shapeID) const {
  bt_assert(shapeID < shapes.size());

  return shapes[shapeID]->max;
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

void problem_c::swapSolutions(unsigned int sol1, unsigned int sol2) {
  bt_assert(sol1 < solutions.size());
  bt_assert(sol2 < solutions.size());

  if (sol1 == sol2) return;

  solution_c * s = solutions[sol1];
  solutions[sol1] = solutions[sol2];
  solutions[sol2] = s;
}

void problem_c::removeSolution(unsigned int sol) {
  bt_assert(sol < solutions.size());
  delete solutions[sol];
  solutions.erase(solutions.begin()+sol);
}

void problem_c::removeAllDisassm(void) {
  for (unsigned int i = 0; i < solutions.size(); i++) {
    solution_c * s = solutions[i];

    if (s->tree) {

      if (!s->treeInfo)
        s->treeInfo = new separationInfo_c(s->tree);

      delete s->tree;
      s->tree = 0;
    }
  }
}

void problem_c::removeDisassm(unsigned int i) {
  bt_assert(i < solutions.size());

  solution_c * s = solutions[i];

  if (s->tree) {

    if (!s->treeInfo)
      s->treeInfo = new separationInfo_c(s->tree);

    delete s->tree;
    s->tree = 0;
  }
}

void problem_c::addDisasmToSolution(unsigned int sol, separation_c * disasm) {
  bt_assert(sol < solutions.size());

  solution_c * s = solutions[sol];

  if (s->tree) delete s->tree;
  if (s->treeInfo) delete s->treeInfo;

  s->treeInfo = 0;
  s->tree = disasm;
}

assembly_c * problem_c::getAssembly(unsigned int sol) {
  bt_assert(sol < solutions.size());

  return solutions[sol]->assembly;
}

const assembly_c * problem_c::getAssembly(unsigned int sol) const {
  bt_assert(sol < solutions.size());

  return solutions[sol]->assembly;
}

separation_c * problem_c::getDisassembly(unsigned int sol) {
  bt_assert(sol < solutions.size());

  return solutions[sol]->tree;
}

const separation_c * problem_c::getDisassembly(unsigned int sol) const {
  bt_assert(sol < solutions.size());

  return solutions[sol]->tree;
}

separationInfo_c * problem_c::getDisassemblyInfo(unsigned int sol) {
  bt_assert(sol < solutions.size());

  if (!solutions[sol]->treeInfo && solutions[sol]->tree)
    solutions[sol]->treeInfo = new separationInfo_c(solutions[sol]->tree);

  return solutions[sol]->treeInfo;
}

const separationInfo_c * problem_c::getDisassemblyInfo(unsigned int sol) const {
  bt_assert(sol < solutions.size());

  if (!solutions[sol]->treeInfo && solutions[sol]->tree)
    solutions[sol]->treeInfo = new separationInfo_c(solutions[sol]->tree);

  return solutions[sol]->treeInfo;
}

unsigned int problem_c::getAssemblyNum(unsigned int sol) const {
  bt_assert(sol < solutions.size());

  return solutions[sol]->assemblyNum;
}

unsigned int problem_c::getSolutionNum(unsigned int sol) const {
  bt_assert(sol < solutions.size());

  return solutions[sol]->solutionNum;
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
  bt_assert(shapeID < shapes.size());

  // not first look, if we already have this group number in our list
  for (unsigned int i = 0; i < shapes[shapeID]->groups.size(); i++)
    if (shapes[shapeID]->groups[i].group == group) {

      /* if we change count, change it, if we set count to 0 remove that entry */
      if (count)
        shapes[shapeID]->groups[i].count = count;
      else
        shapes[shapeID]->groups.erase(shapes[shapeID]->groups.begin()+i);
      return;
    }

  // not found add, but only groups not equal to 0
  if (group && count)
    shapes[shapeID]->addGroup(group, count);
}

unsigned short problem_c::getShapeGroupNumber(unsigned int shapeID) const {
  bt_assert(shapeID < shapes.size());

  return shapes[shapeID]->groups.size();
}

unsigned short problem_c::getShapeGroup(unsigned int shapeID, unsigned int groupID) const {
  bt_assert(shapeID < shapes.size());
  bt_assert(groupID < shapes[shapeID]->groups.size());

  return shapes[shapeID]->groups[groupID].group;
}

unsigned short problem_c::getShapeGroupCount(unsigned int shapeID, unsigned int groupID) const {
  bt_assert(shapeID < shapes.size());
  bt_assert(groupID < shapes[shapeID]->groups.size());

  return shapes[shapeID]->groups[groupID].count;
}

unsigned int problem_c::pieceToShape(unsigned int piece) const {

  unsigned int shape = 0;

  bt_assert(shape < shapes.size());

  while (piece >= shapes[shape]->max) {
    piece -= shapes[shape]->max;
    shape++;
    bt_assert(shape < shapes.size());
  }

  return shape;
}

unsigned int problem_c::pieceToSubShape(unsigned int piece) const {

  unsigned int shape = 0;

  bt_assert(shape < shapes.size());

  while (piece >= shapes[shape]->max) {
    piece -= shapes[shape]->max;
    shape++;
    bt_assert(shape < shapes.size());
  }

  return piece;
}

const gridType_c * problem_c::getGridType(void) const { return puzzle.getGridType(); }
gridType_c * problem_c::getGridType(void) { return puzzle.getGridType(); }


static bool comp_0_assembly(const solution_c * s1, const solution_c * s2)
{
  return s1->assemblyNum < s2->assemblyNum;
}

static bool comp_1_level(solution_c * s1, solution_c * s2)
{
  if (!s1->treeInfo && s1->tree) s1->treeInfo = new separationInfo_c(s1->tree);
  if (!s2->treeInfo && s2->tree) s2->treeInfo = new separationInfo_c(s2->tree);

  if (s1->treeInfo->compare(s2->treeInfo) < 0) printf("oops\n");

  return s1->treeInfo && s2->treeInfo && (s1->treeInfo->compare(s2->treeInfo) < 0);
}

static bool comp_2_moves(solution_c * s1, solution_c * s2)
{
  if (!s1->treeInfo && s1->tree) s1->treeInfo = new separationInfo_c(s1->tree);
  if (!s2->treeInfo && s2->tree) s2->treeInfo = new separationInfo_c(s2->tree);

  return s1->treeInfo && s2->treeInfo && (s1->treeInfo->sumMoves() < s2->treeInfo->sumMoves());
}

static bool comp_3_pieces(const solution_c * s1, const solution_c * s2)
{
  return s1->assembly->comparePieces(s2->assembly) > 0;
}

void problem_c::sortSolutions(int by) {
  switch (by) {
    case 0: stable_sort(solutions.begin(), solutions.end(), comp_0_assembly); break;
    case 1: stable_sort(solutions.begin(), solutions.end(), comp_1_level   ); break;
    case 2: stable_sort(solutions.begin(), solutions.end(), comp_2_moves   ); break;
    case 3: stable_sort(solutions.begin(), solutions.end(), comp_3_pieces  ); break;
  }
}

