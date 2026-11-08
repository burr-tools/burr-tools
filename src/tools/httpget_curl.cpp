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
#include "httpget.h"
#include "curl_abi.h"

#include <dlfcn.h>

#include <mutex>

/* libcurl is loaded with dlopen rather than linked, so a system without it
 * (or with only another soname) still starts BurrTools; the update check
 * then reports itself Unsupported. Nothing here references a curl symbol at
 * link time, so the binary carries no libcurl dependency -- see curl_abi.h
 * for the constants and why they are declared rather than included.
 */
namespace {

  using curl_abi::CURL;
  using curl_abi::CURLcode;
  using curl_abi::CURLINFO;
  using curl_abi::CURLoption;
  using curl_abi::curl_slist;

  struct Curl {
    CURLcode (*global_init)(long);
    CURL * (*easy_init)(void);
    CURLcode (*easy_setopt)(CURL *, CURLoption, ...);
    CURLcode (*easy_perform)(CURL *);
    CURLcode (*easy_getinfo)(CURL *, CURLINFO, ...);
    void (*easy_cleanup)(CURL *);
    const char * (*easy_strerror)(CURLcode);
    curl_slist * (*slist_append)(curl_slist *, const char *);
    void (*slist_free_all)(curl_slist *);
  };

  template <class F>
  bool resolve(void * lib, const char * name, F & fn) {
    fn = reinterpret_cast<F>(dlsym(lib, name));
    return fn != nullptr;
  }

  /* Loaded once and never unloaded; nullptr when libcurl is unavailable. */
  const Curl * loadCurl(void) {
    static Curl api;
    static const Curl * loaded = nullptr;
    static std::once_flag once;
    std::call_once(once, [] {
      void * lib = nullptr;
      // Both the OpenSSL and the GnuTLS flavours export the same ABI, so
      // either soname is accepted.
      for (const char * soname : { "libcurl.so.4", "libcurl-gnutls.so.4", "libcurl.so" })
        if ((lib = dlopen(soname, RTLD_NOW | RTLD_LOCAL)) != nullptr)
          break;
      if (!lib)
        return;
      if (!resolve(lib, "curl_global_init", api.global_init) ||
          !resolve(lib, "curl_easy_init", api.easy_init) ||
          !resolve(lib, "curl_easy_setopt", api.easy_setopt) ||
          !resolve(lib, "curl_easy_perform", api.easy_perform) ||
          !resolve(lib, "curl_easy_getinfo", api.easy_getinfo) ||
          !resolve(lib, "curl_easy_cleanup", api.easy_cleanup) ||
          !resolve(lib, "curl_easy_strerror", api.easy_strerror) ||
          !resolve(lib, "curl_slist_append", api.slist_append) ||
          !resolve(lib, "curl_slist_free_all", api.slist_free_all))
        return;
      /* curl_global_init is not thread safe; this is its only caller. */
      if (api.global_init(curl_abi::CURL_GLOBAL_DEFAULT_FLAGS) != curl_abi::CURLE_OK)
        return;
      loaded = &api;
    });
    return loaded;
  }

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

  HttpResult r;

  const Curl * curl = loadCurl();
  if (!curl) {
    r.kind = HttpResult::Kind::Unsupported;
    r.error = "libcurl (libcurl.so.4) is not installed";
    return r;
  }

  CURL * c = curl->easy_init();
  if (!c) {
    r.error = "could not initialise libcurl";
    return r;
  }

  Sink sink;
  curl_slist * headers = curl->slist_append(nullptr, "Accept: application/vnd.github+json");

  curl->easy_setopt(c, curl_abi::CURLOPT_URL, url.c_str());
  curl->easy_setopt(c, curl_abi::CURLOPT_USERAGENT, userAgent.c_str());
  curl->easy_setopt(c, curl_abi::CURLOPT_HTTPHEADER, headers);
  curl->easy_setopt(c, curl_abi::CURLOPT_WRITEFUNCTION, onData);
  curl->easy_setopt(c, curl_abi::CURLOPT_WRITEDATA, &sink);
  curl->easy_setopt(c, curl_abi::CURLOPT_TIMEOUT, long(timeoutSec));
  curl->easy_setopt(c, curl_abi::CURLOPT_FOLLOWLOCATION, 1L);
  curl->easy_setopt(c, curl_abi::CURLOPT_MAXREDIRS, 5L);
  curl->easy_setopt(c, curl_abi::CURLOPT_NOSIGNAL, 1L);  // worker thread: no SIGALRM timeouts

  CURLcode rc = curl->easy_perform(c);
  curl->easy_getinfo(c, curl_abi::RESPONSE_CODE, &r.status);

  curl->slist_free_all(headers);
  curl->easy_cleanup(c);

  if (sink.tooLarge) {
    r.kind = HttpResult::Kind::TooLarge;
    r.error = "response too large";
  } else if (rc != curl_abi::CURLE_OK) {
    r.kind = HttpResult::Kind::Transport;
    r.error = curl->easy_strerror(rc);
  } else if (r.status != 200) {
    r.kind = HttpResult::Kind::Status;
    r.error = "HTTP " + std::to_string(r.status);
  } else {
    r.kind = HttpResult::Kind::Ok;
    r.body = std::move(sink.body);
  }
  return r;
}
