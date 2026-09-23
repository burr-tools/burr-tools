#!/usr/bin/env bash
# ==============================================================================
# bench/run_suite.sh — BurrTools Solver Benchmark Suite
#
# Runs an interleaved A/B benchmark comparing base (single-threaded) vs parallel
# assembler across a curated corpus of puzzles, measuring:
#   - Wall-clock solve time & speedup factor
#   - User & System CPU times and multi-core utilization (%)
#   - Peak resident memory usage (RSS in MB) & memory delta
#   - Correctness verification (assemblies, solutions, iterations)
#
# Usage:
#   ./bench/run_suite.sh [options] [puzzle:problem ...]
#
# Options:
#   --runs N          Number of interleaved runs per binary (default: 3)
#   --timeout SECS    Max execution time per solve (default: 600s = 10 min)
#   --output FILE     Output CSV file (default: bench/results/results_<timestamp>.csv)
#   --base PATH       Path to base single-threaded binary (default: build/burrTxt-base)
#   --new PATH        Path to current binary (default: build/burrTxt)
#   --list            List the curated puzzle suite with descriptions and exit
#   -h, --help        Show this help message
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BASE_BIN="${REPO_DIR}/build/burrTxt-base"
NEW_BIN="${REPO_DIR}/build/burrTxt"
RUNS=3
TIMEOUT=600
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
OUT_CSV="${SCRIPT_DIR}/results/results_${TIMESTAMP}.csv"
LIST_ONLY=0

# ------------------------------------------------------------------------------
# Curated Puzzle Benchmark Suite
# Each entry contains:
#   PATH:PROB_IDX | DESCRIPTION & CHARACTERISTICS
# ------------------------------------------------------------------------------
PUZZLE_DEFINITIONS=(
  # --- Pure Assembly: Unique Pieces (assembler_0_c Knuth DLX) ---
  "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:1|George Bell 10x10 Lomino Square (prob 1): Pure exact-cover assembly, 11 unique pieces (assembler_0 DLX). ~2s baseline, ~2.6x speedup."
  "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:2|George Bell 10x10 Alt Lomino Square (prob 2): Pure exact-cover assembly, 11 unique pieces (assembler_0 DLX). ~2s baseline, ~2.5x speedup."
  "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:0|George Bell 9x9 Lomino Square (prob 0): Pure exact-cover assembly, 10 unique pieces (assembler_0 DLX). ~0.4s baseline, ~2.4x speedup."

  # --- Mixed Assembly + Disassembly: Unique Pieces (assembler_0_c + disassembler) ---
  "puzzles/BTFiles/James Fortune/kangaroo.xmpuzzle|James Fortune Kangaroo: 6 unique pieces (assembler_0 DLX). 9,831 assemblies (~1.1s) + level 10 disassembly (~0.6s). ~1.7s total."
  "puzzles/BTFiles/James Fortune/unlucky block.xmpuzzle|James Fortune Unlucky Block: 7 unique pieces (assembler_0 DLX). Interlocking block assembly + disassembly."
  "puzzles/BTFiles/Jack Krijnen/Excelsior.xmpuzzle|Jack Krijnen Excelsior: 6 unique pieces (assembler_0 DLX). 7 assemblies (~1.8s) + level 14 disassembly."

  # --- Puzzles with Duplicate Shapes / Ranges (assembler_1_c Huang Algorithm) ---
  "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:3|George Bell 11x11 Lomino Square (prob 3): Pure assembly with piece ranges (assembler_1). Heavier search (~1-5 min)."
  "examples/SolidSixPieceBurrs.xmpuzzle|Solid Six Piece Burrs: 6 pieces with duplicate stick shapes (assembler_1). 588 assemblies (~7.5s), 179 solutions, level 1 disassembly."
  "puzzles/BTFiles/Jack Krijnen/Simplicity.xmpuzzle|Jack Krijnen Simplicity: Interlocking burr with duplicate pieces (assembler_1). 188 assemblies (~3.2s), level 10 disassembly."
  "puzzles/BTFiles/Jack Krijnen/BottomLine.xmpuzzle|Jack Krijnen BottomLine: Duplicate pieces (assembler_1). 76 assemblies (~1.1s), level 11 disassembly."
  "puzzles/BTFiles/Jack Krijnen/Tippy.xmpuzzle|Jack Krijnen Tippy: Duplicate pieces (assembler_1). 460 assemblies, 111 solutions (~0.6s)."
  "puzzles/BTFiles/Tyler Hudson/Third_Times_the_Charm.xmpuzzle|Tyler Hudson Third Times the Charm: Duplicate piece shapes (assembler_1). 71 assemblies (~16s)."

  # --- Micro-Puzzles (Overhead & Latency Lower Bound) ---
  "examples/PelikanBurr.xmpuzzle|Pelikan Burr: Classic 6-piece burr, unique pieces. Micro-search (~5ms assembly, level 4 disassembly). ~0.15s total."
  "examples/DraculasDentalDesaster.xmpuzzle|Dracula's Dental Desaster: 6 unique pieces. 84 assemblies (~8ms assembly, level 8 disassembly). ~0.14s total."
)

# Parse command-line flags
CUSTOM_PUZZLES=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --runs)
      RUNS="$2"
      shift 2
      ;;
    --timeout)
      TIMEOUT="$2"
      shift 2
      ;;
    --output)
      OUT_CSV="$2"
      shift 2
      ;;
    --base)
      BASE_BIN="$2"
      shift 2
      ;;
    --new)
      NEW_BIN="$2"
      shift 2
      ;;
    --list)
      LIST_ONLY=1
      shift
      ;;
    -h|--help)
      grep '^#' "$0" | cut -c 3-
      exit 0
      ;;
    *)
      CUSTOM_PUZZLES+=("$1")
      shift
      ;;
  esac
done

cd "${REPO_DIR}"

if [[ ${LIST_ONLY} -eq 1 ]]; then
  echo "================================================================================"
  echo "Curated BurrTools Benchmark Suite (${#PUZZLE_DEFINITIONS[@]} puzzles defined)"
  echo "================================================================================"
  for def in "${PUZZLE_DEFINITIONS[@]}"; do
    path="${def%%|*}"
    desc="${def#*|}"
    file_only="${path%%:*}"
    status="[present]"
    if [[ ! -f "${file_only}" ]]; then
      status="[missing]"
    fi
    printf "%-10s %-60s\n           %s\n" "${status}" "${path}" "${desc}"
  done
  exit 0
fi

# Check binaries
if [[ ! -x "${BASE_BIN}" ]]; then
  echo "Error: Base binary '${BASE_BIN}' not found."
  echo ""
  echo "How to create the base binary:"
  echo "  1. git checkout <base-commit-or-branch>   # e.g. disassembler-optimizations"
  echo "  2. ninja -C build burrTxt"
  echo "  3. cp build/burrTxt build/burrTxt-base"
  echo "  4. git checkout assembler-parallel"
  echo ""
  exit 1
fi

if [[ ! -x "${NEW_BIN}" ]]; then
  echo "Current binary '${NEW_BIN}' not found. Compiling with ninja..."
  ninja -C build burrTxt
fi

# Select puzzles to run
ACTIVE_PUZZLES=()
echo "================================================================================"
echo "BurrTools Interleaved A/B Benchmark Suite"
echo "================================================================================"
echo "Base Binary:    ${BASE_BIN}"
echo "Current Binary: ${NEW_BIN}"
echo "Interleaved:    ${RUNS} runs per binary (alternating)"
echo "Timeout:        ${TIMEOUT}s per solve (max)"
echo "Output CSV:     ${OUT_CSV}"
echo "--------------------------------------------------------------------------------"
echo "Checking puzzle suite availability:"

if [[ ${#CUSTOM_PUZZLES[@]} -gt 0 ]]; then
  for p in "${CUSTOM_PUZZLES[@]}"; do
    file_only="${p%%:*}"
    if [[ -f "${file_only}" ]]; then
      echo "  [OK]      ${p}"
      ACTIVE_PUZZLES+=("${p}")
    else
      echo "  [MISSING] ${p} (file not found on disk, skipping)"
    fi
  done
else
  for def in "${PUZZLE_DEFINITIONS[@]}"; do
    path="${def%%|*}"
    desc="${def#*|}"
    file_only="${path%%:*}"
    if [[ -f "${file_only}" ]]; then
      printf "  [OK]      %-58s\n            -> %s\n" "${path}" "${desc}"
      ACTIVE_PUZZLES+=("${path}")
    else
      printf "  [SKIP]    %-58s (file not found)\n" "${path}"
    fi
  done
fi

if [[ ${#ACTIVE_PUZZLES[@]} -eq 0 ]]; then
  echo "Error: No matching puzzle files found to benchmark."
  exit 1
fi

echo "--------------------------------------------------------------------------------"
echo "Running benchmark on ${#ACTIVE_PUZZLES[@]} puzzles..."
echo "================================================================================"

mkdir -p "$(dirname "${OUT_CSV}")"

uv run python3 "${SCRIPT_DIR}/bench_solve.py" \
  --ab "${BASE_BIN}" "${NEW_BIN}" \
  --runs "${RUNS}" \
  --timeout "${TIMEOUT}" \
  --output "${OUT_CSV}" \
  "${ACTIVE_PUZZLES[@]}"

echo ""
echo "================================================================================"
echo "Benchmark completed successfully!"
echo "Raw results & summary saved to: ${OUT_CSV}"
echo "================================================================================"
