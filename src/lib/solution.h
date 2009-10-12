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
#ifndef __SOLUTION_H__
#define __SOLUTION_H__

class assembly_c;
class separation_c;
class separationInfo_c;
class xmlParser_c;
class xmlWriter_c;
class gridType_c;
class disassembly_c;

/**
 * This class stores the information for one solution for a
 * problem.
 */
class solution_c
{
  /* the assembly contains the pieces so that they
   * do assemble into the result shape */
  assembly_c * assembly;

  /* the disassembly tree, only not NULL, if we
   * have disassembled the puzzle
   */
  separation_c * tree;

  /* if no separation is given, maybe we have a separationInfo
   * that contains only some of the information of a full separation
   * but requires a lot less memory
   */
  separationInfo_c * treeInfo;

  /* as it is now possible to not save all solutions
   * it might be useful to know the exact number and sequence
   * how solutions were found
   *
   * solutionNum is 0, when no disassembly is known
   */
  unsigned int assemblyNum;
  unsigned int solutionNum;

public:

  /** create a solution with a proper separation */
  solution_c(assembly_c * assm, unsigned int assmNum, separation_c * t, unsigned int solNum) :
    assembly(assm), tree(t), treeInfo(0), assemblyNum(assmNum), solutionNum(solNum) {}

  /** create a solution with only separation information */
  solution_c(assembly_c * assm, unsigned int assmNum, separationInfo_c * ti, unsigned int solNum) :
    assembly(assm), tree(0), treeInfo(ti), assemblyNum(assmNum), solutionNum(solNum) {}

  /** creat a solution with assembly only, no disassembly */
  solution_c(assembly_c * assm, unsigned int assmNum) :
    assembly(assm), tree(0), treeInfo(0), assemblyNum(assmNum), solutionNum(0) {}

  /** load a solution from file */
  solution_c(xmlParser_c & pars, unsigned int pieces, const gridType_c * gt);

  ~solution_c(void);

  /** save the solution to the XML file */
  void save(xmlWriter_c & xml) const;

  /** get the assembly from this solution, it will always be not NULL */
  assembly_c * getAssembly(void) { return assembly; }
  const assembly_c * getAssembly(void) const { return assembly; }

  /** get the full disassembly or 0 if there is none */
  separation_c * getDisassembly(void) { return tree; }
  const separation_c * getDisassembly(void) const { return tree; }

  /** get either the disassembly or the disassembly information or nothing */
  disassembly_c * getDisassemblyInfo(void);
  const disassembly_c * getDisassemblyInfo(void) const;

  /** get the assembly number */
  unsigned int getAssemblyNumber(void) const { return assemblyNum; }
  /** get the solution number (0 if this solution has no disassembly */
  unsigned int getSolutionNumber(void) const { return solutionNum; }

  /** remove an existing disassembly, and replace it by disassembly information */
  void removeDisassembly(void);
  /** add a new disassembly to this solution, deleting an old one, in
   * case such an old exists
   */
  void setDisassembly(separation_c * sep);

  /** change the solution so that shape s1 and s2 are swapped */
  void exchangeShape(unsigned int s1, unsigned int s2);
};

#endif
