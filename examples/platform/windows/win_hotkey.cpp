// Copyright 2026 The PixelGrab Authors
//
// WinPlatformHotkey — Win32 RegisterHotKey / UnregisterHotKey.

#include "platform/windows/win_hotkey.h"

#ifdef _WIN32

#include <algorithm>
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

WinPlatformHotkey::~WinPlatformHotkey() {
  UnregisterAll();
}

bool WinPlatformHotkey::Register(int hotkey_id, int key_code) {
  if (::RegisterHotKey(nullptr, hotkey_id, 0, static_cast<UINT>(key_code))) {
    registered_ids_.push_back(hotkey_id);
    return true;
  }
  std::printf("WARNING: Could not register hotkey id=%d, vk=0x%02X\n",
              hotkey_id, key_code);
  return false;
}

void WinPlatformHotkey::Unregister(int hotkey_id) {
  ::UnregisterHotKey(nullptr, hotkey_id);
  auto it = std::find(registered_ids_.begin(), registered_ids_.end(), hotkey_id);
  if (it != registered_ids_.end()) registered_ids_.erase(it);
}

void WinPlatformHotkey::UnregisterAll() {
  for (int id : registered_ids_)
    ::UnregisterHotKey(nullptr, id);
  registered_ids_.clear();
}

// ===================================================================
// Factory
// ===================================================================

std::unique_ptr<IPlatformHotkey> CreatePlatformHotkey() {
  return std::make_unique<WinPlatformHotkey>();
}

#endif  // _WIN32
