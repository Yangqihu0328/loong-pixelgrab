// Copyright 2026 The loong-pixelgrab Authors
//
// Windows translation backend using WinHTTP (POST) + Wincrypt (MD5).

#ifdef _WIN32

#include "translate/translate_backend.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <cstdio>
#include <string>

#include "core/logger.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")

namespace pixelgrab {
namespace internal {

namespace {

// Parse a URL into host and path components.
// Expects "https://host/path" format.
bool ParseUrl(const std::string& url, std::wstring& host, std::wstring& path) {
  const std::string prefix = "https://";
  if (url.compare(0, prefix.size(), prefix) != 0) return false;

  size_t host_start = prefix.size();
  size_t path_start = url.find('/', host_start);
  if (path_start == std::string::npos) {
    path_start = url.size();
  }

  std::string h = url.substr(host_start, path_start - host_start);
  std::string p = (path_start < url.size()) ? url.substr(path_start) : "/";

  host.resize(h.size());
  for (size_t i = 0; i < h.size(); ++i)
    host[i] = static_cast<wchar_t>(h[i]);

  path.resize(p.size());
  for (size_t i = 0; i < p.size(); ++i)
    path[i] = static_cast<wchar_t>(p[i]);

  return true;
}

}  // namespace

class WinTranslateBackend : public TranslateBackend {
 protected:
  std::string HttpPost(const std::string& url,
                       const std::string& body) override {
    std::wstring host, path;
    if (!ParseUrl(url, host, path)) {
      last_error_detail_ = "Failed to parse URL: " + url;
      PIXELGRAB_LOG_ERROR("{}", last_error_detail_);
      return {};
    }

    constexpr DWORD kAutoProxy = 4;  // WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
    HINTERNET session = WinHttpOpen(
        L"PixelGrab-Translate/1.0", kAutoProxy, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
      session = WinHttpOpen(
          L"PixelGrab-Translate/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!session) {
      last_error_detail_ = "WinHttpOpen failed, GetLastError=" +
                           std::to_string(GetLastError());
      PIXELGRAB_LOG_ERROR("{}", last_error_detail_);
      return {};
    }

    static constexpr DWORD kHttpTimeoutMs = 5000;
    WinHttpSetTimeouts(session, kHttpTimeoutMs, kHttpTimeoutMs,
                       kHttpTimeoutMs, kHttpTimeoutMs);

    HINTERNET connect =
        WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) {
      last_error_detail_ = "WinHttpConnect failed, GetLastError=" +
                           std::to_string(GetLastError());
      PIXELGRAB_LOG_ERROR("{}", last_error_detail_);
      WinHttpCloseHandle(session);
      return {};
    }

    HINTERNET request = WinHttpOpenRequest(
        connect, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) {
      last_error_detail_ = "WinHttpOpenRequest failed, GetLastError=" +
                           std::to_string(GetLastError());
      PIXELGRAB_LOG_ERROR("{}", last_error_detail_);
      WinHttpCloseHandle(connect);
      WinHttpCloseHandle(session);
      return {};
    }

    WinHttpAddRequestHeaders(
        request,
        L"Content-Type: application/x-www-form-urlencoded\r\n",
        static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);

    BOOL ok = WinHttpSendRequest(
        request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()), 0);

    std::string result;
    if (ok) ok = WinHttpReceiveResponse(request, nullptr);
    if (ok) {
      DWORD status_code = 0;
      DWORD sz = sizeof(status_code);
      WinHttpQueryHeaders(request,
                          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &sz,
                          WINHTTP_NO_HEADER_INDEX);
      if (status_code == 200) {
        DWORD bytes_available = 0;
        while (WinHttpQueryDataAvailable(request, &bytes_available) &&
               bytes_available > 0) {
          std::string chunk(bytes_available, '\0');
          DWORD bytes_read = 0;
          WinHttpReadData(request, &chunk[0], bytes_available, &bytes_read);
          chunk.resize(bytes_read);
          result += chunk;
        }
      } else {
        last_error_detail_ = "HTTP POST returned status " +
                             std::to_string(status_code);
        PIXELGRAB_LOG_ERROR("{}", last_error_detail_);
      }
    } else {
      last_error_detail_ = "WinHttpSendRequest/ReceiveResponse failed, "
                           "GetLastError=" +
                           std::to_string(GetLastError());
      PIXELGRAB_LOG_ERROR("{}", last_error_detail_);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;
  }

  std::string ComputeMD5(const std::string& input) override {
    HCRYPTPROV prov = 0;
    if (!CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_FULL,
                              CRYPT_VERIFYCONTEXT)) {
      return {};
    }

    HCRYPTHASH hash = 0;
    if (!CryptCreateHash(prov, CALG_MD5, 0, 0, &hash)) {
      CryptReleaseContext(prov, 0);
      return {};
    }

    CryptHashData(hash, reinterpret_cast<const BYTE*>(input.data()),
                  static_cast<DWORD>(input.size()), 0);

    BYTE digest[16] = {};
    DWORD digest_len = sizeof(digest);
    CryptGetHashParam(hash, HP_HASHVAL, digest, &digest_len, 0);

    CryptDestroyHash(hash);
    CryptReleaseContext(prov, 0);

    char hex[33] = {};
    for (int i = 0; i < 16; ++i) {
      std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
    }
    return std::string(hex, 32);
  }
};

std::unique_ptr<TranslateBackend> CreatePlatformTranslateBackend() {
  return std::make_unique<WinTranslateBackend>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // _WIN32
