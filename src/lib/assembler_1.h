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

#include <vector>
#include <set>
#include <unordered_set>
#include <stack>
#include <atomic>
#include <mutex>
#include <memory>
#include <thread>
#include <stop_token>

class problem_c;
class gridType_c;
class mirrorInfo_c;
class assemblerWorker_1;
class ISimdHuangCover;

/**
 * This class is an assembler class.
 *
 * This assembler is written with ideas from Wei-Hwa Huang. It can handle ranges for the piece
 * numbers and thus also multiple instances of one piece.
 *
 * But for simple cases it is not really optimal.
 *
 * It also has a problem with guessing how much of the analysis it done. This number is growing exponentially
 * meaning in the beginning it is growing very slowly resulting in huge time-left numbers while at the
 * end it is getting very fast and the time-left value dropping really fast.
 */

class assembler_1_c : public assembler_c {

protected:

  const problem_c & problem;

private:

  /* this are the members of the node. One array for each member. This
   * accelerates access.
   *
   * colCount is a shared member. It is column for normal nodes and count for
   * column header nodes
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

  /* this function gets called whenever an assembly was found
   * when a callback is available it will call getAssembly to
   * obtain the assembly for the found solution when the
   * field avoidTransformedAssemblies is true then the assembly
   * is checked, if it has been found before. The assembly
   * is normalised in inserted into a set of assemblies for
   * later reference
   */
  void solution(void);

  /* used to save if the search is running */
  std::atomic<bool> running{false};

  struct SubtreeTask_1 {
    std::vector<unsigned int> task_stack;
    std::vector<unsigned int> next_row_stack;
    std::vector<unsigned int> column_stack;
    std::vector<unsigned int> rows;
    std::vector<unsigned int> hidden_rows;
  };

  void generateTasksAtDepth(unsigned int cutoff_depth, std::vector<SubtreeTask_1> & tasks);
  void generateSubtreeTasks(std::vector<SubtreeTask_1> & tasks, unsigned int targetTasks, unsigned int maxDepth);
  void parallelMultiSearch(unsigned int workers);

  /* true when the next assemble() call will take the parallel path */
  bool willRunParallel(unsigned int threads) const;

  bool canUseSimd(void) const;
  void simdSearch(void);
  std::unique_ptr<ISimdHuangCover> createSimdSolver(void) const;

  friend class assemblerWorker_1;

  std::vector<SubtreeTask_1> parallelTasks;
  std::unordered_set<uint64_t> emittedSignatures;

  /* getFinished() has two progress sources and this picks between them: the
   * task-based one used for the whole of a parallel run, and the
   * single-threaded finished_a/finished_b estimate used otherwise. totalTasks
   * cannot make the choice on its own -- it is 0 while the task list is still
   * being generated, and it keeps an aborted parallel run's value afterwards.
   *
   * Both parallel back ends report through the one task-based expression,
   * completed tasks plus each in-flight worker's fraction of the task it
   * holds. The SIMD back end has no in-flight hook, and publishes no worker
   * slots at all, so for it that expression is just completed over total
   * tasks -- which is exactly what it can report.
   *
   * assemble() clears this, and the task counters with it, for a non-parallel
   * run; see the note there.
   */
  std::atomic<bool> inFlightProgress{false};

  /* Each in-flight worker publishes how far into its current subtree task it
   * has got, so a long-running task contributes continuously instead of
   * nothing until it completes. Read by the GUI thread via getFinished();
   * never dereferences a worker's private matrix.
   *
   * Tasks are weighted EQUALLY here -- one slot is worth 1/totalTasks -- and
   * that is a measured decision, not an omission. assembler_0_c weights its
   * tasks by their structural share of the search tree; the same weighting was
   * implemented for this engine and rejected, because assembler_1_c generates
   * its tasks by running the real search to a cutoff depth and so meets the
   * search's own pruning while doing so. On Burr-Glar that handed instantly
   * dead subtrees 96.9% of the weight inside the first 10 ms, and the bar sat
   * at 0.9694 for a whole 3 s sample -- strictly worse than the unweighted bar
   * it would have replaced.
   *
   * assembler_0_c's structural share has since been measured too, and shows
   * the same anti-correlation at a smaller scale: it opens DiagonalCube at
   * 0.5 before any worker starts. It is kept there rather than rejected; see
   * the prunedTaskShare note in assembler_0.h for the numbers and the reason.
   *
   * std::atomic is neither copyable nor movable, so the slots are held by
   * pointer rather than by value.
   *
   * The slots themselves are atomic, but the vector holding them is not:
   * getFinished() runs on the GUI thread and is already being polled while
   * parallelMultiSearch() is still sizing the vector, which ThreadSanitizer
   * duly flags. progressMutex guards the vector's structure -- not the slot
   * values, which the workers keep publishing lock-free. It is only ever held
   * while the vector is (re)built and while getFinished() walks it, so a
   * worker never blocks on it and the GUI contends with nothing.
   */
  struct WorkerProgress {
    std::atomic<float> fraction{0.0f};
  };
  mutable std::mutex progressMutex;
  std::vector<std::unique_ptr<WorkerProgress>> workerProgress;

  /* true only after a search drained without being aborted. "not running" is
   * not the same as "finished": a prepared-but-unstarted assembler is also
   * not running.
   */
  std::atomic<bool> searchComplete{false};

  /* Pristine base matrix saved before search starts */
  /* set when a parallel search stopped before finishing; such a position is
   * saved as not resumable -- see assembler_1.cpp
   */
  bool parallelInterrupted = false;

  /* set when simdSearch() ran to completion.
   *
   * The SIMD search keeps its position inside the solver, so next_row_stack /
   * task_stack -- what the progress estimate below is derived from -- never
   * move. Completion therefore needs its own flag: "idle and iterations > 0"
   * also holds after setPosition() restores a saved state, and between two
   * assemble() calls, before any search has run.
   */
  std::atomic<bool> simdCompleted{false};

  std::vector<unsigned int> base_left;
  std::vector<unsigned int> base_right;
  std::vector<unsigned int> base_up;
  std::vector<unsigned int> base_down;
  std::vector<unsigned int> base_colCount;
  std::vector<unsigned int> base_weight;

  std::vector<unsigned int> rows;
  std::vector<unsigned int> finished_a;
  std::vector<unsigned int> finished_b;

  /* getFinished() (GUI thread) reads finished_a/finished_b while the worker
   * mutates them. reserve() (see assemble) stops the buffer from moving, but a
   * concurrent pop_back would shrink the size under the reader and expose the
   * popped, now-unconstructed slot. This mutex serialises getFinished with the
   * pop_backs so the reader only ever sees constructed elements; the far more
   * frequent push_back / back()++ stay lock free (they only ever add or bump a
   * value the reader can tolerate reading stale).
   */
  mutable std::mutex finishedMutex;

  /* push/pop the progress stacks under finishedMutex so getFinished (GUI
   * thread) never observes a size change while a slot is being constructed or
   * destructed. back()++ stays lock free - it only bumps an existing value.
   */
  void pushFinished(unsigned int b);
  void popFinished(void);
  std::vector<unsigned int> hidden_rows;  // rows that nodes to rows that are currently hidden
  // because there are several batched of rows that need hiding these batches are separated
  // by a zero because the header row will never get hidden...
  std::vector<unsigned int>task_stack;
  std::vector<unsigned int>next_row_stack;
  std::vector<unsigned int>column_stack;

  unsigned int headerNodes = 0;  // number of nodes within the header

  bool open_column_conditions_fulfillable(void);
  int find_best_unclosed_column(void);
  void cover_column_only(int col);
  void uncover_column_only(int col);
  void cover_column_rows(int col);
  void uncover_column_rows(int col);
  void hiderow(int r);
  void unhiderow(int r);
  void hiderows(unsigned int r);
  void unhiderows(void);
  bool column_condition_fulfilled(int col);
  bool column_condition_fulfillable(int col);
//  void rec(unsigned int next_row);
  void iterative(void);
  void remove_row(unsigned int r);
  void remove_column(unsigned int c);
  unsigned int clumpify(void);


  /**
   * this function is called by the default implementation of prepare
   * to check, if the piece fits at the given position
   */
  bool canPlace(const voxel_c * piece, int x, int y, int z) const;

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

  /* the members for rotations rejection
   */
  bool avoidTransformedAssemblies = false;
  unsigned int avoidTransformedPivot = 0;
  std::unique_ptr<mirrorInfo_c> avoidTransformedMirror;

  /// set to true, when complete analysis is requested
  bool complete = false;

  /* the variables for debugging assembling processes
   */
  bool debug = false;         // debugging enabled
  int debug_loops = 0;    // how many loops to run ?

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
  int AddPieceNode(unsigned int piece, unsigned int rot, unsigned int x, unsigned int y, unsigned int z);

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

  assembler_1_c(const problem_c & problem);
  ~assembler_1_c(void);

  /* functions that are overloaded from assembler_c, for comments see there */
  using assembler_c::assemble;
  errState createMatrix(bool keepMirror, bool keepRotations, bool complete) override;
  void assemble(assembler_cb * callback) override;
  int getErrorsParam(void) override { return errorsParam; }
  float getFinished(void) const override;
  bool stopped(void) const override { return !running.load(std::memory_order_relaxed); }
  void setNumThreads(unsigned int threads) override { numThreads = std::min(threads, 256u); }
  unsigned int getNumThreads(void) const override { return numThreads; }
  unsigned int getRunThreads(void) const override;
  errState setPosition(const char * string, const char * version) override;
  void save(xmlWriter_c & xml) const override;
  void reduce(void) override;
  unsigned int getReducePiece(void) const override { return reducePiece; }
  void debug_step(unsigned long num = 1) override;
  std::unique_ptr<assembly_c> getAssembly(void) override;

  static bool canHandle(const problem_c & p);

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
