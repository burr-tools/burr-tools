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

echo "Creating macOS app bundle..."

# Create bundle structure
BUNDLE="${APP_NAME}.app"
rm -rf "${BUNDLE}"
mkdir -p "${BUNDLE}/Contents/MacOS"
mkdir -p "${BUNDLE}/Contents/Resources"

# Copy executable
cp "${BUILD_DIR}/${EXECUTABLE}" "${BUNDLE}/Contents/MacOS/"
chmod +x "${BUNDLE}/Contents/MacOS/${EXECUTABLE}"

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
	<key>CFBundleVersion</key>
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
			<key>CFBundleTypeRole</key>
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

GETTING STARTED ON macOS:
1. Drag BurrTools.app to your Applications folder (optional)
2. Right-click BurrTools.app and select "Open" (first time only)
3. Click "Open" in the security dialog
4. Open example puzzles from the Examples folder using File > Load

ABOUT THE SECURITY WARNING:
BurrTools is an open-source project distributed without an Apple Developer
signature. The security warning is normal and appears only once. After using
"Right-click > Open", macOS will remember your choice.

Alternatively, you can run this command in Terminal:
  xattr -cr /Applications/BurrTools.app

EXAMPLE PUZZLES:
The Examples folder contains sample puzzle files (.xmpuzzle) that you can
open with BurrTools to explore various puzzle types and designs. Simply use
File > Load in BurrTools and navigate to the Examples folder.

DOCUMENTATION:
The real documentation is inside the executable as on-line help, and also
available as a PDF for off-line reading or printouts.

For more information, documentation, and source code:
https://github.com/burr-tools/burr-tools

User Guide: https://burrtools.sourceforge.net/gui-doc/toc.html
Library documentation: https://burrtools.sourceforge.net/lib-doc/index.html

Report issues at:
https://github.com/burr-tools/burr-tools/issues
READMEEOF
	# Substitute VERSION variable
	sed -i '' "s/\${VERSION}/${VERSION}/g" "${DMG_DIR}/README.txt"
	
	# Create DMG
	hdiutil create -volname "BurrTools ${VERSION}" -srcfolder "${DMG_DIR}" -ov -format UDZO "BurrTools-${VERSION}.dmg"
	
	# Clean up
	rm -rf "${DMG_DIR}"
	
	echo "Created: BurrTools-${VERSION}.dmg"
fi

echo ""
echo "Note: This app is unsigned. Users will need to:"
echo "  1. Right-click the app and select 'Open'"
echo "  2. Or run: xattr -cr BurrTools.app"
