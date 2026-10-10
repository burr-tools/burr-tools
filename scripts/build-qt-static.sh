#!/usr/bin/env bash
# Build burrtools-qt as one standalone Windows program: Qt linked in
# statically (MSYS2's mingw-w64-ucrt-x86_64-qt6-static), the C and C++
# runtimes too, so it needs nothing beside it but Windows 10 or later.
#
# usage: scripts/build-qt-static.sh [build dir] [output dir]
#        (defaults: build-static, artifacts/qt-static; run from the project
#        root in an MSYS2 UCRT64 shell)
#
# Writes <output dir>/burrtools-qt-<version>-windows-<arch>-standalone.zip
# (the program and the example puzzles) and the program itself beside it.
# The release workflow (qt-standalone.yml) runs this; `just build-qt-static`
# too. How the static link works: meson -Dqt_static=true, see
# scripts/qt_static_link.py and src/qtgui/meson.build.
set -euo pipefail

BUILD="${1:-build-static}"
OUT="${2:-artifacts/qt-static}"

case "$(uname -s)" in
  MINGW* | MSYS*) ;;
  *) echo "build-qt-static.sh: Windows (MSYS2) only; other platforms ship scripts/package-qt.sh's bundles" >&2
     exit 1 ;;
esac

PREFIX="${MINGW_PREFIX:-/ucrt64}"
QMAKE="$PREFIX/qt6-static/bin/qmake6.exe"
if [ ! -x "$QMAKE" ]; then
  echo "build-qt-static.sh: no static Qt at $PREFIX/qt6-static;" \
       "install mingw-w64-ucrt-x86_64-qt6-static" >&2
  exit 1
fi

# meson finds the static Qt through its qmake, named in a native file; the
# file stays in the build directory, as meson reads it again whenever it
# regenerates the build
if [ ! -f "$BUILD/build.ninja" ]; then
  mkdir -p "$BUILD"
  NATIVE="$BUILD/qt-static.ini"
  printf "[binaries]\nqmake = '%s'\n" "$(cygpath -m "$QMAKE")" > "$NATIVE"
  meson setup "$BUILD" --buildtype=release -Db_ndebug=true -Dpython=disabled \
    -Dqt_gui=enabled -Dqt_static=true --native-file "$(cygpath -m "$NATIVE")"
fi
ninja -C "$BUILD" burrtools-qt.exe

VERSION="$(git describe --tags --always --dirty 2>/dev/null || echo dev)"
NAME="burrtools-qt-${VERSION}-windows-$(uname -m)-standalone"
mkdir -p "$OUT"
rm -rf "${OUT:?}/$NAME" "$OUT/$NAME.zip"
mkdir -p "$OUT/$NAME"
cp "$BUILD/burrtools-qt.exe" "$OUT/$NAME/"
# the symbol table is more than half the file
strip -s "$OUT/$NAME/burrtools-qt.exe"

# Standalone means it loads only Windows' own DLLs: none of them may be one
# that only MSYS2 has (a library linked dynamically by mistake).
leaked=""
for dll in $(objdump -p "$OUT/$NAME/burrtools-qt.exe" | awk '/DLL Name/ { print $3 }'); do
  if [ -e "$PREFIX/bin/$dll" ] && [ ! -e "/c/Windows/System32/$dll" ]; then
    leaked="$leaked $dll"
  fi
done
if [ -n "$leaked" ]; then
  echo "build-qt-static.sh: burrtools-qt.exe still needs MSYS2 libraries:$leaked" >&2
  exit 1
fi
"$OUT/$NAME/burrtools-qt.exe" --self-check

cp -r examples "$OUT/$NAME/"
(cd "$OUT" && zip -qr9 "$NAME.zip" "$NAME")
cp "$OUT/$NAME/burrtools-qt.exe" "$OUT/burrtools-qt-${VERSION}-windows-$(uname -m)-standalone.exe"
rm -rf "${OUT:?}/$NAME"
ls -la "$OUT"
