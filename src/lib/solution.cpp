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

solution_c::solution_c(std::unique_ptr<assembly_c> assm, unsigned int assmNum, std::unique_ptr<separation_c> t, unsigned int solNum) :
  assembly(std::move(assm)), tree(std::move(t)), treeInfo(nullptr), assemblyNum(assmNum), solutionNum(solNum) {}

solution_c::solution_c(assembly_c * assm, unsigned int assmNum, separation_c * t, unsigned int solNum) :
  assembly(assm), tree(t), treeInfo(nullptr), assemblyNum(assmNum), solutionNum(solNum) {}

solution_c::solution_c(std::unique_ptr<assembly_c> assm, unsigned int assmNum, std::unique_ptr<separationInfo_c> ti, unsigned int solNum) :
  assembly(std::move(assm)), tree(nullptr), treeInfo(std::move(ti)), assemblyNum(assmNum), solutionNum(solNum) {}

solution_c::solution_c(assembly_c * assm, unsigned int assmNum, separationInfo_c * ti, unsigned int solNum) :
  assembly(assm), tree(nullptr), treeInfo(ti), assemblyNum(assmNum), solutionNum(solNum) {}

solution_c::solution_c(std::unique_ptr<assembly_c> assm, unsigned int assmNum) :
  assembly(std::move(assm)), tree(nullptr), treeInfo(nullptr), assemblyNum(assmNum), solutionNum(0) {}

solution_c::solution_c(assembly_c * assm, unsigned int assmNum) :
  assembly(assm), tree(nullptr), treeInfo(nullptr), assemblyNum(assmNum), solutionNum(0) {}

solution_c::solution_c(solution_c &&) noexcept = default;
solution_c & solution_c::operator=(solution_c &&) noexcept = default;

solution_c::solution_c(xmlParser_c & pars, unsigned int pieces, const gridType_c * gt) :
  assembly(nullptr), tree(nullptr), treeInfo(nullptr), assemblyNum(0), solutionNum(0)
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
      assembly = std::make_unique<assembly_c>(pars, pieces, gt);
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
      tree = std::make_unique<separation_c>(pars, pl);

      pars.require(xmlParser_c::END_TAG, "separation");
    }
    else if (pars.getName() == "separationInfo")
    {
      treeInfo = std::make_unique<separationInfo_c>(pars);
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
    treeInfo.reset();
  }
}

void solution_c::save(xmlWriter_c & xml, bool includeRotationFields) const
{
  xml.newTag("solution");

  if (assemblyNum) {
    xml.newAttrib("asmNum", assemblyNum);
  }

  if ((tree || treeInfo) && solutionNum)
    xml.newAttrib("solNum", solutionNum);

  assembly->save(xml);

  if (tree) {            tree->save(xml, 0, includeRotationFields);
  } else if (treeInfo) { treeInfo->save(xml, includeRotationFields);
  }

  xml.endTag("solution");
}

solution_c::~solution_c(void) = default;

void solution_c::exchangeShape(unsigned int s1, unsigned int s2)
{
  if (assembly)
    assembly->exchangeShape(s1, s2);
  if (tree)
    tree->exchangeShape(s1, s2);
}

const disassembly_c * solution_c::getDisassemblyInfo(void) const
{
  if (tree) return tree.get();
  if (treeInfo) return treeInfo.get();
  return 0;
}

disassembly_c * solution_c::getDisassemblyInfo(void)
{
  if (tree) return tree.get();
  if (treeInfo) return treeInfo.get();
  return 0;
}

void solution_c::removeDisassembly(void)
{
  if (tree)
  {
    if (!treeInfo)
      treeInfo = std::make_unique<separationInfo_c>(tree.get());

    tree.reset();
  }
}

void solution_c::setDisassembly(separation_c * sep)
{
  tree.reset(sep);
  treeInfo.reset();
}

void solution_c::setDisassembly(std::unique_ptr<separation_c> sep)
{
  tree = std::move(sep);
  treeInfo.reset();
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

