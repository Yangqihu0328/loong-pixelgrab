// Copyright 2026 The PixelGrab Authors
//
// WinPlatformHotkey — Win32 RegisterHotKey implementation of IPlatformHotkey.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_HOTKEY_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_HOTKEY_H_

#include "core/platform_hotkey.h"

#include <vector>

class WinPlatformHotkey : public IPlatformHotkey {
 public:
  WinPlatformHotkey() = default;
  ~WinPlatformHotkey() override;

  bool Register(int hotkey_id, int key_code) override;
  void Unregister(int hotkey_id) override;
  void UnregisterAll() override;

 private:
  std::vector<int> registered_ids_;
};

#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_HOTKEY_H_
