#!/usr/bin/env bash
# Profiles of burrtools-qt that need no person at the screen: start-up time,
# a QML profile (qmlprofiler) and a heap profile (heaptrack). Each run opens
# a puzzle and quits after --screenshot (a fixed 1.5 s wait, in every time
# below). Reports, not tests: their numbers vary from machine to machine.
#
# usage: scripts/profile-qt.sh <build dir> <output dir> [startup] [qml] [heap]
#        (all three when none is named; from the project root)
#
#   startup  cold (fresh settings: no pipeline cache) and warm starts, RUNS
#            each (default 5): startup.txt
#   qml      a qmlprofiler trace, qml.qtd (open it in Qt Creator); needs a
#            build with -Dqml_debug=true and qmlprofiler on the PATH
#   heap     a heaptrack profile, heap.*.zst, and heap.txt (heaptrack_print's
#            summary); Linux, heaptrack on the PATH
#
# Headless runs: set QT_QPA_PLATFORM=offscreen. BURRTOOLS_PROFILE_PUZZLE
# names another puzzle (default examples/PelikanBurr.xmpuzzle).
# `just startup-time`, `just profile-qml`, `just heap-qt` and the Qt profile
# workflow (.github/workflows/qt-profile.yml) run this.
set -euo pipefail

BUILD="${1:?build dir}"
OUT="${2:?output dir}"
shift 2
WHAT=("$@")
[ ${#WHAT[@]} -eq 0 ] && WHAT=(startup qml heap)

EXE="$BUILD/burrtools-qt"
[ -x "$EXE.exe" ] && EXE="$EXE.exe"
[ -x "$EXE" ] || { echo "profile-qt.sh: no $EXE (build the Qt GUI first)" >&2; exit 1; }
PUZZLE="${BURRTOOLS_PROFILE_PUZZLE:-examples/PelikanBurr.xmpuzzle}"
RUNS="${RUNS:-5}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
SCRATCH="$(mktemp -d)"
trap 'rm -rf "$SCRATCH"' EXIT

# a path the program understands (Windows wants mixed paths from MSYS2)
native() { if command -v cygpath > /dev/null; then cygpath -m "$1"; else echo "$1"; fi; }

# one run with settings in $1 (a directory); prints its wall time in ms
timed_run() {
  local t0 t1
  t0=$(date +%s%N)
  BURRTOOLS_QT_SETTINGS="$(native "$1/s.rc")" "$EXE" "$(native "$PUZZLE")" \
    "--screenshot=$(native "$1/shot.png")" > /dev/null 2>&1
  t1=$(date +%s%N)
  echo $(( (t1 - t0) / 1000000 ))
}

median() { printf '%s\n' "$@" | sort -n | awk '{ v[NR] = $1 } END { print (NR % 2) ? v[(NR + 1) / 2] : int((v[NR / 2] + v[NR / 2 + 1]) / 2) }'; }

for w in "${WHAT[@]}"; do
  case "$w" in

    startup)
      cold=(); warm=()
      for i in $(seq 1 "$RUNS"); do
        d="$SCRATCH/cold$i"; mkdir -p "$d"; cold+=("$(timed_run "$d")")
      done
      d="$SCRATCH/warm"; mkdir -p "$d"; timed_run "$d" > /dev/null        # fills the pipeline cache
      for i in $(seq 1 "$RUNS"); do warm+=("$(timed_run "$d")"); done
      {
        echo "burrtools-qt start-up, $PUZZLE, --screenshot (includes its fixed 1.5 s), ms"
        echo "cold (no pipeline cache): ${cold[*]}  median $(median "${cold[@]}")"
        echo "warm:                     ${warm[*]}  median $(median "${warm[@]}")"
      } | tee "$OUT/startup.txt"
      ;;

    qml)
      command -v qmlprofiler > /dev/null || { echo "profile-qt.sh: qmlprofiler not on the PATH" >&2; exit 1; }
      d="$SCRATCH/qml"; mkdir -p "$d"
      BURRTOOLS_QT_SETTINGS="$(native "$d/s.rc")" \
        qmlprofiler -o "$(native "$OUT/qml.qtd")" "$EXE" "$(native "$PUZZLE")" "--screenshot=$(native "$d/shot.png")"
      test -s "$OUT/qml.qtd" || { echo "profile-qt.sh: no trace (was the build made with -Dqml_debug=true?)" >&2; exit 1; }
      echo "QML profile: $OUT/qml.qtd ($(wc -c < "$OUT/qml.qtd") bytes; open it in Qt Creator)"
      ;;

    heap)
      command -v heaptrack > /dev/null || { echo "profile-qt.sh: heaptrack not on the PATH (Linux)" >&2; exit 1; }
      d="$SCRATCH/heap"; mkdir -p "$d"
      BURRTOOLS_QT_SETTINGS="$d/s.rc" heaptrack -o "$OUT/heap" "$EXE" "$PUZZLE" "--screenshot=$d/shot.png" > /dev/null
      heaptrack_print "$OUT"/heap.*zst > "$OUT/heap.txt"
      grep -E "^(total runtime|calls to allocation functions|temporary memory allocations|peak heap memory consumption|peak RSS|total memory leaked)" \
        "$OUT/heap.txt" | tee "$OUT/heap-summary.txt"
      ;;

    *)
      echo "profile-qt.sh: unknown profile '$w' (startup, qml or heap)" >&2
      exit 1
      ;;
  esac
done
