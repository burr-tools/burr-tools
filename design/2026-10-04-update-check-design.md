# Update Check via GitHub Releases — Design

Status: approved in brainstorming, pending spec review.

## 1. Goal

Tell users of a released BurrTools build when a newer release exists on
GitHub, show that release's notes, and let them open the release page in their
browser with one click. No download or installation is performed.

**Success looks like:** a user running 0.7.1 launches the app after 0.7.2 is
published, sees the 0.7.2 release notes, and reaches the download page in one
click — while the check never slows startup, never hangs when offline, and
never nags a user who has declined that version.

### Decisions

| Question | Decision |
| :--- | :--- |
| Launch check default | **On**, with an opt-out checkbox in Settings. |
| Launch check frequency | At most once per **24 hours** (successful fetches only). Manual checks are never rate-limited. |
| Dialog actions | **Open Release Page**, **Remind Me Later**, **Skip This Version**. Skip is offered only on launch checks. |
| Development builds | Any build past a tag or dirty (`v0.7.1-42-gabc`, `v0.7.1-dirty`) **never auto-checks**. A manual check still works and compares against the base tag. |
| Unparseable version | No auto-check. Manual check reports that the installed version cannot be determined. |
| Pre-releases / drafts | Ignored (`/releases/latest` excludes them; the parser also rejects them defensively). |
| HTTPS transport | Native per platform behind one interface: NSURLSession (macOS), WinHTTP (Windows), libcurl (Linux, optional). |
| JSON | nlohmann/json, header-only, added as a Meson wrap. |
| Release-notes rendering | Plain text in `Fl_Text_Display` (notes are Markdown generated from `NEWS`, readable raw). |

### Non-goals

- Downloading or installing updates.
- Rendering Markdown.
- Pre-release / beta channels.
- Checking anything other than `burr-tools/burr-tools`.

## 2. Context

- BurrTools has no networking code and no HTTP or JSON library today.
  `fl_open_uri()` is already used to open the user guide
  (`src/gui/platform.cpp`).
- `meson.build` sets the project version from `git describe --tags --always
  --dirty`, falling back to `0.7.0-unknown`, but the value is not exposed to
  C++. The About box shows no version.
- Release tags are `vX.Y.Z`. The repository also carries a historical release
  tag `temp-64-bit`, which must be rejected as a version.
- Settings live in `configuration_c` (`src/gui/configuration.{h,cpp}`): typed
  entries registered with `register_entry`, persisted as a Lua `key = value`
  file. The `int` type is 32-bit.
- Background work is observed from the GUI thread by polling with
  `Fl::add_timeout`; nothing calls `Fl::lock()`.
- Menus: `src/gui/mainmenu.cpp` has a portable table (top-level "Settings"
  and "About") and a macOS table, where About/Settings/Quit live in the
  application menu built by `mainmenu::installApplicationMenu()`.

## 3. Architecture

Four units, layered so everything worth testing is free of GUI and network.

### 3.1 `src/tools/updatecheck.{h,cpp}` — pure logic

No I/O, no FLTK. Fully unit-tested.

```cpp
namespace updatecheck {

struct Version {
  unsigned vMajor, vMinor, vPatch;   // not major/minor: glibc macros
  bool isDev;            // commits past the tag, or dirty
};

// "v0.7.1", "0.7.1", "v0.7.1-42-gabc123", "v0.7.1-dirty",
// "v0.7.1-42-gabc123-dirty" parse. "temp-64-bit", "0.7.0-unknown",
// a bare hash, "" do not. Components above 999 are rejected, which is
// what lets packVersion fit a version into the config's 32-bit int.
std::optional<Version> parseVersion(std::string_view s);

// Orders by (major, minor, patch) numerically; isDev is ignored.
int compareVersions(const Version &a, const Version &b);

// "0.7.1" -- no "v", no dev suffix.
std::string toString(const Version &v);

// major*1000000 + minor*1000 + patch; 0 means "none".
int packVersion(const Version &v);
std::optional<Version> unpackVersion(int packed);

struct Release {
  std::string tag;       // "v0.7.2"
  Version version;
  std::string name;      // "BurrTools 0.7.2"; may be empty
  std::string htmlUrl;   // https://github.com/burr-tools/burr-tools/releases/tag/v0.7.2
  std::string body;      // release notes; empty if null; CRLF folded to LF
};

// Parses a GitHub "get latest release" response. Fails on malformed JSON,
// missing/non-string tag_name or html_url, unparseable tag, draft or
// prerelease set, a dev-suffixed tag, or an html_url that is not under
// RELEASES_URL_PREFIX or contains whitespace, quotes, or control characters.
std::variant<Release, std::string /*error*/> parseLatestRelease(std::string_view json);

inline constexpr const char *LATEST_RELEASE_API =
    "https://api.github.com/repos/burr-tools/burr-tools/releases/latest";
inline constexpr const char *RELEASES_URL_PREFIX =
    "https://github.com/burr-tools/burr-tools/";
inline constexpr const char *RELEASES_PAGE =
    "https://github.com/burr-tools/burr-tools/releases";

enum class Mode { Auto, Manual };

struct Settings {
  bool autoCheckEnabled;
  std::int64_t lastCheck;               // epoch seconds; 0 = never
  std::optional<Version> skipped;       // none = nothing skipped
};

// Before the network: should a fetch happen at all?
enum class Gate { Fetch, Skip, UnknownVersion };
Gate gate(Mode, const std::optional<Version> &installed,
          const Settings &, std::int64_t now);

// After a successful fetch: what to show?
enum class Outcome { UpdateAvailable, UpToDate, SkippedByUser };
Outcome evaluate(Mode, const Version &installed, const Release &,
                 const Settings &);
}
```

Rules encoded in `gate`:

- **Auto:** returns `Skip` if auto-check is disabled, the installed version is
  missing or `isDev`, or `0 <= now - lastCheck < 86400`. A clock that moved
  backwards (`now < lastCheck`) is treated as "due", so a bad timestamp cannot
  suppress checks forever.
- **Manual:** returns `UnknownVersion` if the installed version is missing, and
  otherwise `Fetch`.

Rules encoded in `evaluate`:

- If the release is not newer than the installed version (base tag for dev
  builds), the result is `UpToDate`.
- In auto mode, if the release version equals `skipped`, the result is
  `SkippedByUser`.
- Otherwise the result is `UpdateAvailable`.

The single `decide()` sketched during brainstorming is split into `gate` and
`evaluate` because the decision happens at two points: before the request and
after it.

### 3.2 `src/gui/httpget.h` plus one backend — transport

```cpp
struct HttpResult {
  enum class Kind { Ok, Transport, Status, TooLarge, Unsupported } kind;
  long status;           // HTTP status when known
  std::string body;
  std::string error;     // human-readable detail for non-Ok
};
HttpResult httpGet(const std::string &url, const std::string &userAgent,
                   int timeoutSec);
```

Request headers are `User-Agent: BurrTools/<version>` (GitHub's API rejects
requests without one) and `Accept: application/vnd.github+json`. The timeout is
10 s and the body cap is 1 MiB. The function blocks and runs on the worker
thread only.

`meson.build` selects exactly one backend:

| Platform | File | Dependency |
| :--- | :--- | :--- |
| macOS | `httpget_mac.mm` | Foundation (already linked via Cocoa); add `objcpp` to the project languages |
| Windows | `httpget_win.cpp` | `winhttp` (system library, available to mingw) |
| Linux / other | `httpget_curl.cpp` | `dependency('libcurl', required: false)` |
| libcurl not found | `httpget_none.cpp` | none; returns `Unsupported` |

Linux CI jobs add `libcurl4-openssl-dev` to their `apt-get install` lines so
the curl backend is always compiled there.

### 3.3 `src/gui/updatechecker.{h,cpp}` — orchestration

The owner is `mainWindow_c`, which holds one `updateChecker_c`.

- **`start(Mode)`** reads `Settings` from `configuration_c`, runs `gate`, and
  then:
  - `Skip`: returns.
  - `UnknownVersion` (manual): shows an `fl_choice` explaining that the build's
    version is unknown, with an option to open `RELEASES_PAGE`.
  - `Fetch`: if a check is already in flight, returns (a manual click during an
    in-flight auto check just waits for it; the in-flight check is upgraded to
    manual so its result is reported). Otherwise it creates a
    `std::shared_ptr<Shared>` holding a `std::atomic<bool> done` and the
    result, starts a detached `std::thread` that fills it via `httpGet` and
    `parseLatestRelease`, and schedules the poll. In manual mode it sets the
    main window's cursor to `FL_CURSOR_WAIT` until the result arrives. The
    status line can't carry a progress message because
    `mainWindow_c::update()` rewrites it every second.
- **Poll** (`Fl::add_timeout(0.25)`): when `done` is set, it handles the result
  on the main thread:
  - A successful fetch and parse sets `updateLastCheck = now / 60` and saves
    the config, then runs `evaluate`:
    - `UpdateAvailable` opens `updateWindow_c`.
    - `UpToDate` shows "You're running the latest version (X.Y.Z)." in manual
      mode only.
    - `SkippedByUser` stays silent.
  - On failure, auto mode stays silent and leaves `updateLastCheck` unchanged
    (retry next launch). Manual mode shows `fl_alert` with a short reason:
    - "Couldn't reach GitHub: <detail>" (`Transport`)
    - "GitHub returned HTTP <n>." with "(rate limited — try again later)"
      appended for 403/429 (`Status`)
    - "Unexpected response from GitHub." (`TooLarge` or a parse error)
    - "Update checking isn't supported in this build." (`Unsupported`)
- **Shutdown:** the worker is detached and touches only the `shared_ptr`
  state, so quitting mid-request is safe. The worker outlives the window by at
  most the 10 s timeout, or is killed with the process.
- **Test hook:** the environment variable `BURRTOOLS_UPDATE_VERSION_OVERRIDE`,
  when set, replaces the installed version string (e.g. `0.7.0`) so the dialog
  can be exercised against the live API. It is read only here and documented in
  the header.
- **Command-line probe:** `burrtools --check-for-updates` runs the same fetch,
  parse and manual-mode evaluation synchronously, without creating a window,
  prints the outcome, and exits (0 on success, 1 on failure). It follows the
  existing `--self-check` precedent in `main.cpp` and makes it possible to
  verify the Windows and Linux backends without a GUI session.

### 3.4 `src/gui/updatewindow.{h,cpp}` — dialog

A modal `Fl_Double_Window` built with the existing `Layouter` widgets:

- Heading: "BurrTools 0.7.2 is available — you have 0.7.1." The release
  `name` is shown when present.
- A read-only `Fl_Text_Display` with word wrap containing `body` (or "No
  release notes provided.").
- Buttons:
  - **Open Release Page** (default/Return): calls `fl_open_uri(htmlUrl)` and
    closes the dialog.
  - **Remind Me Later** (Escape): closes the dialog.
  - **Skip This Version**: closes the dialog. Only present in auto mode.

The dialog only reports which button was pressed. The checker acts on it:
it opens the URI, or sets `updateSkippedVersion = packVersion(version)` and
saves the config.
- Resizable, with a sensible minimum size.

`htmlUrl` has already been validated against `RELEASES_URL_PREFIX` by the
parser. The notes are inert text.

### 3.5 Version plumbing

`meson.build` writes a generated `version.h` with `configure_file`:

```c
#define BURRTOOLS_VERSION "@0@"   // meson.project_version()
```

The GUI includes it for the checker, the User-Agent, and the About box, which
gains a "Version X" line.

### 3.6 Configuration entries

All three are registered in `configuration_c` with `register_entry`:

| Key | Type | Default | In Settings dialog |
| :--- | :--- | :--- | :--- |
| `checkForUpdates` | bool | `true` | yes — "Check for updates at startup" |
| `updateLastCheck` | int (minutes since the epoch) | `0` | no |
| `updateSkippedVersion` | int (`packVersion`, 0 = none) | `0` | no |

Both hidden entries are ints because `configuration_c::parse()` can only read
back bools and numbers. `luaClass_c` has no string getter and `src/lua/` must
not be modified. Minutes since the epoch fit a 32-bit int for about 4,000
years.

The configuration is otherwise only written by `~configuration_c()`. A public
`save()` is extracted from the destructor so these values persist as soon as
they change, and are not lost if the app later crashes.

### 3.7 Menus and startup

- Portable menu table: add "Check for Updates..." immediately before "About".
- macOS: add "Check for Updates…" to the application menu directly after
  "About BurrTools", in `installApplicationMenu()`.
- `main.cpp`: after `ui->show(...)`, call `ui->startUpdateCheck(Mode::Auto)`.
  The check is never started in the non-GUI paths.

## 4. Error handling summary

| Failure | Auto | Manual |
| :--- | :--- | :--- |
| Opted out / dev build / checked < 24 h ago | no request | — |
| Installed version unparseable | no request | dialog offering the releases page |
| DNS / TLS / timeout / offline | silent, retry next launch | alert with detail |
| HTTP non-200 (incl. 403/429 rate limit) | silent | alert with status |
| Body > 1 MiB, bad JSON, missing fields, draft/prerelease, foreign URL | silent | "Unexpected response" alert |
| Backend not compiled in | silent | "not supported" alert |
| Latest is skipped tag | silent | dialog shown (no Skip button) |
| Up to date | silent | "latest version" message |

No exception crosses the worker boundary. The worker catches everything and
stores it as an error result.

## 5. Testing

New file `test/test_updatecheck.cpp`, tag `[update]`, testing `updatecheck::`
only:

- **`parseVersion`:**
  - Parse: `v0.7.1`, `0.7.1`, `v0.7.1-42-gabc` (dev), `v0.7.1-dirty` (dev),
    `v0.7.1-42-gabc-dirty` (dev), `v1.10.0`.
  - Reject: `temp-64-bit`, `0.7.0-unknown`, `95009ba5a`, `""`, `v1.2`,
    `v1.2.3.4`, `v1.2.x`, and an oversized component.
- **`compareVersions`:** equal; patch/minor/major ordering; `1.10.0 > 1.9.0`;
  `isDev` ignored.
- **`parseLatestRelease`:** a recorded fixture of the real `v0.7.1` response
  (`test/data/github_latest_release_v0.7.1.json`); malformed JSON; missing
  `tag_name`; non-string `body`; `null` body (empty notes); `prerelease: true`;
  `draft: true`; `html_url` on a foreign host; tag `temp-64-bit`.
- **`gate` / `evaluate`:** a table-driven matrix over mode × enabled × dev ×
  unknown version × elapsed time (0, 86399, 86400, negative) × skipped tag
  (none, equal, older) × release (newer, equal, older).

Manual verification (macOS, real GUI, live GitHub):

1. With `BURRTOOLS_UPDATE_VERSION_OVERRIDE=0.7.0`, the launch dialog appears;
   each of the three buttons behaves as specified; Skip suppresses the next
   auto check, and the config file shows the new keys.
2. Without the override, the manual check reports up to date.
3. Offline, launch is silent and the manual check shows the transport alert.
4. With the Settings opt-out, there is no request at launch.

The Windows and Linux backends are verified by the CI `--werror` builds
compiling them. A Windows runtime smoke test is requested from a maintainer.

Gates before the PR: `just build`, `just test-all`, `just check`,
`just build-release` plus `just test-release`, and `just test-regression`.

## 6. Files

New:

- `src/tools/updatecheck.{h,cpp}`
- `src/gui/httpget.h` and `httpget_{mac.mm,win.cpp,curl.cpp,none.cpp}`
- `src/gui/updatechecker.{h,cpp}`
- `src/gui/updatewindow.{h,cpp}`
- `subprojects/nlohmann_json.wrap` (adding a wrap file, not editing a vendored
  subproject)
- `test/test_updatecheck.cpp`
- `test/data/github_latest_release_v0.7.1.json` (trimmed to the fields of
  interest with `jq`)

Changed:

- `meson.build`: version header, nlohmann dependency, backend selection,
  `objcpp` language on macOS
- `src/gui/configuration.{h,cpp}`
- `src/gui/mainmenu.cpp`
- `src/gui/mainwindow.{h,cpp}`: owner, menu callback, About version line
- `src/gui/main.cpp`
- `.github/workflows/build-and-release.yml`: `libcurl4-openssl-dev` on Linux
  jobs
