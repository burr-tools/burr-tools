# Qt GUI: gotchas found the hard way

- An `id` on a `Popup`'s `contentItem` (any name) broke its layout (rows
  102 px wide, clicks missing) in `DisplayMenu.qml`; reach the rows through
  `root.contentItem.children` instead.
- `x as Popup` is null for `QtQuick.Controls.Basic` popups (a separate QML
  type); cast to `Dialog` or the concrete type.
- A JavaScript line starting with `(` continues the previous statement
  (automatic semicolon insertion); assign to a `const` first.
- Menus open as their own windows (`Popup.Window`): a click in the main
  window does not close them by itself (`KeyboardCues::windowPressed`), and
  the application's activation, not the window's, tells when focus left.
- `forceActiveFocus` in a test window can leave focus nowhere; Tab works.
- A delegate that redeclares a property of its base type
  (`required property string title` on an `AccordionSection`) shadows it, and
  the base draws nothing: write `required title`. A property named like an
  `Item` one (`top`) does not load at all, and only a test run says why.
- A binding that calls a function reading a model does not follow the model:
  read something that notifies, in the expression (`rules.revision >= 0 && ...`).
  A `const` that only reads it can be compiled away.
- A `Dropdown` click writes `currentIndex` from JavaScript, which ends a binding
  on it: an owner that binds it binds it again in `onActivated`
  (`Qt.binding`). A test of a dialog clicks its buttons: calling the controller's
  answer directly hid a confirm that closed before it answered.
- `required used` on a delegate marks a base type's own property required; a
  `Popup`'s rows need a frame to be laid out before a test clicks them.
- Collapsing is per workspace: a test that collapses a card in Puzzle resets
  it there, before leaving for Entities.
- A TapHandler never sees TestCase's double-click (`mouseDoubleClick`,
  `mouseDoubleClickSequence`, two `mouseClick`s): QtTest moves its clock a
  double-click interval on after each release, so every press is a first tap.
  Use `Snapshots.doubleClick(item, x, y)` (`import BurrTools.Test`): two presses
  10 ms apart on QtTest's clock, a gap on each side. A delegate rebuilt by the
  first click (a library chip that gained a count) is found again before the
  second.
- The Qt Quick message dialogs are in-window and modal on Windows and Linux:
  count them in `modalOpen`, and dismiss them in tests.
- A C++ enum whose values do not count up from 0 reads as `undefined` in
  ahead-of-time compiled QML; hand QML the numbers as a map
  (`InspectorController::tasks`).
- A `HoverHandler` on a parent sees no hover over the 3D view (a
  `QQuickRhiItem` that takes hover itself): put a strip item with a
  non-blocking `HoverHandler` over the part to watch (`ViewportCard`'s
  reveal zone).
- A `ListView` whose height depends on its `count` resizes inside a model
  reset and lays out rows the reset just took ("DelegateModel::cancel"
  warnings): size it from outside, `cacheBuffer: 0`.
- Positioners (`Row`, `Column`, `Flow`) lay their children out on the next
  polish: a test reads their geometry after `waitForRendering` or
  `tryVerify`, not at once.
- An item that is a Tab stop moves the focus on Tab itself: the key never
  reaches a parent's `Keys.onTabPressed`. Put the handler on the control.
- A `ListView` makes only the rows in sight: Tab stops at the last of them,
  and once scrolled back the chain follows the order the rows were made.
  A list of controls hands the keyboard on row by row and scrolls first
  (`PuzzleLeftCard`'s table: `focusRow`, `tabOut`, `backtabOut`).
- A `ListView` following `currentIndex` glides to it at 400 px/s (late for
  held arrows, moving under reduced motion) or not at all: call
  `positionViewAtIndex(i, ListView.Contain)` when the selection changes.
- A `Canvas` asked to `requestPaint()` paints while hidden too; check
  `visible` first and paint when it shows (`DashedFrame`).
- A `ToolTip` made with `visible` already true shows at once, skipping its
  `delay`: make it hidden, then let it show (`IconButton.wantTip`).
- A locked Windows session does not compose: Qt gets no update requests and
  the window waits seconds for its next frame. Time start-up on an unlocked
  display, or offscreen for the QML and C++ part.
- Windows draws the title bar dark or light from
  `QStyleHints::setColorScheme`; ask for the theme's scheme before the
  first window (`Theme`'s constructor), not only when it changes.
