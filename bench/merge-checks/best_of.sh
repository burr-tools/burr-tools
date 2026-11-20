#!/bin/bash
# best_of.sh [-n RUNS] "VAR=value VAR=value" burrTxt-arguments...
# Wall time of the fastest of RUNS runs (default 3) of the release burrTxt with the given
# environment, e.g.
#   bench/merge-checks/best_of.sh "BURRTOOLS_BITPARALLEL=0" -t 1 examples/SolidSixPieceBurrs.xmpuzzle
# Single runs on a laptop vary by 10% and more; the minimum is what compares.
set -u
runs=3
if [ "${1:-}" = "-n" ]; then runs="$2"; shift 2; fi
envs="${1?environment string (may be empty)}"; shift
binary="${BURRTXT:-$(dirname "$0")/../../build-rel/burrTxt}"
best=""
for _ in $(seq "$runs"); do
  start=$(date +%s%N)
  # shellcheck disable=SC2086
  status=0
  env $envs "$binary" -q -n "$@" >/dev/null 2>&1 || status=$?
  if [ "$status" -ne 0 ]; then
    echo "best_of.sh: $binary failed (exit $status)" >&2
    exit "$status"
  fi
  ms=$(( ($(date +%s%N) - start) / 1000000 ))
  if [ -z "$best" ] || [ "$ms" -lt "$best" ]; then best=$ms; fi
done
printf '%d.%02ds\n' $((best / 1000)) $(((best % 1000) / 10))
