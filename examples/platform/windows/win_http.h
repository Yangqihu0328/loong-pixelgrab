// Copyright 2026 The PixelGrab Authors
//
// WinPlatformHttp — WinHTTP implementation of IPlatformHttp.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_HTTP_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_HTTP_H_

#include "core/platform_http.h"

class WinPlatformHttp : public IPlatformHttp {
 public:
  WinPlatformHttp() = default;
  ~WinPlatformHttp() override = default;

  std::string HttpsGet(const char* host, const char* path) override;
  void OpenUrlInBrowser(const char* url) override;
};

#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_HTTP_H_
