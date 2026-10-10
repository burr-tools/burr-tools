pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-05 Segmented: one option is on; Left/Right move the selection when the
// control has focus.
FocusScope {
    id: root
    property var options: []          // [{ value, label }]
    property var value
    signal activated(var value)

    implicitHeight: 26
    implicitWidth: row.implicitWidth + 4
    activeFocusOnTab: true
    // one choice of several: a group of radio buttons to assistive tools
    Accessible.role: Accessible.Grouping

    function indexOfValue(): int {
        for (let i = 0; i < options.length; i++)
            if (options[i].value === value) return i
        return -1
    }
    Keys.onLeftPressed: { const i = indexOfValue(); if (i > 0) activated(options[i - 1].value) }
    Keys.onRightPressed: { const i = indexOfValue(); if (i >= 0 && i < options.length - 1) activated(options[i + 1].value) }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusControl
        color: Theme.panel2
        border.color: root.activeFocus ? Theme.accent : Theme.line
        border.width: root.activeFocus ? 2 : 1
    }
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 0
        Repeater {
            model: root.options
            delegate: AbstractButton {
                id: segment
                required property var modelData
                objectName: root.objectName + "." + modelData.value
                text: modelData.label
                height: 22
                width: segLabel.implicitWidth + 24
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData.label
                Accessible.checked: root.value === modelData.value
                onClicked: root.activated(modelData.value)
                background: Rectangle {
                    radius: Theme.radiusChip
                    color: root.value === segment.modelData.value ? Theme.panel : "transparent"
                }
                contentItem: Text {
                    id: segLabel
                    text: segment.modelData.label
                    color: root.value === segment.modelData.value ? Theme.text : Theme.muted
                    font.pixelSize: Theme.fontSecondary
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
    opacity: enabled ? 1 : 0.4
}
