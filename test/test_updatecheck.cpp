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
