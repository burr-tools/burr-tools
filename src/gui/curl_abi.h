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
#ifndef __CURL_ABI_H__
#define __CURL_ABI_H__

/* The slice of libcurl's ABI that src/gui/httpget_curl.cpp needs.
 *
 * Why this exists rather than #include <curl/curl.h>: the update check
 * reaches libcurl entirely through dlsym, so no curl symbol is ever
 * referenced at link time and the shipped binary needs no libcurl at all
 * (verified: no libcurl.so.4 in DT_NEEDED, zero undefined curl symbols).
 * The header would therefore be needed only to spell a handful of
 * ABI-stable constants -- which would make every developer without
 * libcurl's headers silently build a *different* update backend
 * (a stub, since dropped), leaving httpget_curl.cpp uncompiled on their
 * machine. Declaring the constants here means that file is compiled and
 * type-checked everywhere, and libcurl stays a purely optional
 * runtime dependency.
 *
 * libcurl's own values, all stable and append-only across curl 7 and 8:
 *   CURLOPT_* are CURLOPTTYPE_* + number; CURLOPTTYPE_LONG = 326,
 *   STRINGPOINT = 325, FUNCTIONPOINT = 316, OBJECTPOINT = 282.
 *   Read out of curl-8.11.1's include/curl/curl.h.
 *
 * Drift cannot be spotted by inspection, so test/test_curl_abi.cpp watches
 * it two ways, with different strengths. Where the build has the real
 * header (the Linux CI jobs install libcurl4-openssl-dev) every constant is
 * static_assert'd against it, which catches any drift at all. Without the
 * header, the suite checks the constants against a live libcurl.so.4 by
 * confirming easy_setopt accepts each one -- that catches a value libcurl
 * no longer recognises, but NOT a value that is valid yet wrong (setting
 * CURLOPT_TIMEOUT to TIMEOUT_MS still returns CURLE_OK). So headerless
 * builds get the coarse check only, and full coverage depends on CI.
 */
namespace curl_abi {

  /* Opaque to us: only ever passed back to libcurl as a pointer. */
  struct CURL;
  struct curl_slist;

  /* Enums in libcurl, hence 4 bytes on every platform we support; the
   * runtime check in test_curl_abi.cpp relies on the width. */
  using CURLcode = int;
  using CURLoption = int;
  using CURLINFO = int;

  inline constexpr CURLcode CURLE_OK = 0;

  /* libcurl's CURL_GLOBAL_DEFAULT is a macro, not an enumerator, so the
   * header would expand that name inside this namespace; hence the suffix. */
  inline constexpr long CURL_GLOBAL_DEFAULT_FLAGS = 3;

  inline constexpr CURLoption CURLOPT_WRITEDATA = 10001;
  inline constexpr CURLoption CURLOPT_URL = 10002;
  inline constexpr CURLoption CURLOPT_USERAGENT = 10018;
  inline constexpr CURLoption CURLOPT_HTTPHEADER = 10023;
  inline constexpr CURLoption CURLOPT_TIMEOUT = 13;
  inline constexpr CURLoption CURLOPT_FOLLOWLOCATION = 52;
  inline constexpr CURLoption CURLOPT_MAXREDIRS = 68;
  inline constexpr CURLoption CURLOPT_NOSIGNAL = 99;
  inline constexpr CURLoption CURLOPT_WRITEFUNCTION = 20011;

  /* CURLINFO_LONG (0x200000) + 2. Read back through a long *. */
  inline constexpr CURLINFO CURLINFO_RESPONSE_CODE = 2097154;
}

#endif