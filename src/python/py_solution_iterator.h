#pragma once

#include "py_solution.h"

#include <memory>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <string>

class puzzle_c;
class assembler_c;
class disassembler_c;

class SolutionIterator {
public:
  SolutionIterator(std::shared_ptr<puzzle_c> puz,
                   unsigned int problem_idx,
                   bool disassemble = true,
                   bool reduce = false,
                   bool keep_rotations = false,
                   bool keep_mirror = false);
  ~SolutionIterator();

  SolutionIterator* iter() { return this; }
  PySolution next();

  void stop();
  unsigned long get_iterations() const;
  bool is_finished() const { return finished.load(); }

private:
  struct QueueItem {
    enum Type { ITEM_SOLUTION, ITEM_FINISHED, ITEM_ERROR } type{ITEM_SOLUTION};
    PySolution solution;
    std::string error_message;
  };

  std::shared_ptr<puzzle_c> puzzle;
  unsigned int problem_idx;
  bool disassemble;
  bool reduce;
  bool keep_rotations;
  bool keep_mirror;

  std::unique_ptr<assembler_c> assm;
  std::unique_ptr<disassembler_c> disasm;
  std::atomic<assembler_c*> active_assm{nullptr};

  std::atomic<bool> stop_requested{false};
  std::atomic<bool> finished{false};
  std::atomic<unsigned long> iterations{0};

  std::mutex queue_mutex;
  std::condition_variable cv_can_pop;
  std::condition_variable cv_can_push;
  std::deque<QueueItem> queue;
  static constexpr size_t MAX_QUEUE_SIZE = 4;

  std::thread worker_thread;

  void worker_run();
  void push_item(QueueItem && item);
};
