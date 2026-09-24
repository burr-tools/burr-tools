#!/usr/bin/env bash
# ==============================================================================
# bench/run_snapshot.sh — BurrTools Single-Commit Solver Snapshot Benchmark
#
# Runs bench/bench_solve.py in single-binary mode over a fixed puzzle corpus
# (the run_suite.sh set plus the Jack Krijnen Supernova problems) with
# disassembly enabled, and stores the result CSV under a self-identifying
# name carrying the timestamp, git hash and commit subject:
#
#   bench/results/results_<YYYYMMDD_HHMMSS>_<short-hash>[-dirty]_<subject-slug>.csv
#
# Re-run after each commit of interest; the filenames make results comparable
# across commits without a separate ledger. A "-dirty" suffix marks runs from
# a worktree with uncommitted changes so they can't be mistaken for the
# commit itself.
#
# Usage:
#   ./bench/run_snapshot.sh [options] [puzzle:problem ...]
#
# Options:
#   --runs N          Number of runs per puzzle (default: 3)
#   --timeout SECS    Max execution time per solve (default: 600s = 10 min)
#   --threads N       Pass -t N to burrTxt (default: binary default, 0 = auto)
#   --output FILE     Output CSV file (default: auto-generated name, see above)
#   --binary PATH     burrTxt binary to measure (default: build/burrTxt)
#   --list            List the snapshot puzzle set with availability and exit
#   -h, --help        Show this help message
#
# Examples:
#   ./bench/run_snapshot.sh
#   ./bench/run_snapshot.sh --runs 5 "puzzles/BTFiles/Jack Krijnen/Supernova.xmpuzzle:0"
#   ./bench/run_snapshot.sh --threads 1 --timeout 120 examples/PelikanBurr.xmpuzzle
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BIN="${REPO_DIR}/build/burrTxt"
RUNS=3
TIMEOUT=600
THREADS=""
OUT_CSV=""
LIST_ONLY=0

# ------------------------------------------------------------------------------
# Snapshot Puzzle Corpus: run_suite.sh set plus the reported Supernova case.
# "path[:problem]" entries; disassembly (-d) is always enabled.
# ------------------------------------------------------------------------------
PUZZLE_DEFINITIONS=(
  "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:1"
  "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:2"
  "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:0"
  "puzzles/BTFiles/James Fortune/kangaroo.xmpuzzle"
  "puzzles/BTFiles/James Fortune/unlucky block.xmpuzzle"
  "puzzles/BTFiles/Jack Krijnen/Excelsior.xmpuzzle"
  "puzzles/BTFiles/George Bell/LominoSquareProblems9-15.xmpuzzle:3"
  "examples/SolidSixPieceBurrs.xmpuzzle"
  "puzzles/BTFiles/Jack Krijnen/Simplicity.xmpuzzle"
  "puzzles/BTFiles/Jack Krijnen/BottomLine.xmpuzzle"
  "puzzles/BTFiles/Jack Krijnen/Tippy.xmpuzzle"
  "puzzles/BTFiles/Tyler Hudson/Third_Times_the_Charm.xmpuzzle"
  "examples/PelikanBurr.xmpuzzle"
  "examples/DraculasDentalDesaster.xmpuzzle"
  "puzzles/BTFiles/Jack Krijnen/Supernova.xmpuzzle:0"
  "puzzles/BTFiles/Jack Krijnen/Supernova.xmpuzzle:1"
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
    --threads)
      THREADS="$2"
      shift 2
      ;;
    --output)
      OUT_CSV="$2"
      shift 2
      ;;
    --binary)
      BIN="$2"
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
  echo "Snapshot Benchmark Corpus (${#PUZZLE_DEFINITIONS[@]} puzzles defined)"
  echo "================================================================================"
  for path in "${PUZZLE_DEFINITIONS[@]}"; do
    file_only="${path%%:*}"
    status="[present]"
    if [[ ! -f "${file_only}" ]]; then
      status="[missing]"
    fi
    printf "%-10s %s\n" "${status}" "${path}"
  done
  exit 0
fi

# Check binary (build it if missing, deriving the build dir from its path so
# --binary build-rel/burrTxt and friends work on a fresh clone too)
if [[ ! -x "${BIN}" ]]; then
  echo "Binary '${BIN}' not found. Compiling with ninja..."
  ninja -C "$(dirname "${BIN}")" "$(basename "${BIN}" .exe)"
fi

# Select puzzles to run
ACTIVE_PUZZLES=()
if [[ ${#CUSTOM_PUZZLES[@]} -gt 0 ]]; then
  for p in "${CUSTOM_PUZZLES[@]}"; do
    file_only="${p%%:*}"
    if [[ -f "${file_only}" ]]; then
      ACTIVE_PUZZLES+=("${p}")
    else
      echo "  [MISSING] ${p} (file not found on disk, skipping)"
    fi
  done
else
  for path in "${PUZZLE_DEFINITIONS[@]}"; do
    file_only="${path%%:*}"
    if [[ -f "${file_only}" ]]; then
      ACTIVE_PUZZLES+=("${path}")
    else
      echo "  [SKIP]    ${path} (file not found)"
    fi
  done
fi

if [[ ${#ACTIVE_PUZZLES[@]} -eq 0 ]]; then
  echo "Error: No matching puzzle files found to benchmark."
  exit 1
fi

# Self-identifying output name: timestamp + git hash (+ -dirty) + subject slug
if [[ -z "${OUT_CSV}" ]]; then
  TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
  GIT_HASH="$(git rev-parse --short HEAD)"
  GIT_SUBJECT="$(git log -1 --format=%s)"
  if [[ -n "$(git status --porcelain)" ]]; then
    GIT_HASH="${GIT_HASH}-dirty"
  fi
  # Slug: lowercase, runs of non-alphanumerics become "-", trimmed, max 40 chars
  SLUG="$(printf '%s' "${GIT_SUBJECT}" | tr '[:upper:]' '[:lower:]' \
    | sed -e 's/[^a-z0-9]\+/-/g; s/^-\+//; s/-\+$//' | cut -c 1-40)"
  [[ -z "${SLUG}" ]] && SLUG="nosubject"
  mkdir -p "${SCRIPT_DIR}/results"
  OUT_CSV="${SCRIPT_DIR}/results/results_${TIMESTAMP}_${GIT_HASH}_${SLUG}.csv"
fi

echo "================================================================================"
echo "BurrTools Single-Commit Snapshot Benchmark"
echo "================================================================================"
echo "Binary:       ${BIN}"
echo "Commit:       $(git rev-parse HEAD) $(git log -1 --format=%s)"
echo "Timestamp:    $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "Runs/puzzle:  ${RUNS} (with disassembly)"
echo "Timeout:      ${TIMEOUT}s per solve (max)"
echo "Threads:      ${THREADS:-default}"
echo "Puzzles:      ${#ACTIVE_PUZZLES[@]}"
echo "Output CSV:   ${OUT_CSV}"
echo "================================================================================"

THREAD_ARGS=()
if [[ -n "${THREADS}" ]]; then
  THREAD_ARGS=(--threads "${THREADS}")
fi

python3 "${SCRIPT_DIR}/bench_solve.py" \
  --binary "${BIN}" \
  --runs "${RUNS}" \
  --timeout "${TIMEOUT}" \
  --output "${OUT_CSV}" \
  "${THREAD_ARGS[@]}" \
  "${ACTIVE_PUZZLES[@]}"

echo ""
echo "================================================================================"
echo "Snapshot completed successfully!"
echo "Results saved to: ${OUT_CSV}"
echo "================================================================================"
