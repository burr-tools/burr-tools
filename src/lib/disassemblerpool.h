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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef __DISASSEMBLER_POOL_H__
#define __DISASSEMBLER_POOL_H__

class assembly_c;
class separation_c;
class problem_c;
class disassembler_0_c;

#include <vector>
#include <thread>
#include <stop_token>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <memory>
#include <functional>
#include <atomic>
#include <exception>

/**
 * Thread pool for concurrent disassembly of independent puzzle assemblies
 * with ordered merging.
 *
 * Assemblies found by the assembler are independent. This pool distributes
 * disassembly jobs across N worker threads, each owning a private disassembler_0_c
 * instance. A dedicated merger thread orders completed results by sequence number
 * before invoking the user callback, preserving strict deterministic solution ordering.
 */
class disassemblerPool_c {
public:
  using ResultCallback = std::function<void(uint64_t seqNo, std::unique_ptr<assembly_c> a, std::unique_ptr<separation_c> s)>;

  disassemblerPool_c(
    const problem_c & puzzle,
    unsigned int num_threads,
    ResultCallback on_result
  );

  ~disassemblerPool_c();

  /** Submit an assembly to be disassembled. May block if queue reaches capacity (backpressure). */
  void submit(std::unique_ptr<assembly_c> a);

  /** Wait for all pending assemblies to be disassembled and merged. */
  void finish();

  /** Immediately cancel and discard any pending work. */
  void abort();

  /** Signal pool to stop accepting work and wake any blocked submitters.
   *  Unlike abort(), does not discard the reorder buffer, so already-completed
   *  results are still delivered.
   */
  void requestStop();

  bool isAborted() const { return aborted.load(std::memory_order_relaxed); }
  bool isStopRequested() const { return stop_requested.load(std::memory_order_relaxed); }

private:
  struct Task {
    uint64_t seqNo = 0;
    std::unique_ptr<assembly_c> assembly;
  };

  struct Result {
    std::unique_ptr<assembly_c> assembly;
    std::unique_ptr<separation_c> separation;
  };

  const problem_c & puzzle;
  unsigned int num_threads;
  ResultCallback on_result;

  std::atomic<bool> aborted{false};
  std::atomic<bool> finished{false};
  std::atomic<bool> stop_requested{false};
  bool is_inline = false;
  std::mutex inline_mutex;
  std::unique_ptr<disassembler_0_c> inline_dis;

  std::atomic<uint64_t> next_submit_seq{0};
  std::atomic<uint64_t> next_merge_seq{0};

  std::mutex lifecycle_mutex;

  std::mutex queue_mutex;
  std::condition_variable_any cv_worker;
  std::condition_variable_any cv_producer;
  std::condition_variable_any cv_assembler;
  std::queue<Task> work_queue;
  unsigned int available_disassembly_permits{0};
  size_t max_queue_size{64};
  size_t max_reorder_size{64};

  std::mutex result_mutex;
  std::condition_variable_any cv_merger;
  std::condition_variable_any cv_reorder;
  std::map<uint64_t, Result> reorder_buffer;

  std::mutex exception_mutex;
  std::exception_ptr worker_exception;

  std::vector<std::jthread> workers;
  std::jthread merger;

  void worker_loop(std::stop_token st);
  void merger_loop(std::stop_token st);
  void check_exception();
};

#endif // __DISASSEMBLER_POOL_H__
