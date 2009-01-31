/*
 * Copyright (C) 2003-2008  Andreas Röver
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

#include <algorithm>

#include <stdio.h>

#include <xmlwrapp/attributes.h>

/* ********* SOLUTION ************** */

class solution_c {

public:

  solution_c(assembly_c * assm, unsigned int assmNum, separation_c * t, unsigned int solNum) :
    assembly(assm), tree(t), treeInfo(0), assemblyNum(assmNum), solutionNum(solNum) {}

  solution_c(assembly_c * assm, unsigned int assmNum, separationInfo_c * ti, unsigned int solNum) :
    assembly(assm), tree(0), treeInfo(ti), assemblyNum(assmNum), solutionNum(solNum) {}

  solution_c(assembly_c * assm, unsigned int assmNum) :
    assembly(assm), tree(0), treeInfo(0), assemblyNum(assmNum), solutionNum(0) {}

  solution_c(const xml::node & node, unsigned int pieces, const gridType_c * gt);

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

  xml::node save(void) const;

  void exchangeShape(unsigned int s1, unsigned int s2) {
    if (assembly)
      assembly->exchangeShape(s1, s2);
    if (tree)
      tree->exchangeShape(s1, s2);
  }
};

solution_c::solution_c(const xml::node & node, unsigned int pieces, const gridType_c * gt) :
  tree(0), treeInfo(0), assemblyNum(0), solutionNum(0) {

  if ((node.get_type() != xml::node::type_element) ||
      (strcmp(node.get_name(), "solution") != 0))
    throw load_error("wrong node type for solution", node);

  if (node.find("assembly") == node.end())
    throw load_error("solution does not contain an assembly", node);

  xml::node::const_iterator it;

  it = node.find("assembly");
  assembly = new assembly_c(*it, pieces, gt);

  it = node.find("separation");
  if (it != node.end()) {

    // find the number of really placed pieces
    unsigned int pl = 0;
    for (unsigned int i = 0; i < assembly->placementCount(); i++)
      if (assembly->isPlaced(i))
        pl++;
    tree = new separation_c(*it, pl);
  } else {

    it = node.find("separationInfo");
    if (it != node.end())
      treeInfo = new separationInfo_c(*it);
  }

  if (node.get_attributes().find("asmNum") != node.get_attributes().end())
    assemblyNum = atoi(node.get_attributes().find("asmNum")->get_value());

  if ((tree || treeInfo) && (node.get_attributes().find("solNum") != node.get_attributes().end()))
    solutionNum = atoi(node.get_attributes().find("solNum")->get_value());
}

xml::node solution_c::save(void) const {
  xml::node nd("solution");

  char tmp[50];

  if (assemblyNum) {
    snprintf(tmp, 50, "%i", assemblyNum);
    nd.get_attributes().insert("asmNum", tmp);
  }

  nd.insert(assembly->save());

  if (tree) {            nd.insert(tree->save());
  } else if (treeInfo) { nd.insert(treeInfo->save());
  }

  if ((tree || treeInfo) && solutionNum) {
    snprintf(tmp, 50, "%i", solutionNum);
    nd.get_attributes().insert("solNum", tmp);
  }

  return nd;
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

problem_c::problem_c(problem_c * orig) :
  puzzle(orig->puzzle), result(orig->result),
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

xml::node problem_c::save(void) const {
  xml::node nd("problem");

  if (name.length() > 0)
    nd.get_attributes().insert("name", name.c_str());

  char tmp[50];

  xml::node::iterator it;

  snprintf(tmp, 50, "%i", solveState);
  nd.get_attributes().insert("state", tmp);

  if (solveState != SS_UNSOLVED) {
    snprintf(tmp, 50, "%li", numAssemblies);
    nd.get_attributes().insert("assemblies", tmp);

    snprintf(tmp, 50, "%li", numSolutions);
    nd.get_attributes().insert("solutions", tmp);

    snprintf(tmp, 50, "%li", usedTime);
    nd.get_attributes().insert("time", tmp);
  }

  if (maxHoles != 0xFFFFFFFF) {
    snprintf(tmp, 50, "%i", maxHoles);
    nd.get_attributes().insert("maxHoles", tmp);
  }

  it = nd.insert(xml::node("shapes"));

  for (unsigned int i = 0; i < shapes.size(); i++) {
    xml::node::iterator it2 = it->insert(xml::node("shape"));

    snprintf(tmp, 50, "%i", shapes[i]->shapeId );
    it2->get_attributes().insert("id", tmp);

    if (shapes[i]->min == shapes[i]->max) {
      snprintf(tmp, 50, "%i", shapes[i]->min);
      it2->get_attributes().insert("count", tmp);
    } else {
      snprintf(tmp, 50, "%i", shapes[i]->min);
      it2->get_attributes().insert("min", tmp);
      snprintf(tmp, 50, "%i", shapes[i]->max);
      it2->get_attributes().insert("max", tmp);
    }

    if (shapes[i]->groups.size() == 0) {
      // do nothing, we don't need to save anything in this case
    } else if ((shapes[i]->groups.size() == 1) &&
               (shapes[i]->groups[0].count == shapes[i]->max)) {
      // this is the case when all pieces are in the same group
      // we only need to save the group, if it is not 0,
      // the loader takes 0 as default anyway
      if (shapes[i]->groups[0].group != 0) {
        snprintf(tmp, 50, "%i", shapes[i]->groups[0].group);
        it2->get_attributes().insert("group", tmp);
      }

    } else {

      for (unsigned int j = 0; j < shapes[i]->groups.size(); j++)
        if (shapes[i]->groups[j].group != 0) {

          xml::node::iterator it3 = it2->insert(xml::node("group"));

          snprintf(tmp, 50, "%i", shapes[i]->groups[j].group);
          it3->get_attributes().insert("group", tmp);

          snprintf(tmp, 50, "%i", shapes[i]->groups[j].count);
          it3->get_attributes().insert("count", tmp);
        }
    }
  }

  it = nd.insert(xml::node("result"));
  snprintf(tmp, 50, "%i", result);
  it->get_attributes().insert("id", tmp);

  it = nd.insert(xml::node("bitmap"));

  for (std::set<uint32_t>::iterator i = colorConstraints.begin(); i != colorConstraints.end(); i++) {
    xml::node::iterator it2 = it->insert(xml::node("pair"));

    snprintf(tmp, 50, "%i", *i >> 16);
    it2->get_attributes().insert("piece", tmp);

    snprintf(tmp, 50, "%i", *i & 0xFFFF);
    it2->get_attributes().insert("result", tmp);
  }

  if (solveState == SS_SOLVING) {
    if (assm)
      nd.insert(assm->save());
    else if (assemblerState != "" && assemblerVersion != "") {
      xml::node::iterator it2 = nd.insert(xml::node("assembler"));
      it2->get_attributes().insert("version", assemblerVersion.c_str());
      it2->set_content(assemblerState.c_str());
    }
  }

  if (solutions.size()) {
    it = nd.insert(xml::node("solutions"));
    for (unsigned int i = 0; i < solutions.size(); i++)
      it->insert(solutions[i]->save());
  }

  return nd;
}

problem_c::problem_c(puzzle_c & puz, const xml::node & node) :
  puzzle(puz), result(0xFFFFFFFF),
  assm(0) {

  if ((node.get_type() != xml::node::type_element) ||
      (strcmp(node.get_name(), "problem") != 0))
    throw load_error("not the right node for the puzzle problem", node);

  if (node.get_attributes().find("name") != node.get_attributes().end())
    name = node.get_attributes().find("name")->get_value();

  xml::node::const_iterator it;

  unsigned int pieces = 0;

  it = node.find("shapes");
  if (it != node.end())
    for (xml::node::const_iterator i = it->begin(); i != it->end(); i++)
      if ((i->get_type() == xml::node::type_element) &&
          (strcmp(i->get_name(), "shape") == 0)) {
        unsigned short id, min, max, grp;

        if (i->get_attributes().find("id") == i->get_attributes().end())
          throw load_error("a shape node must have an 'idt' attribute", *i);

        id = atoi(i->get_attributes().find("id")->get_value());

        if (i->get_attributes().find("count") != i->get_attributes().end())
          min = max = atoi(i->get_attributes().find("count")->get_value());
        else if (i->get_attributes().find("min") != i->get_attributes().end() &&
                 i->get_attributes().find("max") != i->get_attributes().end()) {
          min = atoi(i->get_attributes().find("min")->get_value());
          max = atoi(i->get_attributes().find("max")->get_value());
          if (min > max)
            throw load_error("min of shape count must by <= max", node);
        } else
          min = max = 1;

        if (i->get_attributes().find("group") != i->get_attributes().end())
          grp = atoi(i->get_attributes().find("group")->get_value());
        else
          grp = 0;

        pieces += max;

        if (id >= puzzle.shapeNumber())
          throw load_error("the shape ids must be for valid shapes", *i);

        if (grp)
          shapes.push_back(new shape_c(id, min, max, grp));

        else {
          /* OK we have 2 ways to specify groups for pieces, either
           * a group attribute in the tag. Then all pieces are
           * in the given group, or you specify a list of group
           * tags inside the tag. Each of the group tag gives a
           * group and a count
           */
          i->get_content();

          shapes.push_back(new shape_c(id, min, max));

          for (xml::node::const_iterator i2 = i->begin(); i2 != i->end(); i2++)
          if ((i2->get_type() == xml::node::type_element) &&
              (strcmp(i2->get_name(), "group") == 0)) {

            if (i2->get_attributes().find("group") == i2->get_attributes().end())
              throw load_error("a group node must have a group attribute", *i2);

            if (i2->get_attributes().find("count") == i2->get_attributes().end())
              throw load_error("a group node must have a count attribute", *i2);

            unsigned int cnt = atoi(i2->get_attributes().find("count")->get_value());
            grp = atoi(i2->get_attributes().find("group")->get_value());

            (*shapes.rbegin())->addGroup(grp, cnt);
          }
        }
      }

  it = node.find("result");
  if (it != node.end()) {
    if (it->get_attributes().find("id") == it->get_attributes().end())
      throw load_error("the result node must have an 'id' attribute", *it);

    result = atoi(it->get_attributes().find("id")->get_value());

    // if, for whatever reasons the shape is was not right, we reset it to an empty shape
    if (result >= puzzle.shapeNumber())
      result = 0xFFFFFFFF;
  }

  it = node.find("solutions");
  if (it != node.end()) {
    for (xml::node::const_iterator i = it->begin(); i != it->end(); i++)
      if ((i->get_type() == xml::node::type_element) &&
          (strcmp(i->get_name(), "solution") == 0))
        solutions.push_back(new solution_c(*i, pieces, puzzle.getGridType()));
  }

  if (node.get_attributes().find("state") != node.get_attributes().end())
    solveState = (solveState_e)atoi(node.get_attributes().find("state")->get_value());
  else
    solveState = SS_UNSOLVED;

  if (solveState != SS_UNSOLVED) {
    if (node.get_attributes().find("assemblies") == node.get_attributes().end())
      numAssemblies = 0;
    else
      numAssemblies = atoi(node.get_attributes().find("assemblies")->get_value());

    if (node.get_attributes().find("solutions") == node.get_attributes().end())
      numSolutions = 0;
    else
      numSolutions = atoi(node.get_attributes().find("solutions")->get_value());

    if (node.get_attributes().find("time") == node.get_attributes().end())
      usedTime = 0;
    else
      usedTime = atoi(node.get_attributes().find("time")->get_value());

  } else {
    numAssemblies = 0;
    numSolutions = 0;
    usedTime = 0;
  }

  if (node.get_attributes().find("maxHoles") != node.get_attributes().end())
    maxHoles = atoi(node.get_attributes().find("maxHoles")->get_value());
  else
    maxHoles = 0xFFFFFFFF;

  it = node.find("bitmap");
  if (it != node.end()) {
    for (xml::node::const_iterator i = it->begin(); i != it->end(); i++)
      if ((i->get_type() == xml::node::type_element) &&
          (strcmp(i->get_name(), "pair") == 0)) {

        if (i->get_attributes().find("piece") == i->get_attributes().end())
          throw load_error("a pair node must have a piece attribute", *i);

        if (i->get_attributes().find("result") == i->get_attributes().end())
          throw load_error("a pair node must have a result attribute", *i);

        unsigned int piece = atoi(i->get_attributes().find("piece")->get_value());
        unsigned int result = atoi(i->get_attributes().find("result")->get_value());

        colorConstraints.insert(piece << 16 | result);
      }
  }

  it = node.find("assembler");
  if ((it != node.end()) && (it->get_attributes().find("version") != it->get_attributes().end())) {
    assemblerState = it->get_content();
    assemblerVersion = it->get_attributes().find("version")->get_value();
  }
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
  return puzzle.getShape(result);
}

voxel_c * problem_c::getResultShape(void) {
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

