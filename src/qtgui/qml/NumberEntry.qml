import QtQuick

// What typing in a number field does, for the Stepper and the table's counts.
// The field shows `value` at once, whoever changed it, until a key is typed;
// from there on it holds the text.
//
// With `live`, a short pause commits what is typed while the field stays in use:
// typing 15 slowly commits 1 and then 15. A pause commits only a number in
// liveFrom ... to, so a 0 that would remove a piece waits for Enter.
//
// Enter or leaving the field (finish) commits the text, clamped to from ... to,
// and Esc (cancel) goes back to the value it began from.
Timer {
    id: root
    required property TextInput field
    property int value: 0
    property int from: 0
    property int to: 0
    property int liveFrom: from
    property bool live: true

    signal committed(int value)
    signal started()
    signal ended()
    // the text was no number
    signal rejected()

    // from the first key until the number is finished or dropped
    property bool entering: false
    // text the field holds that no commit has taken yet
    property bool typing: false
    property int began: 0
    // a number being typed that an owner moving on must settle: the commit
    // this entry is making right now is not one, the entry goes on after it
    readonly property bool open: entering && !asking
    property bool asking: false

    interval: 350

    onValueChanged: if (!typing) field.text = String(value)

    function number(): int {
        const text = field.text.trim()
        const n = parseInt(text)
        return !isNaN(n) && String(n) === text ? n : -1
    }

    function ask(v: int): void {
        const c = Math.max(from, Math.min(to, v))
        if (c !== value) {
            asking = true
            committed(c)
            asking = false
        }
    }

    function close(): bool {
        stop()
        typing = false
        const was = entering
        entering = false
        return was
    }

    function edited(): void {
        if (!entering) {
            entering = true
            began = value
            started()
        }
        typing = true
        if (live)
            restart()
    }

    onTriggered: {
        const n = number()
        if (n < liveFrom || n > to)
            return                       // not yet a number to act on
        typing = false
        ask(n)
        if (value !== n)
            field.text = String(value)   // refused, or waiting for an answer
    }

    function finish(): void {
        const pending = typing
        const n = number()
        const was = close()
        if (pending) {
            if (n < 0)
                rejected()
            else
                ask(n)
        }
        field.text = String(value)
        if (was)
            ended()
    }

    function cancel(): void {
        const start = began
        const was = close()
        if (was)
            ask(start)
        field.text = String(value)
        if (was)
            ended()
    }

    // the text goes, the value stays
    function drop(): void {
        const was = close()
        field.text = String(value)
        if (was)
            ended()
    }

    // a step ends any typing: it goes from the value, not from the text
    function step(v: int): void {
        drop()
        ask(v)
        field.text = String(value)
    }
}
