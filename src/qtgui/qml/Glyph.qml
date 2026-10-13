import QtQuick
import BurrTools.Ui

// A coloured glyph (foundations/glyphs) in the variant for the active theme.
Image {
    property string name
    property real size: 28

    width: size
    height: size
    sourceSize: Qt.size(size, size)
    source: name.length > 0 ? "qrc:/burrtools/glyphs/" + Theme.glyphFolder + "/" + name + ".svg" : ""
    fillMode: Image.PreserveAspectFit
    smooth: true
}
