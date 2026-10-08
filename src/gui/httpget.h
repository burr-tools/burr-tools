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
#ifndef __HTTPGET_H__
#define __HTTPGET_H__

#include <cstddef>
#include <string>

/* One blocking HTTPS GET, used by the update check from a worker thread.
 * Exactly one implementation is compiled in, chosen by meson.build per
 * platform: NSURLSession on macOS, WinHTTP on Windows, libcurl elsewhere
 * (or a stub reporting Unsupported when libcurl headers were not found at
 * build time). libcurl is loaded at runtime, so where it is missing the
 * libcurl backend also reports Unsupported instead of the app failing to
 * start.
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
    TooLarge,     ///< body exceeded HTTP_MAX_BODY (libcurl and WinHTTP stop
                  ///< reading at the limit; NSURLSession checks once the
                  ///< body is buffered, bounded by its resource timeout)
    Unsupported,  ///< no HTTPS backend in this build, or libcurl missing
  };
  Kind kind = Kind::Transport;
  long status = 0;
  std::string body;
  std::string error;
};

inline constexpr size_t HTTP_MAX_BODY = 1048576;

HttpResult httpGet(const std::string & url, const std::string & userAgent, int timeoutSec);

#endif
