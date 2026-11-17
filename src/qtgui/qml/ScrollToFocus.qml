import QtQuick
import BurrTools.Ui

// Keeps what the keyboard is on in view of a Flickable: show(item) scrolls just
// far enough, and with `follow` it does so for whatever inside the Flickable
// takes the keyboard's focus. The mouse scrolls for itself: only Tab and the
// arrows move it.
Item {
    id: root
    required property Flickable view
    property bool follow: false
    visible: false

    function inside(item: Item): bool {
        for (let p = item; p !== null; p = p.parent)
            if (p === view.contentItem)
                return true
        return false
    }

    function show(item: Item): void {
        const margin = 4
        const top = item.mapToItem(view.contentItem, 0, 0).y
        const bottom = top + item.height
        const most = Math.max(0, view.contentHeight - view.height)
        if (top - margin < view.contentY)
            view.contentY = Math.max(0, top - margin)
        else if (bottom + margin > view.contentY + view.height)
            view.contentY = Math.min(most, bottom + margin - view.height)
    }

    Connections {
        target: root.Window
        enabled: root.follow
        function onActiveFocusItemChanged() {
            const item = root.Window.activeFocusItem
            if (item !== null && App.keyboardCues.navigating && root.inside(item))
                root.show(item)
        }
    }
}
