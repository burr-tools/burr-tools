/*
 * Copyright (C) 2003-2005  Andreas Röver
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


#include "puzzle.h"
#include "assembler.h"

#include <stdio.h>

#include <xmlwrapp/attributes.h>

#include <functional>

using namespace std;


/* the 2d bitmap
 * the bitmap is always square and alows only for the here necessary modifications
 */
class bitmap_c {

private:

  unsigned char *map;
  unsigned int colors;

public:

  /* create new bitmap with size rows and columns, all bit are cleared */
  bitmap_c(unsigned int col);

  ~bitmap_c(void) { if (map) delete [] map; }

  /* add a new color at the end */
  void add(void);

  /* remove the given color */
  void remove(unsigned int col);

  void set(unsigned int pcCol, unsigned int resCol, bool value) {

    bt_assert(pcCol < colors);
    bt_assert(resCol < colors);

    int idx = resCol * colors + pcCol;

    if (value)
      map[idx >> 3] |= (1 << (idx & 7));
    else
      map[idx >> 3] &= ~(1 << (idx & 7));
  }

  bool get(unsigned int pcCol, unsigned int resCol) const {
    bt_assert(pcCol < colors);
    bt_assert(resCol < colors);

    int idx = resCol * colors + pcCol;

    return ((map[idx >> 3] & (1 << (idx & 7))) != 0);
  }

  xml::node save(void) const;
  void load(const xml::node & node);

  unsigned int getColors(void) const { return colors; }

};

bitmap_c::bitmap_c(unsigned int col) : colors(col) {
  if (colors) {
    unsigned char bytes = (col*col + 7) >> 3;
    map = new unsigned char[bytes];
    memset(map, 0, bytes);
  } else
    map = 0;
}

void bitmap_c::add(void) {

  unsigned char bytes = ((colors+1)*(colors+1) + 7) >> 3;
  unsigned char *m2 = new unsigned char[bytes];
  memset(m2, 0, bytes);

  if (map) {
    for (unsigned int i = 0; i < colors; i++)
      for (unsigned int j = 0; j < colors; j++) {
        unsigned int idx = j * (colors+1) + i;
    
        if (get(i, j))
          m2[idx >> 3] |= (1 << (idx & 7));
        else
          m2[idx >> 3] &= ~(1 << (idx & 7));
      }

    delete [] map;
  }

  map = m2;
  colors++;
}


void bitmap_c::remove(unsigned int col) {

  bt_assert(col <= colors);

  unsigned char *m2 = new unsigned char[((colors-1)*(colors-1) + 7) >> 3];

  for (unsigned int i = 0; i < colors-1; i++)
    for (unsigned int j = 0; j < colors-1; j++) {
      unsigned int idx = j * (colors-1) + i;

      unsigned int k, l;

      if (i < col) k = i; else k = i+1;
      if (j < col) l = j; else l = j+1;
  
      if (get(k, l))
        m2[idx >> 3] |= (1 << (idx & 7));
      else
        m2[idx >> 3] &= ~(1 << (idx & 7));
    }

  delete [] map;

  map = m2;
  colors--;
}

xml::node bitmap_c::save(void) const {
  xml::node nd("bitmap");

  char tmp[50];

  for (unsigned int pc = 0; pc < colors; pc++)
    for (unsigned int res = 0; res < colors; res++)
      if (get(pc, res)) {
        xml::node::iterator it = nd.insert(xml::node("pair"));
    
        snprintf(tmp, 50, "%i", pc);
        it->get_attributes().insert("piece", tmp);
    
        snprintf(tmp, 50, "%i", res);
        it->get_attributes().insert("result", tmp);
      }

  return nd;
}

void bitmap_c::load(const xml::node & node) {

  if ((node.get_type() != xml::node::type_element) ||
      (strcmp(node.get_name(), "bitmap") != 0))
    throw load_error("not the right node for color constrains", node);

  xml::node::const_iterator it;

  for (xml::node::const_iterator i = node.begin(); i != node.end(); i++)
    if ((i->get_type() == xml::node::type_element) &&
        (strcmp(i->get_name(), "pair") == 0)) {

      if (i->get_attributes().find("piece") == i->get_attributes().end())
        throw load_error("no attribute 'piece' found in color constraint node", node);
      if (i->get_attributes().find("result") == i->get_attributes().end())
        throw load_error("no attribute 'result' found in color constraint node", node);

      unsigned int piece = atoi(i->get_attributes().find("piece")->get_value());
      unsigned int result = atoi(i->get_attributes().find("result")->get_value());

      if (piece >= colors)
        throw load_error("piece color too big", node);
      if (result >= colors)
        throw load_error("result color too big", node);

      set(piece, result, true);
    }
}






class solution_c {

public:

  solution_c(assembly_c * assm, separation_c * t) : assembly(assm), tree(t) {}

  solution_c(const xml::node & node, unsigned int pieces);

  ~solution_c(void);

  /* the assembly contains the pieces so that they
   * do assemble into the result shape */
  assembly_c * assembly;

  /* the disassembly tree, only not NULL, if we
   * have disassembleed the puzzle
   */
  separation_c * tree;

  xml::node save(void) const;
};

solution_c::solution_c(const xml::node & node, unsigned int pieces) {

  if ((node.get_type() != xml::node::type_element) ||
      (strcmp(node.get_name(), "solution") != 0))
    throw load_error("wrong node type for solution", node);

  if (node.find("assembly") == node.end())
    throw load_error("slution does not contain an assembly", node);

  xml::node::const_iterator it;

  it = node.find("assembly");
  assembly = new assembly_c(*it, pieces);

  it = node.find("separation");
  if (it != node.end())
    tree = new separation_c(*it, pieces);
  else
    tree = 0;
}

xml::node solution_c::save(void) const {
  xml::node nd("solution");

  nd.insert(assembly->save());

  if (tree)
    nd.insert(tree->save());

  return nd;
}

solution_c::~solution_c(void) {
  if (tree)
    delete tree;

  if (assembly)
    delete assembly;
}

class problem_c {

public:

  problem_c(unsigned int colors) : result(0xFFFFFFFF), colorConstraints(colors), assm(0), numAssemblies(0xFFFFFFFF),
    numSolutions(0xFFFFFFFF)
  {}

  problem_c(const xml::node & node, unsigned int colors, unsigned int shapes);

  // nearly copy constructor, only the problem is copied not the solution
  problem_c(problem_c * prob);

  ~problem_c(void) {
    for (unsigned int i = 0; i < solutions.size(); i++)
      delete solutions[i];

    if (assm)
      delete assm;
  }

  xml::node save(void) const;

  class group_c {

  public:

    group_c(unsigned short gr, unsigned short cnt) : group(gr), count(cnt) {}

    unsigned short group;
    unsigned short count;
  };

  class shape_c {

  public:

    shape_c(unsigned short id, unsigned short cnt, unsigned short grp) : shapeId(id), count(cnt) {
      if (grp)
        groups.push_back(group_c(grp, cnt));
    }

    shape_c(unsigned short id, unsigned short cnt) : shapeId(id), count(cnt) { }

    void addGroup(unsigned short grp, unsigned short cnt) {
      groups.push_back(group_c(grp, cnt));
    }

    unsigned short shapeId;
    unsigned short count;

    vector<group_c> groups;
  };

  class shape_id_removed {
  public:
    unsigned short id;
    shape_id_removed(unsigned short i) : id(i) {}
    bool operator()(shape_c s) { return s.shapeId == id; }
  };

  // the pieces for this problem and how many of the pieces
  // of each hape are there
  std::vector<shape_c> shapes;

  // the result shape
  unsigned int result;

  // the found solutions
  std::vector<solution_c*> solutions;

  /**
   * this array contains the constrains for the colors for each pair of
   * colors is defines, if it is allowed to place a voxel inside a piece shape
   * at the result where the corresponding voxel has a certain color
   */
  bitmap_c colorConstraints;

  /**
   * if we have started to solve this problem this pointer shows us the corresponding assembler
   * if the pointer is 0 we have never started an assembly process
   * statistics can be found in the assembler, too
   */
  assembler_c * assm;

  /**
   * inform the problem about deleted shapes
   * the problem must remove these shapes from the
   * list and also decrement the count for all the other shapes
   */
  void shapeIdRemoved(unsigned short idx);

  /**
   * the name of the problem, so that the user can easily select one
   * out of a list with names
   */
  std::string name;

  /**
   * this state reflecs how far we are with solving this problem
   */
  puzzle_c::SolveState_e solveState;

  /**
   * for this variables 0xFFFFFFFF always stands for unknown and
   * 0xFFFFFFFE for too much to count
   *
   * this is independend of the soluions vector, these are the pure numbers
   */
  unsigned long numAssemblies;
  unsigned long numSolutions;

  /**
   * we only save the information that the assembler needs to reset it's state
   * and use this information once the user wants to continue solving the problem
   * otherwise the loading might take quite a while
   */
  std::string assemblerState;
  /**
   * each assembler also saves a version into the node so that it can check, if
   * the saves state can be resored
   */
  std::string assemblerVersion;

  /**
   * the time used up to get to the current state in the solving progress (in seconds)
   */
  unsigned long usedTime;
};


problem_c::problem_c(problem_c * orig) : result(orig->result), colorConstraints(orig->colorConstraints.getColors()),
numAssemblies(0xFFFFFFFF), numSolutions(0xFFFFFFFF), usedTime(0xFFFFFFFF)
{
  assm = 0;
  name = orig->name;

  for (unsigned int i = 0; i < colorConstraints.getColors(); i++)
    for (unsigned int j = 0; j < colorConstraints.getColors(); j++)
      colorConstraints.set(i, j, orig->colorConstraints.get(i, j));

  for (unsigned int i = 0; i < orig->shapes.size(); i++)
    shapes.push_back(orig->shapes[i]);
}


xml::node problem_c::save(void) const {
  xml::node nd("problem");
  nd.get_attributes().insert("name", name.c_str());

  char tmp[50];

  xml::node::iterator it;

  snprintf(tmp, 50, "%i", solveState);
  nd.get_attributes().insert("state", tmp);

  if (numAssemblies != 0xFFFFFFFF) {
    snprintf(tmp, 50, "%li", numAssemblies);
    nd.get_attributes().insert("assemblies", tmp);
  }

  if (numSolutions != 0xFFFFFFFF) {
    snprintf(tmp, 50, "%li", numSolutions);
    nd.get_attributes().insert("solutions", tmp);
  }

  if (usedTime != 0xFFFFFFFF) {
    snprintf(tmp, 50, "%li", usedTime);
    nd.get_attributes().insert("time", tmp);
  }

  it = nd.insert(xml::node("shapes"));

  for (unsigned int i = 0; i < shapes.size(); i++) {
    xml::node::iterator it2 = it->insert(xml::node("shape"));

    snprintf(tmp, 50, "%i", shapes[i].shapeId );
    it2->get_attributes().insert("id", tmp);

    snprintf(tmp, 50, "%i", shapes[i].count);
    it2->get_attributes().insert("count", tmp);

    if (shapes[i].groups.size() == 0) {
      // do nothing, we don't need to save anything in this case
    } else if ((shapes[i].groups.size() == 1) &&
               (shapes[i].groups[0].count == shapes[i].count)) {
      // this is the case when all pieces are in the same group
      // we only need to save the group, if it is not 0,
      // the loader takes 0 as default anyway
      if (shapes[i].groups[0].group != 0) {
        snprintf(tmp, 50, "%i", shapes[i].groups[0].group);
        it2->get_attributes().insert("group", tmp);
      }

    } else {

      for (unsigned int j = 0; j < shapes[i].groups.size(); j++)
        if (shapes[i].groups[j].group != 0) {

          xml::node::iterator it3 = it2->insert(xml::node("group"));

          snprintf(tmp, 50, "%i", shapes[i].groups[j].group);
          it3->get_attributes().insert("group", tmp);
      
          snprintf(tmp, 50, "%i", shapes[i].groups[j].count);
          it3->get_attributes().insert("count", tmp);
        }
    }
  }

  it = nd.insert(xml::node("result"));
  snprintf(tmp, 50, "%i", result);
  it->get_attributes().insert("id", tmp);

  nd.insert(colorConstraints.save());

  if (assm)
    nd.insert(assm->save());

  if (solutions.size()) {
    it = nd.insert(xml::node("solutions"));
    for (unsigned int i = 0; i < solutions.size(); i++)
      it->insert(solutions[i]->save());
  }

  return nd;
}

problem_c::problem_c(const xml::node & node, unsigned int color, unsigned int shape) : result(0xFFFFFFFF), colorConstraints(color), assm(0) {

  if ((node.get_type() != xml::node::type_element) ||
      (strcmp(node.get_name(), "problem") != 0))
    throw load_error("not the right node for the puzzle problem", node);

  if (node.get_attributes().find("name") == node.get_attributes().end())
    throw load_error("problems need to have a 'name' attribute", node);

  name = node.get_attributes().find("name")->get_value();

  xml::node::const_iterator it;

  unsigned int pieces = 0;

  it = node.find("shapes");
  if (it != node.end())
    for (xml::node::const_iterator i = it->begin(); i != it->end(); i++)
      if ((i->get_type() == xml::node::type_element) &&
          (strcmp(i->get_name(), "shape") == 0)) {
        unsigned short id, cnt, grp;

        if (i->get_attributes().find("id") == i->get_attributes().end())
          throw load_error("a shape node must have an 'idt' attribute", *i);

        if (i->get_attributes().find("count") == i->get_attributes().end())
          throw load_error("a shape node must have a 'count' attribute", *i);

        id = atoi(i->get_attributes().find("id")->get_value());
        cnt = atoi(i->get_attributes().find("count")->get_value());

        if (i->get_attributes().find("group") != i->get_attributes().end())
          grp = atoi(i->get_attributes().find("group")->get_value());
        else
          grp = 0;

        pieces += cnt;

        if (id >= shape)
          throw load_error("the shape ids must be for valid shapes", *i);

        if (grp)
          shapes.push_back(shape_c(id, cnt, grp));

        else {
          /* ok we have 2 ways to specify groups for pieces, either
           * a group attribute in the tag. Then all pieces are
           * in the given group, or you specify a list of group
           * tags inside the tag. Each of the group tag gives a
           * group and a count
           */
          i->get_content();

          shapes.push_back(shape_c(id, cnt));

          for (xml::node::const_iterator i2 = i->begin(); i2 != i->end(); i2++)
          if ((i2->get_type() == xml::node::type_element) &&
              (strcmp(i2->get_name(), "group") == 0)) {

            if (i2->get_attributes().find("group") == i2->get_attributes().end())
              throw load_error("a group node must have a group attribute", *i2);

            if (i2->get_attributes().find("count") == i2->get_attributes().end())
              throw load_error("a group node must have a count attribute", *i2);

            cnt = atoi(i2->get_attributes().find("count")->get_value());
            grp = atoi(i2->get_attributes().find("group")->get_value());

            shapes.rbegin()->addGroup(grp, cnt);
          }
        }
      }

  it = node.find("result");
  if (it != node.end()) {
    if (it->get_attributes().find("id") == it->get_attributes().end())
      throw load_error("the result node must have an 'id' attribute", *it);

    result = atoi(it->get_attributes().find("id")->get_value());
  }

  it = node.find("solutions");
  if (it != node.end()) {
    for (xml::node::const_iterator i = it->begin(); i != it->end(); i++)
      if ((i->get_type() == xml::node::type_element) &&
          (strcmp(i->get_name(), "solution") == 0))
        solutions.push_back(new solution_c(*i, pieces));
  }

  if (node.get_attributes().find("state") != node.get_attributes().end())
    solveState = (puzzle_c::SolveState_e)atoi(node.get_attributes().find("state")->get_value());
  else
    solveState = puzzle_c::SS_UNSOLVED;

  if (node.get_attributes().find("assemblies") != node.get_attributes().end())
    numAssemblies = atoi(node.get_attributes().find("assemblies")->get_value());
  else
    numAssemblies = 0xFFFFFFFF;

  if (node.get_attributes().find("solutions") != node.get_attributes().end())
    numSolutions = atoi(node.get_attributes().find("solutions")->get_value());
  else
    numSolutions = 0xFFFFFFFF;

  if (node.get_attributes().find("time") != node.get_attributes().end())
    usedTime = atoi(node.get_attributes().find("time")->get_value());
  else
    usedTime = 0xFFFFFFFF;

  it = node.find("bitmap");
  if (it != node.end())
    colorConstraints.load(*it);

  it = node.find("assembler");
  if ((it != node.end()) && (it->get_attributes().find("version") != it->get_attributes().end())) {
    assemblerState = it->get_content();
    assemblerVersion = it->get_attributes().find("version")->get_value();
  }
}

void problem_c::shapeIdRemoved(unsigned short idx) {

  if (result == idx)
    result = 0xFFFFFFFF;

  shapes.erase(remove_if(shapes.begin(), shapes.end(), shape_id_removed(idx)), shapes.end());

  for (unsigned int i = 0; i < shapes.size(); i++)
    if (shapes[i].shapeId > idx) shapes[i].shapeId--;
}


/************** PUZZLE ****************/

puzzle_c::puzzle_c(void) {
}


puzzle_c::puzzle_c(const puzzle_c * orig) {

  for (unsigned int i = 0; i < orig->shapes.size(); i++)
    shapes.push_back(new voxel_c(orig->shapes[i]));

  for (unsigned int i = 0; i < orig->problems.size(); i++)
    problems.push_back(new problem_c(orig->problems[i]));

  for (unsigned int i = 0; i < orig->colors.size(); i++)
    colors.push_back(orig->colors[i]);

  designer = orig->designer;
  comment = orig->comment;
}


puzzle_c::~puzzle_c(void) {

  for (unsigned int i = 0; i < shapes.size(); i++)
    delete shapes[i];

  for (unsigned int i = 0; i < problems.size(); i++)
    delete problems[i];
}

void puzzle_c::orthogonalize(void) {
  // for the moment a simple version not looking for rotated pieces

/* FIXME
  int p = 1;
  bool found;

  while (p < getShapeNumber()) {

    found = 0;

    for (int p2 = 0; p2 < p; p2++)
      if (*(shapes[p].piece) == *(shapes[p2].piece)) {
        shapes[p2].count++;
//        shapes.erase(p);  FIXME
        found = true;
        break;
      }

    if (!found)
      p++;
      }
*/
}

void puzzle_c::addColor(unsigned char r, unsigned char g, unsigned char b) {

  colorDef c;

  c.r = r;
  c.g = g;
  c.b = b;

  colors.push_back(c);

  // go through all problems and add the new color to the matrix
  for (vector<problem_c*>::iterator i = problems.begin(); i != problems.end(); i++)
    (*i)->colorConstraints.add();
}

void puzzle_c::removeColor(unsigned int col) {

  bt_assert(col <= colors.size());
  colors.erase(colors.begin() + (col - 1));

  // go through all shapes and remove the deleted color
  for (vector<voxel_c*>::iterator i = shapes.begin(); i != shapes.end(); i++)
    for (unsigned int p = 0; p < (*i)->getXYZ(); p++)
      if ((*i)->getState(p) != voxel_c::VX_EMPTY) {
        if ((*i)->getColor(p) == col)
          (*i)->setColor(p, 0);
        else if ((*i)->getColor(p) > col)
          (*i)->setColor(p, (*i)->getColor(p)-1);
      }

  // FIXME, remove color constrains and shift the colors inside the shapes
  for (vector<problem_c*>::iterator i = problems.begin(); i != problems.end(); i++)
    (*i)->colorConstraints.remove(col);
}

void puzzle_c::changeColor(unsigned int idx, unsigned char r, unsigned char g, unsigned char b) {

  bt_assert(idx < colors.size());

  colors[idx].r = r;
  colors[idx].g = g;
  colors[idx].b = b;
}

void puzzle_c::getColor(unsigned int idx, unsigned char * r, unsigned char * g, unsigned char * b) const {

  bt_assert(idx < colors.size());

  *r = colors[idx].r;
  *g = colors[idx].g;
  *b = colors[idx].b;
}

unsigned int puzzle_c::colorNumber(void) const {
  return colors.size();
}


void puzzle_c::probAllowPlacement(unsigned int prob, unsigned int pc, unsigned int res) {
  bt_assert(prob < problems.size());
  bt_assert(pc <= colors.size());
  bt_assert(res <= colors.size());

  if ((pc == 0) || (res == 0))
    return;

  problems[prob]->colorConstraints.set(pc-1, res-1, true);
}

void puzzle_c::probDisallowPlacement(unsigned int prob, unsigned int pc, unsigned int res) {
  bt_assert(prob < problems.size());
  bt_assert(pc <= colors.size());
  bt_assert(res <= colors.size());

  if ((pc == 0) || (res == 0))
    return;

  problems[prob]->colorConstraints.set(pc-1, res-1, false);
}

bool puzzle_c::probPlacementAllowed(unsigned int prob, unsigned int pc, unsigned int res) const {
  bt_assert(prob < problems.size());
  bt_assert(pc <= colors.size());
  bt_assert(res <= colors.size());

  if (colors.size() == 0)
    return true;

  return (pc == 0) || (res == 0) || problems[prob]->colorConstraints.get(pc-1, res-1);
}

xml::node puzzle_c::save(void) const {

  xml::node nd("puzzle");
  nd.get_attributes().insert("version", "1");

  char tmp[50];

  xml::node::iterator it;

  it = nd.insert(xml::node("colors"));
  for (unsigned int i = 0; i < colors.size(); i++) {
    xml::node::iterator it2 = it->insert(xml::node("color"));

    snprintf(tmp, 50, "%i", colors[i].r);
    it2->get_attributes().insert("red", tmp);

    snprintf(tmp, 50, "%i", colors[i].g);
    it2->get_attributes().insert("green", tmp);

    snprintf(tmp, 50, "%i", colors[i].b);
    it2->get_attributes().insert("blue", tmp);
  }

  it = nd.insert(xml::node("shapes"));
  for (unsigned int i = 0; i < shapes.size(); i++)
    it->insert(shapes[i]->save());

  it = nd.insert(xml::node("problems"));
  for (unsigned int i = 0; i < problems.size(); i++)
    it->insert(problems[i]->save());

  if (designer.length())
    it = nd.insert(xml::node("designer", designer.c_str()));

  if (comment.length())
    it = nd.insert(xml::node("comment", comment.c_str()));

  return nd;
}

puzzle_c::puzzle_c(const xml::node & node) {

  if ((node.get_type() != xml::node::type_element) ||
      (strcmp(node.get_name(), "puzzle") != 0))
    throw load_error("not the right node type for the puzzle", node);

  if (node.get_attributes().find("version") == node.get_attributes().end())
    throw load_error("puzzle node must have a 'version' attribute", node);

  if (atoi(node.get_attributes().find("version")->get_value()) != 1)
    throw load_error("puzzle nodes must have version 1", node);

  xml::node::const_iterator it;

  it = node.find("colors");
  if (it != node.end()) {
    for (xml::node::const_iterator i = it->begin(); i != it->end(); i++) {
      if ((i->get_type() == xml::node::type_element) &&
          (strcmp(i->get_name(), "color") == 0)) {

        if (i->get_attributes().find("red") == i->get_attributes().end())
          throw load_error("color nodes must have a 'red' attribute", *i);

        if (i->get_attributes().find("green") == i->get_attributes().end())
          throw load_error("color nodes must have a 'gree' attribute", *i);

        if (i->get_attributes().find("blue") == i->get_attributes().end())
          throw load_error("color nodes must have a 'blue' attribute", *i);

        colorDef c;

        c.r = atoi(i->get_attributes().find("red")->get_value());
        c.g = atoi(i->get_attributes().find("green")->get_value());
        c.b = atoi(i->get_attributes().find("blue")->get_value());

        colors.push_back(c);
      }
    }
  }

  it = node.find("shapes");
  if (it != node.end())
    for (xml::node::const_iterator i = it->begin(); i != it->end(); i++)
      if ((i->get_type() == xml::node::type_element) &&
          (strcmp(i->get_name(), "voxel") == 0))
        shapes.push_back(new voxel_c(*i));

  it = node.find("problems");
  if (it != node.end())
    for (xml::node::const_iterator i = it->begin(); i != it->end(); i++)
      if ((i->get_type() == xml::node::type_element) &&
          (strcmp(i->get_name(), "problem") == 0))
        problems.push_back(new problem_c(*i, colors.size(), shapes.size()));

  it = node.find("designer");
  if (it != node.end() && it->get_type() == xml::node::type_text)
    designer = it->get_content();

  it = node.find("comment");
  if (it != node.end() && it->get_type() == xml::node::type_text)
    comment = it->get_content();
}



unsigned int puzzle_c::addShape(voxel_c * p) {
  shapes.push_back(p);
  return shapes.size()-1;
}

/* add empty shape of given size */
unsigned int puzzle_c::addShape(int sx, int sy, int sz) {
  shapes.push_back(new voxel_c(sx, sy, sz, voxel_c::VX_EMPTY));
  return shapes.size()-1;
}

/* return the pointer to voxel space with the id */
const voxel_c * puzzle_c::getShape(unsigned int idx) const {
  bt_assert(idx < shapes.size());
  return shapes[idx];
}


voxel_c * puzzle_c::getShape(unsigned int idx) {
  bt_assert(idx < shapes.size());
  return shapes[idx];
}


/* template unary function to delete an object */
template <class T>
inline void deallocate(T * p) { ::operator delete (p); }


class remove_deleted_shape {
public:
  int idx;
  remove_deleted_shape(int i) : idx(i) {}
  void operator()(problem_c *p) { p->shapeIdRemoved(idx); }
};

/* remove the num-th shape
 * be careful this changes all ids and so all problems must be updated
 */
void puzzle_c::removeShape(unsigned int idx) {
  vector<voxel_c*>::iterator i(shapes.begin()+idx);
  delete *i;
  shapes.erase(i);

  /* now remove the shapes from the problem shape list, if that is the one that got deleted */
  for_each(problems.begin(), problems.end(), remove_deleted_shape(idx));
}

/* return how many shapes there are */
unsigned int puzzle_c::shapeNumber(void) const { return shapes.size(); }

/**
 * similar functions for problems
 */
unsigned int puzzle_c::addProblem(void) {
  problems.push_back(new problem_c(colorNumber()));
  return problems.size()-1;
}

/* return number of problems */
unsigned int puzzle_c::problemNumber(void) const { return problems.size(); }

/* remove one problem */
void puzzle_c::removeProblem(unsigned int idx) {
  vector<problem_c*>::iterator i(problems.begin() + idx);
  delete *i;
  problems.erase(i);
}

unsigned int puzzle_c::copyProblem(unsigned int prob) {

  bt_assert(prob < problems.size());

  problems.push_back(new problem_c(problems[prob]));

  return problems.size()-1;
}


/* set the shape-id for the result shape this the problem */
void puzzle_c::probSetResult(unsigned int prob, unsigned int shape) {
  bt_assert(prob < problems.size());

  problems[prob]->result = shape;
}

/* get the id for the result shape */
unsigned int puzzle_c::probGetResult(unsigned prob) const {
  bt_assert(prob < problems.size());

  return problems[prob]->result;
}

/* get the result shape voxel space */
const voxel_c * puzzle_c::probGetResultShape(unsigned int prob) const {
  bt_assert(prob < problems.size());
  bt_assert(problems[prob]->result < shapes.size());

  return shapes[problems[prob]->result];
}

voxel_c * puzzle_c::probGetResultShape(unsigned int prob) {
  bt_assert(prob < problems.size());
  bt_assert(problems[prob]->result < shapes.size());

  return shapes[problems[prob]->result];
}

/* add a shape to the pieces of the problem */
void puzzle_c::probAddShape(unsigned int prob, unsigned int shape, unsigned int count) {
  bt_assert(prob < problems.size());

  problems[prob]->shapes.push_back(problem_c::shape_c(shape, count, 0));
}

/* change the instance count for one shape of the problem */
void puzzle_c::probSetShapeCount(unsigned int prob, unsigned int shapeID, unsigned int count) {
  bt_assert(prob < problems.size());
  bt_assert(shapeID < problems[prob]->shapes.size());

  problems[prob]->shapes[shapeID].count = count;
}

/* remove the shape from the problem */
void puzzle_c::probRemoveShape(unsigned int prob, unsigned int shapeID) {
  bt_assert(prob < problems.size());
  bt_assert(shapeID < problems[prob]->shapes.size());

  problems[prob]->shapes.erase(problems[prob]->shapes.begin() + shapeID);
}

/* return the number of shapes in the problem */
unsigned int puzzle_c::probShapeNumber(unsigned int prob) const {
  bt_assert(prob < problems.size());
  return problems[prob]->shapes.size();
}

/* return the number of pieces in the problem (sum of all counts of all shapes */
unsigned int puzzle_c::probPieceNumber(unsigned int prob) const {
  bt_assert(prob < problems.size());

  unsigned int result = 0;

  for (vector<problem_c::shape_c>::iterator i = problems[prob]->shapes.begin(); i != problems[prob]->shapes.end(); i++)
    result += i->count;

  return result;
}

/* return the shape id of the given shape (index into the shape array of the puzzle */
unsigned int puzzle_c::probGetShape(unsigned int prob, unsigned int shapeID) const {
  bt_assert(prob < problems.size());
  bt_assert(shapeID < problems[prob]->shapes.size());

  return problems[prob]->shapes[shapeID].shapeId;
}

bool puzzle_c::probContainsShape(unsigned int prob, unsigned int shape) const {
  bt_assert(prob < problems.size());

  if (problems[prob]->result == shape)
    return true;

  for (vector<problem_c::shape_c>::iterator i = problems[prob]->shapes.begin(); i != problems[prob]->shapes.end(); i++)
    if (i->shapeId == shape)
      return true;

  return false;
}

const voxel_c * puzzle_c::probGetShapeShape(unsigned int prob, unsigned int shapeID) const {
  bt_assert(prob < problems.size());
  bt_assert(shapeID < problems[prob]->shapes.size());
  bt_assert(problems[prob]->shapes[shapeID].shapeId < shapes.size());

  return shapes[problems[prob]->shapes[shapeID].shapeId];
}

voxel_c * puzzle_c::probGetShapeShape(unsigned int prob, unsigned int shapeID) {
  bt_assert(prob < problems.size());
  bt_assert(shapeID < problems[prob]->shapes.size());
  bt_assert(problems[prob]->shapes[shapeID].shapeId < shapes.size());

  return shapes[problems[prob]->shapes[shapeID].shapeId];
}

/* return the instance count for one shape of the problem */
unsigned int puzzle_c::probGetShapeCount(unsigned int prob, unsigned int shapeID) const {
  bt_assert(prob < problems.size());
  bt_assert(shapeID < problems[prob]->shapes.size());

  return problems[prob]->shapes[shapeID].count;
}

void puzzle_c::probSetName(unsigned int prob, std::string name) {
  problems[prob]->name = name;
}

const std::string & puzzle_c::probGetName(unsigned int prob) const {
  return problems[prob]->name;
}


void puzzle_c::probAddSolution(unsigned int prob, assembly_c * voxel) {
  bt_assert(prob < problems.size());
  bt_assert(problems[prob]->assm);
  problems[prob]->solutions.push_back(new solution_c(voxel, 0));
  problems[prob]->solveState = SS_SOLVING;
}

void puzzle_c::probAddSolution(unsigned int prob, assembly_c * voxel, separation_c * tree) {
  bt_assert(prob < problems.size());
  bt_assert(problems[prob]->assm);

  // find the place to insert and insert the new solution so that
  // they are sorted by the complexity of the disassembly

  unsigned int s = tree->sumMoves();
  bool ins = false;

  for (unsigned int i = 0; i < problems[prob]->solutions.size(); i++)
    if (problems[prob]->solutions[i]->tree->sumMoves() > s) {
      problems[prob]->solutions.insert(problems[prob]->solutions.begin()+i, new solution_c(voxel, tree));
      ins = true;
      break;
    }

  if (!ins)
    problems[prob]->solutions.push_back(new solution_c(voxel, tree));

  problems[prob]->solveState = SS_SOLVING;
}

void puzzle_c::probRemoveAllSolutions(unsigned int prob) {
  bt_assert(prob < problems.size());
  for_each(problems[prob]->solutions.begin(), problems[prob]->solutions.end(), deallocate<solution_c>);
  problems[prob]->solutions.clear();
  delete problems[prob]->assm;
  problems[prob]->assm = 0;
  problems[prob]->assemblerState = "";
  problems[prob]->solveState = SS_UNSOLVED;
  problems[prob]->numAssemblies = 0xFFFFFFFF;
  problems[prob]->numSolutions = 0xFFFFFFFF;
  problems[prob]->usedTime = 0xFFFFFFFF;
}

puzzle_c::SolveState_e puzzle_c::probGetSolveState(unsigned int prob) const {
  bt_assert(prob < problems.size());
  return problems[prob]->solveState;
}

bool puzzle_c::probNumAssembliesKnown(unsigned int prob) const {
  bt_assert(prob < problems.size());
  return problems[prob]->numAssemblies != 0xFFFFFFFF;
}

bool puzzle_c::probNumSolutionsKnown(unsigned int prob) const {
  bt_assert(prob < problems.size());
  return problems[prob]->numSolutions != 0xFFFFFFFF;
}



unsigned long puzzle_c::probGetNumAssemblies(unsigned int prob) const {
  bt_assert(prob < problems.size());
  return problems[prob]->numAssemblies;
}

unsigned long puzzle_c::probGetNumSolutions(unsigned int prob) const {
  bt_assert(prob < problems.size());
  return problems[prob]->numSolutions;
}

void puzzle_c::probAddTime(unsigned int prob, unsigned long time) {
  bt_assert(prob < problems.size());
  if (problems[prob]->usedTime != 0xFFFFFFFF)
    problems[prob]->usedTime += time;
  else
    problems[prob]->usedTime = time;
}

unsigned long puzzle_c::probGetUsedTime(unsigned int prob) const {
  bt_assert(prob < problems.size());
  return problems[prob]->usedTime;
}

bool puzzle_c::probUsedTimeKnown(unsigned int prob) const {
  bt_assert(prob < problems.size());
  return problems[prob]->usedTime != 0xFFFFFFFF;
}


void puzzle_c::probIncNumAssemblies(unsigned int prob) {
  if (problems[prob]->numAssemblies == 0xFFFFFFFF)
    problems[prob]->numAssemblies = 1;
  else
    problems[prob]->numAssemblies++;
}
void puzzle_c::probIncNumSolutions(unsigned int prob) {
  if (problems[prob]->numSolutions == 0xFFFFFFFF)
    problems[prob]->numSolutions = 1;
  else
    problems[prob]->numSolutions++;
}

void puzzle_c::probFinishedSolving(unsigned int prob) {
  problems[prob]->solveState = SS_SOLVED;
}

unsigned int puzzle_c::probSolutionNumber(unsigned int prob) const {
  bt_assert(prob < problems.size());
  return problems[prob]->solutions.size();
}

assembly_c * puzzle_c::probGetAssembly(unsigned int prob, unsigned int sol) {
  bt_assert(prob < problems.size());
  bt_assert(sol < problems[prob]->solutions.size());

  return problems[prob]->solutions[sol]->assembly;
}

const assembly_c * puzzle_c::probGetAssembly(unsigned int prob, unsigned int sol) const {
  bt_assert(prob < problems.size());
  bt_assert(sol < problems[prob]->solutions.size());

  return problems[prob]->solutions[sol]->assembly;
}

separation_c * puzzle_c::probGetDisassembly(unsigned int prob, unsigned int sol) {
  bt_assert(prob < problems.size());
  bt_assert(sol < problems[prob]->solutions.size());

  return problems[prob]->solutions[sol]->tree;
}

const separation_c * puzzle_c::probGetDisassembly(unsigned int prob, unsigned int sol) const {
  bt_assert(prob < problems.size());
  bt_assert(sol < problems[prob]->solutions.size());

  return problems[prob]->solutions[sol]->tree;
}

assembler_c::errState puzzle_c::probSetAssembler(unsigned int prob, assembler_c * assm) {
  bt_assert(prob < problems.size());

  if (problems[prob]->assemblerState.length()) {

    // if we have some assembler position data, try to load that
    assembler_c::errState err = assm->setPosition(problems[prob]->assemblerState.c_str(), problems[prob]->assemblerVersion.c_str());

    // and remove that data
    problems[prob]->assemblerState = "";

    // when we could not load, return with error and reset to unsovled
    if (err != assembler_c::ERR_NONE) {
      problems[prob]->solveState = SS_UNSOLVED;
      return err;
    }

  } else {

    // if no data is available for assemlber restoration reset counters
    problems[prob]->numAssemblies = 0xFFFFFFFF;
    problems[prob]->numSolutions = 0xFFFFFFFF;
  }

  problems[prob]->assm = assm;
  problems[prob]->solveState = SS_SOLVING;
  return assembler_c::ERR_NONE;
}

assembler_c * puzzle_c::probGetAssembler(unsigned int prob) {
  bt_assert(prob < problems.size());
  return problems[prob]->assm;
}

const assembler_c * puzzle_c::probGetAssembler(unsigned int prob) const {
  bt_assert(prob < problems.size());
  return problems[prob]->assm;
}

void puzzle_c::probSetShapeGroup(unsigned int prob, unsigned int shapeID, unsigned short group, unsigned short count) {
  bt_assert(prob < problems.size());
  bt_assert(shapeID < problems[prob]->shapes.size());

  // not first look, if we already have this group number in our list
  for (unsigned int i = 0; i < problems[prob]->shapes[shapeID].groups.size(); i++)
    if (problems[prob]->shapes[shapeID].groups[i].group == group) {

      /* if we change count, change it, if we set count to 0 remove that entry */
      if (count)
        problems[prob]->shapes[shapeID].groups[i].count = count;
      else
        problems[prob]->shapes[shapeID].groups.erase(problems[prob]->shapes[shapeID].groups.begin()+i);
      return;
    }

  // not found add, but only groups not equal to 0
  if (group)
    problems[prob]->shapes[shapeID].addGroup(group, count);
}

unsigned short puzzle_c::probGetShapeGroupNumber(unsigned int prob, unsigned int shapeID) const {

  bt_assert(prob < problems.size());
  bt_assert(shapeID < problems[prob]->shapes.size());

  return problems[prob]->shapes[shapeID].groups.size();
}

unsigned short puzzle_c::probGetShapeGroup(unsigned int prob, unsigned int shapeID, unsigned int groupID) const {
  bt_assert(prob < problems.size());
  bt_assert(shapeID < problems[prob]->shapes.size());

  return problems[prob]->shapes[shapeID].groups[groupID].group;
}

unsigned short puzzle_c::probGetShapeGroupCount(unsigned int prob, unsigned int shapeID, unsigned int groupID) const {
  bt_assert(prob < problems.size());
  bt_assert(shapeID < problems[prob]->shapes.size());

  return problems[prob]->shapes[shapeID].groups[groupID].count;
}

