pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-05 Segmented: one option is on; Left/Right move the selection when the
// control has focus. The option on is drawn as the selected tool and plane
// buttons are -- accent-tinted, with an accent edge and text -- not PR-05's
// panel fill and shadow, which read as hardly different from the track in
// the light theme (user decision 2026-10-28).
FocusScope {
    id: root
    property var options: []          // [{ value, label }]
    property var value
    signal activated(var value)

    implicitHeight: Theme.segmentedButton + 4      // the options on a 2 dp padded track
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
                onClicked: {
                    root.forceActiveFocus(Qt.MouseFocusReason)      // Tab goes on from here
                    root.activated(modelData.value)
                }
                readonly property bool on: root.value === modelData.value
                background: Rectangle {
                    radius: Theme.radiusChip
                    color: segment.on ? Theme.accentSoft : (segment.hovered ? Theme.panel : "transparent")
                    border.width: segment.on ? 1 : 0
                    border.color: Theme.accent
                }
                contentItem: Text {
                    id: segLabel
                    text: segment.modelData.label
                    color: segment.on ? Theme.accent : (segment.hovered ? Theme.text : Theme.muted)
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
