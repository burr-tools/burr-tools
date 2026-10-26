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
