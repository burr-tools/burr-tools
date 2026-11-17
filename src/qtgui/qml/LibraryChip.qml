pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-23 Library chip: a shape in "All pieces". Theme.libraryChip high, radius
// 8; grip, a 10 dp colour square, id, label (at most 60 dp), and − count +
// (the count as the piece's tile reads it). The count has room for "×9", so
// nothing jumps when a piece is added; a longer one ("×10", "2–12") widens
// the chip rather than being cut short. A click selects, double-click and + add, a drag into "In this
// puzzle" adds; − takes one away.
// Hover: accent border on accentSoft, no change of size; selected: an accent
// ring; pieces in use are quieter, on the panel.
Rectangle {
    id: root
    property string idText
    property string label
    property string countText            // empty while the piece is not in the problem
    property color swatch: Theme.muted
    property bool used: false
    property bool selected: false
    // the same piece is under the pointer elsewhere (the 3D view): lit as when hovered
    property bool linked: false
    readonly property bool pointerOver: hover.hovered
    property bool focusRing: false       // the keyboard is in the chips and this is the selected one

    readonly property alias addButton: action
    readonly property alias reduceButton: reduce

    signal tapped()
    signal added()
    signal reduced()
    signal menuRequested(real x, real y)
    signal dragStarted()
    signal dragMoved(real x, real y)
    signal dragEnded()
    signal dragCancelled()

    implicitHeight: Theme.libraryChip
    implicitWidth: content.implicitWidth + 6
    radius: Theme.radiusControl
    color: hover.hovered || linked ? Theme.accentSoft : used ? Theme.panel : Theme.panel2
    border.width: selected ? 2 : 1
    border.color: selected || hover.hovered || linked ? Theme.accent : Theme.line

    Accessible.role: Accessible.ListItem
    Accessible.name: used ? qsTr("%1 %2, %3 in the puzzle").arg(idText).arg(label).arg(countText)
                          : label.length > 0 ? qsTr("%1 %2").arg(idText).arg(label) : idText
    Accessible.selectable: true
    Accessible.selected: selected
    readonly property string tip: qsTr("Drag into the puzzle, double-click or press + to add")
    Accessible.description: tip
    Accessible.onPressAction: root.tapped()

    HoverHandler {
        id: hover
        cursorShape: Qt.OpenHandCursor
        onHoveredChanged: if (hover.hovered) root.wantTip()
    }
    onFocusRingChanged: if (root.focusRing) root.wantTip()
    // the tooltip made the first time it could show, not with each of the shapes' chips
    property BtToolTip toolTip: null
    // made hidden, then armed: one made already shown skips the tooltip's delay
    property bool tipArmed: false
    Component {
        id: tipComponent
        BtToolTip { shown: root.tipArmed && (hover.hovered || root.focusRing); text: root.tip }
    }
    function wantTip(): void {
        if (root.toolTip !== null)
            return
        root.toolTip = tipComponent.createObject(root) as BtToolTip
        root.tipArmed = true
    }
    Component.onCompleted: if (root.focusRing) root.wantTip()
    TapHandler {
        id: tap
        onTapped: if (tap.tapCount === 1) root.tapped()
        onDoubleTapped: root.added()
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (point) => root.menuRequested(point.position.x, point.position.y)
    }
    DragHandler {
        target: null
        onActiveChanged: active ? root.dragStarted() : root.dragEnded()
        onCentroidChanged: if (active) root.dragMoved(centroid.position.x, centroid.position.y)
        onCanceled: root.dragCancelled()
    }

    Rectangle {
        // the keyboard's place: a second line inside the selected chip's border
        objectName: root.objectName + ".ring"
        anchors { fill: parent; margins: 3 }
        visible: root.focusRing
        z: 1
        color: "transparent"
        radius: parent.radius - 2
        border.width: 1
        border.color: Theme.accent
    }

    Row {
        id: content
        anchors { left: parent.left; leftMargin: 3; verticalCenter: parent.verticalCenter }
        spacing: 2
        Icon {
            name: "grip"
            size: 8
            color: hover.hovered ? Theme.muted : Theme.line2
            anchors.verticalCenter: parent.verticalCenter
        }
        Rectangle {
            width: 9; height: 9; radius: 3
            color: root.swatch
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.idText
            Accessible.ignored: true
            color: Theme.text
            font.pixelSize: Theme.fontSecondary
            font.weight: Font.Bold
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: root.label.length > 0
            width: Math.min(implicitWidth, 60)
            elide: Text.ElideRight
            text: root.label
            textFormat: Text.PlainText
            Accessible.ignored: true
            color: hover.hovered ? Theme.text : Theme.muted     // muted is 4.3:1 on accentSoft
            font.pixelSize: Theme.fontMicro
            anchors.verticalCenter: parent.verticalCenter
        }
        component StepButton: AbstractButton {
            id: step
            width: 16
            height: root.height - 6
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            background: Rectangle {
                radius: Theme.radiusChip
                color: step.hovered && step.enabled ? Theme.panel : "transparent"
            }
            contentItem: Text {
                text: step.text
                Accessible.ignored: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: step.enabled ? Theme.text : Theme.line2
                font.pixelSize: Theme.fontMicro
                font.weight: Font.Bold
            }
        }
        StepButton {
            id: reduce
            objectName: root.objectName + ".reduce"
            text: qsTr("−")
            enabled: root.used
            Accessible.name: qsTr("One fewer %1").arg(root.idText)
            onClicked: root.reduced()
        }
        Text {
            objectName: root.objectName + ".count"
            width: Math.max(19, implicitWidth)
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignHCenter
            text: root.countText
            Accessible.ignored: true
            color: Theme.text
            font.pixelSize: Theme.fontMicro
            font.weight: Font.Bold
        }
        StepButton {
            id: action
            objectName: root.objectName + ".add"
            text: qsTr("+")
            Accessible.name: qsTr("Add one %1").arg(root.idText)
            onClicked: root.added()
        }
    }
}
