#pragma once

#include "py_solution.h"
#include "lib/rotationtables.h"

#include <atomic>
#include <memory>

class puzzle_c;
class solverSession_c;

/* Streaming, one-shot solver for the Python bindings, backed by
 * solverSession_c: disassembly runs in parallel across the pool and results
 * stream out of the bounded pull queue as soon as they finish, in no
 * particular order.
 */
class SolutionIterator {
public:
  SolutionIterator(std::shared_ptr<puzzle_c> puz,
                   unsigned int problem_idx,
                   bool disassemble = true,
                   bool reduce = false,
                   bool keep_rotations = false,
                   unsigned int threads = 0,
                   rotationMode_e rotation_mode = ROTATION_OFF);
  ~SolutionIterator();

  SolutionIterator* iter() { return this; }
  PySolution next();

  void stop();
  unsigned long get_iterations() const;
  bool is_finished() const { return finished_.load(std::memory_order_acquire); }

private:
  std::shared_ptr<puzzle_c> puzzle_;
  std::unique_ptr<solverSession_c> session_;
  bool disassemble_{false};
  unsigned int asm_count_{0};
  unsigned int sol_count_{0};
  std::atomic<bool> finished_{false};
  std::atomic<bool> stopped_{false};
  std::atomic<unsigned long> last_iterations_{0};
};
