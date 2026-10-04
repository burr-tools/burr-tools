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
