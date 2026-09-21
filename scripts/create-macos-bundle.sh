#!/bin/bash
set -e

APP_NAME="BurrTools"
BUNDLE_ID="org.burrtools.burrtools"

# Get version from git if not provided
if [ -z "$1" ]; then
	VERSION=$(git describe --tags --always --dirty 2>/dev/null || echo "dev")
	if [ -z "$VERSION" ]; then
		echo "Warning: Could not extract version from git, using 0.7.0"
		VERSION="0.7.0"
	fi
else
	VERSION="$1"
fi

BUILD_DIR="${2:-build}"
EXECUTABLE="burrtools"

# Name artifacts after the architecture they were actually built for, so an
# Apple Silicon build is never mistaken for an Intel one on the release page.
ARCH="$(uname -m)"
DMG_NAME="${APP_NAME}-${VERSION}-macos-${ARCH}.dmg"
if [ "${ARCH}" = "arm64" ]; then
	ARCH_LINE="This build runs on Apple Silicon Macs (M1 and later). On an Intel Mac,"
else
	ARCH_LINE="This build runs on Intel Macs (${ARCH}). On an Apple Silicon Mac,"
fi

echo "Creating macOS app bundle..."

# Create bundle structure
BUNDLE="${APP_NAME}.app"
rm -rf "${BUNDLE}"
mkdir -p "${BUNDLE}/Contents/MacOS"
mkdir -p "${BUNDLE}/Contents/Resources"

# Copy executable
cp "${BUILD_DIR}/${EXECUTABLE}" "${BUNDLE}/Contents/MacOS/"
chmod +x "${BUNDLE}/Contents/MacOS/${EXECUTABLE}"

# Icons
HAVE_APP_ICON=0
HAVE_DOC_ICON=0
if [ -f "mac/BurrTools.icns" ]; then
	cp mac/BurrTools.icns "${BUNDLE}/Contents/Resources/"
	HAVE_APP_ICON=1
else
	echo "warning: mac/BurrTools.icns missing; run scripts/make-macos-icons.sh" >&2
fi
if [ -f "mac/BurrToolsDoc.icns" ]; then
	cp mac/BurrToolsDoc.icns "${BUNDLE}/Contents/Resources/"
	HAVE_DOC_ICON=1
else
	echo "warning: mac/BurrToolsDoc.icns missing; run scripts/make-macos-icons.sh" >&2
fi

# CFBundleIconFile/CFBundleTypeIconFile are only meaningful when the icon
# file actually exists in the bundle; a plist naming a missing icon is
# worse than one naming none, so build these fragments conditionally.
APP_ICON_KEYS=""
if [ "$HAVE_APP_ICON" = "1" ]; then
	APP_ICON_KEYS="	<key>CFBundleIconFile</key>
	<string>BurrTools</string>
"
fi
DOC_ICON_KEYS=""
if [ "$HAVE_DOC_ICON" = "1" ]; then
	DOC_ICON_KEYS="			<key>CFBundleTypeIconFile</key>
			<string>BurrToolsDoc</string>
"
fi

# Create Info.plist
cat > "${BUNDLE}/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>${EXECUTABLE}</string>
	<key>CFBundleIdentifier</key>
	<string>${BUNDLE_ID}</string>
	<key>CFBundleName</key>
	<string>${APP_NAME}</string>
	<key>CFBundleDisplayName</key>
	<string>${APP_NAME}</string>
${APP_ICON_KEYS}	<key>CFBundleVersion</key>
	<string>${VERSION}</string>
	<key>CFBundleShortVersionString</key>
	<string>${VERSION}</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleSignature</key>
	<string>BTLS</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>NSHighResolutionCapable</key>
	<true/>
	<key>LSMinimumSystemVersion</key>
	<string>11.0</string>
	<key>NSHumanReadableCopyright</key>
	<string>BurrTools - Open Source Puzzle Software</string>
	<key>CFBundleDocumentTypes</key>
	<array>
		<dict>
			<key>CFBundleTypeExtensions</key>
			<array>
				<string>xmpuzzle</string>
			</array>
			<key>CFBundleTypeName</key>
			<string>BurrTools Puzzle File</string>
${DOC_ICON_KEYS}			<key>CFBundleTypeRole</key>
			<string>Editor</string>
			<key>LSHandlerRank</key>
			<string>Owner</string>
		</dict>
	</array>
</dict>
</plist>
EOF

echo "App bundle created: ${BUNDLE}"

# Create DMG with app and examples side-by-side
if [ -d "examples" ]; then
	echo "Creating DMG with Examples folder..."
	
	# Create temporary directory for DMG contents
	DMG_DIR="dmg_contents"
	rm -rf "${DMG_DIR}"
	mkdir -p "${DMG_DIR}"
	
	# Copy app bundle and examples
	cp -r "${BUNDLE}" "${DMG_DIR}/"
	cp -r examples "${DMG_DIR}/Examples"
	
	# Create README
	cat > "${DMG_DIR}/README.txt" << 'READMEEOF'
BurrTools - Burr Puzzle Design and Analysis Software
Version ${VERSION}

BurrTools is a library to solve burr-type puzzles. Bundled with the
library comes a graphical program that lets you edit the puzzles and
view the found solutions.

${ARCH_LINE}
build from source: https://github.com/burr-tools/burr-tools/blob/master/BUILD.md

GETTING STARTED ON macOS:
1. Drag BurrTools.app to your Applications folder
2. Open Terminal and run:  xattr -cr /Applications/BurrTools.app
3. Launch BurrTools normally
4. Open example puzzles from the Examples folder using File > Open...

ABOUT "BURRTOOLS.APP IS DAMAGED AND CAN'T BE OPENED":
The app is not damaged. BurrTools is an open-source project distributed
without an Apple Developer signature, so macOS quarantines it on download and
then reports the missing signature with that message. Step 2 above clears the
quarantine flag, and only needs to be done once.

If you would rather not use Terminal, try to open the app once, then go to
System Settings > Privacy & Security, scroll to the Security section, and
click "Open Anyway" next to the message about BurrTools.

Control-clicking the app and choosing "Open" does not get past this message,
and no longer bypasses Gatekeeper at all as of macOS 15 Sequoia.

EXAMPLE PUZZLES:
The Examples folder contains sample puzzle files (.xmpuzzle) that you can
open with BurrTools to explore various puzzle types and designs. Simply use
File > Open... in BurrTools and navigate to the Examples folder, or
double-click an .xmpuzzle file in the Finder.

DOCUMENTATION:
The user guide is published online. Help > BurrTools User Guide in the
application opens it in your browser; the link is also listed below.

For more information, documentation, and source code:
https://github.com/burr-tools/burr-tools

User Guide: https://burrtools.sourceforge.net/gui-doc/toc.html
Library documentation: https://burrtools.sourceforge.net/lib-doc/index.html

Report issues at:
https://github.com/burr-tools/burr-tools/issues
READMEEOF
	# Substitute VERSION variable
	sed -i '' "s/\${VERSION}/${VERSION}/g" "${DMG_DIR}/README.txt"
	sed -i '' "s|\${ARCH_LINE}|${ARCH_LINE}|g" "${DMG_DIR}/README.txt"
	
	# Create DMG
	hdiutil create -volname "BurrTools ${VERSION}" -srcfolder "${DMG_DIR}" -ov -format UDZO "${DMG_NAME}"
	
	# Clean up
	rm -rf "${DMG_DIR}"
	
	echo "Created: ${DMG_NAME}"
fi

echo ""
echo "Note: This app is unsigned. Users will need to run:"
echo "  xattr -cr BurrTools.app"
echo "(or System Settings > Privacy & Security > Open Anyway)"
