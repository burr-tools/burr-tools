# Qt GUI: accessibility and keyboard

The accessibility tree is what screen readers and UI automation use; treat
it as part of the UI. WCAG 2.1 AA is the yardstick.

- **Role, name, state on every control built from primitives** (a Rectangle
  with a TapHandler is invisible to assistive tech otherwise):
  `Accessible.role`, `Accessible.name` (what a sighted user reads, e.g.
  "S2 Cage, weight 2"), state (`checkable`/`checked`, `selectable`/`selected`),
  and `Accessible.onPressAction` doing what a click does. Mark decorative or
  duplicated texts `Accessible.ignored: true`. Toggle-like buttons set
  `toggle: true` (`IconButton`, `ToolbarButton`); workspace tabs are
  `PageTab`s.
- **Names for fields and groups** — a `NumberField`, `Segmented`,
  `ProblemList` takes its visible label as `Accessible.name`; a label beside
  it is not linked automatically.
- **Tooltips are not hover-only**: `shown: hovered || visualFocus`, and the
  same text as `Accessible.description` on the control it explains.
- **Focus:** buttons take focus on click too (`focusPolicy: Qt.StrongFocus`),
  so Tab goes on from what was last used; the ring shows only for keyboard
  focus (`visualFocus`). Read-only things are not Tab stops. When a control
  disappears under the user's hands (collapse, a popup closing), give the
  focus to its counterpart (`Main.qml` `keepToggleFocus`, `DisplayMenu`'s
  `onClosed`). Popups that the keyboard should drive take `focus: true`.
- **Keys:** Esc closes the innermost thing first (dialogs and menus count
  themselves in `openPopups`); Enter presses a dialog's default button;
  arrows move within lists and menus. New single-letter shortcuts need care
  (WCAG 2.1.4): prefer them scoped to the view they act on.
- **Contrast and colour:** text ≥ 4.5:1, borders, focus rings and state
  indicators ≥ 3:1 against what they sit on, in both themes; never colour or
  hue alone for a state (add text, a mark, a weight or a bar).
  `tst_quality.qml` computes contrast — extend its pairs when you add a
  surface.
- **Status changes are announced:** a message to the user goes through
  `App.status.flash(text)`, which the status bar announces
  (`Accessible.announce`) and keeps as its description.
- The accessibility work still open (text scaling, high contrast, small
  screens, the screen-reader pass, …) is in the deferred table of
  `src/qtgui/README.md`; don't re-open fixed items (the keyboard 3D view
  and reduced motion are done), and add new gaps there with a reason.
- **Motion through `Theme`:** durations come from its tokens
  (`durCollapse`, `durSwitch`, `motion()`), which are 0 while the system
  asks for reduced motion; a literal duration or an animation that ignores
  them breaks that.
