#!/usr/bin/env bash
# Package burrtools-qt -- the redesigned Qt 6 Quick GUI (src/qtgui) -- as a
# self-contained preview build for the platform this runs on, so people can
# try it without building it:
#
#   Windows (MSYS2 UCRT64)  a folder with every DLL it needs, zipped
#   macOS                   burrtools-qt.app made by macdeployqt, zipped
#   Linux                   an AppImage made by linuxdeploy and its Qt plugin
#
# usage: scripts/package-qt.sh [build dir] [output dir]
#        (defaults: build-rel, artifacts/qt; run from the project root)
#
# CI runs it after the Qt tests (build-and-release.yml) and uploads the
# result as a workflow artifact; it is not part of a release.
set -euo pipefail

BUILD="${1:-build-rel}"
OUT="${2:-artifacts/qt}"
VERSION="$(git describe --tags --always --dirty 2>/dev/null || echo dev)"
ARCH="$(uname -m)"
mkdir -p "$OUT"

case "$(uname -s)" in

  MINGW* | MSYS* | CYGWIN*)
    NAME="burrtools-qt-${VERSION}-windows-${ARCH}"
    DIR="$OUT/$NAME"
    rm -rf "$DIR"
    mkdir -p "$DIR"
    cp "$BUILD/src/qtgui/burrtools-qt.exe" "$DIR/"
    # Qt and what the QML imports need -- not the plugins the program never
    # loads: QML debugging, touch input, network information, TLS, and the
    # image formats besides SVG (PNG is built into Qt). Direct3D's shader
    # compiler comes with Windows 10 and later.
    windeployqt6 --qmldir src/qtgui/qml --no-translations --no-opengl-sw \
      --no-system-d3d-compiler --no-system-dxc-compiler \
      --skip-plugin-types qmltooling,generic,networkinformation,tls \
      --exclude-plugins qgif,qico,qjpeg \
      "$DIR/burrtools-qt.exe"
    # Qt Quick Controls styles other than Basic, which main() sets and the
    # program never leaves
    for style in FluentWinUI3 Fusion Imagine Material Universal Windows; do
      rm -rf "$DIR/qml/QtQuick/Controls/$style"
    done
    rm -rf "$DIR/qml/QtQuick/NativeStyle"
    rm -f "$DIR"/Qt6QuickControls2{FluentWinUI3StyleImpl,Fusion,FusionStyleImpl,Imagine,ImagineStyleImpl,Material,MaterialStyleImpl,Universal,UniversalStyleImpl,WindowsStyleImpl}.dll
    # the symbol table is more than half the file (macdeployqt and
    # linuxdeploy strip by themselves)
    strip -s "$DIR/burrtools-qt.exe"
    # MSYS2's Qt looks for its QML modules under share/qt6/qml; windeployqt
    # puts them, and the plugins, beside the program
    printf '[Paths]\nPrefix = .\nPlugins = .\nQmlImports = qml\n' > "$DIR/qt.conf"
    # windeployqt brings Qt and its plugins, not the MSYS2 libraries Qt itself
    # links against (ICU, zstd, HarfBuzz, the GCC runtime ...): copy whatever
    # ldd finds under the MSYS2 prefix, again until nothing new turns up.
    while :; do
      missing="$(find "$DIR" \( -iname '*.exe' -o -iname '*.dll' \) -print0 |
        xargs -0 ldd 2>/dev/null | awk '$3 ~ /^\/(ucrt64|mingw64|clang64)\// { print $3 }' | sort -u |
        while read -r lib; do [ -e "$DIR/$(basename "$lib")" ] || echo "$lib"; done)"
      [ -z "$missing" ] && break
      echo "$missing" | xargs cp -t "$DIR/"
    done
    cp -r examples "$DIR/"
    (cd "$OUT" && rm -f "$NAME.zip" && zip -qr "$NAME.zip" "$NAME")
    rm -rf "$DIR"
    ;;

  Darwin)
    APP="$OUT/burrtools-qt.app"
    rm -rf "$APP"
    mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
    cp "$BUILD/src/qtgui/burrtools-qt" "$APP/Contents/MacOS/"
    cp mac/BurrTools.icns "$APP/Contents/Resources/"
    cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>burrtools-qt</string>
	<key>CFBundleIdentifier</key>
	<string>net.sourceforge.burrtools.qt</string>
	<key>CFBundleName</key>
	<string>BurrTools</string>
	<key>CFBundleDisplayName</key>
	<string>BurrTools (Qt preview)</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>${VERSION}</string>
	<key>CFBundleIconFile</key>
	<string>BurrTools</string>
	<key>LSMinimumSystemVersion</key>
	<string>14.0</string>
	<key>NSHighResolutionCapable</key>
	<true/>
	<key>NSRequiresAquaSystemAppearance</key>
	<false/>
</dict>
</plist>
PLIST
    macdeployqt "$APP" -qmldir=src/qtgui/qml
    (cd "$OUT" && ditto -c -k --keepParent burrtools-qt.app "burrtools-qt-${VERSION}-macos-${ARCH}.zip")
    rm -rf "$APP"
    ;;

  Linux)
    TOOLS="$OUT/.tools"
    APPDIR="$OUT/AppDir"
    rm -rf "$TOOLS" "$APPDIR"
    mkdir -p "$TOOLS"
    for t in linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
             linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage; do
      curl -fsSL -o "$TOOLS/$(basename "$t")" "https://github.com/$t"
      chmod +x "$TOOLS/$(basename "$t")"
    done
    # linuxdeploy takes the usual icon sizes only, not the 1024 px source
    convert mac/icon-source.png -resize 256x256 "$TOOLS/burrtools-qt.png"
    cat > "$TOOLS/burrtools-qt.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=BurrTools (Qt preview)
Comment=Design and solve interlocking burr puzzles
Exec=burrtools-qt
Icon=burrtools-qt
Categories=Game;LogicGame;Education;
DESKTOP
    export PATH="$TOOLS:$PATH"            # the Qt plugin is found by name
    export APPIMAGE_EXTRACT_AND_RUN=1     # CI runners have no FUSE
    export QML_SOURCES_PATHS="$PWD/src/qtgui/qml"
    export QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake)}"
    export LDAI_OUTPUT="$OUT/burrtools-qt-${VERSION}-linux-${ARCH}.AppImage"
    linuxdeploy-x86_64.AppImage --appdir "$APPDIR" \
      --executable "$BUILD/src/qtgui/burrtools-qt" \
      --desktop-file "$TOOLS/burrtools-qt.desktop" \
      --icon-file "$TOOLS/burrtools-qt.png" \
      --plugin qt --output appimage
    rm -rf "$TOOLS" "$APPDIR"
    ;;

  *)
    echo "package-qt.sh: no packaging for $(uname -s)" >&2
    exit 1
    ;;
esac

ls -la "$OUT"
