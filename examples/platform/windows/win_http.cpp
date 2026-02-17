// Copyright 2026 The PixelGrab Authors
//
// WinPlatformHttp — WinHTTP + ShellExecuteW implementation.

#include "platform/windows/win_http.h"

#ifdef _WIN32

#include <cstdio>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>

#pragma comment(lib, "winhttp.lib")

std::string WinPlatformHttp::HttpsGet(const char* host, const char* path) {
  std::string result;

  // Convert host/path to wide strings.
  wchar_t whost[256] = {};
  for (int i = 0; i < 255 && host[i]; ++i)
    whost[i] = static_cast<wchar_t>(host[i]);

  wchar_t wpath[1024] = {};
  for (int i = 0; i < 1023 && path[i]; ++i)
    wpath[i] = static_cast<wchar_t>(path[i]);

  constexpr DWORD kAutoProxy = 4;

  HINTERNET hSession = WinHttpOpen(
      L"PixelGrab-UpdateChecker/1.0",
      kAutoProxy,
      WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS,
      0);
  if (!hSession) {
    hSession = WinHttpOpen(
        L"PixelGrab-UpdateChecker/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
  }
  if (!hSession) return result;

  HINTERNET hConnect = WinHttpConnect(hSession, whost,
                                       INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    return result;
  }

  HINTERNET hRequest = WinHttpOpenRequest(
      hConnect, L"GET", wpath, nullptr,
      WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES,
      WINHTTP_FLAG_SECURE);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
  }

  WinHttpAddRequestHeaders(hRequest,
      L"Accept: application/vnd.github.v3+json\r\n",
      static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);

  BOOL ok = WinHttpSendRequest(hRequest,
      WINHTTP_NO_ADDITIONAL_HEADERS, 0,
      WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

  if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

  if (ok) {
    DWORD statusCode = 0;
    DWORD size = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &size, WINHTTP_NO_HEADER_INDEX);

    if (statusCode == 200) {
      DWORD bytesAvailable = 0;
      while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) &&
             bytesAvailable > 0) {
        std::string chunk(bytesAvailable, '\0');
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, &chunk[0], bytesAvailable, &bytesRead);
        chunk.resize(bytesRead);
        result += chunk;
      }
    }
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return result;
}

void WinPlatformHttp::OpenUrlInBrowser(const char* url) {
  if (!url || !url[0]) return;
  wchar_t wurl[512] = {};
  for (int i = 0; i < 511 && url[i]; ++i)
    wurl[i] = static_cast<wchar_t>(url[i]);
  ShellExecuteW(nullptr, L"open", wurl, nullptr, nullptr, SW_SHOWNORMAL);
}

// ===================================================================
// Factory
// ===================================================================

std::unique_ptr<IPlatformHttp> CreatePlatformHttp() {
  return std::make_unique<WinPlatformHttp>();
}

#endif  // _WIN32
