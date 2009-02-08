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
#ifndef __PROBLEM_H__
#define __PROBLEM_H__

/** @ file
 * Contains the definitions for the puzzle problem class
 */
#include "assembler.h"
#include "bt_assert.h"

#include <xmlwrapp/node.h>

#include <vector>
#include <set>

class voxel_c;
class separation_c;
class separationInfo_c;
class assembly_c;
class gridType_c;
class puzzle_c;
class shape_c;
class solution_c;

/**
 * this state reflects how far we are with solving this problem
 */
typedef enum {
  SS_UNSOLVED,    // nothing done yet
  SS_SOLVING,     // started and not finished in this state assm must contain the assembler
  SS_SOLVED       // finished, the assembler has been destroyed and all the information is available
} solveState_e;


/**
 * This class defines a problem, if always has to belong to a puzzle and can not be
 * put somewhere else
 * A puzzle is a collection of shapes and a set of problems associated
 * with these shapes
 * each problem defines a solution shape and a set of pieces (multiple
 * occurrences are possible) that need to be assembles into the given solution
 * shape
 * the class also handles the solutions that belong to the problem
 */
class problem_c {

private:

  /**
   * the puzzle this problem belongs to, it must always be there
   */
  puzzle_c & puzzle;

  /**
   * the pieces for this problem and how many of the pieces
   * of each shape are there, the shape class contains indices into the
   * shape list of the puzzle and some counters, ...
   */
  std::vector<shape_c *> shapes;

  /**
   * the result shape shape as index into the puzzle shapes
   */
  unsigned int result;

  /**
   * (some of) the found solutions. Not all of even none might be
   * in this vector if the user decides to only count, or not keep them
   * all. This vector contains the solutions that were kept
   */
  std::vector<solution_c*> solutions;

  /**
   * this set contains the paits of colors that are allowed when a piece
   * is placed. The piece color is in the high 16 bits, the result color
   * in the lower 16. As right now only 64 colors are possible this will
   * allow enough space for the future
   */
  std::set<uint32_t> colorConstraints;

  /**
   * if we have started to solve this problem this pointer shows us the corresponding assembler
   * if the pointer is 0 we have never started an assembly process within this session
   * statistics can be found in the assembler, too
   */
  assembler_c * assm;

  /**
   * the name of the problem, so that the user can easily select one
   * out of a list with names
   */
  std::string name;

  /**
   * this state reflects how far we are with solving this problem
   */
  solveState_e solveState;

  /**
   * for this variables 0xFFFFFFFF always stands for unknown and
   * 0xFFFFFFFE for too much to count
   *
   * this is independent of the solutions vector, these are the pure numbers
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
   * the saves state can be restored
   */
  std::string assemblerVersion;

  /**
   * the time used up to get to the current state in the solving progress (in seconds)
   */
  unsigned long usedTime;

  /**
   * number of holes maximally allowed
   * this value *may* be used to limit the number of holes, if you have piece ranges
   * in your puzzle. As soon as there are no piece ranges, this value can be calculated
   * exaclty and there is no need to limit the number
   * the value 0xFFFFFFFF is used for undefined maximum number
   */
  unsigned int maxHoles;

public:

  /**
   * constructors:
   * - create new empty problem for the given puzzle
   * - load a problem from the given XML node for the given puzzlw
   * - copy the given problem, except for label and solutions
   */
  problem_c(puzzle_c & puz);
  problem_c(puzzle_c & puz, const xml::node & node);
  problem_c(const problem_c * prob, puzzle_c & puz);

  /**
   * destructor: free all ressources
   */
  ~problem_c(void);

  /**
   * save the problem into the returned XML node
   */
  xml::node save(void) const;

  /**
   * return the current set grid type for this puzzle
   * the grid type is taken from the puzzle this problem belongs to
   */
  const gridType_c * getGridType(void) const;
  gridType_c * getGridType(void);

  /**
   * get and set the name of the problem, keep is short because it is used
   * as the label for the problem
   */
  const std::string & getName(void) const { return name; }
  void setName(std::string nm) { name = nm; }

  /**
   * result shape handling:
   * - set the puzzle shape id for the result shape, be sure that it is valid in the puzzle
   *   before this function is called the result shape might be invalid
   * - check if there is a valid shape set
   * - get the puzzle shape id for the result shape of the problem
   * - get the voxel space of the result shape
   * be sure to only call getResult[Shape] when you know that the shape is valid
   */
  void setResult(unsigned int shape);
  bool resultInvalid(void) const;
  unsigned int getResult(void) const;
  const voxel_c * getResultShape(void) const;
  voxel_c * getResultShape(void);

  /**
   * problem piece handling:
   * a problem uses the shapes within the puzzle to define its pieces
   * (see the distinction, a shape is within the puzzle, a piece within the
   * problem, there can be several pieces of the same shape, ...)
   * each shape has a min and max count atatched to define how many times
   * this shape is (may) be used
   * - when a shape is removed from the puzzle this function is called, it
   *   removes all occurences of this shape in the problem and also updates
   *   the indices because the larger ones have shifted
   * - get and set the min and max values for a puzzle shape
   * - find out, if a puzzle shape is used in the problem (as piece or as result)
   * - how many different puzzle shapes have been used in this problem (its
   *   NOT the number of pieces in the problem
   * - getShapeMin Max similar as the getShapeCount functions but this time using
   *   the shape id instead of the puzzle shape
   * - getShape: get the puzzle shape for a problem shape id
   * - get the problem shape id for a puzzle shape, only call with actually
   *   used puzzle shapes
   * - get the shape for a given index within the problem, also get the
   *   voxel shape for that shape
   * - swap 2 pieces in the order of pieces of the problem
   * - 2 shapes have been swaped in the puzzle, swap them in the problem
   *   as well
   */
  void shapeIdRemoved(unsigned short idx);
  void setShapeCountMin(unsigned int shape, unsigned int count);
  void setShapeCountMax(unsigned int shape, unsigned int count);
  unsigned int getShapeCountMin(unsigned int shape) const;
  unsigned int getShapeCountMax(unsigned int shape) const;
  bool containsShape(unsigned int shape) const;
  unsigned int shapeNumber(void) const { return shapes.size(); }
  unsigned int getShapeMin(unsigned int shapeID) const;
  unsigned int getShapeMax(unsigned int shapeID) const;
  unsigned int getShape(unsigned int shapeID) const;
  unsigned int getShapeId(unsigned int shape) const;
  const voxel_c * getShapeShape(unsigned int shapeID) const;
  voxel_c * getShapeShape(unsigned int shapeID);
  void exchangeShape(unsigned int s1, unsigned int s2);
  void exchangeShapeId(unsigned int s1, unsigned int s2);

  /**
   * find out what puzzle shape is behind a piece number in a given problem
   * - find out how many pieces there are. This is the maximum valid value for the
   *   other 2 functions
   * - find out which shape id (index in the problem shapes is behind a given piece
   * - find out the how manieth of a shape one piece is
   */
  unsigned int pieceNumber(void) const;
  unsigned int pieceToShape(unsigned int piece) const;
  unsigned int pieceToSubShape(unsigned int piece) const;

  /**
   * edit color placement constraints:
   * the color 0 in this functions is always ignored as the placement
   * of color 0 is always possible
   * - allow placing pieces of one color into a result color
   * - disallow the placing
   * - check if placing is allowed
   */
  void allowPlacement(unsigned int pc, unsigned int res);
  void disallowPlacement(unsigned int pc, unsigned int res);
  bool placementAllowed(unsigned int pc, unsigned int res) const;

  /* these functions can be used to define groups that pieces belong to
   * this information is used when disassembling the puzzle, if all pieces
   * left in one clump belong to the same group the disassembler doesn't try
   * to continue disassembling
   * group 0 is different, it means that the piece doesn't belong to any
   * group and must be single
   * The problem with this are pieces with the same shape. Each of this piece
   * can belong into another group. But the assembler doesn't know about this
   * and will only return one assembly. So the disassembler has to decide
   * which piece belongs to what group
   * all the
   */
  void setShapeGroup(unsigned int shapeID, unsigned short group, unsigned short count);
  unsigned short getShapeGroupNumber(unsigned int shapeID) const;
  unsigned short getShapeGroup(unsigned int shapeID, unsigned int groupID) const;
  unsigned short getShapeGroupCount(unsigned int shapeID, unsigned int groupID) const;

  /**
   * functions to limit the number of holes a solution is allowed to have
   * - find out the the number is defined, if not there isno limit
   * - get the number (only when it is defined)
   * - set the number to a valid value
   * - invalidate the value
   */
  bool maxHolesDefined(void) const { return maxHoles != 0xFFFFFFFF; }
  unsigned int getMaxHoles(void) const { bt_assert(maxHoles != 0xFFFFFFFF); return maxHoles; }
  void setMaxHoles(unsigned int value) { bt_assert(value < 0xFFFFFFFF); maxHoles = value; }
  void setMaxHolesInvalid(void) { maxHoles = 0xFFFFFFFF; }

  /**
   * remove all known solutions, reset time, counter, assembler, ...
   * prepare for solving the problem
   * it also removes maybe saved assembler state so that solving starts
   * from the start
   */
  void removeAllSolutions(void);

  /**
   * functions used while solving the puzzle:
   * the assembler engine can be put into the problem to save it together
   * with the problem and later on resume, or even to just pause solving
   * when saved into XML only a string containing assembler dependent information
   * is saved, this information is resuurected when setting the assembler
   * - set the assembler, reset it to a saved state, when that information is
   *   available, if not simply set the assembler
   * - get the assembler to get statistics or whatever
   * - for each found assembly or solution (disassemable assembly) call incNum...
   * - add time use to solve the puzzle (in seconds) the value is added to the
   *   already accumulated time, or
   * - add solutions. There are several different possibilities for that, just an
   *   assembly, or with disassembly information (the first should be call just assembly
   *   if we want to stay to our definitions)
   *   for real solutions it is possible to gie an index where to insert it, to keep the
   *   found solutions sorted when the index is larger than the current list it is always
   *   added to the end
   * - once finished call finishedSolving, then adding of time and solutions is no longer
   *   allowed
   */
  assembler_c::errState setAssembler(assembler_c * assm);
  assembler_c * getAssembler(void) { return assm; }
  const assembler_c * getAssembler(void) const { return assm; }
  void incNumAssemblies(void) { bt_assert(solveState == SS_SOLVING); numAssemblies++; }
  void incNumSolutions(void) { bt_assert(solveState == SS_SOLVING); numSolutions++; }
  void addTime(unsigned long time) { bt_assert(solveState == SS_SOLVING); usedTime += time; }
  void addSolution(assembly_c * assm);
  void addSolution(assembly_c * assm, separationInfo_c * disasm, unsigned int pos = 0xFFFFFFFF);
  void addSolution(assembly_c * assm, separation_c * disasm, unsigned int pos = 0xFFFFFFFF);
  void finishedSolving(void) { solveState = SS_SOLVED; }

  /**
   * functions used after solving to get information:
   * - find out how far we are with solving (no, started, finished)
   * - find out if we may use the number of assemblies or solutions
   * - get number of assemblies and solutions, this is only valid if we already
   *   started solving
   * - same for time
   * - get number of solutions that were acutally saved
   * - get the assembly or disassembly information for one of the saved
   *   solutions, disassembly info is not the real disassembly but just some
   *   information about it
   * - get the number of the assembly and solution for the saved solution number x
   */
  solveState_e getSolveState(void) const { return solveState; }
  bool numAssembliesKnown(void) const { return solveState != SS_UNSOLVED; }
  bool numSolutionsKnown(void) const { return solveState != SS_UNSOLVED; }
  unsigned long getNumAssemblies(void) const { bt_assert(solveState != SS_UNSOLVED); return numAssemblies; }
  unsigned long getNumSolutions(void) const { bt_assert(solveState != SS_UNSOLVED); return numSolutions; }
  bool usedTimeKnown(void) const { return solveState != SS_UNSOLVED; }
  unsigned long getUsedTime(void) const { bt_assert(solveState != SS_UNSOLVED); return usedTime; }
  unsigned int solutionNumber(void) const { return solutions.size(); }
  assembly_c * getAssembly(unsigned int sol);
  const assembly_c * getAssembly(unsigned int sol) const;
  separation_c * getDisassembly(unsigned int sol);
  const separation_c * getDisassembly(unsigned int sol) const;
  separationInfo_c * getDisassemblyInfo(unsigned int sol);
  const separationInfo_c * getDisassemblyInfo(unsigned int sol) const;
  unsigned int getAssemblyNum(unsigned int sol) const;
  unsigned int getSolutionNum(unsigned int sol) const;

  /**
   * organze solutions:
   * - remove the i-th of the found solutions
   * - remove disassemblies from saved solutions and replace them
   *   with separationInfo objects, that saves quite some memory and still contains
   *   the information about the level of the disassembly
   * - add disassembly information to a solution within the list replacing already
   *   available information
   * - swap to solutions in the list, this is useful for sorting the solutions
   * - sort solutions by 0=assembly, 1=level, 2=sumMoves, 3=pieces
   */
  void removeSolution(unsigned int sol);
  void removeAllDisassm(void);
  void removeDisassm(unsigned int sol);
  void addDisasmToSolution(unsigned int sol, separation_c * disasm);
  void swapSolutions(unsigned int sol1, unsigned int sol2);
  void sortSolutions(int by);

};

#endif
