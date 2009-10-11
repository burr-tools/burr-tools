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

#include <vector>
#include <set>
#include <string>

class voxel_c;
class separation_c;
class separationInfo_c;
class disassembly_c;
class assembly_c;
class gridType_c;
class puzzle_c;
class shape_c;
class solution_c;
class xmlWriter_c;
class xmlParser_c;

/**
 * this state reflects how far we are with solving this problem
 */
typedef enum {
  SS_UNSOLVED,    ///< nothing done yet
  SS_SOLVING,     ///< started and not finished in this state assm must contain the assembler
  SS_SOLVED       ///< finished, the assembler has been destroyed and all the information is available
} solveState_e;


/**
 * This class defines a problem,
 *
 * It always has to belong to a puzzle and can not be put somewhere else.
 * A puzzle is a collection of shapes and a set of problems associated
 * with these shapes
 * each problem defines a solution shape and a set of pieces (multiple
 * occurrences are possible) that need to be assembles into the given solution
 * shape the class also handles the solutions that belong to the problem
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
   * this set contains the pairs of colours that are allowed when a piece
   * is placed. The piece colour is in the high 16 bits, the result colour
   * in the lower 16. As right now only 64 colours are possible this will
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
   * Number of found assemblies for the problem.
   *
   * this is independent of the solutions vector, these are the pure numbers.
   * 0xFFFFFFFF stands for too many to count
   */
  unsigned long numAssemblies;

  /**
   * Number of found solutions for the problem.
   *
   * this is independent of the solutions vector, these are the pure numbers.
   * 0xFFFFFFFF stands for too many to count
   */
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
   * exactly and there is no need to limit the number
   * the value 0xFFFFFFFF is used for undefined maximum number
   */
  unsigned int maxHoles;

public:

  /**
   * constructor: create new empty problem for the given puzzle
   */
  problem_c(puzzle_c & puz);
  /**
   * constructor: load a problem from the given XML node for the given puzzle
   */
  problem_c(puzzle_c & puz, xmlParser_c & pars);
  /**
   * constructor: copy the given problem, except for label and solutions
   */
  problem_c(const problem_c * prob, puzzle_c & puz);

  /**
   * destructor: free all resources
   */
  ~problem_c(void);

  /**
   * save the problem into the returned XML node
   */
  void save(xmlWriter_c & xml) const;

  /**
   * return the current set grid type for this puzzle.
   * the grid type is taken from the puzzle this problem belongs to
   */
  const gridType_c * getGridType(void) const;
  /**
   * return the current set grid type for this puzzle.
   * the grid type is taken from the puzzle this problem belongs to
   */
  gridType_c * getGridType(void);

  /**
   * get the name of the problem.
   */
  const std::string & getName(void) const { return name; }
  /**
   * set the name of the problem.
   */
  void setName(std::string nm) { name = nm; }

  /** \name Result shape handling */
  //@{
  /**
   * set the puzzle shape id for the result shape.
   * Make sure that it is valid in the puzzle
   * before this function is called the result shape might be invalid
   */
  void setResult(unsigned int shape);
  /**
   * Check if there is a valid shape set.
   */
  bool resultValid(void) const;
  /**
   * Get the puzzle shape id for the result shape of the problem.
   * Make sure to only call getResult[Shape] when you know that the shape is valid
   */
  unsigned int getResult(void) const;
  /**
   * get the voxel space of the result shape.
   * Make sure to only call getResult[Shape] when you know that the shape is valid
   */
  const voxel_c * getResultShape(void) const;
  /**
   * get the voxel space of the result shape.
   * Make sure to only call getResult[Shape] when you know that the shape is valid
   */
  voxel_c * getResultShape(void);
  //@}

  /** \name problem piece handling
   * a problem uses the shapes within the puzzle to define its pieces
   * (you see the distinction, a shape is within the puzzle, a piece within the
   * problem, there can be several pieces of the same shape)
   * each shape has a min and max count attached to define how many times
   * this shape is (may) be used.
   * You also need to separate the shape ID a shape has as index in the puzzle
   * or the shape ID a shape as when it is the x-th shape in the problem.
   * For example shape number 3 defined in the problem may be the first shape in the problem.
   * TODO: try to find a proper name and interface to make sure this is done right.
   */
  //@{
  /**
   * Remove the usage of a shape from a problem.
   *
   * when a shape is removed from the puzzle this function is called, it
   * removes all ocurences of this shape in the problem and also updates
   * the indices because the larger ones have shifted.
   *
   * idx is the shape id form the puzzle, not the index of the shape in the problem.
   */
  void shapeIdRemoved(unsigned short idx);
  /** set the minimum number of times the shape may be used */
  void setShapeCountMin(unsigned int shape, unsigned int count);
  /** set the maximum number of times the shape may be used */
  void setShapeCountMax(unsigned int shape, unsigned int count);
  /** get the minimum number of times the shape may be used.
   * if the shape is not used in the problem, the function returns 0
   */
  unsigned int getShapeCountMin(unsigned int shape) const;
  /** get the maximum number of times the piece may be used.
   * if the shape is not used in the problem, the function returns 0
   */
  unsigned int getShapeCountMax(unsigned int shape) const;
  /** find out, if a shape is used in the problem (as piece or as result) */
  bool containsShape(unsigned int shape) const;
  /** how many different puzzle shapes have been used in this problem.
   *
   * This is NOT the number of pieces in the problem
   */
  unsigned int shapeNumber(void) const { return shapes.size(); }
  /** get the minimum number of times a shape is used.
   * This similar as the getShapeCountMin function but this time
   * the piece index instead of the shape index
   */
  unsigned int getShapeMin(unsigned int piece) const;
  /** get the maximum number of times a shape is used.
   * This similar as the getShapeCountMin function but this time
   * the piece index instead of the shape index
   */
  unsigned int getShapeMax(unsigned int piece) const;
  /** get the shape of a piece */
  unsigned int getShape(unsigned int piece) const;
  /** piece number that shape has in this problem */
  unsigned int getShapeId(unsigned int shape) const;
  /** get the voxel space for a given piece */
  const voxel_c * getShapeShape(unsigned int piece) const;
  /** get the voxel space for a given piece */
  voxel_c * getShapeShape(unsigned int piece);
  /** swap the 2 pieces in the piece list of the problem */
  void exchangeShape(unsigned int p1, unsigned int p2);
  /** the 2 shapes have been swapped in the puzzle, swap them here as well */
  void exchangeShapeId(unsigned int s1, unsigned int s2);
  //@}

  /** \name find out what puzzle shape is behind a piece number in a given problem */
  //@{
  /** find out how many pieces there are in this problem.
   * This is the maximum valid value for the other 2 functions in this group
   */
  unsigned int pieceNumber(void) const;
  /** find out which shape id (index in the problem shapes) is behind a given piece number */
  unsigned int pieceToShape(unsigned int pieceNr) const;
  /** find out the how manieth of a shape one piece is */
  unsigned int pieceToSubShape(unsigned int pieceNr) const;
  //@}

  /** \name edit color placement constraints.
   * the color 0 in this functions is always ignored as the placement
   * of color 0 is always possible
   */
  //@{
  /** allow placing pieces of one color into a result color */
  void allowPlacement(unsigned int pc, unsigned int res);
  /** disallow the placing */
  void disallowPlacement(unsigned int pc, unsigned int res);
  /** check if placing is allowed */
  bool placementAllowed(unsigned int pc, unsigned int res) const;
  //@}

  /** \name grouping information.
   * these functions can be used to define groups that pieces belong to
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
  //@{
  /** set the group and a count for the piece */
  void setShapeGroup(unsigned int piece, unsigned short group, unsigned short count);
  /** get the number of groups that a piece is a member of */
  unsigned short getShapeGroupNumber(unsigned int piece) const;
  /** get the x-th group that a piece is a member of */
  unsigned short getShapeGroup(unsigned int piece, unsigned int groupID) const;
  /** find out how many instances of the piece may be a member of the x-th group */
  unsigned short getShapeGroupCount(unsigned int piece, unsigned int groupID) const;
  //@}

  /** \name functions to limit the number of holes a solution is allowed to have. */
  //@{
  /** find out the the number is defined, if not there is no limit */
  bool maxHolesDefined(void) const { return maxHoles != 0xFFFFFFFF; }
  /** get the number (only when it is defined) */
  unsigned int getMaxHoles(void) const { bt_assert(maxHoles != 0xFFFFFFFF); return maxHoles; }
  /** set the number to a valid value */
  void setMaxHoles(unsigned int value) { bt_assert(value < 0xFFFFFFFF); maxHoles = value; }
  /** invalidate the value */
  void setMaxHolesInvalid(void) { maxHoles = 0xFFFFFFFF; }
  //@}

  /**
   * remove all known solutions, reset time, counter, assembler.
   * prepare for solving the problem
   * it also removes maybe saved assembler state so that solving starts
   * from the start
   */
  void removeAllSolutions(void);

  /** \name  functions used while solving the puzzle.
   * the assembler engine can be put into the problem to save it together
   * with the problem and later on resume, or even to just pause solving
   * when saved into XML only a string containing assembler dependent information
   * is saved, this information is resurrected when setting the assembler
   */
  //@{
  /** set the assembler.
   *
   * The set assembler will be reset to a saved state, when that information is
   * available. If not simply set the assembler
   */
  assembler_c::errState setAssembler(assembler_c * assm);
  /** get the assembler */
  assembler_c * getAssembler(void) { return assm; }
  /** get the assembler */
  const assembler_c * getAssembler(void) const { return assm; }
  /** call this for each found assembly */
  void incNumAssemblies(void) { bt_assert(solveState == SS_SOLVING); numAssemblies++; }
  /** call this for each found solution */
  void incNumSolutions(void) { bt_assert(solveState == SS_SOLVING); numSolutions++; }
  /** add time used to solve the puzzle (in seconds) the value is added to the already accumulated time. */
  void addTime(unsigned long time) { bt_assert(solveState == SS_SOLVING); usedTime += time; }
  /** add an assembly as a solution */
  void addSolution(assembly_c * assm);
  /** add an assembly with disassembly information as a solution.
   * You can give the index, where to add it. This defaults to the end of the list
   */
  void addSolution(assembly_c * assm, separationInfo_c * disasm, unsigned int pos = 0xFFFFFFFF);
  /** add an assembly with disassembly proper as a solution.
   * You can give the index, where to add it. This defaults to the end of the list
   */
  void addSolution(assembly_c * assm, separation_c * disasm, unsigned int pos = 0xFFFFFFFF);
  /** once finished analysing call finishedSolving for finish off all actions.
   * After that call no more modifications are possible, no more addSOlution, incNumAssemblies and so on.
   * */
  void finishedSolving(void) { solveState = SS_SOLVED; }
  //@}

  /** \name functions used after solving to get information.
   * For each analysis some information is saved: the number of assemblies and solutions and
   * the time required for the analysis.
   * Additionally some or all of the found solutions may be saved. Information about
   * those solutions can be acquired. Each of those saved solutions also has some information
   * attached: the assembly and solution number
   */
  //@{
  /** find out how far we are with solving (no, started, finished) */
  solveState_e getSolveState(void) const { return solveState; }
  /** find out if we have an idea about the number of assemblies */
  bool numAssembliesKnown(void) const { return solveState != SS_UNSOLVED; }
  /** get number of assemblies found so far. Throws an exception, when not known */
  unsigned long getNumAssemblies(void) const { bt_assert(solveState != SS_UNSOLVED); return numAssemblies; }
  /** find out if we have an idea about the number of solutions */
  bool numSolutionsKnown(void) const { return solveState != SS_UNSOLVED; }
  /** get number of solutions found so far. Throws an exception, when not known */
  unsigned long getNumSolutions(void) const { bt_assert(solveState != SS_UNSOLVED); return numSolutions; }
  /** find out, if we know something about the time for solving the puzzle */
  bool usedTimeKnown(void) const { return solveState != SS_UNSOLVED; }
  /** find out the time used to solve the puzzle up to the current state. Throws an exception when unknown */
  unsigned long getUsedTime(void) const { bt_assert(solveState != SS_UNSOLVED); return usedTime; }
  /** get number of solutions that were stored */
  unsigned int solutionNumber(void) const { return solutions.size(); }
  /** get the assembly for one of the stored solutions */
  assembly_c * getAssembly(unsigned int sol);
  /** get the assembly for one of the stored solutions */
  const assembly_c * getAssembly(unsigned int sol) const;
  /** get the disassembly for one of the stored solutions. Returns 0 if there is none */
  separation_c * getDisassembly(unsigned int sol);
  /** get the disassembly for one of the stored solutions. Returns 0 if there is none */
  const separation_c * getDisassembly(unsigned int sol) const;
  /** get the disassembly information for one of the stored solutions.
   * Returns 0 if there is none, Creates one if an disassembly is available
   */
  disassembly_c * getDisassemblyInfo(unsigned int sol);
  /** get the disassembly information for one of the stored solutions.
   * Returns 0 if there is none, Creates one if an disassembly is available
   **/
  const disassembly_c * getDisassemblyInfo(unsigned int sol) const;
  /** get the number of the assembly for the saved solution number x */
  unsigned int getAssemblyNum(unsigned int sol) const;
  /** get the number of the solution for the saved solution number x */
  unsigned int getSolutionNum(unsigned int sol) const;
  //@}


  /** \name organize solutions */
  //@{
  /** remove the i-th solution from the solution list */
  void removeSolution(unsigned int sol);
  /** remove all disassemblies.
   *
   * remove disassemblies from saved solutions and replace them
   * with separationInfo objects, that saves quite some memory and still contains
   * the information about the level of the disassembly
   */
  void removeAllDisassm(void);
  /** remove the i-th solution from the solution list.
   *
   * remove disassemblies from saved solutions and replace them
   * with separationInfo objects, that saves quite some memory and still contains
   * the information about the level of the disassembly
   */
  void removeDisassm(unsigned int sol);
  /** add disassembly information to a solution within the list replacing already available information */
  void addDisasmToSolution(unsigned int sol, separation_c * disasm);
  /** swap to solutions in the list, this is useful for sorting the solutions */
  void swapSolutions(unsigned int sol1, unsigned int sol2);
  /** sort solutions by 0=assembly, 1=level, 2=sumMoves, 3=pieces */
  void sortSolutions(int by);
  //@}

private:

  // no copying and assigning
  problem_c(const problem_c&);
  void operator=(const problem_c&);

};

#endif
