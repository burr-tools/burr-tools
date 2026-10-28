import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-09 AccordionSection: a header -- chevron, title 13/600, the summary
// right-aligned in 12 muted -- over a body shown while `open`. A click,
// Enter or Space on the header toggles it (`toggled`; the owner keeps
// `open`, persisted). Header Theme.accordionHeader high, body padding
// 2 10 10 (Minimal 2 8 8); a 1 dp line under the section. Minimal hides
// the summary: the header's tooltip says it then (density.md section 2).
// Focusing the section focuses its header. A toggle slides the body open or
// shut (Theme.durCollapse); a change of its content, or of `open` from
// elsewhere (a restored setting), does not animate.
FocusScope {
    id: root
    property string title
    property string summary
    property bool open: true
    default property alias content: body.data
    readonly property alias header: headerButton
    signal toggled()

    // a toggle by the header is under way: the body's height animates
    property bool sliding: false
    function toggle() {
        sliding = true
        toggled()
    }

    implicitHeight: headerButton.height + bodyClip.height + 1

    AbstractButton {
        id: headerButton
        objectName: root.objectName.length > 0 ? root.objectName + ".header" : ""
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: Theme.accordionHeader
        focus: true
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        leftPadding: Theme.minimal ? 8 : 10
        rightPadding: Theme.minimal ? 8 : 10
        onClicked: root.toggle()
        Keys.onReturnPressed: root.toggle()
        Keys.onEnterPressed: root.toggle()

        Accessible.role: Accessible.Button
        Accessible.name: root.title
        // the state in words: Accessible.expanded is Qt 6.9's (we build with 6.8)
        Accessible.description: (root.open ? qsTr("Expanded. ") : qsTr("Collapsed. ")) + root.summary
        Accessible.expandable: true
        BtToolTip { shown: Theme.minimal && (headerButton.hovered || headerButton.visualFocus); text: root.summary }

        background: Rectangle {
            color: headerButton.hovered ? Theme.panel2 : "transparent"
            border.width: headerButton.visualFocus ? 2 : 0
            border.color: Theme.accent
            radius: headerButton.visualFocus ? Theme.radiusChip : 0
        }
        contentItem: Item {
            Icon {
                id: chevron
                anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                name: "chevD"
                size: 14
                rotation: root.open ? 0 : -90
                Behavior on rotation { NumberAnimation { duration: Theme.durChevron; easing.type: Easing.OutCubic } }
            }
            Text {
                id: titleText
                anchors { left: chevron.right; leftMargin: 8; verticalCenter: parent.verticalCenter }
                text: root.title
                color: Theme.text
                font.pixelSize: Theme.minimal ? Theme.fontActionTitle : Theme.fontBody
                font.weight: Font.DemiBold
            }
            Text {
                anchors { left: titleText.right; leftMargin: 8; right: parent.right; verticalCenter: parent.verticalCenter }
                visible: !Theme.minimal
                text: root.summary
                horizontalAlignment: Text.AlignRight
                color: Theme.muted
                font.pixelSize: Theme.fontSecondary
                elide: Text.ElideRight
            }
        }
    }

    Item {
        id: bodyClip
        anchors { left: parent.left; right: parent.right; top: headerButton.bottom }
        height: root.open ? body.implicitHeight + body.topPadding + body.bottomPadding : 0
        clip: true
        // shut, nothing in it is shown or reached by Tab
        visible: height > 0
        Behavior on height {
            enabled: root.sliding
            NumberAnimation {
                duration: Theme.durCollapse
                easing.type: Easing.OutCubic
                onRunningChanged: if (!running) root.sliding = false
            }
        }

        Column {
            id: body
            property real topPadding: 2
            property real bottomPadding: Theme.minimal ? 8 : 10
            anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: topPadding
                      leftMargin: Theme.minimal ? 8 : 10; rightMargin: Theme.minimal ? 8 : 10 }
            spacing: 0
        }
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Theme.line
    }
}
