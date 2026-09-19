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
#include "disassemblerpool.h"
#include "disassembler_0.h"
#include "assembly.h"
#include "disassembly.h"
#include "problem.h"

#include <cstdlib>

disassemblerPool_c::disassemblerPool_c(
  const problem_c & puz,
  unsigned int requested_threads,
  ResultCallback cb
) : puzzle(puz),
    num_threads(requested_threads),
    on_result(std::move(cb))
{
  if (std::getenv("BURRTOOLS_NO_DISASM_POOL") != nullptr) {
    is_inline = true;
    inline_dis = std::make_unique<disassembler_0_c>(puzzle);
    return;
  }

  if (num_threads == 0) {
    if (const char * env = std::getenv("BURRTOOLS_THREADS")) {
      int t = std::atoi(env);
      if (t > 0) num_threads = static_cast<unsigned int>(t);
    }
    if (num_threads == 0) {
      num_threads = std::thread::hardware_concurrency();
      if (num_threads == 0) num_threads = 1;
    }
  }

  if (num_threads == 1) {
    is_inline = true;
    inline_dis = std::make_unique<disassembler_0_c>(puzzle);
    return;
  }

  // Start worker threads
  workers.reserve(num_threads);
  for (unsigned int i = 0; i < num_threads; ++i) {
    workers.emplace_back(&disassemblerPool_c::worker_loop, this);
  }

  // Start merger thread
  merger = std::thread(&disassemblerPool_c::merger_loop, this);
}

disassemblerPool_c::~disassemblerPool_c() {
  abort();
}

void disassemblerPool_c::submit(std::unique_ptr<assembly_c> a) {
  if (aborted.load(std::memory_order_relaxed) || finished.load(std::memory_order_relaxed))
    return;

  if (is_inline) {
    uint64_t seq = next_submit_seq++;
    std::unique_ptr<separation_c> s;
    if (a && a->placementCount() > 1) {
      s = inline_dis->disassemble(a.get());
    }
    if (on_result) {
      on_result(seq, std::move(a), std::move(s));
    }
    return;
  }

  std::unique_lock<std::mutex> lock(queue_mutex);
  cv_producer.wait(lock, [this]() {
    return work_queue.size() < MAX_QUEUE_SIZE || aborted.load(std::memory_order_relaxed);
  });

  if (aborted.load(std::memory_order_relaxed))
    return;

  uint64_t seq = next_submit_seq++;
  work_queue.push(Task{seq, std::move(a)});
  cv_worker.notify_one();
}

void disassemblerPool_c::worker_loop() {
  disassembler_0_c dis(puzzle);

  while (true) {
    Task task;
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      cv_worker.wait(lock, [this]() {
        return !work_queue.empty() || finished.load(std::memory_order_relaxed) || aborted.load(std::memory_order_relaxed);
      });

      if (aborted.load(std::memory_order_relaxed))
        return;

      if (work_queue.empty()) {
        if (finished.load(std::memory_order_relaxed))
          return;
        continue;
      }

      task = std::move(work_queue.front());
      work_queue.pop();
      cv_producer.notify_one();
    }

    std::unique_ptr<separation_c> sep;
    if (task.assembly && task.assembly->placementCount() > 1 && !aborted.load(std::memory_order_relaxed)) {
      sep = dis.disassemble(task.assembly.get());
    }

    {
      std::lock_guard<std::mutex> lock(result_mutex);
      reorder_buffer.emplace(task.seqNo, Result{std::move(task.assembly), std::move(sep)});
      cv_merger.notify_one();
    }
  }
}

void disassemblerPool_c::merger_loop() {
  while (true) {
    Result res;
    uint64_t seq = 0;
    {
      std::unique_lock<std::mutex> lock(result_mutex);
      cv_merger.wait(lock, [this]() {
        return reorder_buffer.find(next_merge_seq) != reorder_buffer.end() ||
               aborted.load(std::memory_order_relaxed) ||
               (finished.load(std::memory_order_relaxed) && next_merge_seq == next_submit_seq.load(std::memory_order_relaxed));
      });

      if (aborted.load(std::memory_order_relaxed))
        return;

      auto it = reorder_buffer.find(next_merge_seq);
      if (it != reorder_buffer.end()) {
        seq = it->first;
        res = std::move(it->second);
        reorder_buffer.erase(it);
        next_merge_seq++;
      } else if (finished.load(std::memory_order_relaxed) && next_merge_seq == next_submit_seq.load(std::memory_order_relaxed)) {
        return;
      } else {
        continue;
      }
    }

    if (on_result) {
      on_result(seq, std::move(res.assembly), std::move(res.separation));
    }
  }
}

void disassemblerPool_c::finish() {
  if (is_inline)
    return;

  std::lock_guard<std::mutex> lock(lifecycle_mutex);
  if (finished.load(std::memory_order_relaxed) || aborted.load(std::memory_order_relaxed))
    return;

  finished.store(true, std::memory_order_release);
  cv_worker.notify_all();

  for (auto &w : workers) {
    if (w.joinable())
      w.join();
  }
  workers.clear();

  {
    std::lock_guard<std::mutex> rlock(result_mutex);
    cv_merger.notify_all();
  }
  if (merger.joinable())
    merger.join();
}

void disassemblerPool_c::abort() {
  if (is_inline)
    return;

  std::lock_guard<std::mutex> lock(lifecycle_mutex);
  if (aborted.load(std::memory_order_relaxed))
    return;

  aborted.store(true, std::memory_order_release);

  {
    std::lock_guard<std::mutex> qlock(queue_mutex);
    while (!work_queue.empty()) work_queue.pop();
    cv_worker.notify_all();
    cv_producer.notify_all();
  }

  {
    std::lock_guard<std::mutex> rlock(result_mutex);
    reorder_buffer.clear();
    cv_merger.notify_all();
  }

  for (auto &w : workers) {
    if (w.joinable())
      w.join();
  }
  workers.clear();

  if (merger.joinable())
    merger.join();
}
