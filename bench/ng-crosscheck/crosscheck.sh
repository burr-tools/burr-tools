#!/bin/bash
# Run this repository's burrTxt and NG's ngbench on the same problems and
# compare assembly (and optionally solution) counts. See README.md.
set -u

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd "$here/../.." && pwd)
ngbin=""
burr="$repo/build-rel/burrTxt"
threads=1
disasm=0
described=0
tmo=240
problems=()

usage() {
  echo "usage: $0 --ng-bin <ngbench> [--burrtxt <burrTxt>] [--threads N] [--disasm] [--described] [--timeout S] <file.xmpuzzle>[:problemIndex] ..." >&2
  exit 2
}

while [ $# -gt 0 ]; do
  case "$1" in
    --ng-bin)  [ $# -ge 2 ] || usage; ngbin=$2; shift 2 ;;
    --burrtxt) [ $# -ge 2 ] || usage; burr=$2; shift 2 ;;
    --threads) [ $# -ge 2 ] || usage; threads=$2; shift 2 ;;
    --timeout) [ $# -ge 2 ] || usage; tmo=$2; shift 2 ;;
    --disasm)  disasm=1; shift ;;
    --described) described=1; shift ;;
    -h|--help) usage ;;
    -*)        echo "unknown option: $1" >&2; usage ;;
    *)         problems+=("$1"); shift ;;
  esac
done

[ -n "$ngbin" ] && [ -x "$ngbin" ] || { echo "ngbench not found: '$ngbin' (use --ng-bin)" >&2; exit 2; }
[ -x "$burr" ] || { echo "burrTxt not found: '$burr' (use --burrtxt)" >&2; exit 2; }
[ ${#problems[@]} -gt 0 ] || usage

dflag=()
[ "$disasm" = 1 ] && dflag=(-d)
# --described: our side solves the puzzle converted to its described grid
# (burrTxt -D), the grid NG's importer converts it to as well
[ "$described" = 1 ] && dflag+=(-D)
bad=0

now() { date +%s.%N; }

for spec in "${problems[@]}"; do
  file=$spec
  idx=0
  # A trailing ":<digits>" is the problem index (paths may contain ':' otherwise).
  if [[ "$spec" =~ ^(.*):([0-9]+)$ ]]; then
    file=${BASH_REMATCH[1]}
    idx=${BASH_REMATCH[2]}
  fi

  t0=$(now)
  out=$(timeout "$tmo" "$burr" -q -n -t "$threads" "${dflag[@]}" -o "$idx" "$file" 2>&1)
  t1=$(now)
  our_time=$(echo "$t1 - $t0" | bc -l)
  our_line=$(grep -Eo '[0-9]+ assemblies and [0-9]+ solutions' <<<"$out" | tail -1)

  t0=$(now)
  out=$(timeout "$tmo" "$ngbin" "$file" "$threads" "$disasm" "$idx" 2>&1)
  t1=$(now)
  ng_time=$(echo "$t1 - $t0" | bc -l)
  ng_line=$(grep -E 'assemblies=[0-9]+ solutions=[0-9]+' <<<"$out" | tail -1)

  status=OK
  if [ -z "$our_line" ] || [ -z "$ng_line" ]; then
    status=TIMEOUT/NONE
  else
    our_a=$(sed -E 's/^([0-9]+) assemblies and ([0-9]+) solutions$/\1/' <<<"$our_line")
    our_s=$(sed -E 's/^([0-9]+) assemblies and ([0-9]+) solutions$/\2/' <<<"$our_line")
    ng_a=$(sed -E 's/.*assemblies=([0-9]+).*/\1/' <<<"$ng_line")
    ng_s=$(sed -E 's/.*solutions=([0-9]+).*/\1/' <<<"$ng_line")
    [ "$our_a" = "$ng_a" ] || status=MISMATCH
    if [ "$disasm" = 1 ] && [ "$our_s" != "$ng_s" ]; then status=MISMATCH; fi
    our_sum="$our_a/$our_s"; ng_sum="$ng_a/$ng_s"
  fi
  [ "$status" = OK ] || bad=1

  printf '%s:%s | t=%s d=%s | ours %.2fs %s | ng %.2fs %s | %s\n' \
    "$(basename "$file")" "$idx" "$threads" "$disasm" \
    "$our_time" "${our_sum:-none}" "$ng_time" "${ng_sum:-none}" "$status"
  unset our_sum ng_sum
done

exit $bad
