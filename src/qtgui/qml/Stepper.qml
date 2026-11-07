pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-06 Stepper: [axis] [−] [value] [＋] in one box, h28 (Minimal 26). The value is
// typed (NumberEntry: Enter or leaving the field commits, Esc goes back to what
// it was, invalid text reverts), stepped with ↑ / ↓ or the wheel while focused,
// and with − / ＋, which repeat while held. It shows `value` and asks for a new
// one with committed(value); the owner decides (a size may be refused).
Rectangle {
    id: root
    property int value: 1
    property int from: 1
    property int to: 1000
    property string axis
    property color axisColor: Theme.text
    property string tip
    /* typed digits go through after a short pause, without Enter: for values
     * something else shows at once (the Puzzle workspace's counts). A pause
     * commits nothing below liveFrom: 0 removes a piece, and waits for Enter. */
    property bool commitWhileTyping: false
    property int liveFrom: from
    signal committed(int value)
    // a number being typed: from the first key until it is committed or dropped
    signal entryStarted()
    signal entryEnded()
    readonly property bool entering: entry.open

    // the decrement, the field and the increment, for tests
    readonly property alias decButton: dec
    readonly property alias field: input
    readonly property alias incButton: inc

    implicitWidth: 112
    implicitHeight: Theme.stepper
    radius: Theme.radiusControl
    color: Theme.panel
    border.width: input.activeFocus ? 2 : 1
    border.color: input.activeFocus ? Theme.accent : (invalid ? Theme.danger : Theme.line2)
    opacity: enabled ? 1 : 0.4

    property bool invalid: false
    Timer { id: invalidTimer; interval: 600; onTriggered: root.invalid = false }

    // what is being typed goes in, or is dropped: for an owner that moves on
    function commitText(): void { entry.finish() }
    function dropText(): void { entry.drop() }

    NumberEntry {
        id: entry
        field: input
        value: root.value
        from: root.from
        to: root.to
        liveFrom: root.liveFrom
        live: root.commitWhileTyping
        onCommitted: (v) => root.committed(v)
        onStarted: root.entryStarted()
        onEnded: root.entryEnded()
        onRejected: {
            root.invalid = true
            invalidTimer.restart()
            App.status.flash(qsTr("Enter a whole number"))    // the red border alone is hue only
        }
    }

    component StepButton: AbstractButton {
        id: sb
        property string sign
        implicitWidth: 22
        implicitHeight: root.height - 2
        focusPolicy: Qt.NoFocus          // the field is the Tab stop; ↑ / ↓ step there
        autoRepeat: true
        autoRepeatDelay: 400
        autoRepeatInterval: 80
        Accessible.name: sb.sign === "+" ? qsTr("Increase %1").arg(root.axis) : qsTr("Decrease %1").arg(root.axis)
        background: Rectangle {
            radius: Theme.radiusChip
            color: sb.pressed ? Theme.line : (sb.hovered ? Theme.panel2 : "transparent")
        }
        contentItem: Text {
            text: sb.sign === "+" ? qsTr("+") : qsTr("−")
            color: sb.enabled ? Theme.text : Theme.muted
            font.pixelSize: Theme.fontBody
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        opacity: enabled ? 1 : 0.4
    }

    Row {
        anchors { fill: parent; leftMargin: 8; rightMargin: 2 }
        spacing: 0
        Text {
            id: axisLabel
            width: Math.max(12, implicitWidth + 3)     // a word (Min) needs more than a letter
            height: parent.height
            text: root.axis
            color: root.axisColor
            font.pixelSize: Theme.fontSecondary
            font.weight: Font.Bold
            verticalAlignment: Text.AlignVCenter
        }
        StepButton {
            id: dec
            objectName: root.objectName + ".dec"
            sign: "-"
            anchors.verticalCenter: parent.verticalCenter
            enabled: root.value > root.from
            onClicked: entry.step(root.value - 1)
        }
        TextInput {
            id: input
            objectName: root.objectName + ".input"
            width: parent.width - axisLabel.width - dec.width - inc.width
            height: parent.height
            text: String(root.value)
            color: Theme.text
            selectionColor: Theme.accent
            selectedTextColor: Theme.onAccent
            font.pixelSize: Theme.fontBody
            horizontalAlignment: TextInput.AlignHCenter
            verticalAlignment: TextInput.AlignVCenter
            selectByMouse: true
            inputMethodHints: Qt.ImhDigitsOnly
            activeFocusOnTab: true
            Accessible.role: Accessible.SpinBox
            Accessible.name: root.tip.length > 0 ? root.tip : root.axis
            onActiveFocusChanged: if (activeFocus) selectAll(); else entry.finish()
            onTextEdited: entry.edited()
            Keys.onReturnPressed: entry.finish()
            Keys.onEnterPressed: entry.finish()
            // while something is typed Esc is the field's, before the window's Esc ladder
            Keys.onShortcutOverride: (event) => { if (event.key === Qt.Key_Escape && entry.entering) event.accepted = true }
            Keys.onEscapePressed: (event) => {
                event.accepted = entry.entering   // nothing to revert: Esc goes on (Focus 2D)
                entry.cancel()
            }
            Keys.onUpPressed: entry.step(root.value + 1)
            Keys.onDownPressed: entry.step(root.value - 1)
            WheelHandler {
                enabled: input.activeFocus
                onWheel: (event) => entry.step(root.value + (event.angleDelta.y > 0 ? 1 : -1))
            }
        }
        StepButton {
            id: inc
            objectName: root.objectName + ".inc"
            sign: "+"
            anchors.verticalCenter: parent.verticalCenter
            enabled: root.value < root.to
            onClicked: entry.step(root.value + 1)
        }
    }
    BtToolTip { shown: hover.hovered && !input.activeFocus; text: root.tip }
    HoverHandler { id: hover }
}
