import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// A slider with its value read out at the right (C12 Worker threads): a
// line2 track filled with the accent up to the handle, whole steps.
Item {
    id: root
    property alias from: slider.from
    property alias to: slider.to
    property alias value: slider.value
    property alias pressed: slider.pressed
    signal moved(int value)

    implicitWidth: 230
    implicitHeight: 30

    Slider {
        id: slider
        objectName: root.objectName.length > 0 ? root.objectName + ".slider" : ""
        anchors { left: parent.left; right: readout.left; rightMargin: 10; verticalCenter: parent.verticalCenter }
        stepSize: 1
        Accessible.name: root.Accessible.name
        snapMode: Slider.SnapAlways
        onMoved: root.moved(Math.round(value))

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 4
            radius: 2
            color: Theme.line2
            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: 2
                color: Theme.accent
            }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 16
            height: 16
            radius: 8
            color: Theme.panel
            border.width: slider.visualFocus ? 3 : 2
            border.color: Theme.accent
        }
    }

    Text {
        id: readout
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        width: 28
        horizontalAlignment: Text.AlignRight
        text: Math.round(slider.value)
        color: Theme.text
        font.pixelSize: Theme.fontBody
        font.weight: Font.Bold
    }
    opacity: enabled ? 1 : 0.4
}
