import QtQuick
import QtQuick.Controls.Basic

// A group of buttons as one Tab stop (a roving tab index, as a toolbar):
// Tab enters at the button last used -- the first shown, until one is --
// and goes on past the group; the arrows move between its buttons by where
// they sit: ← / → along the reading order, ↑ / ↓ to the nearest one in the
// row above or below, Home / End to the first and the last. A click makes
// a button the group's stop too. A button joins with `roving: theGroup`
// (GlyphButton, ActionButton); a button hidden or disabled is passed by,
// and a group with none shown is no stop at all.
QtObject {
    id: rf

    // the buttons, in the order they joined
    property list<AbstractButton> members
    // the one used last; null until one is
    property AbstractButton current: null
    // the button Tab stops on: the one used last while it is shown, else the first shown
    readonly property AbstractButton stop: {
        if (current !== null && current.visible && current.enabled)
            return current
        for (let i = 0; i < members.length; i++)
            if (members[i].visible && members[i].enabled)
                return members[i]
        return null
    }

    function join(b: AbstractButton): void {
        const next = []
        for (let i = 0; i < members.length; i++)
            next.push(members[i])
        next.push(b)
        members = next
    }
    function leave(b: AbstractButton): void {
        const next = []
        for (let i = 0; i < members.length; i++)
            if (members[i] !== b)
                next.push(members[i])
        members = next
        if (current === b)
            current = null
    }

    // the buttons shown, in reading order: by row (a row is the buttons
    // whose middles are level), then left to right
    function shown(): list<AbstractButton> {
        const out = []
        for (let i = 0; i < members.length; i++)
            if (members[i].visible && members[i].enabled)
                out.push(members[i])
        out.sort((a, b) => {
            const pa = middle(a), pb = middle(b)
            const rowGap = Math.min(a.height, b.height) / 2
            if (Math.abs(pa.y - pb.y) >= rowGap)
                return pa.y - pb.y
            return pa.x - pb.x
        })
        return out
    }

    function middle(b: AbstractButton): point {
        return b.mapToItem(null, b.width / 2, b.height / 2)
    }

    // the key's move from `from`; true when the key was the group's
    function navigate(from: AbstractButton, key: int): bool {
        const list = shown()
        let i = list.length - 1
        while (i >= 0 && list[i] !== from)
            i--
        if (i < 0)
            return false
        let to = null
        switch (key) {
        case Qt.Key_Left:  to = i > 0 ? list[i - 1] : null; break
        case Qt.Key_Right: to = i < list.length - 1 ? list[i + 1] : null; break
        case Qt.Key_Home:  to = list[0]; break
        case Qt.Key_End:   to = list[list.length - 1]; break
        case Qt.Key_Up:
        case Qt.Key_Down:  to = nearestAcross(list, from, key === Qt.Key_Down ? 1 : -1); break
        default:           return false
        }
        if (to !== null && to !== from)
            to.forceActiveFocus(Qt.TabFocusReason)      // keyboard focus: the ring and tooltip show
        return true
    }

    // the button in the next row up (dir -1) or down (+1) nearest across
    function nearestAcross(list: list<AbstractButton>, from: AbstractButton, dir: int): AbstractButton {
        const f = middle(from)
        let best = null
        let bestDy = 0, bestDx = 0
        for (let i = 0; i < list.length; i++) {
            const b = list[i]
            const p = middle(b)
            const dy = (p.y - f.y) * dir
            if (dy < Math.min(b.height, from.height) / 2)
                continue                                 // the same row, or the other way
            const dx = Math.abs(p.x - f.x)
            if (best === null || dy < bestDy - 1 || (Math.abs(dy - bestDy) <= 1 && dx < bestDx)) {
                best = b
                bestDy = dy
                bestDx = dx
            }
        }
        return best
    }
}
