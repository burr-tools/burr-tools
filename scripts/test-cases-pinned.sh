#!/usr/bin/env bash
# Run each test case of a Catch2 binary as its own process, one worker per
# CPU, each worker pinned to its CPU, and fail if any case fails. Made for
# the coverage build, whose gcov counters are atomic (GCC's default with
# -pthread): a case's solver threads sharing one CPU update them without
# contention, while threads on different CPUs fight over every counter in
# the hot loops -- unpinned, one 4-thread case took over 35 minutes instead
# of 11. The processes' counters add up, as gcov merges each .gcda under a
# lock.
#
# A queue rather than fixed shards: Catch2's --shard-index splits the cases
# in contiguous blocks, and the heavy solver cases sit together, so one
# shard got 277 s of work and most under 5 s. Each worker takes the next
# case when it is done, which bounds the wall time by the total over the
# workers plus the longest case.
#
# Without taskset or GNU xargs' --process-slot-var (macOS) the workers run
# unpinned.
#
# usage: scripts/test-cases-pinned.sh <test binary> <test spec> [catch2 arguments...]
set -euo pipefail

bin=$1
spec=$2
shift 2
n=$(nproc 2>/dev/null || sysctl -n hw.ncpu)
logs=$(mktemp -d)
trap 'rm -rf "$logs"' EXIT

# Worker k runs on the k-th CPU this process may use (not CPU k: a
# container's CPU set need not start at 0).
slot=()
cpus=""
if command -v taskset >/dev/null && xargs --process-slot-var=SLOT true </dev/null 2>/dev/null; then
    slot=(--process-slot-var=SLOT)
    IFS=, read -ra ranges < <(sed -n 's/^Cpus_allowed_list:[[:space:]]*//p' /proc/self/status)
    for r in "${ranges[@]}"; do
        for ((c = ${r%-*}; c <= ${r#*-}; c++)); do cpus="$cpus $c"; done
    done
fi
export CPUS=$cpus

# The queue: each case's number and name, the name escaped, as Catch2 reads
# , [ ] * " ~ and \ in a test spec as syntax.
"$bin" "$spec" --list-tests --verbosity quiet >"$logs/cases"
total=$(grep -c '' "$logs/cases" || true)
i=0
while IFS= read -r name; do
    i=$((i + 1))
    printf '%s\0%s\0' "$i" "$(printf '%s' "$name" | sed -e 's/\\/\\\\/g' -e 's/\([],[*"~]\)/\\\1/g')"
done <"$logs/cases" >"$logs/queue"

# xargs appends a case's number and name to: log directory, binary, arguments
status=0
xargs -0 -n 2 -P "$n" ${slot[@]+"${slot[@]}"} bash -c '
    args=("$@")
    last=$((${#args[@]} - 1))   # no negative indices: macOS has bash 3.2
    number=${args[last - 1]} name=${args[last]} logs=${args[0]}
    cmd=("${args[1]}" "$name" "${args[@]:2:${#args[@]}-4}")
    if [ -n "${SLOT:-}" ]; then
        read -ra cpus <<<"$CPUS"
        cmd=(taskset -c "${cpus[SLOT]}" "${cmd[@]}")
    fi
    # 4: every test skipped -- a case that SKIPs itself, run on its own
    "${cmd[@]}" >"$logs/$number.log" 2>&1 || [ $? = 4 ] || { mv "$logs/$number.log" "$logs/$number.failed"; exit 1; }
' bash "$logs" "$bin" "$@" <"$logs/queue" || status=$?

failed=$(find "$logs" -name '*.failed' | wc -l)
for f in "$logs"/*.failed; do
    [ -e "$f" ] || continue
    echo "== FAILED: $(sed -n "$(basename "$f" .failed)p" "$logs/cases")"
    cat "$f"
done
echo "$total test cases in $n pinned workers: $((total - failed)) passed, $failed failed"
exit $(( status != 0 || failed != 0 ))
