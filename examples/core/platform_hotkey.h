// Copyright 2026 The PixelGrab Authors
//
// IPlatformHotkey — abstract interface for system-wide global hotkeys.
//
// Windows:  RegisterHotKey / UnregisterHotKey
// macOS:    CGEventTap / NSEvent.addGlobalMonitorForEvents
// Linux:    XGrabKey (X11) / libkeybinder

#ifndef PIXELGRAB_EXAMPLES_CORE_PLATFORM_HOTKEY_H_
#define PIXELGRAB_EXAMPLES_CORE_PLATFORM_HOTKEY_H_

#include <memory>

class IPlatformHotkey {
 public:
  virtual ~IPlatformHotkey() = default;

  /// Register a global hotkey.
  /// @param hotkey_id  Application-defined identifier (e.g. kHotkeyF1).
  /// @param key_code   Platform-neutral virtual-key code (see kKeyF1..kKeyF12
  ///                   in core/app_defs.h — values match Win32 VK_F* codes).
  /// @return true if the hotkey was successfully registered.
  virtual bool Register(int hotkey_id, int key_code) = 0;

  /// Unregister a previously registered hotkey.
  virtual void Unregister(int hotkey_id) = 0;

  /// Unregister all hotkeys registered through this instance.
  virtual void UnregisterAll() = 0;

 protected:
  IPlatformHotkey() = default;

 private:
  IPlatformHotkey(const IPlatformHotkey&) = delete;
  IPlatformHotkey& operator=(const IPlatformHotkey&) = delete;
};

/// Factory — returns the platform-specific implementation.
std::unique_ptr<IPlatformHotkey> CreatePlatformHotkey();

#endif  // PIXELGRAB_EXAMPLES_CORE_PLATFORM_HOTKEY_H_
