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
#include "solution.h"

#include "bt_assert.h"
#include "disassembly.h"
#include "assembly.h"

#include "../tools/xml.h"

#include <stdlib.h>

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

void solution_c::exchangeShape(unsigned int s1, unsigned int s2)
{
  if (assembly)
    assembly->exchangeShape(s1, s2);
  if (tree)
    tree->exchangeShape(s1, s2);
}

const disassembly_c * solution_c::getDisassemblyInfo(void) const
{
  if (tree) return tree;
  if (treeInfo) return treeInfo;
  return 0;
}

disassembly_c * solution_c::getDisassemblyInfo(void)
{
  if (tree) return tree;
  if (treeInfo) return treeInfo;
  return 0;
}

void solution_c::removeDisassembly(void)
{
  if (tree)
  {
    if (!treeInfo)
      treeInfo = new separationInfo_c(tree);

    delete tree;
    tree = 0;
  }
}

void solution_c::setDisassembly(separation_c * sep)
{
  if (tree) delete tree;
  tree = sep;

  if (treeInfo) delete treeInfo;
  treeInfo = 0;
}


void solution_c::removePieces(unsigned int start, unsigned int count)
{
  if (assembly)
    assembly->removePieces(start, count);
  if (tree)
    tree->removePieces(start, count);
}

void solution_c::addNonPlacedPieces(unsigned int start, unsigned int count)
{
  if (assembly)
    assembly->addNonPlacedPieces(start, count);
  if (tree)
    tree->addNonPlacedPieces(start, count);
}

