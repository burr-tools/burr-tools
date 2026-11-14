import QtQuick
import BurrTools.Ui

// A dashed outline over an optional fill: C16's drop zones while dragging,
// C18's hover frame in the Puzzle scene. Qt Quick has no dashed border
// without Qt Quick Shapes, which is not in the build; a Canvas draws one, as
// ShortcutKeys' mouse tokens do. It repaints only when its size or looks
// change, and draws nothing while hidden.
Item {
    id: root
    property color color: Theme.accent
    property color fill: "transparent"
    property real lineWidth: 1.5
    property real radius: Theme.radiusControl
    property real dashLength: 4
    property real gapLength: 3

    Rectangle {
        anchors.fill: parent
        visible: root.fill.a > 0
        radius: root.radius
        color: root.fill
    }
    Canvas {
        id: canvas
        objectName: root.objectName.length > 0 ? root.objectName + ".outline" : ""
        anchors.fill: parent
        onPaint: {
            const c = getContext("2d")
            c.reset()
            const w = root.lineWidth
            if (width <= w || height <= w)
                return
            // the pattern counts in line widths (QPen's dash pattern)
            c.setLineDash([root.dashLength / w, root.gapLength / w])
            c.strokeStyle = root.color
            c.lineWidth = w
            c.beginPath()
            const r = Math.max(0, root.radius - w / 2)
            c.roundedRect(w / 2, w / 2, width - w, height - w, r, r)
            c.stroke()
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onVisibleChanged: if (visible) requestPaint()
    }
    onColorChanged: canvas.requestPaint()
    onLineWidthChanged: canvas.requestPaint()
    onRadiusChanged: canvas.requestPaint()
    onDashLengthChanged: canvas.requestPaint()
    onGapLengthChanged: canvas.requestPaint()
}
