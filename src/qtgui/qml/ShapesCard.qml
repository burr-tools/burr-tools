import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// C01 Shapes card, first cut: the list with positional ids (S1 ... SN) and
// selection, which drives the 3D view. Editing (new, rename, reorder, ...)
// arrives with phase P2.
Card {
    id: root
    objectName: "entities.shapes"
    title: qsTr("Shapes")
    collapseIcon: "panelL"
    collapseObjectName: "entities.shapes.collapse"
    onCollapseClicked: App.layout.leftCollapsed = true

    headerExtras: [
        Rectangle {
            objectName: "entities.shapes.count"
            height: 20
            width: Math.max(24, countText.implicitWidth + 12)
            radius: 10
            color: Theme.muted
            Text {
                id: countText
                anchors.centerIn: parent
                text: App.shapes.count
                color: Theme.panel
                font.pixelSize: Theme.fontMicro
                font.weight: Font.DemiBold
            }
        }
    ]

    ListView {
        id: list
        objectName: "entities.shapes.list"
        anchors { fill: parent; margins: 6 }
        clip: true
        spacing: 2
        model: App.shapes
        currentIndex: App.shapes.selected
        keyNavigationEnabled: true
        activeFocusOnTab: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Keys.onUpPressed: App.shapes.select(Math.max(0, App.shapes.selected - 1))
        Keys.onDownPressed: App.shapes.select(App.shapes.selected + 1)

        delegate: Rectangle {
            id: row
            required property int index
            required property string idText
            required property string label
            required property color chipColor
            required property color chipTextColor
            required property int weight

            objectName: "entities.shapes.row." + index
            width: ListView.view.width
            height: Theme.listRow
            radius: Theme.radiusControl
            color: index === App.shapes.selected ? Theme.accentSoft : (hover.hovered ? Theme.panel2 : "transparent")
            border.width: index === App.shapes.selected ? 1 : 0
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)

            HoverHandler { id: hover }
            TapHandler {
                onTapped: { App.shapes.select(row.index); list.forceActiveFocus() }
            }

            Row {
                anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                spacing: 8
                Rectangle {
                    objectName: "entities.shapes.row." + row.index + ".chip"
                    width: Math.max(34, chipText.implicitWidth + 12)
                    height: 26
                    radius: Theme.radiusChip
                    color: row.chipColor
                    Text {
                        id: chipText
                        anchors.centerIn: parent
                        text: row.idText
                        color: row.chipTextColor
                        font.pixelSize: Theme.fontMicro
                        font.weight: Font.Bold
                    }
                }
                Text {
                    objectName: "entities.shapes.row." + row.index + ".label"
                    anchors.verticalCenter: parent.verticalCenter
                    width: row.width - 120
                    text: row.label
                    color: Theme.muted
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                objectName: "entities.shapes.row." + row.index + ".weightBadge"
                visible: row.weight !== 1
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                height: 18
                width: weightText.implicitWidth + 10
                radius: 5
                color: Theme.panel2
                border.color: Theme.line
                Text {
                    id: weightText
                    anchors.centerIn: parent
                    text: "W" + row.weight
                    color: Theme.muted
                    font.pixelSize: Theme.fontMicro
                    font.weight: Font.Bold
                }
            }
        }

        Text {
            visible: list.count === 0
            anchors.centerIn: parent
            text: qsTr("No shapes")
            color: Theme.muted
            font.pixelSize: Theme.fontSecondary
        }
    }
}
