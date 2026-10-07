import QtQuick
import QtQuick.Controls
import QtTest
import BurrTools.Ui
import BurrTools.Test

// The component gallery (Gallery.qml) against the spec, in the light and the
// dark theme, at the device-pixel ratio of the run (meson runs this file at
// 1, 1.5 and 2 -- QT_SCALE_FACTOR):
//   * sizes from design-tokens §4, the same in dp at every ratio;
//   * state colours from design-tokens §5, read from the drawn pixels;
//   * dp scaling: grabs are the ratio times the dp size, 1 dp lines survive;
//   * every row and the glyph / icon sheets against reference images
//     (Snapshots, tolerant diff; a platform without references skips).
TestCase {
    id: tc
    name: "Gallery"
    when: windowShown

    Component { id: galleryComponent; Gallery {} }

    property var win

    function initTestCase() {
        App.settings.restoreAllDefaults()
        win = galleryComponent.createObject(null)
        verify(win !== null, "Gallery.qml failed to load")
        // every grab draws the whole window: keep it small, rows scroll in
        win.height = 800
        win.requestActivate()
        tryVerify(() => win.active, 3000)
        waitForRendering(win.contentItem)
    }

    function cleanupTestCase() {
        win.destroy()
        App.settings.restoreAllDefaults()
    }

    function init() {
        failOnWarning(/^(?!No QRhi found for window|This plugin does not support grabbing the keyboard).*/)
    }

    function cleanup() {
        rest()
    }

    // --- helpers -----------------------------------------------------------------

    function cell(name) {
        const c = findChild(win.contentItem, "gallery." + name)
        verify(c !== null, "no gallery cell " + name)
        return c
    }
    function control(name) {
        const c = cell(name).control
        verify(c !== null, "gallery cell " + name + " is empty")
        return c
    }

    // a gallery tooltip, its row scrolled into view (only then is it shown)
    function tooltip(kind) {
        reveal(findChild(win.contentItem, "gallery.tooltip"))
        const t = cell("tooltip." + kind).popup
        verify(t !== null, "no gallery tooltip " + kind)
        verify(t.visible, "gallery tooltip " + kind + " not shown")
        return t
    }

    // scroll the body so the item is in view (grabs see only the window)
    // always to the same scroll position for a row, whatever ran before it: at
    // a fractional ratio (1.5) text lands on another sub-pixel phase when the
    // row sits elsewhere, so a filtered run (BURRTOOLS_SNAPSHOTS_ONLY) has to
    // place each row exactly as a full run does
    function reveal(item) {
        const flick = findChild(win.contentItem, "gallery.body")
        const body = flick.contentItem
        const top = item.mapToItem(body, 0, 0).y
        flick.contentY = Math.max(0, Math.min(top - 16, flick.contentHeight - flick.height))
    }

    // the pointer off every control, the keyboard focus on none
    function rest() {
        mouseMove(win.header, win.header.width - 4, 4)
        win.contentItem.forceActiveFocus()
    }

    // bring a row into view with its "hover" and "focus" cells in their states
    function arrange(rowName) {
        rest()
        reveal(findChild(win.contentItem, "gallery." + rowName))
        const hover = findChild(win.contentItem, "gallery." + rowName + ".hover")
        const focus = findChild(win.contentItem, "gallery." + rowName + ".focus")
        if (focus)
            focus.control.forceActiveFocus(Qt.TabFocusReason)
        if (hover)
            mouseMove(hover.control, hover.control.width / 2, hover.control.height / 2)
    }

    // straight alpha over an opaque colour
    function over(top, bottom) {
        const a = top.a
        return Qt.rgba(top.r * a + bottom.r * (1 - a), top.g * a + bottom.g * (1 - a), top.b * a + bottom.b * (1 - a), 1)
    }

    function near(a, b, tol) {
        const t = (tol === undefined ? 3 : tol) / 255
        return Math.abs(a.r - b.r) <= t && Math.abs(a.g - b.g) <= t && Math.abs(a.b - b.b) <= t
    }

    function hex(c) {
        const h = (v) => ("0" + Math.round(v * 255).toString(16)).slice(-2)
        return "#" + h(c.r) + h(c.g) + h(c.b)
    }

    /* Arrange a row, then check colours read from one grab. Each check is
     * [cell name or item, x, y, expected colour or predicate, what], with x
     * and y in dp in the item's own coordinates. */
    function expectColors(rowName, checks) {
        arrange(rowName)
        const items = checks.map(c => typeof c[0] === "string" ? control(c[0]) : c[0])
        const got = Snapshots.colors(checks.map((c, i) => [items[i], c[1], c[2]]))
        compare(got.length, checks.length)
        for (let i = 0; i < checks.length; i++) {
            const want = checks[i][3]
            if (typeof want === "function")
                verify(want(got[i]), checks[i][4] + ": " + hex(got[i]))
            else
                verify(near(got[i], want), checks[i][4] + ": " + hex(got[i]) + ", expected " + hex(want))
        }
    }

    // switch theme and let colour animations (the switch's 150 ms) finish;
    // grabs draw the window themselves, so no frame needs waiting for
    function useTheme(theme) {
        if (App.settings.theme === theme)
            return
        App.settings.theme = theme
        wait(250)
    }

    function themes() {
        return [ { tag: "light", theme: "light" }, { tag: "dark", theme: "dark" } ]
    }

    // --- coverage ------------------------------------------------------------------

    function test_everyBuiltPrimitiveHasARow() {
        for (const row of ["iconButton", "button", "buttonPrimary", "toolbarButton", "railButton",
                           "segmented", "switch", "checkBox", "radio", "radioCard", "checkRow", "dropdown",
                           "searchField", "numberField", "slider", "menuItem", "menuPanel", "tooltip",
                           "keyBadge", "card"])
            verify(findChild(win.contentItem, "gallery." + row) !== null, row)
    }

    function test_theContactSheetsShowEveryAsset() {
        // PR-17: every glyph of the theme's set and every UI icon
        const glyphs = App.assetNames("glyphs/light")
        verify(glyphs.length >= 40, glyphs.length + " glyphs")
        compare(App.assetNames("glyphs/dark"), glyphs)       // both themes have the whole set
        compare(findChild(win.contentItem, "gallery.glyphs").names.length, glyphs.length)
        compare(findChild(win.contentItem, "gallery.icons").names, App.assetNames("icons"))
        verify(App.assetNames("icons").indexOf("trash") >= 0)
    }

    // --- design-tokens §4: sizes in dp ------------------------------------------------

    function test_sizesFollowTheTokens_data() {
        return [
            { tag: "icon button 28", cell: "iconButton.normal", w: 28, h: 28 },
            { tag: "button h30", cell: "button.normal", h: 30 },
            { tag: "small button h26", cell: "button.small", h: 26 },
            { tag: "3D toolbar button 48x44", cell: "toolbarButton.normal", w: 48, h: 44 },
            { tag: "compact toolbar button 34", cell: "toolbarButton.compact", w: 34, h: 34 },
            { tag: "switch 32x18", cell: "switch.off", w: 32, h: 18 },
            { tag: "dropdown h30", cell: "dropdown.normal", h: 30, minW: 120 },
            { tag: "segmented: h22 options on a 2 dp padded track", cell: "segmented.normal", h: 26 },
            { tag: "popup item h30", cell: "menuItem.normal", h: 30 },
            { tag: "check row h30", cell: "checkRow.normal", h: 30 },
            { tag: "number field h30", cell: "numberField.normal", h: 30 },
            { tag: "rail button 52x50", cell: "railButton.normal", w: 52, h: 50 },
        ]
    }
    function test_sizesFollowTheTokens(data) {
        const c = control(data.cell)
        if (data.w !== undefined)
            compare(c.width, data.w)
        if (data.h !== undefined)
            compare(c.height, data.h)
        if (data.minW !== undefined)
            verify(c.width >= data.minW)
    }

    function test_tooltipFollowsTheTokens() {
        // design-tokens §7: padding 4x8, 12 dp text, at most 280 dp wide
        const tips = [tooltip("short"), tooltip("long")]
        for (const t of tips) {
            verify(t.visible)
            compare(t.topPadding, 4)
            compare(t.bottomPadding, 4)
            compare(t.leftPadding, 8)
            compare(t.rightPadding, 8)
            compare(t.font.pixelSize, 12)
            verify(t.width <= 280)
        }
        verify(tips[0].width < 280)
        compare(tips[1].width, 280)                          // the long one wraps at the maximum
        verify(tips[1].contentItem.lineCount >= 2)
    }

    // --- design-tokens §5: state colours, read from the pixels ---------------------------

    function test_stateColoursFollowTheTokens_data() { return themes() }
    function test_stateColoursFollowTheTokens(data) {
        useTheme(data.theme)
        const panel = Theme.panel, panel2 = Theme.panel2, accent = Theme.accent, line2 = Theme.line2
        const soft = over(Theme.accentSoft, panel)
        const white = Qt.color("white")

        // PR-01: transparent on the card; hover panel2; on accentSoft; focus 2 dp accent
        expectColors("iconButton", [
            ["iconButton.normal", 3, 14, panel, "icon button"],
            ["iconButton.on", 3, 14, soft, "icon button on"],
            ["iconButton.hover", 3, 14, panel2, "icon button hover"],
            ["iconButton.focus", 1, 14, accent, "icon button focus"],
        ])
        // PR-04: panel fill, 1 dp line2 border, hover panel2, focus ring outside
        expectColors("button", [
            ["button.normal", 5, 15, panel, "button fill"],
            ["button.normal", 0.25, 15, line2, "button border"],
            ["button.hover", 5, 15, panel2, "button hover"],
            ["button.focus", -2, 15, accent, "button focus ring"],
        ])
        // primary: accent fill, +8 % on hover; disabled at 40 %
        expectColors("buttonPrimary", [
            ["buttonPrimary.normal", 5, 15, accent, "primary button"],
            ["buttonPrimary.hover", 5, 15, (c) => c.hslLightness > accent.hslLightness, "primary hover is lighter"],
            ["buttonPrimary.disabled", 5, 15, over(Qt.rgba(accent.r, accent.g, accent.b, 0.4), panel), "primary button disabled"],
        ])
        // 3D toolbar and rail buttons: on / selected = accentSoft
        expectColors("toolbarButton", [ ["toolbarButton.on", 3, 22, soft, "toolbar button on"] ])
        expectColors("railButton", [ ["railButton.selected", 8, 4, soft, "rail button selected"] ])
        // PR-05 segmented: panel2 track, the chosen option on panel
        expectColors("segmented", [
            ["segmented.normal", 1.5, 13, panel2, "segmented track"],
            ["segmented.normal", 4, 13, panel, "segmented option"],
        ])
        // PR-07 switch: off line2 track, on accent track, white knob that moves
        expectColors("switch", [
            ["switch.off", 25, 9, line2, "switch off track"],
            ["switch.off", 9, 9, white, "switch off knob"],
            ["switch.on", 7, 9, accent, "switch on track"],
            ["switch.on", 23, 9, white, "switch on knob"],
        ])
        // check box: line2 box, accent when on, muted border on hover
        expectColors("checkBox", [
            ["checkBox.off", 1, 13, line2, "check box border"],
            ["checkBox.on", 1.5, 13, accent, "check box on"],
            ["checkBox.hover", 1, 13, Theme.muted, "check box hover"],
        ])
        // PR-27 radio card: selected accentSoft, hover panel2
        const card = control("radioCard.normal")
        expectColors("radioCard", [
            ["radioCard.normal", card.width - 6, card.height / 2, panel, "radio card"],
            ["radioCard.selected", card.width - 6, card.height / 2, soft, "radio card selected"],
            ["radioCard.hover", card.width - 6, card.height / 2, panel2, "radio card hover"],
        ])
        // PR-16 / PR-11 rows: hover and highlight panel2
        expectColors("checkRow", [ ["checkRow.hover", 160, 15, panel2, "check row hover"] ])
        expectColors("menuItem", [ ["menuItem.highlighted", 4, 15, panel2, "menu row highlighted"] ])
        // PR-19 dropdown: panel fill, line2 border, hover panel2, focus accent
        expectColors("dropdown", [
            ["dropdown.normal", 4, 15, panel, "dropdown fill"],
            ["dropdown.normal", 0.25, 15, line2, "dropdown border"],
            ["dropdown.hover", 4, 15, panel2, "dropdown hover"],
            ["dropdown.focus", 0.5, 15, accent, "dropdown focus"],
        ])
        // PR-15 key badge: panel2 on a line2 border
        const badge = control("keyBadge.keys").children[0]
        expectColors("keyBadge", [
            [badge, 2.5, badge.height / 2, panel2, "key badge fill"],
            [badge, 0.25, badge.height / 2, line2, "key badge border"],
        ])
        // PR-14 tooltip: panel on a line2 border
        const tip = tooltip("short")
        expectColors("tooltip", [
            [tip.background, 3, tip.height / 2, panel, "tooltip fill"],
            [tip.background, 0.25, tip.height / 2, line2, "tooltip border"],
        ])
    }

    function test_disabledIsFortyPercent() {
        for (const name of ["iconButton.disabled", "button.disabled", "buttonPrimary.disabled",
                            "toolbarButton.disabled", "segmented.disabled", "switch.disabled", "checkBox.disabled", "checkRow.disabled",
                            "dropdown.disabled", "numberField.disabled", "slider.disabled"])
            compare(control(name).opacity, 0.4, name)
    }

    // --- dp scaling -------------------------------------------------------------------

    function test_theRunsRatioIsTheWindowsRatio() {
        compare(Snapshots.ratio(win.contentItem), Snapshots.expectedRatio)
    }

    function test_grabsAreTheRatioTimesTheDpSize() {
        const r = Snapshots.ratio(win.contentItem)
        for (const name of ["iconButton.normal", "button.normal", "switch.off", "dropdown.normal", "toolbarButton.normal"]) {
            const c = control(name)
            reveal(c)
            const s = Snapshots.grabSize(c)
            verify(Math.abs(s.width - c.width * r) <= 1 && Math.abs(s.height - c.height * r) <= 1,
                   name + ": " + s.width + "x" + s.height + " px for " + c.width + "x" + c.height + " dp at " + r)
        }
    }

    function test_hairlinesSurviveEveryRatio_data() { return themes() }
    function test_hairlinesSurviveEveryRatio(data) {
        // a 1 dp border must still read as its colour, not a blur of it
        useTheme(data.theme)
        const line2 = Theme.line2
        expectColors("button", [8, 15, 22].map(y => ["button.normal", 0.25, y, line2, "button border at y " + y]))
        const d = control("dropdown.normal")
        expectColors("dropdown", [ ["dropdown.normal", d.width - 0.25, 15, line2, "dropdown right border"] ])
    }

    // --- reference images -------------------------------------------------------------

    function test_snapshots_data() {
        const rows = ["iconButton", "button", "buttonPrimary", "toolbarButton", "railButton",
                      "segmented", "switch", "checkBox", "radio", "radioCard", "checkRow", "dropdown",
                      "searchField", "numberField", "slider", "menuItem", "menuPanel", "tooltip",
                      "keyBadge", "card", "glyphs", "icons"]
        const out = []
        for (const t of ["light", "dark"])
            for (const r of rows)
                if (Snapshots.wanted(r))
                    out.push({ tag: r + " " + t, row: r, theme: t })
        return out
    }
    function test_snapshots(data) {
        useTheme(data.theme)
        const row = findChild(win.contentItem, "gallery." + data.row)
        verify(row !== null)
        reveal(row)
        arrange(data.row)
        // grabbed at once: nothing waits long enough for a hover tooltip
        const result = Snapshots.check(row, data.row + "-" + data.theme)
        if (result.startsWith("missing: "))
            skip(result)
        verify(result === "", result)
    }
}
