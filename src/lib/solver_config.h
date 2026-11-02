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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef __SOLVER_CONFIG_H__
#define __SOLVER_CONFIG_H__

#include "rotationtables.h"
#include "split_fanout.h"

#include <cstdint>
#include <optional>
#include <stdexcept>

/**
 * The most a run may use of the vector instructions; the hardware decides
 * what of that is there (solverSettings_t::avx2 and friends).
 */
enum vectorIsa_e {
  VECTOR_AUTO,     ///< whatever the hardware has
  VECTOR_SCALAR,   ///< portable 64-bit word loops only
  VECTOR_AVX2,     ///< x86-64: AVX2, not AVX-512
  VECTOR_AVX512,   ///< x86-64: AVX-512 and AVX2
  VECTOR_NEON      ///< ARM: NEON
};

/**
 * Solver hyperparameters and A/B toggles, resolved: what the assemblers,
 * the disassembly pool and the disassemblers run with. They never read the
 * environment themselves; solverConfig_t::resolve() does, in one place.
 */
struct solverSettings_t {
  /** worker threads of the assembler and of the disassembly pool, each;
   *  0 = hardware concurrency (both clamp to their own maximum) */
  unsigned int threads = 0;
  /** the bit-parallel exact cover engines (SIMD); false: Knuth's DLX */
  bool bitParallel = true;
  vectorIsa_e vector = VECTOR_AUTO;
  /** vector kernels in the disassembler's movement closure */
  bool disassemblerVector = true;
  /** disassemble in a pool of worker threads; false: inline, in order */
  bool disassemblerPool = true;
  /** Memory the Huang SIMD engine's matrix may take before assembler_1
   *  falls back to DLX (issue #92), 0 disables that engine. SIMD buys
   *  speed, never capability, so erring toward DLX past this is always
   *  safe. Desktop-class floor, deliberately conservative.
   *
   *  Scope note: the gate caps the matrix only (rows x tier bytes + nodes x
   *  per-node overhead). Per-task state is not counted: generateTasks()
   *  copies the whole SearchContext per SubtreeTask, including
   *  col_weights/col_counts (2 x (num_columns + 1) x 4 B) and the level-0
   *  active rows. That state is bounded by tasks x columns, not by this
   *  budget. */
  uint64_t huangMemoryBytes = uint64_t(256) << 20;
  /** fan-out cap when a search level is divided into tasks
   *  (src/lib/split_fanout.h) */
  unsigned int splitFanout = SplitFanout::kDefault;
  /** assembler_0 queues split children at the back (breadth-first) */
  bool splitBreadthFirst = false;
  /** split on demand: running tasks hand work to waiting siblings */
  bool donate = true;
  /** tests only: also donate every N nodes, 0 = only on demand */
  unsigned long donateEvery = 0;
  /** the shared thread budget across assembly and disassembly
   *  (thread_budget.h); false: both pools run uncapped */
  bool threadBudget = true;
  rotationSettings_t rotation = {};

  /** vector kernels allowed by `vector` and present in this CPU */
  bool avx2(void) const;
  bool avx512(void) const;
  bool neon(void) const;
};

/**
 * What a front end chooses. Every field it leaves unset comes from the
 * environment, else from the default: solverSettings_t's and
 * rotationSettings_t's initializers, so solverSettings_t{} is the defaults
 * with no environment involved (what tests use). Only front ends resolve;
 * the solver's constructors require the settings, so none can be created
 * with an environment the caller did not choose to read.
 *
 * This is the one table of the variables; AGENTS.md says what each is for.
 *
 * | field              | environment                                       |
 * |--------------------|---------------------------------------------------|
 * | threads            | BURRTOOLS_THREADS=N                               |
 * | bitParallel        | BURRTOOLS_BITPARALLEL=0\|1                        |
 * | vector             | BURRTOOLS_VECTOR=auto\|scalar\|avx2\|avx512\|neon |
 * | disassemblerVector | BURRTOOLS_DISASM_VECTOR=0\|1                      |
 * | disassemblerPool   | BURRTOOLS_DISASM_POOL=0\|1                        |
 * | huangMemoryMB      | BURRTOOLS_HUANG_MEM_MB=N                          |
 * | splitFanout        | BURRTOOLS_SPLIT_FANOUT=N (1 to 4096)              |
 * | splitBreadthFirst  | BURRTOOLS_SPLIT_ORDER=depth\|breadth              |
 * | donate             | BURRTOOLS_DONATE=0\|1                             |
 * | donateEvery        | BURRTOOLS_DONATE_EVERY=N                          |
 * | threadBudget       | BURRTOOLS_THREAD_BUDGET=0\|1                      |
 * | rotation.model     | -                                                 |
 * | rotation.family    | BURRTOOLS_ROT_FAMILY=compass\|wide                |
 * | rotation.planner   | BURRTOOLS_ROT_PLANNER=0\|1                        |
 * | rotation.slack     | -                                                 |
 *
 * An unparsable value, or one of the names these had before 2026-10-28, is
 * an error (std::invalid_argument), never silently the default: an A/B run
 * must not measure the same code twice.
 */
struct solverConfig_t {
  std::optional<unsigned int> threads;
  std::optional<bool> bitParallel;
  std::optional<vectorIsa_e> vector;
  std::optional<bool> disassemblerVector;
  std::optional<bool> disassemblerPool;
  std::optional<uint64_t> huangMemoryMB;
  std::optional<unsigned int> splitFanout;
  std::optional<bool> splitBreadthFirst;
  std::optional<bool> donate;
  std::optional<unsigned long> donateEvery;
  std::optional<bool> threadBudget;
  rotationConfig_t rotation = {};

  solverSettings_t resolve(void) const;
};

#endif
