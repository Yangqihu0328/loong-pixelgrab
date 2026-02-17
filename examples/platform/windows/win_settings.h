// Copyright 2026 The PixelGrab Authors
//
// WinPlatformSettings — Windows Registry implementation of IPlatformSettings.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_SETTINGS_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_SETTINGS_H_

#include "core/platform_settings.h"

class WinPlatformSettings : public IPlatformSettings {
 public:
  WinPlatformSettings() = default;
  ~WinPlatformSettings() override = default;

  bool GetInt(const char* key, int* out_value) override;
  bool SetInt(const char* key, int value) override;
  bool GetString(const char* key, char* buf, int buf_size) override;
  bool SetString(const char* key, const char* value) override;
  bool IsAutoStartEnabled() override;
  void SetAutoStart(bool enable) override;
};

#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_SETTINGS_H_
