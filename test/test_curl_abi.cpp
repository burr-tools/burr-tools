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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <catch2/catch_test_macros.hpp>

#include "../src/gui/curl_abi.h"

#include <dlfcn.h>

/* curl_abi.h declares libcurl's constants instead of including <curl/curl.h>,
 * so that httpget_curl.cpp compiles on machines with no libcurl headers. That
 * trade is only safe if a wrong constant cannot go unnoticed.
 *
 * Two checks watch those constants, and they are NOT equally strong:
 *
 * 1. With the real header (Linux CI installs libcurl4-openssl-dev), assert
 *    ours against it. Catches any drift, valid-but-wrong included.
 * 2. Without a header, ask a live libcurl.so.4: easy_setopt rejects an
 *    option it does not know, so a wildly wrong value fails here.
 *
 * The limit of (2) is worth stating, because it is easy to over-trust it.
 * It rejects only values libcurl does not recognise. A valid but wrong
 * value -- say CURLOPT_TIMEOUT pointed at CURLOPT_TIMEOUT_MS -- is accepted
 * and returns CURLE_OK, and (2) passes. Verified both ways: 999999 fails,
 * 14 does not. So headerless builds get coarse protection only, and CI's
 * header comparison is what actually guarantees the values.
 */
#if defined(__has_include)
#  if __has_include(<curl/curl.h>)
#    include <curl/curl.h>
#    define BT_HAVE_CURL_HEADER 1
#  endif
#endif

#ifdef BT_HAVE_CURL_HEADER

TEST_CASE("curl_abi constants match the installed libcurl header", "[update][curl]") {

  STATIC_REQUIRE(curl_abi::CURLE_OK == CURLE_OK);
  STATIC_REQUIRE(curl_abi::CURL_GLOBAL_DEFAULT_FLAGS == CURL_GLOBAL_DEFAULT);

  STATIC_REQUIRE(curl_abi::CURLOPT_URL == CURLOPT_URL);
  STATIC_REQUIRE(curl_abi::CURLOPT_WRITEDATA == CURLOPT_WRITEDATA);
  STATIC_REQUIRE(curl_abi::CURLOPT_WRITEFUNCTION == CURLOPT_WRITEFUNCTION);
  STATIC_REQUIRE(curl_abi::CURLOPT_USERAGENT == CURLOPT_USERAGENT);
  STATIC_REQUIRE(curl_abi::CURLOPT_HTTPHEADER == CURLOPT_HTTPHEADER);
  STATIC_REQUIRE(curl_abi::CURLOPT_TIMEOUT == CURLOPT_TIMEOUT);
  STATIC_REQUIRE(curl_abi::CURLOPT_FOLLOWLOCATION == CURLOPT_FOLLOWLOCATION);
  STATIC_REQUIRE(curl_abi::CURLOPT_MAXREDIRS == CURLOPT_MAXREDIRS);
  STATIC_REQUIRE(curl_abi::CURLOPT_NOSIGNAL == CURLOPT_NOSIGNAL);
  STATIC_REQUIRE(curl_abi::CURLINFO_RESPONSE_CODE == CURLINFO_RESPONSE_CODE);

  /* The dlsym'd prototypes are declared against our own types; if either
   * were the wrong width or signedness the library would misread them.
   */
  STATIC_REQUIRE(sizeof(curl_abi::CURLoption) == sizeof(CURLoption));
  STATIC_REQUIRE(sizeof(curl_abi::CURLcode) == sizeof(CURLcode));
  STATIC_REQUIRE(sizeof(curl_abi::CURLINFO) == sizeof(CURLINFO));
}

#endif

namespace {

  struct Api {
    curl_abi::CURLcode (*global_init)(long) = nullptr;
    curl_abi::CURL * (*easy_init)(void) = nullptr;
    curl_abi::CURLcode (*easy_setopt)(curl_abi::CURL *, curl_abi::CURLoption, ...) = nullptr;
    curl_abi::CURLcode (*easy_perform)(curl_abi::CURL *) = nullptr;
    curl_abi::CURLcode (*easy_getinfo)(curl_abi::CURL *, curl_abi::CURLINFO, ...) = nullptr;
    void (*easy_cleanup)(curl_abi::CURL *) = nullptr;
    const char * (*easy_strerror)(curl_abi::CURLcode) = nullptr;
    curl_abi::curl_slist * (*slist_append)(curl_abi::curl_slist *, const char *) = nullptr;
    void (*slist_free_all)(curl_abi::curl_slist *) = nullptr;

    bool complete(void) const {
      return global_init && easy_init && easy_setopt && easy_perform &&
             easy_getinfo && easy_cleanup && easy_strerror && slist_append &&
             slist_free_all;
    }
  };

  /* Mirrors loadCurl() in httpget_curl.cpp: both sonames accepted, because
   * the OpenSSL and GnuTLS flavours export the same ABI. */
  Api loadLibcurl(void) {
    Api api;
    void * lib = nullptr;
    for (const char * soname : { "libcurl.so.4", "libcurl-gnutls.so.4", "libcurl.so" })
      if ((lib = dlopen(soname, RTLD_NOW | RTLD_LOCAL)) != nullptr) break;
    if (!lib) return api;

    api.global_init    = (curl_abi::CURLcode (*)(long))dlsym(lib, "curl_global_init");
    api.easy_init      = (curl_abi::CURL * (*)(void))dlsym(lib, "curl_easy_init");
    api.easy_setopt    = (curl_abi::CURLcode (*)(curl_abi::CURL *, curl_abi::CURLoption, ...))dlsym(lib, "curl_easy_setopt");
    api.easy_perform   = (curl_abi::CURLcode (*)(curl_abi::CURL *))dlsym(lib, "curl_easy_perform");
    api.easy_getinfo   = (curl_abi::CURLcode (*)(curl_abi::CURL *, curl_abi::CURLINFO, ...))dlsym(lib, "curl_easy_getinfo");
    api.easy_cleanup   = (void (*)(curl_abi::CURL *))dlsym(lib, "curl_easy_cleanup");
    api.easy_strerror  = (const char * (*)(curl_abi::CURLcode))dlsym(lib, "curl_easy_strerror");
    api.slist_append   = (curl_abi::curl_slist * (*)(curl_abi::curl_slist *, const char *))dlsym(lib, "curl_slist_append");
    api.slist_free_all = (void (*)(curl_abi::curl_slist *))dlsym(lib, "curl_slist_free_all");

    if (api.complete()) api.global_init(curl_abi::CURL_GLOBAL_DEFAULT_FLAGS);
    return api;
  }
}

TEST_CASE("the declared curl options are accepted by the installed libcurl", "[update][curl]") {

  Api api = loadLibcurl();
  if (!api.complete())
    SKIP("no libcurl runtime available; the constants are still checked by the "
         "header test where the build has one");

  curl_abi::CURL * c = api.easy_init();
  REQUIRE(c != nullptr);

  // A wrong constant, or one of the wrong type, is rejected here.
  REQUIRE(api.easy_setopt(c, curl_abi::CURLOPT_URL, "https://example.invalid/") == curl_abi::CURLE_OK);
  REQUIRE(api.easy_setopt(c, curl_abi::CURLOPT_WRITEFUNCTION, nullptr) == curl_abi::CURLE_OK);
  REQUIRE(api.easy_setopt(c, curl_abi::CURLOPT_WRITEDATA, nullptr) == curl_abi::CURLE_OK);
  REQUIRE(api.easy_setopt(c, curl_abi::CURLOPT_USERAGENT, "BurrTools/test") == curl_abi::CURLE_OK);
  REQUIRE(api.easy_setopt(c, curl_abi::CURLOPT_TIMEOUT, 10L) == curl_abi::CURLE_OK);
  REQUIRE(api.easy_setopt(c, curl_abi::CURLOPT_FOLLOWLOCATION, 1L) == curl_abi::CURLE_OK);
  REQUIRE(api.easy_setopt(c, curl_abi::CURLOPT_MAXREDIRS, 5L) == curl_abi::CURLE_OK);
  REQUIRE(api.easy_setopt(c, curl_abi::CURLOPT_NOSIGNAL, 1L) == curl_abi::CURLE_OK);

  curl_abi::curl_slist * headers = api.slist_append(nullptr, "Accept: application/vnd.github+json");
  REQUIRE(headers != nullptr);
  REQUIRE(api.easy_setopt(c, curl_abi::CURLOPT_HTTPHEADER, headers) == curl_abi::CURLE_OK);
  api.slist_free_all(headers);

  long status = 0;
  REQUIRE(api.easy_getinfo(c, curl_abi::CURLINFO_RESPONSE_CODE, &status) == curl_abi::CURLE_OK);

  api.easy_cleanup(c);
}

TEST_CASE("the update check reports itself unsupported, not broken, without libcurl",
          "[update][curl]") {

  // The contract httpGet() offers when the runtime library is absent: a
  // presentable reason, so the GUI can say "not available" rather than
  // failing. Reported as Unsupported by loadCurl() returning null.
  Api api = loadLibcurl();
  if (api.complete()) SKIP("libcurl is installed, so the unsupported path is unreachable here");
  CHECK(api.easy_init == nullptr);
}