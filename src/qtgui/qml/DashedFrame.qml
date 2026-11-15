import QtQuick
import BurrTools.Ui

// A dashed outline over an optional fill: C16's drop zones while dragging,
// C18's hover frame in the Puzzle scene. Qt Quick has no dashed border
// without Qt Quick Shapes, which is not in the build; a Canvas draws one, as
// ShortcutKeys' mouse tokens do. It repaints only when its size or looks
// change, and only while shown: a Canvas asked to repaint paints hidden too,
// and the Puzzle scene has a hidden frame on every object, all resized at
// each frame of a zoom, a pan or a card sliding (review 2026-11-15). Shown
// again, it paints once.
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
        onWidthChanged: root.repaint()
        onHeightChanged: root.repaint()
        onVisibleChanged: root.repaint()
    }
    // `visible` is the effective one: false under a hidden parent too
    function repaint(): void {
        if (canvas.visible)
            canvas.requestPaint()
    }
    onColorChanged: repaint()
    onLineWidthChanged: repaint()
    onRadiusChanged: repaint()
    onDashLengthChanged: repaint()
    onGapLengthChanged: repaint()
}
