Based on the design mock in #128 , I asked Opus 5.5 high to do an analysis between Qt6 and RmlUI. The analysis focused on the following points:
* mockup coverage possibility and any custom changes to implement 100% of the mockup
* expected performance (and reasons for any gain/loss of performance over other)
* testability of the UI
* Community support
* 5 year future view of the codebase with respect to both maintainability and upgrades to latest versions of the framework
* New contributors, learning curve to make UI changes (with/without AI agent help) etc.

## tldr; super short summary view:

**Verdict: we will go ahead with Qt6 (qt quick).**
3 Point For and Against view:

**RmlUi**

For:
1. Lightest and fastest: quick startup, low memory, small package.
2. HTML/CSS-like, so the mock ports almost as-is.
3. MIT licence and small codebase, easy to pin and cross-compile.

Against:
1. CSS gaps (no grid, no dashed borders) and no native dialogs or accessibility.
2. Testing is do-it-yourself, with no test framework.
3. One lead maintainer and breaking changes between major versions.

**Qt 6 (Qt Quick)**

For:
1. Mature testing in CI, plus native dialogs, high DPI and OS theme built in.
2. Large community and strong AI support.
3. Predictable releases, and reaches 100% of the mock.

Against:
1. Heavier: slower startup, more memory, larger package.
2. Every screen rebuilt in QML, and a 3D graphics-API decision on Windows.
3. Must move to each new minor release, and Windows builds are harder.


## Full Analysis

<p>I'd pick <strong>Qt 6 with Qt Quick (QML)</strong>, with your C++ controllers behind it and the 3D view drawn natively through Qt's GPU layer. RmlUi is the better engine for small size, minimal rendering overhead and porting the HTML mock almost as-is. Qt wins on testability, native platform features, contributor pool, AI tooling and the 5-year outlook. For a volunteer-run open-source project those factors outweigh RmlUi's performance lead, which BurrTools' workload mostly won't notice.</p>
<p>The rest of this explains why, and what each choice costs. The performance figures are my estimates, not measurements; a short spike should confirm them.</p>
<h2>Starting point</h2>
<p>The codebase constrains both options. It builds with Meson on C++11 and depends on Boost, OpenGL with GLU and freeglut, FLTK 1.3, libpng and zlib; Windows binaries are cross-compiled from Linux with MinGW. The repository is GPL-3.0, its latest release is 0.7.1 (November 2025), and it has about 36 stars and 14 forks.</p>
<p>Three consequences apply whichever framework you choose:</p>
<ul>
<li><strong>Decoupling is the biggest job.</strong> <code>mainwindow.cpp</code> is a 4,500-line FLTK window class full of callbacks. Pulling that out into controllers that implement the spec's UI contract (<code>overview/06</code>) is most of the work, and it is framework-neutral. It also makes a later framework switch much cheaper.</li>
<li><strong>The 3D view needs modernising anyway.</strong> The legacy fixed-function OpenGL and GLU code won't sit cleanly under either framework's modern rendering path. Budget a rewrite of the renderer to modern OpenGL, or to Qt's graphics layer.</li>
<li><strong>Licensing is not a blocker.</strong> Qt's LGPL and GPL modules and RmlUi's MIT licence are both compatible with GPL-3.0.</li>
</ul>
<h2>1. Mock coverage and what each needs to reach 100%</h2>
<p>The mock is HTML/CSS. RmlUi's markup and stylesheets resemble it, so the page structure ports almost directly and only the JavaScript logic moves into C++. QML needs every screen rebuilt as components. Both can reach 100%, but the gaps fall in different places.</p>

Mock feature | RmlUi 6.2 | Qt Quick 6.11
-- | -- | --
Flex layouts, cards, rails | Supported, with small limits (order and flex-basis: content are missing) | Row, Column and layout types
CSS grid (transform matrix, list rows, detail tiles, status grid) | No grid support; rewrite as flexbox or tables | GridLayout
Rounded corners, shadows, gradients, grey-out filter | Supported since 6.0, but only the OpenGL 3 renderer implements the new effects; other backends keep the old feature set | Rectangle radius, gradients, MultiEffect
Dashed and dotted borders (variable voxels, wireframe chips, drop zones) | Not supported: the border shorthand excludes border style. Needs image or SVG decorators, or a custom decorator | Shape with a dash pattern
Vertical rail titles | transform: rotate | rotation
Text ellipsis | Supported, added in 6.2 with ellipsis and custom strings | elide
Font fallback lists | Only one font family per rule; fallbacks are registered in the engine | Uses system fallback
Menus, popovers, tooltips, modals | Build them yourself in markup | Popup, Menu, ToolTip, Dialog
Native file dialogs, colour picker | None; needs a helper library and a custom colour picker | Built in (QtQuick.Dialogs)
Drag and drop (shape reorder, adding pieces) | Drag events with a drag: clone property | DragHandler and DropArea
Text input, IME | Supported, with a sample showing IME; support varies by platform | Full
SVG glyphs | Plugin, now with an SVG document and texture cache | Qt SVG / VectorImage
Composed edit cursors | Via the system interface and an OS cursor call (custom work) | QCursor from a pixmap
Light / Dark / System theme with OS change detection | Swap stylesheets; detect via SDL3's system-theme event | Built-in colour-scheme hint with a change signal
High DPI | Per-context scaling ratio; the Win32 backend supports high DPI only on Windows 10 and newer | Automatic
2D voxel grid editor | Custom element drawing through the render interface | Custom QQuickItem with scene-graph geometry
3D viewport and view cube | Your OpenGL draws straight into the window | Custom item rendered through Qt's GPU layer (QRhi), or an underlay
OS accessibility tree (also used by UI automation) | None | Yes


<p>To reach 100% with <strong>RmlUi</strong> you would need to:</p>
<ul>
<li>move to the GL3 renderer;</li>
<li>replace every grid with flexbox or tables;</li>
<li>build a dashed-border decorator;</li>
<li>write custom elements for the voxel grid and the 3D view;</li>
<li>add a platform layer (SDL3 or GLFW) for windows, cursors, DPI, clipboard and theme detection, plus native file dialogs and a colour picker built in markup;</li>
<li>write your own test harness.</li>
</ul>
<p>The "Show zones" overlay and the transitions port almost unchanged.</p>
<p>To reach 100% with <strong>Qt</strong> you would need to:</p>
<ul>
<li>build about 30 QML primitives matching <code>foundations/primitives.md</code>;</li>
<li>write custom scene-graph items for the voxel grid and dashed shapes;</li>
<li>decide how the 3D view renders on Windows (see below);</li>
<li>raise the codebase to C++17 and use Meson's <code>qt6</code> module.</li>
</ul>
<p>Meson's module handles moc and rcc and can build QML modules that mix C++ and QML.</p>
<p>Qt Widgets with stylesheets would reach roughly 85% fidelity. Shadows, animations and the chip and card styling get awkward there, so I'd rule it out for this mock.</p>
<h2>2. Expected performance</h2>
<p><strong>RmlUi is lighter</strong>, mainly for structural reasons:</p>
<ul>
<li>The application owns the window and the OpenGL context. The 3D view draws straight into the back buffer and the UI draws on top, with no extra copy.</li>
<li>The library is small, it has no script engine, and it loads in tens of milliseconds.</li>
<li>It can idle at near-zero CPU, because since 5.1 the app can ask the context how long until the next update is needed.</li>
</ul>
<p><strong>Qt costs more in startup time, memory and package size, but frame rates are a tie.</strong></p>
<ul>
<li><strong>Startup and memory:</strong> the QML engine and Qt libraries typically add a few hundred milliseconds to cold start. Memory is often 2–4× RmlUi's, and the shipped package adds tens of megabytes of DLLs.</li>
<li><strong>Frame rate:</strong> the scene graph is GPU-batched and runs on a separate render thread, which keeps animations smooth.</li>
<li><strong>3D view detail:</strong> Qt's 3D widget class defaults to Direct3D 11 on Windows and always renders into a backing texture rather than straight to the window. That means one extra full-viewport texture pass, which is negligible on any modern GPU.</li>
<li><strong>Closing the gap:</strong> you can match RmlUi's direct path with an underlay that renders before the scene graph, or by forcing the OpenGL backend. Qt Quick has had a custom item type for GPU-API rendering (QQuickRhiItem) since 6.7, replacing the older framebuffer-object approach.</li>
</ul>
<p>For BurrTools both will hold 60 fps. The heavy work, solving and 3D drawing, is native code under either framework. Where you will notice a difference is cold start, memory and package size, all in RmlUi's favour. Large lists would favour Qt, whose ListView only creates visible rows, but you've said scale isn't a concern.</p>
<h2>3. Testability</h2>
<p><strong>Qt is clearly stronger.</strong></p>
<ul>
<li><strong>Test frameworks:</strong> Qt Test covers the C++ side. Qt Quick Test writes test cases as JavaScript functions inside a TestCase element, and running with <code>-platform offscreen</code> avoids opening a window, which suits CI.</li>
<li><strong>Screenshot tests:</strong> item-level screenshot capture makes visual regression tests easy.</li>
<li><strong>Stable ids:</strong> <code>objectName</code> can carry the spec's ids.</li>
<li><strong>External automation:</strong> the accessibility tree lets Windows UI Automation tools drive the app.</li>
</ul>
<p><strong>RmlUi testing is do-it-yourself, but workable.</strong></p>
<ul>
<li><strong>Logic tests:</strong> you can inject input into a context and query the element tree by id, so behaviour tests are deterministic.</li>
<li><strong>Visual tests:</strong> these need an OpenGL context in CI, such as Mesa's software renderer. RmlUi's own visual test suite, which compares captures against references, is a usable model.</li>
<li><strong>No external tools:</strong> with no accessibility tree, outside automation tools can't see the widgets.</li>
</ul>
<h2>4. Community and the 5-year outlook</h2>
<p><strong>Qt</strong> has a long track record, with a commercial company, a large open-source ecosystem and a predictable release cadence. Qt 6.11 shipped in March 2026, highlighting hardware-accelerated 2D rendering, and 6.12 LTS is planned for October 2026.</p>
<p>There are two things to plan for:</p>
<ul>
<li><strong>Open-source users can't sit on an LTS release.</strong> Only the first patch releases of an LTS version go to open-source users; ongoing LTS releases are for commercial customers. The project should therefore move to each new minor release roughly every six months. That is usually routine, because Qt aims for source compatibility across releases of the same major version.</li>
<li><strong>The GPU-layer classes can change between releases.</strong> The QRhi classes come with only limited compatibility guarantees, so keep the 3D renderer behind a thin adapter. A Qt 7 may arrive within five years. Qt 5 to 6 was work, but it was documented and gradual.</li>
</ul>
<p><strong>RmlUi</strong> is healthy but small. It has about 4.2k stars and 446 forks, and 6.2 was released on 11 January 2026. It is also used in commercial games such as Nightdive's remasters.</p>
<p>The risks to plan for:</p>
<ul>
<li><strong>Single maintainer.</strong> The lead maintainer has written that the issue and pull-request backlog has grown and that their time is limited.</li>
<li><strong>Breaking changes between major versions.</strong> 6.0 brought an overhauled render interface and renamed CMake targets and options.</li>
<li><strong>Mitigation:</strong> vendor a pinned version, which the MIT licence and the small codebase make easy, and keep your render and system interfaces thin. Then a stall upstream costs you little.</li>
</ul>
<h2>5. Contributors and learning curve</h2>
<p>For a new open-source contributor, Qt is the more familiar platform. Many C++ desktop developers have used it, and its documentation, examples and Stack Overflow coverage are extensive. QML takes a week or two to learn, and Qt Creator and the QML language server help.</p>
<p>AI assistants have a lot of Qt training data. The Qt Company has also released agentic development skills and a QML coding skill for AI agents.</p>
<p>RmlUi feels easy at first to anyone who knows HTML and CSS. Its live stylesheet reload and in-app debugger, now including a live data-model viewer, make style tweaks quick. The trap is that its stylesheet language is a subset of CSS.</p>
<p>Contributors, and especially AI agents, will confidently write grid layouts, dashed borders or font fallback lists that silently don't work. Without agents you need the spec's RmlUi implementation guide; with them you need it plus a lint step. Data bindings and custom elements are C++-only, which raises the bar for UI-only contributors.</p>
<h2>Recommendation and next step</h2>
<p>Choose <strong>Qt 6 (Qt Quick)</strong> for:</p>
<ul>
<li>the best testability and native dialogs;</li>
<li>the accessibility tree, which helps UI automation even though screen readers aren't a goal;</li>
<li>the largest contributor pool and strongest AI support;</li>
<li>a well-trodden upgrade path over five years.</li>
</ul>
<p>Choose <strong>RmlUi</strong> only if the smallest footprint and lowest rendering overhead are hard requirements. In that case accept owning the platform layer, the test harness and a pinned, vendored copy of the library.</p>
<p>Before committing, I'd run a <strong>two-week spike in both</strong>: the Solver right card plus the 3D view with the view cube. Measure cold start, memory, frame time while dragging, package size, and how hard a screenshot test is to write in CI. That would replace my estimates with numbers.</p>
<p>Either way, build the controllers first against the spec's UI contract; that work carries over to whichever framework wins.</p>
<p>If you choose Qt, the spec's behaviour documents stay valid. I'd replace <code>foundations/rmlui-implementation-guide.md</code> with a Qt Quick guide and map the contract's ids and classes to QML object names and states.</p>
