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
#include "puzzle.h"

#include "problem.h"
#include "voxel.h"

#include <xmlwrapp/attributes.h>
#include <vector>
#include <stdio.h>

puzzle_c::puzzle_c(const puzzle_c * orig) {

  gt = new gridType_c(*orig->gt);

  for (unsigned int i = 0; i < orig->shapes.size(); i++)
    shapes.push_back(gt->getVoxel(orig->shapes[i]));

  for (unsigned int i = 0; i < orig->problems.size(); i++)
    problems.push_back(new problem_c(orig->problems[i]));

  for (unsigned int i = 0; i < orig->colors.size(); i++)
    colors.push_back(orig->colors[i]);

  comment = orig->comment;
  commentPopup = orig->commentPopup;
}

puzzle_c::~puzzle_c(void) {

  for (unsigned int i = 0; i < shapes.size(); i++)
    delete shapes[i];

  for (unsigned int i = 0; i < problems.size(); i++)
    delete problems[i];

  delete gt;
}

unsigned int puzzle_c::addColor(unsigned char r, unsigned char g, unsigned char b) {
  colors.push_back(r | (uint32_t)g << 8 | (uint32_t)b << 16);
  return colors.size()-1;
}

void puzzle_c::removeColor(unsigned int col) {

  bt_assert(col <= colors.size());

  // go through all shapes and remove the deleted colour
  for (unsigned int i = 0; i < shapes.size(); i++)
    for (unsigned int p = 0; p < shapes[i]->getXYZ(); p++)
      if (shapes[i]->getState(p) != voxel_c::VX_EMPTY) {
        if (shapes[i]->getColor(p) == col)
          shapes[i]->setColor(p, 0);
        else if (shapes[i]->getColor(p) > col)
          shapes[i]->setColor(p, shapes[i]->getColor(p)-1);
      }

  // remove colour constraint rules that include this colour
  for (unsigned int i = 0; i < problems.size(); i++) {
    for (unsigned int c = 0; c <= colors.size(); c++) {
      problems[i]->disallowPlacement(col, c);
      problems[i]->disallowPlacement(c, col);
    }

    /* we need to decrease the colors with numbers > col */
    for (unsigned int c1 = col+1; c1 <= colors.size(); c1++)
      for (unsigned int c2 = 0;  c2 <= colors.size(); c2++)
        if (problems[i]->placementAllowed(c1, c2)) {
          problems[i]->disallowPlacement(c1, c2);
          problems[i]->allowPlacement(c1-1, c2);
        }

    for (unsigned int c1 = col+1; c1 <= colors.size(); c1++)
      for (unsigned int c2 = 0;  c2 <= colors.size(); c2++)
        if (problems[i]->placementAllowed(c2, c1)) {
          problems[i]->disallowPlacement(c2, c1);
          problems[i]->allowPlacement(c2, c1-1);
        }
  }

  colors.erase(colors.begin() + (col - 1));
}

void puzzle_c::changeColor(unsigned int idx, unsigned char r, unsigned char g, unsigned char b) {

  bt_assert(idx < colors.size());

  colors[idx] = r || (uint32_t)g << 8 | (uint32_t)b << 16;
}

void puzzle_c::getColor(unsigned int idx, unsigned char * r, unsigned char * g, unsigned char * b) const {

  bt_assert(idx < colors.size());

  *r = (colors[idx] >>  0) & 0xFF;
  *g = (colors[idx] >>  8) & 0xFF;
  *b = (colors[idx] >> 16) & 0xFF;
}

xml::node puzzle_c::save(void) const {

  xml::node nd("puzzle");
  nd.get_attributes().insert("version", "2");

  char tmp[50];

  xml::node::iterator it;

  nd.insert(gt->save());

  it = nd.insert(xml::node("colors"));
  for (unsigned int i = 0; i < colors.size(); i++) {
    xml::node::iterator it2 = it->insert(xml::node("color"));

    snprintf(tmp, 50, "%i", (colors[i] >>  0) & 0xFF);
    it2->get_attributes().insert("red", tmp);

    snprintf(tmp, 50, "%i", (colors[i] >>  8) & 0xFF);
    it2->get_attributes().insert("green", tmp);

    snprintf(tmp, 50, "%i", (colors[i] >> 16) & 0xFF);
    it2->get_attributes().insert("blue", tmp);
  }

  it = nd.insert(xml::node("shapes"));
  for (unsigned int i = 0; i < shapes.size(); i++)
    it->insert(shapes[i]->save());

  it = nd.insert(xml::node("problems"));
  for (unsigned int i = 0; i < problems.size(); i++)
    it->insert(problems[i]->save());

  if (comment.length()) {
    it = nd.insert(xml::node("comment", comment.c_str()));
    if (commentPopup)
      it->get_attributes().insert("popup", "");
  }

  return nd;
}

puzzle_c::puzzle_c(const xml::node & node) {

  if ((node.get_type() != xml::node::type_element) ||
      (strcmp(node.get_name(), "puzzle") != 0))
    throw load_error("not the right node type for the puzzle", node);

  if (node.get_attributes().find("version") == node.get_attributes().end())
    throw load_error("puzzle node must have a 'version' attribute", node);

  unsigned int version;
  version = atoi(node.get_attributes().find("version")->get_value());

  if ((version != 1) && (version != 2))
    throw load_error("can only load files of version 1 and 2", node);

  xml::node::const_iterator it;

  it = node.find("gridType");
  if (it != node.end())
    gt = new gridType_c(*it);
  else
    // this creates a grid type for cubes, so all puzzle filed
    // that do contain no grid type definition are build out of cubes
    gt = new gridType_c();

  it = node.find("colors");
  if (it != node.end()) {
    for (xml::node::const_iterator i = it->begin(); i != it->end(); i++) {
      if ((i->get_type() == xml::node::type_element) &&
          (strcmp(i->get_name(), "color") == 0)) {

        if (i->get_attributes().find("red") == i->get_attributes().end())
          throw load_error("color nodes must have a 'red' attribute", *i);

        if (i->get_attributes().find("green") == i->get_attributes().end())
          throw load_error("color nodes must have a 'green' attribute", *i);

        if (i->get_attributes().find("blue") == i->get_attributes().end())
          throw load_error("color nodes must have a 'blue' attribute", *i);


        colors.push_back(
            ((uint32_t)atoi(i->get_attributes().find(  "red")->get_value()) & 0xFF <<  0) ||
            ((uint32_t)atoi(i->get_attributes().find("green")->get_value()) & 0xFF <<  8) ||
            ((uint32_t)atoi(i->get_attributes().find( "blue")->get_value()) & 0xFF << 16));
      }
    }
  }

  it = node.find("shapes");
  if (it != node.end())
    for (xml::node::const_iterator i = it->begin(); i != it->end(); i++)
      if ((i->get_type() == xml::node::type_element) &&
          (strcmp(i->get_name(), "voxel") == 0))
        shapes.push_back(gt->getVoxel(*i));

  it = node.find("problems");
  if (it != node.end())
    for (xml::node::const_iterator i = it->begin(); i != it->end(); i++)
      if ((i->get_type() == xml::node::type_element) &&
          (strcmp(i->get_name(), "problem") == 0))
        problems.push_back(new problem_c(*this, *i));

  it = node.find("comment");
  if (it != node.end() && it->get_type() == xml::node::type_element) {
    comment = it->get_content();
    commentPopup = it->get_attributes().find("popup") != it->get_attributes().end();
  } else
    commentPopup = false;
}

unsigned int puzzle_c::addShape(voxel_c * p) {
  shapes.push_back(p);
  return shapes.size()-1;
}

/* add empty shape of given size */
unsigned int puzzle_c::addShape(unsigned int sx, unsigned int sy, unsigned int sz) {
  shapes.push_back(gt->getVoxel(sx, sy, sz, voxel_c::VX_EMPTY, voxel_c::VX_EMPTY));
  return shapes.size()-1;
}

/* remove the num-th shape
 * be careful this changes all ids and so all problems must be updated
 */
void puzzle_c::removeShape(unsigned int idx) {
  bt_assert(idx < shapes.size());
  delete shapes[idx];
  shapes.erase(shapes.begin()+idx);

  /* now remove the shapes from the problem shape list, if that is the one that got deleted */
  for (unsigned int i = 0; i < problems.size(); i++)
    problems[i]->shapeIdRemoved(idx);
}

void puzzle_c::exchangeShape(unsigned int s1, unsigned int s2) {
  bt_assert(s1 < shapes.size());
  bt_assert(s2 < shapes.size());

  voxel_c * v = shapes[s1];
  shapes[s1] = shapes[s2];
  shapes[s2] = v;

  for (unsigned int i = 0; i < problems.size(); i++)
    problems[i]->exchangeShapeId(s1, s2);
}

/**
 * similar functions for problems
 */
unsigned int puzzle_c::addProblem(void) {
  problems.push_back(new problem_c(*this));
  return problems.size()-1;
}

/* remove one problem */
void puzzle_c::removeProblem(unsigned int idx) {
  bt_assert(idx < problems.size());
  delete problems[idx];
  problems.erase(problems.begin()+idx);
}

unsigned int puzzle_c::copyProblem(unsigned int prob) {

  bt_assert(prob < problems.size());

  problems.push_back(new problem_c(problems[prob]));

  return problems.size()-1;
}

void puzzle_c::exchangeProblem(unsigned int p1, unsigned int p2) {

  bt_assert(p1 < problems.size());
  bt_assert(p2 < problems.size());

  problem_c * p = problems[p1];
  problems[p1] = problems[p2];
  problems[p2] = p;
}

void puzzle_c::setGridType(gridType_c * newGt) {
  delete gt;
  gt = newGt;
}

