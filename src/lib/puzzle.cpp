/* Burr Solver
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
#include "puzzle.h"

#include "problem.h"
#include "voxel.h"
#include "xml.h"

#include <vector>
#include <stdio.h>
#include <algorithm>

puzzle_c::puzzle_c(const puzzle_c * orig) {

  gt = new gridType_c(*orig->gt);

  for (unsigned int i = 0; i < orig->shapes.size(); i++)
    shapes.push_back(gt->getVoxel(orig->shapes[i]));

  for (unsigned int i = 0; i < orig->problems.size(); i++)
    problems.push_back(new problem_c(orig->problems[i], *this));

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
  bt_assert(colors.size() < 63);  // only 63 colors are allowed, color 0 is special
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

  colors[idx] = r | (uint32_t)g << 8 | (uint32_t)b << 16;
}

void puzzle_c::getColor(unsigned int idx, unsigned char * r, unsigned char * g, unsigned char * b) const {

  bt_assert(idx < colors.size());

  *r = (colors[idx] >>  0) & 0xFF;
  *g = (colors[idx] >>  8) & 0xFF;
  *b = (colors[idx] >> 16) & 0xFF;
}

void puzzle_c::save(xmlWriter_c & xml) const
{
  xml.newTag("puzzle");
  xml.newAttrib("version", "2");

  gt->save(xml);

  xml.newTag("colors");
  for (unsigned int i = 0; i < colors.size(); i++)
  {
    xml.newTag("color");

    xml.newAttrib("red",   (colors[i] >>  0) & 0xFF);
    xml.newAttrib("green", (colors[i] >>  8) & 0xFF);
    xml.newAttrib("blue",  (colors[i] >> 16) & 0xFF);

    xml.endTag("color");
  }
  xml.endTag("colors");

  xml.newTag("shapes");
  for (unsigned int i = 0; i < shapes.size(); i++)
    shapes[i]->save(xml);
  xml.endTag("shapes");

  xml.newTag("problems");
  for (unsigned int i = 0; i < problems.size(); i++)
    problems[i]->save(xml);
  xml.endTag("problems");

  xml.newTag("comment");
  if (comment.length())
  {
    if (commentPopup)
      xml.newAttrib("popup", "");
    xml.addContent(comment);
  }
  xml.endTag("comment");

  xml.endTag("puzzle");
}

puzzle_c::puzzle_c(xmlParser_c & pars)
{
  pars.nextTag();

  pars.require(xmlParser_c::START_TAG, "puzzle");

  std::string versionStr = pars.getAttributeValue("version");
  if (!versionStr.length())
    pars.exception("puzzle node must have a 'version' attribute");

  unsigned int version = atoi(versionStr.c_str());

  if ((version != 1) && (version != 2))
    pars.exception("can only load files of version 1 and 2");

  gt = 0;
  commentPopup = false;

  do {
    int state = pars.nextTag();

    if (state == xmlParser_c::END_TAG) break;
    pars.require(xmlParser_c::START_TAG, "");

    if (pars.getName() == "gridType")
    {
      if (gt != 0)
        pars.exception("only one gridtype can be defined, and it must be before the first shape");

      gt = new gridType_c(pars);
    }
    else if (pars.getName() == "colors")
    {
      do
      {
        state = pars.nextTag();

        if (state == xmlParser_c::END_TAG) break;
	pars.require(xmlParser_c::START_TAG, "");

        if (pars.getName() == "color")
        {
          colors.push_back(
              (((uint32_t)atoi(pars.getAttributeValue("red").c_str()) & 0xFF) <<  0) |
              (((uint32_t)atoi(pars.getAttributeValue("green").c_str()) & 0xFF) <<  8) |
              (((uint32_t)atoi(pars.getAttributeValue("blue").c_str()) & 0xFF) << 16));
        }

        pars.skipSubTree();

      } while (true);

      pars.require(xmlParser_c::END_TAG, "colors");
    }
    else if (pars.getName() == "shapes")
    {
      // if no gridtype has been defined, we assume cubes
      if (!gt) gt = new gridType_c(gridType_c::GT_BRICKS);

      do
      {
        state = pars.nextTag();

        if (state == xmlParser_c::END_TAG) break;
	pars.require(xmlParser_c::START_TAG, "");

        if (pars.getName() == "voxel")
	{
          shapes.push_back(gt->getVoxel(pars));
	  pars.require(xmlParser_c::END_TAG, "voxel");
	}
        else
          pars.skipSubTree();

      } while (true);

      pars.require(xmlParser_c::END_TAG, "shapes");
    }
    else if (pars.getName() == "problems")
    {
      do
      {
        state = pars.nextTag();

        if (state == xmlParser_c::END_TAG) break;
        if (state != xmlParser_c::START_TAG)
          pars.exception("a new tag required, but something else found");

        if (pars.getName() == "problem")
	{
          problems.push_back(new problem_c(*this, pars));
 	  pars.require(xmlParser_c::END_TAG, "problem");
	}
        else
          pars.skipSubTree();

      } while (true);

      pars.require(xmlParser_c::END_TAG, "problems");
    }
    else if (pars.getName() == "comment")
    {
      for (int a = 0; a < pars.getAttributeCount(); a++)
        if (pars.getAttributeName(a) == "popup")
          commentPopup = true;

      state = pars.next();
      if (state == xmlParser_c::TEXT)
      {
        comment = pars.getText();
        state = pars.next();
      }

      pars.require(xmlParser_c::END_TAG, "comment");
    }
    else
      pars.skipSubTree();

    pars.require(xmlParser_c::END_TAG, "");

  } while (true);

  pars.require(xmlParser_c::END_TAG, "puzzle");
}

unsigned int puzzle_c::addShape(voxel_c * p) {
  bt_assert(gt->getType() == p->getGridType()->getType());
  shapes.push_back(p);
  return shapes.size()-1;
}

/* add empty shape of given size */
unsigned int puzzle_c::addShape(unsigned int sx, unsigned int sy, unsigned int sz) {
  shapes.push_back(gt->getVoxel(sx, sy, sz, voxel_c::VX_EMPTY));
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

unsigned int puzzle_c::addProblem(const problem_c * prob) {

  problems.push_back(new problem_c(prob, *this));

  return problems.size()-1;
}

/* remove one problem */
void puzzle_c::removeProblem(unsigned int idx) {
  bt_assert(idx < problems.size());
  delete problems[idx];
  problems.erase(problems.begin()+idx);
}

void puzzle_c::exchangeProblem(unsigned int p1, unsigned int p2) {

  bt_assert(p1 < problems.size());
  bt_assert(p2 < problems.size());

  problem_c * p = problems[p1];
  problems[p1] = problems[p2];
  problems[p2] = p;
}

