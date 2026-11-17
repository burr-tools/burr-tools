import QtQuick
import BurrTools.Ui

// PR-22 Piece tile: a piece in "In this puzzle". Fixed size (Theme.puzzleTile:
// 96 x 44, Minimal 78 x 40), radius 8, 1 dp line on panel2, a 6 dp stripe of
// the shape's colour. Row 1: grip, id, label, the groups' badge; row 2: − count +,
// always there. Selected: accent border and ring on accentSoft. Nothing
// changes size on hover, so tiles never move under the pointer (AC-C16-03).
Rectangle {
    id: root
    property string idText
    property string label
    property string countText
    property string groupText
    property color stripe: Theme.muted
    property bool selected: false
    // the same piece is under the pointer elsewhere (the 3D view): an accent border
    property bool linked: false
    readonly property bool pointerOver: hover.hovered
    property bool focusRing: false      // the keyboard is in the tiles and this is the selected one
    property bool dragEnabled: true

    // the − and + buttons, for tests
    readonly property alias minusButton: minus
    readonly property alias plusButton: plus

    signal tapped()
    signal stepped(int delta)
    signal menuRequested(real x, real y)
    signal dragStarted()
    signal dragMoved(real x, real y)    // in the tile's coordinates
    signal dragEnded()
    signal dragCancelled()

    // the Minimal tile is 18 dp narrower: its − and + give way, not the count
    readonly property real stepWidth: Theme.minimal ? 18 : 22

    implicitWidth: Theme.puzzleTileWidth
    implicitHeight: Theme.puzzleTileHeight
    radius: Theme.radiusControl
    color: selected ? Theme.accentSoft : Theme.panel2
    border.width: selected ? 2 : 1
    border.color: selected || linked ? Theme.accent : Theme.line
    clip: true

    Accessible.role: Accessible.ListItem
    Accessible.name: groupText.length > 0 ? qsTr("%1 %2, %3, in %4").arg(idText).arg(label).arg(countText).arg(groupText)
                                          : qsTr("%1 %2, %3").arg(idText).arg(label).arg(countText)
    Accessible.selectable: true
    Accessible.selected: selected
    Accessible.onPressAction: root.tapped()

    HoverHandler { id: hover; cursorShape: Qt.OpenHandCursor }
    TapHandler { onTapped: root.tapped() }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (point) => root.menuRequested(point.position.x, point.position.y)
    }
    DragHandler {
        id: drag
        target: null
        enabled: root.dragEnabled
        onActiveChanged: active ? root.dragStarted() : root.dragEnded()
        onCentroidChanged: if (active) root.dragMoved(centroid.position.x, centroid.position.y)
        onCanceled: root.dragCancelled()
    }

    Rectangle {
        // the keyboard's place: a second line inside the selected tile's border
        objectName: root.objectName + ".ring"
        anchors { fill: parent; margins: 3 }
        visible: root.focusRing
        z: 1
        color: "transparent"
        radius: parent.radius - 2
        border.width: 1
        border.color: Theme.accent
    }
    Rectangle {
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 1 }
        width: 6
        radius: 2
        color: root.stripe
    }

    Column {
        anchors { fill: parent; leftMargin: 9; rightMargin: 3; topMargin: 1; bottomMargin: 1 }
        spacing: 0

        Row {
            width: parent.width
            height: 17
            spacing: 3
            Icon {
                name: "grip"
                size: 10
                color: hover.hovered ? Theme.muted : Theme.line2
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                id: idLabel
                text: root.idText
                Accessible.ignored: true
                color: Theme.text
                font.pixelSize: Theme.fontSecondary
                font.weight: Font.Bold
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                width: Math.max(0, parent.width - 13 - idLabel.width - badge.width - 12)
                text: root.label
                textFormat: Text.PlainText
                Accessible.ignored: true
                color: root.selected ? Theme.text : Theme.muted
                font.pixelSize: Theme.fontMicro
                elide: Text.ElideRight
                anchors.verticalCenter: parent.verticalCenter
            }
            Rectangle {
                id: badge
                visible: root.groupText.length > 0
                width: visible ? Math.min(badgeText.implicitWidth + 6, 34) : 0
                height: 14
                radius: 5
                color: Theme.accent
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    id: badgeText
                    objectName: "groupBadge"
                    anchors.centerIn: parent
                    width: Math.min(implicitWidth, parent.width - 2)
                    elide: Text.ElideRight
                    text: root.groupText
                    Accessible.ignored: true
                    color: Theme.onAccent
                    font.pixelSize: Theme.fontMicro - 1
                    font.weight: Font.Bold
                }
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            height: root.height - 19
            IconButton {
                id: minus
                objectName: root.objectName + ".minus"
                width: root.stepWidth; height: parent.height
                iconName: "minus"
                iconSize: 14
                focusPolicy: Qt.NoFocus      // the tiles are one Tab stop; − and + are its keys
                tip: qsTr("One fewer of %1").arg(root.idText)
                onClicked: root.stepped(-1)
            }
            Text {
                objectName: root.objectName + ".count"
                width: Math.max(Theme.minimal ? 24 : 34, implicitWidth)
                height: parent.height
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: root.countText
                Accessible.ignored: true
                color: Theme.text
                font.pixelSize: Theme.fontSecondary
                font.weight: Font.Bold
            }
            IconButton {
                id: plus
                objectName: root.objectName + ".plus"
                width: root.stepWidth; height: parent.height
                iconName: "plus"
                iconSize: 14
                focusPolicy: Qt.NoFocus
                tip: qsTr("One more of %1").arg(root.idText)
                onClicked: root.stepped(+1)
            }
        }
    }
}
