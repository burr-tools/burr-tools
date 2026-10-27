import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-06 Stepper: [axis] [−] [value] [＋] in one 30 dp box. The value is
// typed (Enter or leaving the field commits, Esc reverts, invalid text
// reverts), stepped with ↑ / ↓ or the wheel while focused, and with − / ＋,
// which repeat while held. It shows `value` and asks for a new one with
// committed(value); the owner decides (a size may be refused).
Rectangle {
    id: root
    property int value: 1
    property int from: 1
    property int to: 1000
    property string axis
    property color axisColor: Theme.text
    property string tip
    signal committed(int value)

    // the decrement, the field and the increment, for tests
    readonly property alias decButton: dec
    readonly property alias field: input
    readonly property alias incButton: inc

    implicitWidth: 112
    implicitHeight: 30
    radius: Theme.radiusControl
    color: Theme.panel
    border.width: input.activeFocus ? 2 : 1
    border.color: input.activeFocus ? Theme.accent : (invalid ? Theme.danger : Theme.line2)
    opacity: enabled ? 1 : 0.4

    property bool invalid: false
    Timer { id: invalidTimer; interval: 600; onTriggered: root.invalid = false }

    function ask(v) {
        const c = Math.max(from, Math.min(to, v))
        if (c !== value)
            committed(c)
    }

    function commitText() {
        const v = parseInt(input.text)
        if (isNaN(v) || String(v) !== input.text.trim()) {
            invalid = true
            invalidTimer.restart()
        } else {
            ask(v)
        }
        input.text = String(value)
    }

    onValueChanged: if (!input.activeFocus) input.text = String(value)

    component StepButton: AbstractButton {
        id: sb
        property string sign
        implicitWidth: 22
        implicitHeight: 26
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
            text: sb.sign === "+" ? "+" : "−"
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
            width: 12
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
            onClicked: root.ask(root.value - 1)
        }
        TextInput {
            id: input
            objectName: root.objectName + ".input"
            width: parent.width - axisLabel.width - dec.width - inc.width
            height: parent.height
            text: String(root.value)
            color: Theme.text
            selectionColor: Theme.accent
            selectedTextColor: "white"
            font.pixelSize: Theme.fontBody
            horizontalAlignment: TextInput.AlignHCenter
            verticalAlignment: TextInput.AlignVCenter
            selectByMouse: true
            inputMethodHints: Qt.ImhDigitsOnly
            activeFocusOnTab: true
            Accessible.role: Accessible.SpinBox
            Accessible.name: root.tip.length > 0 ? root.tip : root.axis
            onActiveFocusChanged: if (activeFocus) selectAll(); else root.commitText()
            Keys.onReturnPressed: root.commitText()
            Keys.onEnterPressed: root.commitText()
            Keys.onEscapePressed: (event) => {
                if (text !== String(root.value)) {
                    text = String(root.value)
                    event.accepted = true
                } else {
                    event.accepted = false     // nothing to revert: Esc goes on (Focus 2D)
                }
            }
            Keys.onUpPressed: { root.ask(root.value + 1); text = String(root.value) }
            Keys.onDownPressed: { root.ask(root.value - 1); text = String(root.value) }
            WheelHandler {
                enabled: input.activeFocus
                onWheel: (event) => {
                    root.ask(root.value + (event.angleDelta.y > 0 ? 1 : -1))
                    input.text = String(root.value)
                }
            }
        }
        StepButton {
            id: inc
            objectName: root.objectName + ".inc"
            sign: "+"
            anchors.verticalCenter: parent.verticalCenter
            enabled: root.value < root.to
            onClicked: root.ask(root.value + 1)
        }
    }
    BtToolTip { shown: hover.hovered && !input.activeFocus; text: root.tip }
    HoverHandler { id: hover }
}
