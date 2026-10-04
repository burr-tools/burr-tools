# Update Check via GitHub Releases — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** At launch (rate-limited, opt-out) and from a menu item, ask GitHub for
the latest BurrTools release; if it is newer than the running build, show its
release notes and offer to open the release page in the browser.

**Architecture:** The work is split into four layers:

- A pure, unit-tested logic module (`src/tools/updatecheck.*`): version
  parsing, release-JSON parsing, and the gate/evaluate decision rules.
- A one-function HTTPS layer (`src/gui/httpget*`) with a native backend per
  platform.
- A GUI-thread orchestrator (`src/gui/updatechecker.*`) that runs the fetch on a
  detached thread and polls for the result with `Fl::add_timeout`.
- A dumb modal dialog (`src/gui/updatewindow.*`).

**Tech Stack:** C++20, FLTK 1.4, Meson, Catch2 v3, nlohmann/json (new Meson wrap),
NSURLSession (macOS, Objective-C++), WinHTTP (Windows), libcurl (Linux, optional).

**Spec:** `design/2026-10-04-update-check-design.md`. Read it before starting;
this plan implements it and does not repeat its rationale.

## Global Constraints

- The API URL is `https://api.github.com/repos/burr-tools/burr-tools/releases/latest`.
- Only open URLs that start with `https://github.com/burr-tools/burr-tools/`; the
  releases page fallback is `https://github.com/burr-tools/burr-tools/releases`.
- Request headers: `User-Agent: BurrTools/<version>` and
  `Accept: application/vnd.github+json`. The timeout is 10 s and the body cap is
  1 MiB (1048576 bytes).
- The launch check runs at most once per 86400 s, counted from the last
  *successful* fetch; a clock that went backwards counts as due.
- There is no launch check when the opt-out is set, the build is a dev build
  (commits past a tag, or dirty), or the version is unparseable.
- Version components are 0–999. The packed form is
  `major*1000000 + minor*1000 + patch`, and 0 means none.
- Config keys are `checkForUpdates` (bool, default `true`, visible),
  `updateLastCheck` (int, minutes since the epoch, default `0`, hidden), and
  `updateSkippedVersion` (int, packed, default `0`, hidden).
- **Never** edit `subprojects/*/` contents or `src/lua/`. Adding a `.wrap` file is
  allowed.
- Use `bt_te()` for checks that must fire in release builds, never `bt_assert`.
- No exception may cross the worker-thread boundary.
- Comments describe the present only (see the user's CLAUDE.md). Do not narrate
  changes in comments.
- Every task ends with `just build` and `just test` green. The final task runs
  the full gate list from `AGENTS.md`.

## Review Focus

1. **A GitHub response with `\r\n` line endings in `body`.** The notes must
   display without stray `\r` glyphs. Pinned in Task 2 (CRLF test).
2. **An `html_url` that passes the prefix check but carries a space, quote, or
   control character** (e.g. a `"` that would break a Windows `ShellExecute`
   command line). It must be rejected. Pinned in Task 2.
3. **A manual "Check for Updates" click while the launch check is still in
   flight.** The click must not spawn a second request, and the result must be
   reported as manual. This is GUI-only and verified by hand in Task 8, step 7.
4. **A skipped version, followed by an even newer release.** The newer release
   must still prompt. Pinned in Task 3 (`evaluate` with skipped older than the
   release).
5. **A config file from an older BurrTools, which lacks the new keys.** The
   defaults must apply: check on, never checked, nothing skipped. This is
   covered by `configuration_c::parse()` seeding defaults before reading the
   file; verified by hand in Task 6, step 4.

---

## File Structure

| File | Responsibility |
| :--- | :--- |
| `src/tools/updatecheck.h/.cpp` | Pure logic: `Version`, `parseVersion`, `compareVersions`, `toString`, `packVersion`/`unpackVersion`, `Release`, `parseLatestRelease`, `Settings`, `gate`, `evaluate`, URL constants. No FLTK, no I/O. |
| `test/test_updatecheck.cpp` | Catch2 tests for the above, tag `[update]`. |
| `test/data/github_latest_release_v0.7.1.json` | A recorded API response fixture. |
| `subprojects/nlohmann_json.wrap` | The Meson wrap for nlohmann/json. |
| `src/gui/httpget.h` | `HttpResult` and `httpGet()` declarations. |
| `src/gui/httpget_mac.mm` / `httpget_win.cpp` / `httpget_curl.cpp` / `httpget_none.cpp` | One backend each; Meson compiles exactly one. |
| `src/gui/updatewindow.h/.cpp` | The modal dialog: heading, notes, and three buttons. It returns the user's choice and has no side effects. |
| `src/gui/updatechecker.h/.cpp` | Orchestration: gate, worker thread, poll, result handling, config writes, and the CLI probe. |
| `meson.build` | `version.h`, json dep, backend selection, new sources, test sources. |
| `src/gui/configuration.h/.cpp` | Three entries, plus a public `save()`. |
| `src/gui/mainmenu.h/.cpp` | The menu item on both tables; the macOS app menu. |
| `src/gui/mainwindow.h/.cpp` | Owns `updateChecker_c`; menu callback; About version line. |
| `src/gui/main.cpp` | The launch call and the `--check-for-updates` CLI. |
| `.github/workflows/build-and-release.yml` | `libcurl4-openssl-dev` on the Linux jobs. |

---

### Task 1: Version parsing, comparison and packing

**Files:**
- Create: `src/tools/updatecheck.h`
- Create: `src/tools/updatecheck.cpp`
- Create: `test/test_updatecheck.cpp`
- Modify: `meson.build` (the `test_burrtools` source list)

**Interfaces:**
- Produces: namespace `updatecheck` with `struct Version { unsigned vMajor, vMinor, vPatch; bool isDev; }`,
  `std::optional<Version> parseVersion(std::string_view)`,
  `int compareVersions(const Version&, const Version&)` (returns -1/0/1),
  `std::string toString(const Version&)`, `int packVersion(const Version&)`,
  `std::optional<Version> unpackVersion(int)`, and
  `inline constexpr unsigned MAX_COMPONENT = 999`.

- [ ] **Step 1: Write the failing tests**

Create `test/test_updatecheck.cpp`:

```cpp
/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <catch2/catch_test_macros.hpp>
#include "../src/tools/updatecheck.h"

using namespace updatecheck;

static Version V(unsigned a, unsigned b, unsigned c, bool dev = false) {
  Version v;
  v.vMajor = a; v.vMinor = b; v.vPatch = c; v.isDev = dev;
  return v;
}

static bool is(const std::optional<Version> & got, const Version & want) {
  return got && got->vMajor == want.vMajor && got->vMinor == want.vMinor &&
         got->vPatch == want.vPatch && got->isDev == want.isDev;
}

TEST_CASE("parseVersion accepts release and git-describe forms", "[update]") {
  CHECK(is(parseVersion("v0.7.1"), V(0, 7, 1)));
  CHECK(is(parseVersion("0.7.1"), V(0, 7, 1)));
  CHECK(is(parseVersion("v1.10.0"), V(1, 10, 0)));
  CHECK(is(parseVersion("v999.999.999"), V(999, 999, 999)));
  CHECK(is(parseVersion("v0.7.1-42-g95009ba5a"), V(0, 7, 1, true)));
  CHECK(is(parseVersion("v0.7.1-dirty"), V(0, 7, 1, true)));
  CHECK(is(parseVersion("v0.7.1-42-g95009ba5a-dirty"), V(0, 7, 1, true)));
}

TEST_CASE("parseVersion rejects everything else", "[update]") {
  CHECK_FALSE(parseVersion(""));
  CHECK_FALSE(parseVersion("v"));
  CHECK_FALSE(parseVersion("temp-64-bit"));
  CHECK_FALSE(parseVersion("0.7.0-unknown"));
  CHECK_FALSE(parseVersion("95009ba5a"));
  CHECK_FALSE(parseVersion("1a2b3c"));
  CHECK_FALSE(parseVersion("v1.2"));
  CHECK_FALSE(parseVersion("v1.2."));
  CHECK_FALSE(parseVersion("v1.2.3.4"));
  CHECK_FALSE(parseVersion("v1.2.x"));
  CHECK_FALSE(parseVersion("v1000.0.0"));
  CHECK_FALSE(parseVersion("v1.2.3-"));
  CHECK_FALSE(parseVersion("v1.2.3-42"));
  CHECK_FALSE(parseVersion("v1.2.3-42-g"));
  CHECK_FALSE(parseVersion("v1.2.3-42-gxyz"));
  CHECK_FALSE(parseVersion("v1.2.3-dirty-dirty"));
  CHECK_FALSE(parseVersion("v1.2.3 "));
  CHECK_FALSE(parseVersion("v99999999999.0.0"));
}

TEST_CASE("compareVersions orders numerically and ignores isDev", "[update]") {
  CHECK(compareVersions(V(0, 7, 1), V(0, 7, 1)) == 0);
  CHECK(compareVersions(V(0, 7, 1), V(0, 7, 2)) < 0);
  CHECK(compareVersions(V(0, 8, 0), V(0, 7, 9)) > 0);
  CHECK(compareVersions(V(1, 0, 0), V(0, 999, 999)) > 0);
  CHECK(compareVersions(V(1, 10, 0), V(1, 9, 0)) > 0);
  CHECK(compareVersions(V(0, 7, 1, true), V(0, 7, 1)) == 0);
}

TEST_CASE("toString drops the prefix and dev suffix", "[update]") {
  CHECK(toString(V(0, 7, 1)) == "0.7.1");
  CHECK(toString(V(1, 10, 0, true)) == "1.10.0");
}

TEST_CASE("packVersion round-trips and 0 means none", "[update]") {
  CHECK(packVersion(V(0, 7, 1)) == 7001);
  CHECK(packVersion(V(999, 999, 999)) == 999999999);
  CHECK(is(unpackVersion(7001), V(0, 7, 1)));
  CHECK(is(unpackVersion(packVersion(V(12, 0, 345))), V(12, 0, 345)));
  CHECK_FALSE(unpackVersion(0));
  CHECK_FALSE(unpackVersion(-5));
  CHECK_FALSE(unpackVersion(1000000000));
}
```

Add both new files to the `test_burrtools` source list in `meson.build`. Append
them after `'src/gui/puzzlehistory.cpp'`:

```meson
    'test/test_puzzle_history.cpp', 'src/gui/puzzlehistory.cpp',
    'test/test_updatecheck.cpp', 'src/tools/updatecheck.cpp'],
```

Create an empty `src/tools/updatecheck.cpp` containing only the license header
and `#include "updatecheck.h"`, and a header that declares nothing yet, so the
build fails on the missing symbols rather than on missing files.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `just test`
Expected: a compile FAIL in `test_updatecheck.cpp`, because `parseVersion`,
`Version` and the rest are undeclared.

- [ ] **Step 3: Write the implementation**

`src/tools/updatecheck.h` (with the same license header as above):

```cpp
#ifndef __UPDATECHECK_H__
#define __UPDATECHECK_H__

#include <optional>
#include <string>
#include <string_view>

/* The decision logic of the update check: version strings, the GitHub
 * release response, and whether to check and what to tell the user. Pure
 * functions only -- no network, no GUI, no clock -- so all of it is unit
 * tested; src/gui/updatechecker.cpp supplies the I/O.
 */
namespace updatecheck {

  /* Field names avoid major/minor, which older glibc defines as macros. */
  struct Version {
    unsigned vMajor = 0;
    unsigned vMinor = 0;
    unsigned vPatch = 0;
    bool isDev = false;   ///< commits past the tag, or a dirty worktree
  };

  /* Components are capped so packVersion() fits a 32-bit int. */
  inline constexpr unsigned MAX_COMPONENT = 999;

  /* Accepts "X.Y.Z" with an optional leading "v", optionally followed by
   * git describe's "-N-gHASH" and/or "-dirty" (either makes it a dev
   * version). Anything else -- "temp-64-bit", "0.7.0-unknown", a bare
   * hash -- has no comparable version and yields nullopt.
   */
  std::optional<Version> parseVersion(std::string_view s);

  /* <0, 0, >0 by (major, minor, patch). isDev does not participate: a dev
   * build is compared as the release it was built from.
   */
  int compareVersions(const Version & a, const Version & b);

  /* "X.Y.Z" */
  std::string toString(const Version & v);

  /* A version as one int, for the configuration file, which can only store
   * bools and numbers. 0 is reserved for "none"; 0.0.0 is not a release.
   */
  int packVersion(const Version & v);
  std::optional<Version> unpackVersion(int packed);
}

#endif
```

`src/tools/updatecheck.cpp`:

```cpp
#include "updatecheck.h"

namespace updatecheck {

  namespace {

    bool isDigit(char c) { return c >= '0' && c <= '9'; }

    bool isHex(char c) {
      return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    /* Consumes one decimal component at pos. Fails on no digits or a value
     * above MAX_COMPONENT; the overflow check runs per digit, so a long
     * run of digits cannot wrap.
     */
    bool component(std::string_view s, size_t & pos, unsigned & out) {
      size_t start = pos;
      unsigned val = 0;
      while (pos < s.size() && isDigit(s[pos])) {
        val = val * 10 + unsigned(s[pos] - '0');
        if (val > MAX_COMPONENT) return false;
        pos++;
      }
      out = val;
      return pos > start;
    }

    /* The git describe suffix: "-N-gHASH", then optionally "-dirty"; or
     * just "-dirty".
     */
    bool devSuffix(std::string_view rest) {
      if (rest == "-dirty") return true;

      size_t pos = 0;
      if (pos >= rest.size() || rest[pos] != '-') return false;
      pos++;
      size_t start = pos;
      while (pos < rest.size() && isDigit(rest[pos])) pos++;
      if (pos == start) return false;

      if (rest.substr(pos, 2) != "-g") return false;
      pos += 2;
      start = pos;
      while (pos < rest.size() && isHex(rest[pos])) pos++;
      if (pos == start) return false;

      rest.remove_prefix(pos);
      return rest.empty() || rest == "-dirty";
    }
  }

  std::optional<Version> parseVersion(std::string_view s) {
    if (!s.empty() && s[0] == 'v') s.remove_prefix(1);

    Version v;
    size_t pos = 0;
    if (!component(s, pos, v.vMajor)) return std::nullopt;
    if (pos >= s.size() || s[pos] != '.') return std::nullopt;
    pos++;
    if (!component(s, pos, v.vMinor)) return std::nullopt;
    if (pos >= s.size() || s[pos] != '.') return std::nullopt;
    pos++;
    if (!component(s, pos, v.vPatch)) return std::nullopt;

    std::string_view rest = s.substr(pos);
    if (rest.empty()) return v;
    if (!devSuffix(rest)) return std::nullopt;
    v.isDev = true;
    return v;
  }

  int compareVersions(const Version & a, const Version & b) {
    if (a.vMajor != b.vMajor) return a.vMajor < b.vMajor ? -1 : 1;
    if (a.vMinor != b.vMinor) return a.vMinor < b.vMinor ? -1 : 1;
    if (a.vPatch != b.vPatch) return a.vPatch < b.vPatch ? -1 : 1;
    return 0;
  }

  std::string toString(const Version & v) {
    return std::to_string(v.vMajor) + "." + std::to_string(v.vMinor) + "." +
           std::to_string(v.vPatch);
  }

  int packVersion(const Version & v) {
    return int(v.vMajor * 1000000u + v.vMinor * 1000u + v.vPatch);
  }

  std::optional<Version> unpackVersion(int packed) {
    if (packed <= 0 || packed > 999999999) return std::nullopt;
    Version v;
    v.vMajor = unsigned(packed / 1000000);
    v.vMinor = unsigned(packed / 1000 % 1000);
    v.vPatch = unsigned(packed % 1000);
    return v;
  }
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `just test && ./build/test_burrtools "[update]"`
Expected: PASS, with all `[update]` cases green.

- [ ] **Step 5: Commit**

```bash
git add src/tools/updatecheck.h src/tools/updatecheck.cpp test/test_updatecheck.cpp meson.build
git commit -m "feat(update): version parsing, comparison and packing"
```

---

### Task 2: Parse the GitHub latest-release response

**Files:**
- Create: `subprojects/nlohmann_json.wrap` (via `meson wrap install`)
- Create: `test/data/github_latest_release_v0.7.1.json`
- Modify: `src/tools/updatecheck.h`, `src/tools/updatecheck.cpp`, `test/test_updatecheck.cpp`, `meson.build`

**Interfaces:**
- Consumes: `parseVersion` and `Version` from Task 1.
- Produces: `struct Release { std::string tag; Version version; std::string name; std::string htmlUrl; std::string body; }`,
  `std::variant<Release, std::string> parseLatestRelease(std::string_view json)`
  (the `std::string` alternative is the error), and the constants
  `LATEST_RELEASE_API`, `RELEASES_URL_PREFIX` and `RELEASES_PAGE`.
  It also produces the Meson variable `json_dep`, which later tasks don't need.

- [ ] **Step 1: Add the dependency and the fixture**

```bash
meson wrap install nlohmann_json
gh api repos/burr-tools/burr-tools/releases/latest \
  | jq '{url, html_url, tag_name, name, draft, prerelease, created_at, published_at, body,
         assets: [.assets[] | {name, size, browser_download_url}]}' \
  > test/data/github_latest_release_v0.7.1.json
```

Check that `jq -r .tag_name test/data/github_latest_release_v0.7.1.json`
prints `v0.7.1`.

If a release newer than 0.7.1 exists by the time this runs, fetch
`repos/burr-tools/burr-tools/releases/tags/v0.7.1` instead; that endpoint has
the same shape.

In `meson.build`, directly after the `if/else` block that defines `cocoa_dep`
(around line 205), add the code below. It has to come before the first build
target, `libburr_core`, because later tasks put `add_project_arguments` next
to it:

```meson
# Header-only; marked as a system include so its internals never trip
# -Werror in the release jobs.
json_dep = dependency('nlohmann_json', include_type: 'system')
```

Add `json_dep` to the `dependencies:` list of `test_burrtools`.

- [ ] **Step 2: Write the failing tests**

Append to `test/test_updatecheck.cpp`:

```cpp
#include <fstream>
#include <sstream>
#include <variant>

static std::string readFile(const char * path) {
  std::ifstream f(path, std::ios::binary);
  REQUIRE(f.good());
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static std::string minimal(const std::string & extra = "",
                           const std::string & tag = "\"v0.7.2\"",
                           const std::string & url =
                             "\"https://github.com/burr-tools/burr-tools/releases/tag/v0.7.2\"") {
  return "{\"tag_name\":" + tag + ",\"html_url\":" + url +
         ",\"name\":\"BurrTools 0.7.2\",\"draft\":false,\"prerelease\":false" +
         extra + "}";
}

static const Release & ok(const std::variant<Release, std::string> & r) {
  if (auto e = std::get_if<std::string>(&r)) FAIL("unexpected error: " << *e);
  return std::get<Release>(r);
}

static bool failed(const std::variant<Release, std::string> & r) {
  return std::holds_alternative<std::string>(r) && !std::get<std::string>(r).empty();
}

TEST_CASE("parseLatestRelease reads the recorded v0.7.1 response", "[update]") {
  auto r = parseLatestRelease(readFile("test/data/github_latest_release_v0.7.1.json"));
  const Release & rel = ok(r);
  CHECK(rel.tag == "v0.7.1");
  CHECK(is(rel.version, V(0, 7, 1)) );
  CHECK(rel.name == "BurrTools 0.7.1");
  CHECK(rel.htmlUrl == "https://github.com/burr-tools/burr-tools/releases/tag/v0.7.1");
  CHECK(rel.body.find("Bug Fixes") != std::string::npos);
  CHECK(rel.body.find('\r') == std::string::npos);
}

TEST_CASE("parseLatestRelease handles optional and odd fields", "[update]") {
  CHECK(ok(parseLatestRelease(minimal())).body.empty());
  CHECK(ok(parseLatestRelease(minimal(",\"body\":null"))).body.empty());
  CHECK(ok(parseLatestRelease(minimal(",\"body\":\"a\\r\\nb\\r\\n\""))).body == "a\nb\n");
  CHECK(ok(parseLatestRelease(
      "{\"tag_name\":\"v0.7.2\",\"html_url\":"
      "\"https://github.com/burr-tools/burr-tools/releases/tag/v0.7.2\"}")).name.empty());
}

TEST_CASE("parseLatestRelease rejects malformed or unwanted releases", "[update]") {
  CHECK(failed(parseLatestRelease("")));
  CHECK(failed(parseLatestRelease("not json")));
  CHECK(failed(parseLatestRelease("[]")));
  CHECK(failed(parseLatestRelease("{\"message\":\"Not Found\"}")));
  CHECK(failed(parseLatestRelease(minimal(",\"body\":42"))));
  CHECK(failed(parseLatestRelease(minimal(",\"draft\":true"))));
  CHECK(failed(parseLatestRelease(minimal(",\"prerelease\":true"))));
  CHECK(failed(parseLatestRelease(minimal("", "42"))));
  CHECK(failed(parseLatestRelease(minimal("", "\"temp-64-bit\""))));
  CHECK(failed(parseLatestRelease(minimal("", "\"v0.7.2-3-gabc\""))));
  CHECK(failed(parseLatestRelease(minimal("", "\"v0.7.2\"", "null"))));
  CHECK(failed(parseLatestRelease(minimal("", "\"v0.7.2\"",
      "\"https://evil.example/burr-tools/burr-tools/\""))));
  CHECK(failed(parseLatestRelease(minimal("", "\"v0.7.2\"",
      "\"http://github.com/burr-tools/burr-tools/releases/tag/v0.7.2\""))));
  CHECK(failed(parseLatestRelease(minimal("", "\"v0.7.2\"",
      "\"https://github.com/burr-tools/burr-tools-evil/x\""))));
  CHECK(failed(parseLatestRelease(minimal("", "\"v0.7.2\"",
      "\"https://github.com/burr-tools/burr-tools/a b\""))));
  CHECK(failed(parseLatestRelease(minimal("", "\"v0.7.2\"",
      "\"https://github.com/burr-tools/burr-tools/a\\\"b\""))));
  CHECK(failed(parseLatestRelease(minimal("", "\"v0.7.2\"",
      "\"https://github.com/burr-tools/burr-tools/a\\u0001b\""))));
}
```

Note that the duplicate `"draft"` and `"prerelease"` keys produced by
`minimal(",\"draft\":true")` are intentional. nlohmann keeps the *last*
occurrence, so these strings exercise the rejection path.

- [ ] **Step 3: Run the tests to verify they fail**

Run: `just test`
Expected: a compile FAIL, because `parseLatestRelease` and `Release` are
undeclared.

- [ ] **Step 4: Write the implementation**

Add to `src/tools/updatecheck.h` inside the namespace, after `unpackVersion`,
and add `#include <variant>` to the includes:

```cpp
  inline constexpr const char * LATEST_RELEASE_API =
      "https://api.github.com/repos/burr-tools/burr-tools/releases/latest";

  /* Every URL the update check opens in a browser starts with this. */
  inline constexpr const char * RELEASES_URL_PREFIX =
      "https://github.com/burr-tools/burr-tools/";

  inline constexpr const char * RELEASES_PAGE =
      "https://github.com/burr-tools/burr-tools/releases";

  struct Release {
    std::string tag;       ///< "v0.7.2"
    Version version;       ///< parsed from tag; never isDev
    std::string name;      ///< "BurrTools 0.7.2"; may be empty
    std::string htmlUrl;   ///< validated against RELEASES_URL_PREFIX
    std::string body;      ///< release notes, LF line endings; may be empty
  };

  /* Parses a GitHub "get the latest release" response. On failure the
   * string alternative holds a short, user-presentable reason. html_url
   * is checked here because it is handed to the OS to open: it must start
   * with RELEASES_URL_PREFIX and contain no whitespace, quotes or control
   * characters.
   */
  std::variant<Release, std::string> parseLatestRelease(std::string_view json);
```

Add to `src/tools/updatecheck.cpp`, with `#include <nlohmann/json.hpp>` at the
top:

```cpp
  namespace {

    bool flagSet(const nlohmann::json & j, const char * key) {
      auto it = j.find(key);
      return it != j.end() && it->is_boolean() && it->get<bool>();
    }

    const std::string * stringField(const nlohmann::json & j, const char * key) {
      auto it = j.find(key);
      if (it == j.end() || !it->is_string()) return nullptr;
      return it->get_ptr<const std::string *>();
    }

    bool safeUrl(const std::string & url) {
      if (url.rfind(RELEASES_URL_PREFIX, 0) != 0) return false;
      for (unsigned char c : url)
        if (c <= ' ' || c == 0x7f || c == '"' || c == '\'' || c == '<' ||
            c == '>' || c == '\\' || c == '`')
          return false;
      return true;
    }

    std::string foldCrlf(const std::string & s) {
      std::string out;
      out.reserve(s.size());
      for (size_t i = 0; i < s.size(); i++)
        if (!(s[i] == '\r' && i + 1 < s.size() && s[i + 1] == '\n'))
          out += s[i];
      return out;
    }
  }

  std::variant<Release, std::string> parseLatestRelease(std::string_view json) {
    nlohmann::json j = nlohmann::json::parse(json.begin(), json.end(), nullptr, false);
    if (j.is_discarded() || !j.is_object())
      return std::string("the response is not a JSON object");

    if (flagSet(j, "draft") || flagSet(j, "prerelease"))
      return std::string("the latest release is a draft or pre-release");

    const std::string * tag = stringField(j, "tag_name");
    if (!tag) return std::string("the response has no tag_name");

    std::optional<Version> v = parseVersion(*tag);
    if (!v || v->isDev)
      return std::string("the release tag \"" + *tag + "\" is not a version number");

    const std::string * url = stringField(j, "html_url");
    if (!url || !safeUrl(*url))
      return std::string("the release page address is missing or not on GitHub");

    Release r;
    r.tag = *tag;
    r.version = *v;
    r.htmlUrl = *url;
    if (const std::string * name = stringField(j, "name")) r.name = *name;

    auto body = j.find("body");
    if (body != j.end() && !body->is_null()) {
      if (!body->is_string()) return std::string("the release notes are not text");
      r.body = foldCrlf(body->get<std::string>());
    }
    return r;
  }
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `just test && ./build/test_burrtools "[update]"`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add subprojects/nlohmann_json.wrap test/data/github_latest_release_v0.7.1.json \
        src/tools/updatecheck.h src/tools/updatecheck.cpp test/test_updatecheck.cpp meson.build
git commit -m "feat(update): parse the GitHub latest-release response"
```

---

### Task 3: Gate and evaluate decision rules

**Files:**
- Modify: `src/tools/updatecheck.h`, `src/tools/updatecheck.cpp`, `test/test_updatecheck.cpp`

**Interfaces:**
- Consumes: `Version`, `compareVersions`, `Release` (Tasks 1–2).
- Produces: `enum class Mode { Auto, Manual }`;
  `struct Settings { bool autoCheckEnabled; std::int64_t lastCheck; std::optional<Version> skipped; }`;
  `enum class Gate { Fetch, Skip, UnknownVersion }`;
  `Gate gate(Mode, const std::optional<Version> & installed, const Settings &, std::int64_t now)`;
  `enum class Outcome { UpdateAvailable, UpToDate, SkippedByUser }`;
  `Outcome evaluate(Mode, const Version & installed, const Release &, const Settings &)`;
  `inline constexpr std::int64_t CHECK_INTERVAL_SECONDS = 86400`.

- [ ] **Step 1: Write the failing tests**

Append to `test/test_updatecheck.cpp`:

```cpp
static Settings S(bool enabled, std::int64_t last, std::optional<Version> skipped = std::nullopt) {
  Settings s;
  s.autoCheckEnabled = enabled;
  s.lastCheck = last;
  s.skipped = skipped;
  return s;
}

static Release R(unsigned a, unsigned b, unsigned c) {
  Release r;
  r.version = V(a, b, c);
  r.tag = "v" + toString(r.version);
  r.htmlUrl = std::string(RELEASES_URL_PREFIX) + "releases/tag/" + r.tag;
  return r;
}

TEST_CASE("gate: manual always fetches unless the version is unknown", "[update]") {
  const std::int64_t now = 1'800'000'000;
  CHECK(gate(Mode::Manual, V(0, 7, 1), S(false, now), now) == Gate::Fetch);
  CHECK(gate(Mode::Manual, V(0, 7, 1, true), S(true, now), now) == Gate::Fetch);
  CHECK(gate(Mode::Manual, std::nullopt, S(true, 0), now) == Gate::UnknownVersion);
}

TEST_CASE("gate: auto honours opt-out, dev builds and unknown versions", "[update]") {
  const std::int64_t now = 1'800'000'000;
  CHECK(gate(Mode::Auto, V(0, 7, 1), S(true, 0), now) == Gate::Fetch);
  CHECK(gate(Mode::Auto, V(0, 7, 1), S(false, 0), now) == Gate::Skip);
  CHECK(gate(Mode::Auto, V(0, 7, 1, true), S(true, 0), now) == Gate::Skip);
  CHECK(gate(Mode::Auto, std::nullopt, S(true, 0), now) == Gate::Skip);
}

TEST_CASE("gate: auto is rate limited to once per 24 hours", "[update]") {
  const std::int64_t now = 1'800'000'000;
  CHECK(gate(Mode::Auto, V(0, 7, 1), S(true, now), now) == Gate::Skip);
  CHECK(gate(Mode::Auto, V(0, 7, 1), S(true, now - 86399), now) == Gate::Skip);
  CHECK(gate(Mode::Auto, V(0, 7, 1), S(true, now - 86400), now) == Gate::Fetch);
  CHECK(gate(Mode::Auto, V(0, 7, 1), S(true, now - 10 * 86400), now) == Gate::Fetch);
  // clock moved backwards past the stored time: due, not stuck forever
  CHECK(gate(Mode::Auto, V(0, 7, 1), S(true, now + 3600), now) == Gate::Fetch);
}

TEST_CASE("evaluate: newer, equal and older releases", "[update]") {
  const Settings s = S(true, 0);
  for (Mode m : {Mode::Auto, Mode::Manual}) {
    CHECK(evaluate(m, V(0, 7, 1), R(0, 7, 2), s) == Outcome::UpdateAvailable);
    CHECK(evaluate(m, V(0, 7, 1), R(0, 7, 1), s) == Outcome::UpToDate);
    CHECK(evaluate(m, V(0, 7, 1), R(0, 7, 0), s) == Outcome::UpToDate);
    // dev builds compare as their base tag
    CHECK(evaluate(m, V(0, 7, 1, true), R(0, 7, 1), s) == Outcome::UpToDate);
    CHECK(evaluate(m, V(0, 7, 1, true), R(0, 7, 2), s) == Outcome::UpdateAvailable);
  }
}

TEST_CASE("evaluate: a skipped version silences only auto, and only that version", "[update]") {
  const Settings skip072 = S(true, 0, V(0, 7, 2));
  CHECK(evaluate(Mode::Auto,   V(0, 7, 1), R(0, 7, 2), skip072) == Outcome::SkippedByUser);
  CHECK(evaluate(Mode::Manual, V(0, 7, 1), R(0, 7, 2), skip072) == Outcome::UpdateAvailable);
  CHECK(evaluate(Mode::Auto,   V(0, 7, 1), R(0, 7, 3), skip072) == Outcome::UpdateAvailable);
  CHECK(evaluate(Mode::Auto,   V(0, 7, 2), R(0, 7, 2), skip072) == Outcome::UpToDate);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `just test`
Expected: a compile FAIL, because `Settings`, `gate`, `evaluate` and `Mode` are
undeclared.

- [ ] **Step 3: Write the implementation**

Add to `src/tools/updatecheck.h` (with `#include <cstdint>` among the includes),
after `parseLatestRelease`:

```cpp
  enum class Mode { Auto, Manual };

  inline constexpr std::int64_t CHECK_INTERVAL_SECONDS = 86400;

  struct Settings {
    bool autoCheckEnabled = true;
    std::int64_t lastCheck = 0;        ///< epoch seconds of the last successful fetch; 0 = never
    std::optional<Version> skipped;    ///< version the user chose to skip
  };

  enum class Gate {
    Fetch,           ///< ask GitHub
    Skip,            ///< do nothing, silently
    UnknownVersion,  ///< manual check of a build whose version cannot be compared
  };

  /* Decided before any request is made. Auto checks run only for release
   * builds with the setting on, at most once per CHECK_INTERVAL_SECONDS;
   * a stored time in the future (the clock moved back) counts as due.
   * Manual checks always fetch if there is a version to compare.
   */
  Gate gate(Mode mode, const std::optional<Version> & installed,
            const Settings & settings, std::int64_t now);

  enum class Outcome { UpdateAvailable, UpToDate, SkippedByUser };

  /* Decided after a successful fetch. A skipped version silences auto
   * checks for exactly that version; a manual check still offers it.
   */
  Outcome evaluate(Mode mode, const Version & installed, const Release & release,
                   const Settings & settings);
```

Add to `src/tools/updatecheck.cpp`:

```cpp
  Gate gate(Mode mode, const std::optional<Version> & installed,
            const Settings & settings, std::int64_t now) {
    if (mode == Mode::Manual)
      return installed ? Gate::Fetch : Gate::UnknownVersion;

    if (!settings.autoCheckEnabled || !installed || installed->isDev)
      return Gate::Skip;

    std::int64_t elapsed = now - settings.lastCheck;
    if (settings.lastCheck > 0 && elapsed >= 0 && elapsed < CHECK_INTERVAL_SECONDS)
      return Gate::Skip;

    return Gate::Fetch;
  }

  Outcome evaluate(Mode mode, const Version & installed, const Release & release,
                   const Settings & settings) {
    if (compareVersions(release.version, installed) <= 0)
      return Outcome::UpToDate;

    if (mode == Mode::Auto && settings.skipped &&
        compareVersions(*settings.skipped, release.version) == 0)
      return Outcome::SkippedByUser;

    return Outcome::UpdateAvailable;
  }
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `just test && ./build/test_burrtools "[update]"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/tools/updatecheck.h src/tools/updatecheck.cpp test/test_updatecheck.cpp
git commit -m "feat(update): gate and evaluate rules for launch and manual checks"
```

---

### Task 4: Expose the build version (`version.h`) and show it in About

**Files:**
- Modify: `meson.build`
- Modify: `src/gui/mainwindow.cpp` (`cb_About`, around line 2081)

**Interfaces:**
- Produces: a generated header `version.h`, included as `#include "version.h"`,
  that defines `BURRTOOLS_VERSION` as a string literal (e.g.
  `"v0.7.1-43-g7d148083d"`).

- [ ] **Step 1: Generate the header**

In `meson.build`, directly after `inc = include_directories('src')`, add:

```meson
# The version git describe produced at configure time, for the About box and
# the update check. Like the project version itself, it refreshes on
# reconfigure, not on every commit.
version_conf = configuration_data()
version_conf.set_quoted('BURRTOOLS_VERSION', meson.project_version())
version_h = configure_file(output: 'version.h', configuration: version_conf)
```

Meson puts the output in the build root, which every target defined in this
top-level `meson.build` already has on its include path.

- [ ] **Step 2: Show it in About**

In `src/gui/mainwindow.cpp`, add `#include "version.h"` with the other
includes. Change the first lines of the `fl_message` in `cb_About` to:

```cpp
  fl_message("This is the GUI for BurrTools\n"
             "Version %s\n"
             "BurrTools (c) 2003-2025 by Andreas Röver\n"
```

Then pass `BURRTOOLS_VERSION` as the argument after the format string, which
ends just before `);`:

```cpp
             "- tr by Brian Paul (http://www.mesa3d.org/brianp/TR.html)\n",
             BURRTOOLS_VERSION);
```

- [ ] **Step 3: Verify**

Run: `just build && cat build/version.h`
Expected: the build passes, and the header contains
`#define BURRTOOLS_VERSION "v0.7.1-…"`, matching `git describe --tags --always --dirty`.

Launch `./build/burrtools`, open About, and confirm the version line appears.

- [ ] **Step 4: Commit**

```bash
git add meson.build src/gui/mainwindow.cpp
git commit -m "feat(gui): expose the build version and show it in About"
```

---

### Task 5: `httpGet` with native backends, plus the `--check-for-updates` probe

**Files:**
- Create: `src/gui/httpget.h`, `src/gui/httpget_mac.mm`, `src/gui/httpget_win.cpp`, `src/gui/httpget_curl.cpp`, `src/gui/httpget_none.cpp`
- Create: `src/gui/updatechecker.h`, `src/gui/updatechecker.cpp` (this task adds only the CLI probe; Task 8 adds the rest)
- Modify: `meson.build`, `src/gui/main.cpp`, `.github/workflows/build-and-release.yml`, `justfile` (only if cppcheck chokes on `.mm`, see step 5)

**Interfaces:**
- Consumes: `updatecheck::*` (Tasks 1–3) and `BURRTOOLS_VERSION` (Task 4).
- Produces:
  ```cpp
  struct HttpResult {
    enum class Kind { Ok, Transport, Status, TooLarge, Unsupported };
    Kind kind = Kind::Transport;
    long status = 0;
    std::string body;
    std::string error;
  };
  inline constexpr size_t HTTP_MAX_BODY = 1048576;
  HttpResult httpGet(const std::string & url, const std::string & userAgent, int timeoutSec);
  ```
  It also produces `namespace updatechecker { std::string userAgent(); std::optional<updatecheck::Version> installedVersion(); std::string installedVersionString(); std::string describeFailure(const HttpResult &); int runCli(); }`
  in `updatechecker.h`.

The backends get no unit tests: they are thin OS wrappers, and testing them
would hit the network. They're verified through the CLI probe.

- [ ] **Step 1: The interface**

`src/gui/httpget.h` (with the license header):

```cpp
#ifndef __HTTPGET_H__
#define __HTTPGET_H__

#include <cstddef>
#include <string>

/* One blocking HTTPS GET, used by the update check from a worker thread.
 * Exactly one implementation is compiled in, chosen by meson.build per
 * platform: NSURLSession on macOS, WinHTTP on Windows, libcurl elsewhere
 * (or a stub reporting Unsupported when libcurl is not available).
 *
 * Sends the given User-Agent (GitHub's API rejects requests without one)
 * and "Accept: application/vnd.github+json", follows redirects, and gives
 * up after timeoutSec. Never throws.
 */
struct HttpResult {
  enum class Kind {
    Ok,           ///< HTTP 200; body holds the response
    Transport,    ///< DNS, TLS, timeout, offline...; error says which
    Status,       ///< a response other than 200; status holds it
    TooLarge,     ///< body exceeded HTTP_MAX_BODY
    Unsupported,  ///< this build has no HTTPS backend
  };
  Kind kind = Kind::Transport;
  long status = 0;
  std::string body;
  std::string error;
};

inline constexpr size_t HTTP_MAX_BODY = 1048576;

HttpResult httpGet(const std::string & url, const std::string & userAgent, int timeoutSec);

#endif
```

- [ ] **Step 2: The four backends**

`src/gui/httpget_curl.cpp`:

```cpp
#include "httpget.h"

#include <curl/curl.h>

#include <mutex>

namespace {

  struct Sink {
    std::string body;
    bool tooLarge = false;
  };

  size_t onData(char * data, size_t size, size_t count, void * user) {
    Sink * sink = static_cast<Sink *>(user);
    size_t len = size * count;
    if (sink->body.size() + len > HTTP_MAX_BODY) {
      sink->tooLarge = true;
      return 0;   // makes libcurl abort the transfer
    }
    sink->body.append(data, len);
    return len;
  }
}

HttpResult httpGet(const std::string & url, const std::string & userAgent, int timeoutSec) {

  /* curl_global_init is not thread safe; this is its only caller. */
  static std::once_flag once;
  std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });

  HttpResult r;
  CURL * c = curl_easy_init();
  if (!c) {
    r.error = "could not initialise libcurl";
    return r;
  }

  Sink sink;
  curl_slist * headers = curl_slist_append(nullptr, "Accept: application/vnd.github+json");

  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_USERAGENT, userAgent.c_str());
  curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, onData);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &sink);
  curl_easy_setopt(c, CURLOPT_TIMEOUT, long(timeoutSec));
  curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(c, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);   // worker thread: no SIGALRM timeouts

  CURLcode rc = curl_easy_perform(c);
  curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &r.status);

  curl_slist_free_all(headers);
  curl_easy_cleanup(c);

  if (sink.tooLarge) {
    r.kind = HttpResult::Kind::TooLarge;
    r.error = "response too large";
  } else if (rc != CURLE_OK) {
    r.kind = HttpResult::Kind::Transport;
    r.error = curl_easy_strerror(rc);
  } else if (r.status != 200) {
    r.kind = HttpResult::Kind::Status;
    r.error = "HTTP " + std::to_string(r.status);
  } else {
    r.kind = HttpResult::Kind::Ok;
    r.body = std::move(sink.body);
  }
  return r;
}
```

`src/gui/httpget_win.cpp`:

```cpp
#include "httpget.h"

#include <windows.h>
#include <winhttp.h>

#include <vector>

namespace {

  /* Closes a WinHTTP handle on scope exit. */
  struct Handle {
    HINTERNET h;
    explicit Handle(HINTERNET handle) : h(handle) {}
    ~Handle() { if (h) WinHttpCloseHandle(h); }
    Handle(const Handle &) = delete;
    Handle & operator=(const Handle &) = delete;
  };

  std::wstring widen(const std::string & s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
    return w;
  }

  HttpResult transportError(const char * what) {
    HttpResult r;
    r.kind = HttpResult::Kind::Transport;
    r.error = std::string(what) + " failed (WinHTTP error " + std::to_string(GetLastError()) + ")";
    return r;
  }
}

HttpResult httpGet(const std::string & url, const std::string & userAgent, int timeoutSec) {

  std::wstring wurl = widen(url);

  URL_COMPONENTS parts;
  ZeroMemory(&parts, sizeof(parts));
  parts.dwStructSize = sizeof(parts);
  parts.dwHostNameLength = DWORD(-1);
  parts.dwUrlPathLength = DWORD(-1);
  parts.dwExtraInfoLength = DWORD(-1);
  if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts))
    return transportError("parsing the URL");

  std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  if (parts.lpszExtraInfo) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

  Handle session(WinHttpOpen(widen(userAgent).c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  if (!session.h) return transportError("opening a session");

  int ms = timeoutSec * 1000;
  WinHttpSetTimeouts(session.h, ms, ms, ms, ms);

  Handle connection(WinHttpConnect(session.h, host.c_str(), parts.nPort, 0));
  if (!connection.h) return transportError("connecting");

  Handle request(WinHttpOpenRequest(connection.h, L"GET", path.c_str(), nullptr,
                                    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                    parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0));
  if (!request.h) return transportError("creating the request");

  if (!WinHttpSendRequest(request.h, L"Accept: application/vnd.github+json", DWORD(-1),
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
    return transportError("sending the request");

  if (!WinHttpReceiveResponse(request.h, nullptr))
    return transportError("receiving the response");

  HttpResult r;
  DWORD status = 0;
  DWORD statusSize = sizeof(status);
  WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
  r.status = long(status);

  std::vector<char> buf(16384);
  for (;;) {
    DWORD got = 0;
    if (!WinHttpReadData(request.h, buf.data(), DWORD(buf.size()), &got))
      return transportError("reading the response");
    if (got == 0) break;
    if (r.body.size() + got > HTTP_MAX_BODY) {
      r.kind = HttpResult::Kind::TooLarge;
      r.error = "response too large";
      r.body.clear();
      return r;
    }
    r.body.append(buf.data(), got);
  }

  if (r.status != 200) {
    r.kind = HttpResult::Kind::Status;
    r.error = "HTTP " + std::to_string(r.status);
    r.body.clear();
    return r;
  }
  r.kind = HttpResult::Kind::Ok;
  return r;
}
```

`src/gui/httpget_mac.mm`:

```objc
#include "httpget.h"

#import <Foundation/Foundation.h>

/* NSURLSession is asynchronous; the caller is already a worker thread, so
 * the completion handler fills the result and a semaphore turns it back
 * into a blocking call. The session's own timeout guarantees the handler
 * runs. Built without ARC, hence the explicit release of the semaphore.
 */
HttpResult httpGet(const std::string & url, const std::string & userAgent, int timeoutSec) {

  HttpResult r;

  @autoreleasepool {

    NSURL * nsurl = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
    if (!nsurl) {
      r.error = "invalid URL";
      return r;
    }

    NSMutableURLRequest * req =
        [NSMutableURLRequest requestWithURL:nsurl
                                cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                            timeoutInterval:timeoutSec];
    [req setValue:[NSString stringWithUTF8String:userAgent.c_str()] forHTTPHeaderField:@"User-Agent"];
    [req setValue:@"application/vnd.github+json" forHTTPHeaderField:@"Accept"];

    NSURLSessionConfiguration * cfg = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    cfg.timeoutIntervalForRequest = timeoutSec;
    cfg.timeoutIntervalForResource = timeoutSec;
    NSURLSession * session = [NSURLSession sessionWithConfiguration:cfg];

    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    HttpResult * out = &r;

    NSURLSessionDataTask * task =
        [session dataTaskWithRequest:req
                   completionHandler:^(NSData * data, NSURLResponse * response, NSError * err) {
      if (err) {
        const char * msg = err.localizedDescription.UTF8String;
        out->kind = HttpResult::Kind::Transport;
        out->error = msg ? msg : "unknown network error";
      } else {
        if ([response isKindOfClass:[NSHTTPURLResponse class]])
          out->status = long([(NSHTTPURLResponse *)response statusCode]);

        if (data.length > HTTP_MAX_BODY) {
          out->kind = HttpResult::Kind::TooLarge;
          out->error = "response too large";
        } else if (out->status != 200) {
          out->kind = HttpResult::Kind::Status;
          out->error = "HTTP " + std::to_string(out->status);
        } else {
          out->kind = HttpResult::Kind::Ok;
          out->body.assign(static_cast<const char *>(data.bytes), data.length);
        }
      }
      dispatch_semaphore_signal(done);
    }];

    [task resume];
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
    [session finishTasksAndInvalidate];
    dispatch_release(done);
  }

  return r;
}
```

`src/gui/httpget_none.cpp`:

```cpp
#include "httpget.h"

HttpResult httpGet(const std::string &, const std::string &, int) {
  HttpResult r;
  r.kind = HttpResult::Kind::Unsupported;
  r.error = "this build has no HTTPS support";
  return r;
}
```

Every file gets the standard license header.

- [ ] **Step 3: Meson selection**

Add this to `meson.build` directly after the `json_dep` line from Task 2. Like
that line, it must come before the first build target, because Meson rejects
`add_project_arguments` once any target has been declared:

```meson
# The update check's HTTPS backend; see src/gui/httpget.h.
if host_machine.system() == 'darwin'
  add_languages('objcpp', native: false)
  add_project_arguments('-std=c++20', language: 'objcpp')
  httpget_src = ['src/gui/httpget_mac.mm']
  httpget_dep = dependency('appleframeworks', modules: ['Foundation'])
elif host_machine.system() == 'windows'
  httpget_src = ['src/gui/httpget_win.cpp']
  httpget_dep = cxx.find_library('winhttp')
else
  httpget_dep = dependency('libcurl', required: false)
  if httpget_dep.found()
    httpget_src = ['src/gui/httpget_curl.cpp']
  else
    warning('libcurl not found: the update check will report itself unsupported')
    httpget_src = ['src/gui/httpget_none.cpp']
  endif
endif
```

Then change the `burrtools` executable to compile the new sources and link the
new dependencies:

```meson
executable('burrtools', gui_src + httpget_src + ['src/tools/updatecheck.cpp', 'src/gui/updatechecker.cpp', version_h],
          include_directories: inc,
          link_with: [libburr_core, vendored_gui_lib, vendored_lua_lib],
          dependencies: [manifold_dep, thread_dep, zlib_dep, gl_dep, glu_dep, gdiplus_dep, fltk_dep, fltkgl_dep, fltkimages_dep, fltkpng_dep, fltkz_dep, libpng_dep, cocoa_dep, json_dep, httpget_dep],
          win_subsystem: 'windows',
	  )
```

- [ ] **Step 4: The CLI probe (shared helpers live in `updatechecker`)**

`src/gui/updatechecker.h`:

```cpp
#ifndef __UPDATECHECKER_H__
#define __UPDATECHECKER_H__

#include "../tools/updatecheck.h"

#include <optional>
#include <string>

struct HttpResult;

namespace updatechecker {

  /* "BurrTools/<version>", sent with every request. */
  std::string userAgent(void);

  /* The running build's version string: BURRTOOLS_VERSION, unless the
   * environment variable BURRTOOLS_UPDATE_VERSION_OVERRIDE is set, in
   * which case that value is used instead. The override exists so the
   * update dialog can be exercised against the live API by pretending to
   * be an older release, e.g. BURRTOOLS_UPDATE_VERSION_OVERRIDE=0.7.0.
   */
  std::string installedVersionString(void);
  std::optional<updatecheck::Version> installedVersion(void);

  /* One-line, user-facing reason for a failed fetch. */
  std::string describeFailure(const HttpResult & r);

  /* `burrtools --check-for-updates`: fetch, evaluate as a manual check,
   * print the outcome, and return the process exit code (0 when the check
   * completed, 1 when it could not). No window is created.
   */
  int runCli(void);
}

#endif
```

`src/gui/updatechecker.cpp`:

```cpp
#include "updatechecker.h"
#include "httpget.h"

#include "version.h"

#include <cstdio>
#include <cstdlib>
#include <variant>

using namespace updatecheck;

static constexpr int TIMEOUT_SECONDS = 10;

namespace updatechecker {

  std::string userAgent(void) {
    return std::string("BurrTools/") + BURRTOOLS_VERSION;
  }

  std::string installedVersionString(void) {
    const char * o = std::getenv("BURRTOOLS_UPDATE_VERSION_OVERRIDE");
    return (o && *o) ? std::string(o) : std::string(BURRTOOLS_VERSION);
  }

  std::optional<Version> installedVersion(void) {
    return parseVersion(installedVersionString());
  }

  std::string describeFailure(const HttpResult & r) {
    switch (r.kind) {
      case HttpResult::Kind::Transport:
        return "Couldn't reach GitHub: " + r.error;
      case HttpResult::Kind::Status:
        if (r.status == 403 || r.status == 429)
          return "GitHub returned HTTP " + std::to_string(r.status) +
                 " (rate limited - try again later).";
        return "GitHub returned HTTP " + std::to_string(r.status) + ".";
      case HttpResult::Kind::TooLarge:
        return "Unexpected response from GitHub.";
      case HttpResult::Kind::Unsupported:
        return "Update checking isn't supported in this build.";
      case HttpResult::Kind::Ok:
        break;
    }
    return "";
  }

  int runCli(void) {
    std::string installedStr = installedVersionString();
    std::optional<Version> installed = installedVersion();
    printf("installed: %s\n", installedStr.c_str());

    HttpResult http = httpGet(LATEST_RELEASE_API, userAgent(), TIMEOUT_SECONDS);
    if (http.kind != HttpResult::Kind::Ok) {
      printf("error: %s\n", describeFailure(http).c_str());
      return 1;
    }

    auto parsed = parseLatestRelease(http.body);
    if (auto err = std::get_if<std::string>(&parsed)) {
      printf("error: unexpected response from GitHub: %s\n", err->c_str());
      return 1;
    }
    const Release & rel = std::get<Release>(parsed);
    printf("latest: %s (%s)\n", rel.tag.c_str(), rel.htmlUrl.c_str());

    if (!installed) {
      printf("result: installed version cannot be compared\n");
      return 0;
    }

    switch (evaluate(Mode::Manual, *installed, rel, Settings())) {
      case Outcome::UpdateAvailable: printf("result: update available\n"); break;
      case Outcome::UpToDate:        printf("result: up to date\n"); break;
      case Outcome::SkippedByUser:   printf("result: skipped\n"); break;
    }
    return 0;
  }
}
```

In `src/gui/main.cpp`, add `#include "updatechecker.h"`. Directly after the
`--self-check` block, add:

```cpp
  // Exercises the HTTPS backend and the update logic without a window, so
  // the per-platform network code can be checked from a terminal.
  if (argc == 2 && strcmp(argv[1], "--check-for-updates") == 0)
    return updatechecker::runCli();
```

On Windows the binary is `win_subsystem: 'windows'`, so stdout isn't attached
to a console. Run it as `burrtools.exe --check-for-updates > out.txt` and read
the file.

- [ ] **Step 5: CI and static analysis**

In `.github/workflows/build-and-release.yml`, append ` libcurl4-openssl-dev` to
each of the four Linux `sudo apt-get install -y libboost-all-dev …` lines. In
the current file these are the lines containing `zlib1g-dev`: the build job,
`clang-x86-64`, `tsan-parallel`, and coverage.

Run: `just check`

If cppcheck reports a syntax error in `httpget_mac.mm`, which happens because
it can't parse Objective-C, add `-i src/gui/httpget_mac.mm \` to the
`check-cppcheck` recipe in `justfile`, below `-i src/lua \`. Add a comment that
cppcheck cannot parse Objective-C++.

- [ ] **Step 6: Verify**

```bash
just build
./build/burrtools --check-for-updates
BURRTOOLS_UPDATE_VERSION_OVERRIDE=0.7.0 ./build/burrtools --check-for-updates
BURRTOOLS_UPDATE_VERSION_OVERRIDE=temp-64-bit ./build/burrtools --check-for-updates
```

Expected:
1. The first command prints `latest: v0.7.1 (https://github.com/burr-tools/burr-tools/releases/tag/v0.7.1)`
   and `result: up to date`, because this dev build has base tag 0.7.1.
2. The second prints `result: update available`.
3. The third prints `result: installed version cannot be compared`.

All three exit 0.

Then turn Wi-Fi off and run the first command again. Expected: `error: Couldn't reach GitHub: …` and exit 1.

Optional Linux check with Docker, if it's available: build in the repo's
`Docker/` image with `libcurl4-openssl-dev` installed and run the same
commands.

- [ ] **Step 7: Commit**

```bash
git add src/gui/httpget.h src/gui/httpget_*.cpp src/gui/httpget_mac.mm \
        src/gui/updatechecker.h src/gui/updatechecker.cpp src/gui/main.cpp \
        meson.build .github/workflows/build-and-release.yml justfile
git commit -m "feat(update): native HTTPS fetch per platform and --check-for-updates probe"
```

---

### Task 6: Configuration entries and an explicit save

**Files:**
- Modify: `src/gui/configuration.h`, `src/gui/configuration.cpp`

**Interfaces:**
- Produces, on `configuration_c`: `bool checkForUpdates(void) const`,
  `int updateLastCheckMinutes(void) const`, `void updateLastCheckMinutes(int)`,
  `int updateSkippedVersion(void) const`, `void updateSkippedVersion(int)`, and
  `void save(void)`.

- [ ] **Step 1: Header**

In `configuration.h`, after `showViewCube()`, add:

```cpp
  /* Update check. The last-check time is in minutes since the epoch and the
   * skipped version is updatecheck::packVersion() form (0 = none): the
   * configuration file can only carry bools and numbers back in, and both
   * values fit an int that way.
   */
  bool checkForUpdates(void) const { return i_check_for_updates; }
  int updateLastCheckMinutes(void) const { return i_update_last_check; }
  void updateLastCheckMinutes(int val) { i_update_last_check = val; }
  int updateSkippedVersion(void) const { return i_update_skipped_version; }
  void updateSkippedVersion(int val) { i_update_skipped_version = val; }

  /* Write the configuration file now. The destructor also writes it. */
  void save(void);
```

Add the members after `bool i_show_view_cube;`:

```cpp
  bool i_check_for_updates;
  int i_update_last_check;
  int i_update_skipped_version;
```

- [ ] **Step 2: Registration and save**

In the `configuration_c` constructor, after the `showviewcube` entry, add:

```cpp
  CNF_BOOL_D("checkForUpdates", &i_check_for_updates, "Check for updates at startup",
             "Once a day at most, ask GitHub whether a newer BurrTools release exists and "
             "offer its release notes. Check for Updates in the menu works either way.",
             "true");
```

After the `windowposh` entry, add:

```cpp
  CNF_INT("updateLastCheck",      &i_update_last_check, "0");
  CNF_INT("updateSkippedVersion", &i_update_skipped_version, "0");
```

Rename the destructor body to `void configuration_c::save(void)`, unchanged
apart from the signature. Then add:

```cpp
configuration_c::~configuration_c(void) {
  save();
}
```

- [ ] **Step 3: Build**

Run: `just build && just test`
Expected: PASS.

- [ ] **Step 4: Verify by hand**

1. Back up and remove your config file:
   `cp ~/.burrtools.rc /tmp/rc.bak 2>/dev/null; rm -f ~/.burrtools.rc`.
2. Launch `./build/burrtools`. The Settings dialog shows "Check for updates at
   startup", ticked.
3. Quit, then `grep -E "checkForUpdates|updateLastCheck|updateSkippedVersion" ~/.burrtools.rc`.
   Expected: the three keys appear with their defaults.
4. Restore the backup with `cp /tmp/rc.bak ~/.burrtools.rc`. This is an older
   config without the keys, which is the Review Focus 5 case. Relaunch and
   confirm the checkbox is still ticked.

   On macOS, `homedir()` may point elsewhere. Check
   `src/tools/homedir.cpp` for the real path before step 1.

- [ ] **Step 5: Commit**

```bash
git add src/gui/configuration.h src/gui/configuration.cpp
git commit -m "feat(config): update-check settings and an explicit save()"
```

---

### Task 7: The update dialog

**Files:**
- Create: `src/gui/updatewindow.h`, `src/gui/updatewindow.cpp`
- Modify: `meson.build` (add `src/gui/updatewindow.cpp` to the `burrtools` sources next to `updatechecker.cpp`)

**Interfaces:**
- Produces:
  ```cpp
  class updateWindow_c : public Fl_Double_Window {
  public:
    enum class Choice { OpenPage, Later, Skip };
    updateWindow_c(const std::string & heading, const std::string & notes, bool offerSkip);
    ~updateWindow_c();
    Choice run(void);   // shows modally, returns the button pressed
  };
  ```

- [ ] **Step 1: Header**

`src/gui/updatewindow.h` (with the license header):

```cpp
#ifndef __UPDATEWINDOW_H__
#define __UPDATEWINDOW_H__

#include <FL/Fl_Double_Window.H>

#include <string>

class Fl_Text_Buffer;
class Fl_Text_Display;

/* Announces a newer release: a heading, the release notes as plain text,
 * and the choice of what to do about it. It only reports the choice; the
 * caller opens the browser or records a skipped version.
 */
class updateWindow_c : public Fl_Double_Window {

  public:

    enum class Choice { OpenPage, Later, Skip };

    /* offerSkip adds "Skip This Version", which only makes sense for the
     * launch check -- a manual check is an explicit request to see it.
     */
    updateWindow_c(const std::string & heading, const std::string & notes, bool offerSkip);
    ~updateWindow_c();

    /* Shows the window modally and returns the button pressed; closing the
     * window or pressing Escape counts as Later.
     */
    Choice run(void);

  private:

    Fl_Text_Buffer * buffer;
    Fl_Text_Display * display;
    Choice choice;

    static void cb_open(Fl_Widget *, void * v);
    static void cb_later(Fl_Widget *, void * v);
    static void cb_skip(Fl_Widget *, void * v);
    void finish(Choice c);
};

#endif
```

- [ ] **Step 2: Implementation**

`src/gui/updatewindow.cpp` (with the license header):

```cpp
#include "updatewindow.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>

#define SZ_WIN_X 560
#define SZ_WIN_Y 420
#define SZ_GAP 10
#define SZ_HEADING 40
#define SZ_BUTTON_Y 25

updateWindow_c::updateWindow_c(const std::string & heading, const std::string & notes, bool offerSkip)
  : Fl_Double_Window(SZ_WIN_X, SZ_WIN_Y, "Software Update"), choice(Choice::Later) {

  Fl_Box * head = new Fl_Box(SZ_GAP, SZ_GAP, SZ_WIN_X - 2*SZ_GAP, SZ_HEADING);
  head->copy_label(heading.c_str());
  head->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_WRAP);
  head->labelfont(FL_HELVETICA_BOLD);

  int notesY = SZ_GAP + SZ_HEADING + SZ_GAP;
  int buttonsY = SZ_WIN_Y - SZ_GAP - SZ_BUTTON_Y;

  buffer = new Fl_Text_Buffer();
  buffer->text(notes.empty() ? "No release notes provided." : notes.c_str());

  display = new Fl_Text_Display(SZ_GAP, notesY, SZ_WIN_X - 2*SZ_GAP, buttonsY - SZ_GAP - notesY);
  display->buffer(buffer);
  display->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);

  int x = SZ_WIN_X - SZ_GAP;

  x -= 150;
  Fl_Return_Button * open = new Fl_Return_Button(x, buttonsY, 150, SZ_BUTTON_Y, "Open Release Page");
  open->callback(cb_open, this);

  x -= SZ_GAP + 130;
  Fl_Button * later = new Fl_Button(x, buttonsY, 130, SZ_BUTTON_Y, "Remind Me Later");
  later->callback(cb_later, this);

  if (offerSkip) {
    Fl_Button * skip = new Fl_Button(SZ_GAP, buttonsY, 130, SZ_BUTTON_Y, "Skip This Version");
    skip->callback(cb_skip, this);
  }

  end();

  /* Escape and the close box both arrive as the window callback. */
  callback(cb_later, this);
  resizable(display);
  size_range(400, 250);
  set_modal();
}

updateWindow_c::~updateWindow_c() {
  /* Fl_Text_Display does not own its buffer, and the display is destroyed
   * by Fl_Group's destructor after this body runs; detach it first so it
   * never touches a deleted buffer.
   */
  display->buffer(nullptr);
  delete buffer;
}

updateWindow_c::Choice updateWindow_c::run(void) {
  show();
  while (shown())
    Fl::wait();
  return choice;
}

void updateWindow_c::finish(Choice c) {
  choice = c;
  hide();
}

void updateWindow_c::cb_open(Fl_Widget *, void * v)  { static_cast<updateWindow_c *>(v)->finish(Choice::OpenPage); }
void updateWindow_c::cb_later(Fl_Widget *, void * v) { static_cast<updateWindow_c *>(v)->finish(Choice::Later); }
void updateWindow_c::cb_skip(Fl_Widget *, void * v)  { static_cast<updateWindow_c *>(v)->finish(Choice::Skip); }
```

Add `'src/gui/updatewindow.cpp'` to the `burrtools` executable's extra sources
from Task 5:

```meson
executable('burrtools', gui_src + httpget_src + ['src/tools/updatecheck.cpp', 'src/gui/updatechecker.cpp', 'src/gui/updatewindow.cpp', version_h],
```

- [ ] **Step 3: Build**

Run: `just build`
Expected: PASS, with no new warnings. Check for warnings with
`ninja -C build 2>&1 | grep -i warning | grep updatewindow`, which should print
nothing.

The window is exercised by hand in Task 8.

- [ ] **Step 4: Commit**

```bash
git add src/gui/updatewindow.h src/gui/updatewindow.cpp meson.build
git commit -m "feat(gui): update dialog with release notes and three choices"
```

---

### Task 8: Orchestration, menu items and launch wiring

**Files:**
- Modify: `src/gui/updatechecker.h`, `src/gui/updatechecker.cpp`, `src/gui/mainmenu.h`, `src/gui/mainmenu.cpp`, `src/gui/mainwindow.h`, `src/gui/mainwindow.cpp`, `src/gui/main.cpp`

**Interfaces:**
- Consumes: everything above.
- Produces: `class updateChecker_c { public: explicit updateChecker_c(Fl_Window * parent); ~updateChecker_c(); void start(updatecheck::Mode); };`,
  `void mainWindow_c::startUpdateCheck(bool manual)`, and
  `void cb_CheckForUpdates_stub(Fl_Widget*, void*)`.

- [ ] **Step 1: Declare the checker class**

Append to `src/gui/updatechecker.h`, before `#endif`, adding `#include <memory>`
to the includes and `class Fl_Window;` to the forward declarations:

```cpp
/* Runs update checks for the GUI. The request runs on a detached worker
 * thread; the main thread polls for its result with Fl::add_timeout, so no
 * FLTK call ever happens off the main thread and Fl::lock() is not needed.
 *
 * The worker shares only a reference-counted result block with this object,
 * so quitting mid-request is safe: the worker finishes (bounded by the
 * request timeout) into memory it co-owns, or dies with the process.
 */
class updateChecker_c {

  public:

    explicit updateChecker_c(Fl_Window * parent);
    ~updateChecker_c();

    /* Starts a check unless the rules in updatecheck::gate() say not to.
     * While a check is running, another start() never adds a request; a
     * Manual start upgrades the running check so its result is reported.
     */
    void start(updatecheck::Mode mode);

  private:

    struct Shared;

    Fl_Window * parent;
    std::shared_ptr<Shared> inFlight;
    updatecheck::Mode inFlightMode;

    static void pollCb(void * v);
    void poll(void);
    void finish(const Shared & result, updatecheck::Mode mode);
    void offer(const updatecheck::Release & rel, const updatecheck::Version & installed,
               updatecheck::Mode mode);
};
```

- [ ] **Step 2: Implement it**

Append to `src/gui/updatechecker.cpp`, adding these includes:

```cpp
#include "configuration.h"
#include "updatewindow.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include <atomic>
#include <ctime>
#include <thread>
```

Then add this code outside `namespace updatechecker`:

```cpp
struct updateChecker_c::Shared {
  std::atomic<bool> done{false};
  HttpResult http;
  std::variant<Release, std::string> parsed{std::string("not fetched")};
};

static Settings currentSettings(void) {
  Settings s;
  s.autoCheckEnabled = config.checkForUpdates();
  s.lastCheck = std::int64_t(config.updateLastCheckMinutes()) * 60;
  s.skipped = unpackVersion(config.updateSkippedVersion());
  return s;
}

updateChecker_c::updateChecker_c(Fl_Window * p) : parent(p), inFlightMode(Mode::Auto) {}

updateChecker_c::~updateChecker_c() {
  Fl::remove_timeout(pollCb, this);
}

void updateChecker_c::start(Mode mode) {

  if (inFlight) {
    if (mode == Mode::Manual) {
      inFlightMode = Mode::Manual;
      parent->cursor(FL_CURSOR_WAIT);
    }
    return;
  }

  std::optional<Version> installed = updatechecker::installedVersion();

  switch (gate(mode, installed, currentSettings(), std::int64_t(time(nullptr)))) {

    case Gate::Skip:
      return;

    case Gate::UnknownVersion: {
      std::string v = updatechecker::installedVersionString();
      if (fl_choice("This build's version (%s) can't be compared with published releases.\n"
                    "Open the releases page to look for yourself?",
                    "Cancel", "Open Releases Page", nullptr, v.c_str()) == 1) {
        char msg[512];
        if (!fl_open_uri(RELEASES_PAGE, msg, sizeof(msg)))
          fl_alert("Couldn't open the browser: %s", msg);
      }
      return;
    }

    case Gate::Fetch:
      break;
  }

  auto shared = std::make_shared<Shared>();
  inFlight = shared;
  inFlightMode = mode;
  if (mode == Mode::Manual)
    parent->cursor(FL_CURSOR_WAIT);

  std::string ua = updatechecker::userAgent();
  std::thread([shared, ua] {
    try {
      shared->http = httpGet(LATEST_RELEASE_API, ua, TIMEOUT_SECONDS);
      if (shared->http.kind == HttpResult::Kind::Ok)
        shared->parsed = parseLatestRelease(shared->http.body);
    } catch (const std::exception & e) {
      shared->http.kind = HttpResult::Kind::Transport;
      shared->http.error = e.what();
    } catch (...) {
      shared->http.kind = HttpResult::Kind::Transport;
      shared->http.error = "unexpected error";
    }
    shared->done.store(true, std::memory_order_release);
  }).detach();

  Fl::add_timeout(0.25, pollCb, this);
}

void updateChecker_c::pollCb(void * v) {
  static_cast<updateChecker_c *>(v)->poll();
}

void updateChecker_c::poll(void) {
  if (!inFlight->done.load(std::memory_order_acquire)) {
    Fl::repeat_timeout(0.25, pollCb, this);
    return;
  }

  /* Release the in-flight slot before showing any dialog: the dialogs run
   * a nested event loop, during which a new check may be started.
   */
  std::shared_ptr<Shared> result;
  result.swap(inFlight);
  Mode mode = inFlightMode;
  if (mode == Mode::Manual)
    parent->cursor(FL_CURSOR_DEFAULT);

  finish(*result, mode);
}

void updateChecker_c::finish(const Shared & result, Mode mode) {

  const bool manual = mode == Mode::Manual;

  if (result.http.kind != HttpResult::Kind::Ok) {
    if (manual)
      fl_alert("%s", updatechecker::describeFailure(result.http).c_str());
    return;
  }

  if (auto err = std::get_if<std::string>(&result.parsed)) {
    if (manual)
      fl_alert("Unexpected response from GitHub:\n%s", err->c_str());
    return;
  }

  config.updateLastCheckMinutes(int(time(nullptr) / 60));
  config.save();

  const Release & rel = std::get<Release>(result.parsed);
  std::optional<Version> installed = updatechecker::installedVersion();
  if (!installed) return;   // gate() already refused unknown versions

  switch (evaluate(mode, *installed, rel, currentSettings())) {
    case Outcome::UpToDate:
      if (manual)
        fl_message("You're running the latest version of BurrTools (%s).",
                   toString(*installed).c_str());
      break;
    case Outcome::SkippedByUser:
      break;
    case Outcome::UpdateAvailable:
      offer(rel, *installed, mode);
      break;
  }
}

void updateChecker_c::offer(const Release & rel, const Version & installed, Mode mode) {

  std::string title = rel.name.empty() ? "BurrTools " + toString(rel.version) : rel.name;
  std::string heading = title + " is available. You have " + toString(installed) + ".";

  updateWindow_c win(heading, rel.body, mode == Mode::Auto);

  switch (win.run()) {
    case updateWindow_c::Choice::OpenPage: {
      char msg[512];
      if (!fl_open_uri(rel.htmlUrl.c_str(), msg, sizeof(msg)))
        fl_alert("Couldn't open the browser: %s", msg);
      break;
    }
    case updateWindow_c::Choice::Skip:
      config.updateSkippedVersion(packVersion(rel.version));
      config.save();
      break;
    case updateWindow_c::Choice::Later:
      break;
  }
}
```

`fl_open_uri` is declared in `<FL/filename.H>`; add that include too.
`TIMEOUT_SECONDS` is the file-level constant from Task 5.

- [ ] **Step 3: Menu items**

In `src/gui/mainmenu.h`, add next to the other declarations:

```cpp
void cb_CheckForUpdates_stub(Fl_Widget*, void*);
```

In `src/gui/mainmenu.cpp`, `menu_Portable`, insert this before the `About` line:

```cpp
    {"Check for Updates", 0, cb_CheckForUpdates_stub, 0, 0, 0, 0, 14, 56},
```

In `installApplicationMenu()`, replace the `appItems` array and its
`user_data` line with:

```cpp
  static Fl_Menu_Item appItems[] = {
    { "Check for Updates...", 0, cb_CheckForUpdates_stub, 0, FL_MENU_DIVIDER, 0, 0, 14, 56 },
    { "Settings...", FL_COMMAND + ',', cb_Config_stub, 0, 0, 0, 0, 14, 56 },
    { }
  };
  appItems[0].user_data(win);
  appItems[1].user_data(win);
```

Update the comment above it to say both Check for Updates and Settings belong
in the application menu on macOS.

In `assertTablesConsistent()`, extend `appMenuOnly`:

```cpp
  static Fl_Callback * const appMenuOnly[] = {
    cb_About_stub, cb_Config_stub, cb_CheckForUpdates_stub
  };
```

Update its explanatory comment ("The macOS table intentionally omits About,
Settings and Quit…") to include Check for Updates. Update the header comment on
`installApplicationMenu` in `mainmenu.h` from "(About, Settings)" to
"(About, Check for Updates, Settings)".

- [ ] **Step 4: Main window ownership**

In `src/gui/mainwindow.h`:
- Add the forward declaration `class updateChecker_c;`.
- Add the private member `updateChecker_c * updateChecker;`.
- Add the public method `void startUpdateCheck(bool manual);` next to `update(void)`.
- Add `void cb_CheckForUpdates(void);` next to `cb_About` at line 379. It is in
  the `public:` section, like every other `cb_*` method.

In `src/gui/mainwindow.cpp`, add `#include "updatechecker.h"`. In the
constructor body, right after `MainMenu->copy(mainmenu::table(), this);` (around
line 4447), add:

```cpp
  updateChecker = new updateChecker_c(this);
```

In the existing destructor `mainWindow_c::~mainWindow_c()` (around line 4504),
add `delete updateChecker;` as its first statement.

Next to `cb_About`, add:

```cpp
void cb_CheckForUpdates_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_CheckForUpdates(); }
void mainWindow_c::cb_CheckForUpdates(void) {
  startUpdateCheck(true);
}

void mainWindow_c::startUpdateCheck(bool manual) {
  updateChecker->start(manual ? updatecheck::Mode::Manual : updatecheck::Mode::Auto);
}
```

- [ ] **Step 5: Launch check**

In `src/gui/main.cpp`, directly after `ui->show(argc, argv);`, add:

```cpp
    ui->startUpdateCheck(false);
```

- [ ] **Step 6: Build and self-check**

Run: `just build && just test && ./build/burrtools --self-check`
Expected: PASS, and `self-check OK`, confirming the menu tables are still
consistent.

- [ ] **Step 7: Manual GUI verification on macOS**

Before starting, back up `~/.burrtools.rc` (or the path from Task 6).

1. Set the stored last check to 0 by editing the config (`updateLastCheck = 0`).
   Run `BURRTOOLS_UPDATE_VERSION_OVERRIDE=0.7.0 ./build/burrtools`. Within about
   a second of launch, the dialog appears: "BurrTools 0.7.1 is available. You
   have 0.7.0." with the notes and all three buttons.
2. Click **Remind Me Later**. The dialog closes, and `updateLastCheck` in the
   config is now about `date +%s / 60`.
3. Relaunch with the same override. No dialog appears, because of the 24h rate
   limit.
4. Use the menu: BurrTools > Check for Updates…. The dialog appears **without**
   Skip. Click **Open Release Page**, and the browser opens the v0.7.1 release
   page.
5. Reset `updateLastCheck = 0` and relaunch with the override. Click **Skip
   This Version**, and the config shows `updateSkippedVersion = 7001`. Reset
   `updateLastCheck = 0` again and relaunch. No dialog appears. The manual
   check still shows it.
6. Without the override, use Check for Updates…. It reports "You're running the
   latest version of BurrTools (0.7.1)." The launch check is skipped silently
   because this is a dev build.
7. Review Focus 3: reset `updateLastCheck = 0`, use the override, turn on
   Network Link Conditioner or a slow proxy if you have one, otherwise go
   offline. Launch, then immediately choose Check for Updates… twice. Only one
   dialog or alert appears, and the cursor returns to normal.
8. With Wi-Fi off, Check for Updates… shows "Couldn't reach GitHub: …".
   Launching while offline with `updateLastCheck = 0` shows nothing, and
   `updateLastCheck` stays 0.
9. Untick the setting in Settings, reset `updateLastCheck = 0`, and relaunch
   with the override. No dialog appears.
10. Restore the backed-up config.

Record which steps passed in the PR description.

- [ ] **Step 8: Commit**

```bash
git add src/gui/updatechecker.h src/gui/updatechecker.cpp src/gui/mainmenu.h src/gui/mainmenu.cpp \
        src/gui/mainwindow.h src/gui/mainwindow.cpp src/gui/main.cpp
git commit -m "feat(gui): check for updates at launch and from the menu"
```

---

### Task 9: Full verification gates

**Files:** none new. Fix anything the gates surface in the file it comes from.

- [ ] **Step 1: Run the gates from `AGENTS.md`**

```bash
just build
just test-all
just check
just build-release
just test-release
just test-regression
```

Expected: all green. `just build-release` uses `--werror`. Any new warning in
the `httpget_*`, `updatechecker` or `updatewindow` files must be fixed, not
suppressed.

- [ ] **Step 2: Clang x86-64 / cross-build sanity check (if the tools are installed)**

If `x86_64-w64-mingw32-g++` is installed (`which x86_64-w64-mingw32-g++`),
run:

```bash
meson setup build-win --cross-file cross-mingw64.txt --buildtype=release --werror -Db_ndebug=true -Dpython=disabled && ninja -C build-win
```

This compiles `httpget_win.cpp`. If the toolchain isn't installed, say so in
the PR and let CI's Windows job be the check.

- [ ] **Step 3: Commit any fixes**

```bash
git add -A
git commit -m "fix(update): address release-build and static-analysis findings"
```

Skip this step if there's nothing to fix.
