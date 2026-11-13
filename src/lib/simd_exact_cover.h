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
#ifndef __SIMD_EXACT_COVER_H__
#define __SIMD_EXACT_COVER_H__

#include "bt_assert.h"
#include "solver_config.h"
#include "split_fanout.h"
#include <cstdint>
#include <vector>
#include <functional>
#include <atomic>
#include <stop_token>
#include <cstring>
#include <algorithm>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

/**
 * Bitset templated on N_WORDS (multiples of 4, i.e. 256-bit chunks),
 * aligned to 64 bytes for AVX2 vector operations.
 */
template <size_t N_WORDS>
struct alignas(64) SimdBitset {
  static_assert(N_WORDS > 0 && (N_WORDS % 4 == 0), "N_WORDS must be a positive multiple of 4");
  static constexpr size_t NUM_WORDS = N_WORDS;
  uint64_t words[N_WORDS];

  constexpr SimdBitset() : words{} {}

  template <typename... Args>
  constexpr SimdBitset(Args... args) : words{static_cast<uint64_t>(args)...} {}

  void set(unsigned int bit) {
    bt_assert(bit < N_WORDS * 64);
    words[bit >> 6] |= (1ULL << (bit & 63));
  }

  void reset(unsigned int bit) {
    bt_assert(bit < N_WORDS * 64);
    words[bit >> 6] &= ~(1ULL << (bit & 63));
  }

  bool test(unsigned int bit) const {
    bt_assert(bit < N_WORDS * 64);
    return (words[bit >> 6] & (1ULL << (bit & 63))) != 0;
  }

  void clear() {
    for (size_t i = 0; i < N_WORDS; ++i) words[i] = 0;
  }

  bool empty() const {
    uint64_t acc = 0;
    for (size_t i = 0; i < N_WORDS; ++i) acc |= words[i];
    return acc == 0;
  }

  bool containsAll(const SimdBitset &target) const {
    uint64_t diff = 0;
    for (size_t i = 0; i < N_WORDS; ++i) {
      diff |= (target.words[i] & ~words[i]);
    }
    return diff == 0;
  }

  SimdBitset operator|(const SimdBitset &other) const {
    SimdBitset res;
    for (size_t i = 0; i < N_WORDS; ++i) res.words[i] = words[i] | other.words[i];
    return res;
  }

  SimdBitset operator&(const SimdBitset &other) const {
    SimdBitset res;
    for (size_t i = 0; i < N_WORDS; ++i) res.words[i] = words[i] & other.words[i];
    return res;
  }

  SimdBitset operator^(const SimdBitset &other) const {
    SimdBitset res;
    for (size_t i = 0; i < N_WORDS; ++i) res.words[i] = words[i] ^ other.words[i];
    return res;
  }

  bool operator==(const SimdBitset &other) const {
    for (size_t i = 0; i < N_WORDS; ++i) {
      if (words[i] != other.words[i]) return false;
    }
    return true;
  }

  bool operator!=(const SimdBitset &other) const {
    return !(*this == other);
  }
};

using SimdBitset256 = SimdBitset<4>;
using SimdBitset512 = SimdBitset<8>;
using SimdBitset1024 = SimdBitset<16>;
using SimdBitset2048 = SimdBitset<32>;
using SimdBitset4096 = SimdBitset<64>;
using SimdBitset8192 = SimdBitset<128>;
using SimdBitset16384 = SimdBitset<256>;
using SimdBitset32768 = SimdBitset<512>;

/**
 * Returns true if bitsets a and b have NO overlapping 1-bits ((a & b) == 0).
 */
template <size_t N_WORDS>
inline bool is_disjoint_scalar(const SimdBitset<N_WORDS> &a, const SimdBitset<N_WORDS> &b) {
  for (size_t i = 0; i < N_WORDS; ++i) {
    if ((a.words[i] & b.words[i]) != 0) return false;
  }
  return true;
}

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
template <size_t N_WORDS>
__attribute__((target("avx2")))
inline bool is_disjoint_avx2(const SimdBitset<N_WORDS> &a, const SimdBitset<N_WORDS> &b) {
  constexpr size_t N_VEC = N_WORDS / 4;
  for (size_t i = 0; i < N_VEC; ++i) {
    __m256i va = _mm256_load_si256(reinterpret_cast<const __m256i*>(&a.words[i * 4]));
    __m256i vb = _mm256_load_si256(reinterpret_cast<const __m256i*>(&b.words[i * 4]));
    if (!_mm256_testz_si256(va, vb)) return false;
  }
  return true;
}
template <size_t N_WORDS>
__attribute__((target("avx512f")))
inline bool is_disjoint_avx512(const SimdBitset<N_WORDS> &a, const SimdBitset<N_WORDS> &b) {
  if constexpr (N_WORDS >= 8) {
    constexpr size_t N_VEC = N_WORDS / 8;
    for (size_t i = 0; i < N_VEC; ++i) {
      __m512i va = _mm512_load_si512(reinterpret_cast<const void*>(&a.words[i * 8]));
      __m512i vb = _mm512_load_si512(reinterpret_cast<const void*>(&b.words[i * 8]));
      if (_mm512_test_epi64_mask(va, vb) != 0) return false;
    }
    return true;
  } else {
    return is_disjoint_avx2(a, b);
  }
}
#endif

#if defined(__aarch64__) || defined(__ARM_NEON)
template <size_t N_WORDS>
inline bool is_disjoint_neon(const SimdBitset<N_WORDS> &a, const SimdBitset<N_WORDS> &b) {
  constexpr size_t N_VEC = N_WORDS / 2;
  for (size_t i = 0; i < N_VEC; ++i) {
    uint64x2_t va = vld1q_u64(&a.words[i * 2]);
    uint64x2_t vb = vld1q_u64(&b.words[i * 2]);
    uint64x2_t c = vandq_u64(va, vb);
    if ((vgetq_lane_u64(c, 0) | vgetq_lane_u64(c, 1)) != 0) return false;
  }
  return true;
}
#endif

/**
 * Interface for hardware-vectorized exact cover solvers.
 */
class ISimdExactCover {
public:
  virtual ~ISimdExactCover() = default;

  using SolutionCallback = std::function<bool(const std::vector<unsigned int> &solution_nodes)>;

  virtual void setRequiredColumn(unsigned int col) = 0;
  /**
   * Mark a column optional (assembler_0 variable voxels): it may be covered
   * at most once like any other column (disjointness still applies), but it
   * is never selected as a pivot and the goal does not require it. An
   * optional column with no covering row left consumes hole budget instead
   * of dead-ending the search (see setHoleBudget).
   */
  virtual void setOptionalColumn(unsigned int col) = 0;
  /**
   * How many optional columns may be simultaneously uncovered and
   * uncoverable ("holes"). Mirrors assembler_0's holes budget: the search
   * prunes a node whose hole count exceeds it. There is deliberately no
   * budget check at the goal itself, matching DLX (which checks holes at
   * column-selection time but reports as soon as ring 0 is empty).
   */
  virtual void setHoleBudget(unsigned int h) = 0;
  virtual uint32_t addRow(unsigned int node_id, unsigned int piece_id, const std::vector<unsigned int> &cols) = 0;
  virtual void registerNodeAlias(unsigned int node_id, uint32_t row_idx) = 0;

  virtual void solve(
    SolutionCallback callback,
    std::stop_token stop,
    std::atomic<uint64_t> &iterations
  ) const = 0;

  /**
   * Remainder of an interrupted subtree search, collected while search()
   * unwinds after a stop. Every frame on the stack returns through a stop
   * check, so the trace sees the whole live path:
   *
   * - entry check: the node was never explored -- one piece over the full
   *   path (for the root: the subtree itself, rest position included);
   * - post-backtrack check in the frame at depth d: its current branch is
   *   fully searched and the branches after it are untouched -- each
   *   becomes a piece over the path plus that row, up to `fanout`, then
   *   one rest piece for the remainder of the list.
   *
   * These are disjoint and cover exactly the unexplored leaves; the
   * searched region is dropped. Solutions found but declined by the stop
   * are added by the caller (only it knows about delivery). An empty
   * remainder after a stop means the subtree was fully searched.
   */
  struct SubtreeRemainder {
    struct Piece {
      std::vector<unsigned int> nodes;
      // > 0: the branches of the frame at depth nodes.size(), starting at
      // this index of its active-row list (re-derived identically from
      // the replayed prefix); 0: the whole subtree under nodes
      uint32_t restSkip = 0;
    };
    unsigned int rootDepth = 0;
    uint32_t rootRestSkip = 0;
    // branches per frame emitted as singletons before the rest (the
    // shared fan-out cap, see split_fanout.h); set by the caller
    unsigned int fanout = SplitFanout::kDefault;
    std::vector<Piece> pieces;

    void addPiece(std::vector<unsigned int> nodes, uint32_t restSkip = 0) {
      pieces.push_back(Piece{std::move(nodes), restSkip});
    }
  };

  /**
   * Split on demand (a running task gives untried work to waiting
   * siblings, see split_fanout.h): polled from inside the search every `interval` nodes; when wanted() says a sibling
   * is waiting, the search finds the shallowest frame that still has
   * untried branches and offers them to give() as one rest (nodes = the
   * path down to that frame, restSkip = the frame's first untried index,
   * > 0 always). give() returns whether it took them (it may lose the
   * claim to another donor); if it did, the frame ends after its current
   * branch. `fraction` is the structural estimate of the piece's share of
   * this subtree (product of 1/branching above it x remaining/branching at
   * it, where a rest's root frame branches only over the candidates it
   * owns).
   */
  struct DonationSink {
    virtual ~DonationSink() = default;
    virtual bool wanted() = 0;
    virtual bool give(const std::vector<unsigned int> &nodes, uint32_t restSkip,
                      double fraction) = 0;
    uint64_t interval = 256;
  };

  /**
   * Search the subtree below the placed prefix. With a non-null remainder,
   * a stop leaves the unexplored part of the subtree in it (see
   * SubtreeRemainder). start > 0 resumes a rest piece: the root frame's
   * branch loop begins at that index.
   */
  virtual void solveSubtree(
    const std::vector<unsigned int> &prefix_node_ids,
    SolutionCallback callback,
    std::stop_token stop,
    std::atomic<uint64_t> &iterations,
    SubtreeRemainder *remainder = nullptr,
    size_t start = 0,
    DonationSink *donor = nullptr
  ) const = 0;

  virtual unsigned int getNumRows() const = 0;
  virtual unsigned int getNumColumns() const = 0;
  virtual unsigned int getNumPieces() const = 0;
};

/**
 * High-performance hardware-vectorized exact cover solver templated on BitsetType.
 * Supported tiers: 256, 512, 1024, 2048, 4096, 8192, 16384, and 32768 bits.
 *
 * Column capacity considerations:
 * - In Assembler 0 (DLX), usage is capped at 2048 columns (SimdBitset2048). Beyond 2048,
 *   dense bitset rows, recursion stack frames (4 KB per depth level at 32k), and per-node
 *   col_counts zeroing (128 KB at 32k) cause cache thrashing and thread stack overflow risks,
 *   making classical sparse DLX superior.
 * - Higher tiers (4096..32768) exist for tier benchmarking and Assembler 1 (SimdHuangCover).
 */
template <typename BitsetType>
class SimdExactCover : public ISimdExactCover {
public:
  struct Row {
    BitsetType mask;
    unsigned int node_id = 0;
    unsigned int piece_id = 0;
    std::vector<unsigned int> columns;
  };

  /** settings: the vector kernels it may use (solverSettings_t::avx2 ...) */
  SimdExactCover(unsigned int num_columns, unsigned int num_pieces, const solverSettings_t & settings);

  void setRequiredColumn(unsigned int col) override;
  void setRequiredColumns(const BitsetType &required);
  void setOptionalColumn(unsigned int col) override;
  void setHoleBudget(unsigned int h) override { holes = h; }
  uint32_t addRow(unsigned int node_id, unsigned int piece_id, const std::vector<unsigned int> &cols) override;
  void registerNodeAlias(unsigned int node_id, uint32_t row_idx) override;

  void solve(
    SolutionCallback callback,
    std::stop_token stop,
    std::atomic<uint64_t> &iterations
  ) const override;

  void solveSubtree(
    const std::vector<unsigned int> &prefix_node_ids,
    SolutionCallback callback,
    std::stop_token stop,
    std::atomic<uint64_t> &iterations,
    SubtreeRemainder *remainder = nullptr,
    size_t start = 0,
    DonationSink *donor = nullptr
  ) const override;

  unsigned int getNumRows() const override { return rows.size(); }
  unsigned int getNumColumns() const override { return num_columns; }
  unsigned int getNumPieces() const override { return num_pieces; }

private:
  unsigned int num_columns;
  unsigned int num_pieces;
  BitsetType required_columns;
  BitsetType optional_columns;
  std::vector<unsigned int> optional_column_list;
  unsigned int holes = 0;
  std::vector<Row> rows;
  std::vector<unsigned int> active_column_list;
  std::unordered_map<unsigned int, uint32_t> node_to_row_idx;
  bool use_avx2 = false;
  bool use_avx512 = false;
  [[maybe_unused]] bool use_neon = false;

  struct SearchContext {
    std::vector<std::vector<uint32_t>> scratch_active_rows;
    std::vector<unsigned int> current_solution;
    std::vector<uint32_t> col_counts;
    uint64_t local_iterations = 0;
    // Nodes already published to the shared counter (see flushIterations):
    // batching keeps per-node overhead at one increment plus a predictable
    // branch, while readers still observe advancing -- never frozen -- counts.
    uint64_t flushed_iterations = 0;
    // Stop-path sink (see SubtreeRemainder); null unless solveSubtree was
    // given one. Never touched per node.
    SubtreeRemainder *trace = nullptr;
    // Donation (see DonationSink); null unless solveSubtree was given one.
    // Per frame at depth d (live while its branch loop runs): the branch
    // column, its candidate count (at the root, only those from `start`
    // on), the first untried index and the loop end -- a donation lowers
    // frame_end to cut the frame. A frame's untried range only shrinks, so
    // once found closed it stays closed until the frame is re-registered:
    // live frames shallower than donate_scan_from are known closed and are
    // not rescanned.
    DonationSink *donor = nullptr;
    uint64_t next_donate_poll = 0;
    unsigned int root_depth = 0;
    unsigned int donate_scan_from = 0;
    std::vector<unsigned int> frame_col;
    std::vector<uint32_t> frame_count;
    std::vector<size_t> frame_next;
    std::vector<size_t> frame_end;
  };

  /* Donate the shallowest frame's untried branches (see DonationSink);
   * depth is the node being entered, so frames root..depth-1 are live. */
  void donateFrame(unsigned int depth, SearchContext &ctx) const;

  // Publish visited-but-unpublished nodes to the shared counter. Exact:
  // every node is published exactly once (batches of 256 plus remainders).
  // Called at each reported solution so live readers (GUI progress, the
  // Python iterator API) observe iterations > 0 as soon as the first
  // assembly exists, even for searches shorter than one batch.
  static void flushIterations(SearchContext &ctx, std::atomic<uint64_t> &iterations) {
    uint64_t unflushed = ctx.local_iterations - ctx.flushed_iterations;
    if (unflushed > 0) {
      iterations.fetch_add(unflushed, std::memory_order_relaxed);
      ctx.flushed_iterations = ctx.local_iterations;
    }
  }

  // Recursive hot path: the token is borrowed, not copied, so billions of
  // backtracking nodes pay no atomic refcount traffic. Safe: every caller
  // passes a token owned by a frame that outlives the recursion.
  // start > 0 resumes a rest: the branch loop of this (root) frame begins
  // at that index of its active-row list. Recursive calls pass 0.
  void search(
    unsigned int depth,
    const BitsetType &occupied,
    SearchContext &ctx,
    SolutionCallback &callback,
    const std::stop_token &stop,
    std::atomic<uint64_t> &iterations,
    size_t start = 0
  ) const;

  /* Stop path of the frame at depth: record its untouched branches (from
   * index next) into ctx.trace. */
  void recordFrame(unsigned int depth, unsigned int best_col, size_t next,
                   size_t end, SearchContext &ctx) const;

  void filterRows(
    const std::vector<uint32_t> &src,
    const BitsetType &chosen_mask,
    std::vector<uint32_t> &dst
  ) const;

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  void filterRowsAvx512(
    const std::vector<uint32_t> &src,
    const BitsetType &chosen_mask,
    std::vector<uint32_t> &dst
  ) const;
  void filterRowsAvx2(
    const std::vector<uint32_t> &src,
    const BitsetType &chosen_mask,
    std::vector<uint32_t> &dst
  ) const;
#elif defined(__aarch64__) || defined(__ARM_NEON)
  void filterRowsNeon(
    const std::vector<uint32_t> &src,
    const BitsetType &chosen_mask,
    std::vector<uint32_t> &dst
  ) const;
#endif
};

using SimdExactCover256 = SimdExactCover<SimdBitset256>;
using SimdExactCover512 = SimdExactCover<SimdBitset512>;
using SimdExactCover1024 = SimdExactCover<SimdBitset1024>;
using SimdExactCover2048 = SimdExactCover<SimdBitset2048>;
using SimdExactCover4096 = SimdExactCover<SimdBitset4096>;
using SimdExactCover8192 = SimdExactCover<SimdBitset8192>;
using SimdExactCover16384 = SimdExactCover<SimdBitset16384>;
using SimdExactCover32768 = SimdExactCover<SimdBitset32768>;

#endif // __SIMD_EXACT_COVER_H__
