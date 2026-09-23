#pragma once

#include "py_solution.h"

#include <memory>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <stop_token>
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
                   bool keep_mirror = false,
                   unsigned int threads = 0);
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
  unsigned int threads;

  std::unique_ptr<assembler_c> assm;
  std::unique_ptr<disassembler_c> disasm;
  std::atomic<assembler_c*> active_assm{nullptr};

  std::atomic<bool> finished{false};
  std::atomic<unsigned long> iterations{0};

  std::mutex queue_mutex;
  // condition_variable_any (not plain condition_variable): the wait overloads
  // taking std::stop_token register a stop callback, so request_stop() wakes
  // a worker blocked in push_item without manual notify_all() races.
  std::condition_variable_any cv_can_pop;
  std::condition_variable_any cv_can_push;
  std::deque<QueueItem> queue;
  static constexpr size_t MAX_QUEUE_SIZE = 4;

  std::jthread worker_thread;
  // Single-joiner guard: stop() may run from explicit user calls and the
  // destructor; joining the same jthread twice would terminate.
  std::once_flag stop_join_once_;

  void worker_run(std::stop_token st);
  void push_item(QueueItem && item, std::stop_token st);
};
