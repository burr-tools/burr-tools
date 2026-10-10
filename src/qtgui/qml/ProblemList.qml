pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// A single-choice list of the puzzle's problems (legacy ProblemSelector):
// "P2 - name" on the problem's chip colour, with its saved solution count.
Rectangle {
    id: root
    property var problems: []
    property int currentProblem: 0
    property bool showSolutions: true

    implicitHeight: 112
    implicitWidth: 360
    radius: Theme.radiusControl
    color: Theme.panel
    border.color: Theme.line2
    opacity: enabled ? 1 : 0.4

    ListView {
        id: list
        anchors { fill: parent; margins: 4 }
        clip: true
        model: root.problems
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        delegate: AbstractButton {
            id: row
            required property var modelData
            required property int index
            objectName: "problem." + index
            width: ListView.view.width
            height: 28
            hoverEnabled: true
            focusPolicy: Qt.TabFocus
            Accessible.role: Accessible.RadioButton
            Accessible.name: modelData.label
            Accessible.checked: root.currentProblem === modelData.index
            onClicked: root.currentProblem = modelData.index

            background: Rectangle {
                radius: Theme.radiusChip
                color: root.currentProblem === row.modelData.index ? Theme.accentSoft
                     : row.hovered ? Theme.panel2 : "transparent"
                border.width: root.currentProblem === row.modelData.index || row.visualFocus ? 1 : 0
                border.color: Theme.accent
            }
            contentItem: Item {
                Rectangle {
                    id: chip
                    anchors { left: parent.left; leftMargin: 6; verticalCenter: parent.verticalCenter }
                    height: 20
                    width: Math.min(chipText.implicitWidth + 14, parent.width - 120)
                    radius: Theme.radiusChip
                    color: row.modelData.color
                    Text {
                        id: chipText
                        anchors { fill: parent; leftMargin: 7; rightMargin: 7 }
                        verticalAlignment: Text.AlignVCenter
                        text: row.modelData.label
                        elide: Text.ElideRight
                        color: row.modelData.textColor
                        font.pixelSize: Theme.fontSecondary
                        font.weight: Font.DemiBold
                    }
                }
                Text {
                    visible: root.showSolutions
                    anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    text: row.modelData.solutions === 1 ? qsTr("1 solution") : qsTr("%1 solutions").arg(row.modelData.solutions)
                    color: Theme.muted
                    font.pixelSize: Theme.fontMicro
                }
            }
        }
    }
}
