// Copyright 2026 The PixelGrab Authors
// macOS implementation of IPlatformHotkey using CGEventTap.

#include "core/platform_hotkey.h"

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#include <cstdio>
#include <vector>

// Map VK_F* codes (Windows-style, used in core/app_defs.h) to macOS key codes.
static UInt32 VKToMacKeyCode(int vk) {
  // VK_F1 = 0x70 ... VK_F12 = 0x7B
  if (vk >= 0x70 && vk <= 0x7B) {
    // macOS function key codes: F1=122, F2=120, F3=99, F4=118, ...
    static const UInt32 fkeys[] = {
      kVK_F1, kVK_F2, kVK_F3, kVK_F4, kVK_F5, kVK_F6,
      kVK_F7, kVK_F8, kVK_F9, kVK_F10, kVK_F11, kVK_F12
    };
    return fkeys[vk - 0x70];
  }
  return static_cast<UInt32>(vk);
}

class MacPlatformHotkey : public IPlatformHotkey {
 public:
  ~MacPlatformHotkey() override { UnregisterAll(); }

  bool Register(int hotkey_id, int key_code) override {
    EventHotKeyRef ref = nullptr;
    EventHotKeyID hk_id = { 'PxGr', static_cast<UInt32>(hotkey_id) };
    OSStatus err = RegisterEventHotKey(
        VKToMacKeyCode(key_code), 0, hk_id,
        GetApplicationEventTarget(), 0, &ref);
    if (err != noErr) {
      std::printf("  [macOS] Failed to register hotkey %d\n", hotkey_id);
      return false;
    }
    entries_.push_back({hotkey_id, ref});
    return true;
  }

  void Unregister(int hotkey_id) override {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
      if (it->id == hotkey_id) {
        UnregisterEventHotKey(it->ref);
        entries_.erase(it);
        return;
      }
    }
  }

  void UnregisterAll() override {
    for (auto& e : entries_) {
      UnregisterEventHotKey(e.ref);
    }
    entries_.clear();
  }

 private:
  struct HotkeyEntry {
    int id;
    EventHotKeyRef ref;
  };
  std::vector<HotkeyEntry> entries_;
};

std::unique_ptr<IPlatformHotkey> CreatePlatformHotkey() {
  return std::make_unique<MacPlatformHotkey>();
}

#endif  // __APPLE__
