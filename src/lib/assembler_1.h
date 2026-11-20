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
#ifndef __ASSEMBLER_1_H__
#define __ASSEMBLER_1_H__

#include "assembler.h"
#include "simd_huang_cover.h"

#include <vector>
#include <set>
#include <unordered_set>
#include <stack>
#include <atomic>
#include <mutex>
#include <memory>
#include <thread>
#include <stop_token>

struct solverSpec_t;
class gridType_c;
class mirrorInfo_c;
class ISimdHuangCover;

/**
 * This class is an assembler class.
 *
 * This assembler is written with ideas from Wei-Hwa Huang. It can handle ranges for the piece
 * numbers and thus also multiple instances of one piece; it takes every problem.
 *
 * It searches with one of two exact cover engines: the bit-parallel
 * SimdHuangCover where the matrix fits, else Dancing Cells (CellsCover).
 * Both run in a task pool and leave a stopped search as a list of seeds
 * (HuangSeed).
 */

class assembler_1_c : public assembler_c {

protected:

  /** the problem; `problem` is *spec, which the assembler keeps alive */
  std::shared_ptr<const solverSpec_t> spec;
  const solverSpec_t & problem;

private:

  /* this are the members of the node. One array for each member. This
   * accelerates access.
   *
   * colCount is a shared member. It is column for normal nodes and count for
   * column header nodes
   *
   * The matrix is built and reduced in these dancing-links arrays; the
   * search runs on a copy in one of the two engines (createSimdSolver).
   */
  std::vector<unsigned int> left;
  std::vector<unsigned int> right;
  std::vector<unsigned int> up;
  std::vector<unsigned int> down;
  std::vector<unsigned int> colCount;
  std::vector<unsigned int> weight;
  std::vector<unsigned int> min;
  std::vector<unsigned int> max;

  /* this vector contains all columns that are used for the hole
   * optimisation: up to "holes" instances of these columns might
   * be zero
   */
  std::vector<unsigned int> holeColumns;
  unsigned int holes = 0;

  /* used to save if the search is running */
  std::atomic<bool> running{false};

  // tasks donated by running workers to idle siblings (diagnostics, tests)
  std::atomic<unsigned long> donations{0};

  /* the search: the engine, its task pool and the remainder of a stop */
  void search(unsigned int workers);

  /* true when the next assemble() call has work to do */
  bool willRunParallel(unsigned int threads) const;

  /** whether the bit-parallel engine takes this matrix; Dancing Cells
   *  takes it otherwise */
  bool canUseSimd(void) const;
  /** whether the bit-parallel engine can represent this matrix at all,
   *  whatever the settings and the memory budget say: part of canUseSimd(),
   *  and what a restored remainder of that engine must meet. forceMerge:
   *  judge the columns merged whatever settings.mergeColumns says (a
   *  bit-parallel remainder, which may only fit merged) */
  bool simdEngineSupports(bool forceMerge = false) const;
  /** the solver for this matrix: the Dancing Cells engine, else the
   *  bit-parallel one in the tier the matrix needs; forceMerge as for
   *  simdEngineSupports() */
  std::unique_ptr<ISimdHuangCover> createSimdSolver(bool cells, bool forceMerge = false) const;
  /** The columns the engines get: the matrix's live columns in ring order,
   *  less, with settings.mergeColumns, every voxel column covered by exactly
   *  the same rows as an earlier one. map[c] is column c's number in the
   *  engine, numbered from 1 in the same order, 0 when it is left out.
   *  Returns the engine's column count; rows and nodes (if not null) get
   *  the rows and the matrix nodes the engine will hold. forceMerge merges
   *  as if settings.mergeColumns were on. */
  unsigned int engineColumns(std::vector<unsigned int> & map, uint64_t * rows, uint64_t * nodes,
                             bool forceMerge = false) const;

  /* grants test/test_solver.cpp read access to totalTasks/completedTasks,
   * to assert that donations and stops conserve progress units directly
   * (getFinished() reports 1 for any completed search, whatever the
   * counters say).
   */
  friend struct Assembler1UnitsTestAccess;

  /* true only after a search drained without being aborted. "not running" is
   * not the same as "finished": a prepared-but-unstarted assembler is also
   * not running.
   */
  std::atomic<bool> searchComplete{false};
  /* The remainder of a stopped search, as seeds (issue #111): interrupted
   * in-flight tasks plus the pool remainder, filled by parallelSolve on
   * stop. The next assemble() feeds them back instead of starting over;
   * save() persists them for a resume in another session.
   */
  std::vector<HuangSeed> pendingHuangPrefixes;
  /* the seeds above are those of the Dancing Cells engine (CellsCover),
   * which only that engine can read; set whenever they are */
  bool pendingSeedsCells = false;
  /* The solver setPosition() built to check that the restored seeds
   * replay; the resumed run takes it instead of building a second one.
   * Set only together with restored seeds, and dropped with them.
   */
  std::unique_ptr<ISimdHuangCover> restoredSimdSolver;

  unsigned int headerNodes = 0;  // number of nodes within the header

  /* matrix operations of reduce() */
  bool open_column_conditions_fulfillable(void);
  void hiderow(int r);
  void unhiderow(int r);
  void remove_row(unsigned int r);
  void remove_column(unsigned int c);
  unsigned int clumpify(void);


  /* this function creates the matrix for the search function
   * because we need to know how many nodes we need to allocate the
   * arrays with the right size, we add a parameter. If this is true
   * the function will not access the array but only count the number
   * of nodes used. This number is returned
   *
   * return error codes
   */
  int prepare(bool hasRange, unsigned int rangeMin, unsigned int rangeMax);

  /* internal error state */
  errState errorsState = ERR_NONE;
  int errorsParam = 0;

  /* now this isn't hard to guess, is it? */
  unsigned int piecenumber = 0;

  /* the message object that gets called with the solutions as param */
  assembler_cb * asm_bc = nullptr;

  /* this vector contains the placement (transformation and position) for
   * a piece in a row
   */
  class piecePosition {

  public:

    int x, y, z;
    unsigned char transformation;
    unsigned int row;            // first node in this row
    unsigned int piece;

    piecePosition(unsigned int pc_, int x_, int y_, int z_, unsigned char transformation_, unsigned int row_) : x(x_), y(y_), z(z_),
      transformation(transformation_), row(row_), piece(pc_) {}
  };
  std::vector<piecePosition> piecePositions;

  /* every matrix node's index into piecePositions, built once before the
   * bit-parallel search (buildNodePositions): problems with millions of
   * assemblies look up every placement of every assembly, which was a
   * binary search over piecePositions. Empty: getPieceInformation searches. */
  std::vector<uint32_t> nodePositions;
  void buildNodePositions(void);

  /* the members for rotations rejection
   */
  bool avoidTransformedAssemblies = false;
  unsigned int avoidTransformedPivot = 0;
  std::unique_ptr<mirrorInfo_c> avoidTransformedMirror;

  /// set to true, when complete analysis is requested
  bool complete = false;

  std::atomic<unsigned long> iterations{0};  // single-writer counter, read cross-thread by getIterations

protected:

  /* as this is only a back end doing the processing on the matrix, there needs to
   * be a front end creating the matrix and evaluating the results. These functions
   * are helpers for the front end
   */

  /* this function creates the first row of the matrix. As the createMatrix function
   * has already set up some variables you only need to specify the value res_filled that is
   * given to you as a parameter to the function prepare. You normally call this function
   * in prepare
   */
  void GenerateFirstRow(unsigned int res_filled);

  /* this function adds a node to the matrix that belongs to the first columns that represent
   * the pieces. This is normally the first thing you do, when you start a new line in the matrix
   * The information you provide is required to restore the exact piece in placement that this
   * line stands for
   * the return value is a number that has to be given to the voxel node creation routine
   * it contains the number of the node that is created with this function
   */
  int AddPieceNode(unsigned int piece, unsigned int rot, int x, int y, int z);

  /* adds a node with a certain weight, for piece range node counting
   */
  void AddRangeNode(unsigned int col, unsigned int piecenode, unsigned int weight);


  /* this is in a way the inverse of the function above. You give a node number and get
   * the exact piece and placement the line this node belongs to stands for
   * this function is used in the solution function to restore the placement of the piece
   */
  void getPieceInformation(unsigned int node, unsigned int * piece, unsigned char *tran, int *x, int *y, int *z) const;

  /* this adds a normal node that represents a used voxel within the solution
   * piece-node is the number that you get from AddPieceNode, col is a number
   * that can be calculated from the x, y and z position of the voxel
   */
  void AddVoxelNode(unsigned int col, unsigned int piecenode);

  /* these functions provide access to the cover information for you */
  unsigned int getRight(int pos) { return right[pos]; }
  unsigned int getColCount(int pos) { return colCount[pos]; }

  /* finally after assembling a puzzle and creating something meaningful from the cover
   * information you need to call the call-back of the user, use this function to get the
   * call-back class
   */
  assembler_cb * getCallback(void) { return asm_bc; }

  unsigned int getPiecenumber(void) { return piecenumber; }

  /* call this function if you think that there might be
   * rotated assemblies found. Here a description of how the whole aspect of
   * rotation avoiding is supposed to work
   * the front end is supposed to initialise the assembler so that as few as
   * possible double assemblies are found by selecting one piece and not placing
   * this piece in all possible positions. But this will not always work, if
   * the front end is are not absolutely certain that it has avoided all possible
   * rotations it should call this function. This will then add an additional check
   * for each found assembly
   */
  void checkForTransformedAssemblies(unsigned int pivot, std::unique_ptr<mirrorInfo_c> mir);

  std::atomic<unsigned int> reducePiece;  // written by worker, read by GUI via getReducePiece

public:

  assembler_1_c(std::shared_ptr<const solverSpec_t> spec, const solverSettings_t & settings);
  ~assembler_1_c(void);

  /* functions that are overloaded from assembler_c, for comments see there */
  using assembler_c::assemble;
  errState createMatrix(bool keepRotations, bool complete) override;
  void assemble(assembler_cb * callback) override;
  int getErrorsParam(void) override { return errorsParam; }
  float getFinished(void) const override;
  /** Number of tasks running workers donated to idle siblings so far (see
   * split_fanout.h); diagnostics only. */
  unsigned long getDonations(void) const { return donations.load(std::memory_order_relaxed); }
  bool stopped(void) const override { return !running.load(std::memory_order_relaxed); }
  unsigned int getRunThreads(void) const override;
  errState setPosition(const char * string, const char * version) override;
  std::pair<std::string, std::string> saveState(void) const override;
  void reduce(void) override;
  unsigned int getReducePiece(void) const override { return reducePiece; }

  static bool canHandle(const solverSpec_t & p);

  /* some more special information to find out possible piece placements */
  bool getPiecePlacementSupported(void) const override { return true; }
  unsigned int getPiecePlacement(unsigned int node, int delta, unsigned int piece, unsigned char *tran, int *x, int *y, int *z) const override;
  unsigned int getPiecePlacementCount(unsigned int piece) const override;
  unsigned long getIterations(void) override { return iterations; }

private:

  // no copying and assigning
  assembler_1_c(const assembler_1_c&);
  void operator=(const assembler_1_c&);
};

#endif
