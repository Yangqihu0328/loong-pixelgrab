// Copyright 2026 The PixelGrab Authors
//
// IPlatformHttp — abstract interface for HTTPS requests and URL opening.
//
// Windows:  WinHTTP + ShellExecuteW
// macOS:    NSURLSession + NSWorkspace
// Linux:    libcurl (or socket+OpenSSL) + xdg-open

#ifndef PIXELGRAB_EXAMPLES_CORE_PLATFORM_HTTP_H_
#define PIXELGRAB_EXAMPLES_CORE_PLATFORM_HTTP_H_

#include <memory>
#include <string>

class IPlatformHttp {
 public:
  virtual ~IPlatformHttp() = default;

  /// Perform a synchronous HTTPS GET request.
  /// @param host  Server hostname, e.g. "api.github.com".
  /// @param path  Request path, e.g. "/repos/owner/repo/releases/latest".
  /// @return Response body as UTF-8 string, or empty string on failure.
  virtual std::string HttpsGet(const char* host, const char* path) = 0;

  /// Open the given URL in the system default browser.
  virtual void OpenUrlInBrowser(const char* url) = 0;

 protected:
  IPlatformHttp() = default;

 private:
  IPlatformHttp(const IPlatformHttp&) = delete;
  IPlatformHttp& operator=(const IPlatformHttp&) = delete;
};

/// Factory — returns the platform-specific implementation.
std::unique_ptr<IPlatformHttp> CreatePlatformHttp();

#endif  // PIXELGRAB_EXAMPLES_CORE_PLATFORM_HTTP_H_
