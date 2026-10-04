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
