import QtQuick
import BurrTools.Ui

// A monochrome UI icon (foundations/ui-icons) tinted with `color`, rendered
// by the C++ icon provider at the item size.
Image {
    id: root
    property string name
    property color color: Theme.muted
    property real size: 20

    // the provider takes the colour as bare hex: a '#' would start a URL fragment
    function hex(c: color): string { return c.toString().replace("#", "").slice(-6) }

    width: size
    height: size
    sourceSize: Qt.size(size, size)
    source: name.length > 0 ? "image://icon/" + name + "?color=" + hex(color) : ""
    fillMode: Image.PreserveAspectFit
    smooth: true
}
