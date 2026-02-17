// Copyright 2026 The PixelGrab Authors
//
// IPlatformSettings — abstract interface for persistent application settings.
//
// Windows:  Windows Registry  (HKCU\Software\PixelGrab)
// macOS:    NSUserDefaults / plist
// Linux:    ~/.config/pixelgrab/settings.ini

#ifndef PIXELGRAB_EXAMPLES_CORE_PLATFORM_SETTINGS_H_
#define PIXELGRAB_EXAMPLES_CORE_PLATFORM_SETTINGS_H_

#include <memory>

class IPlatformSettings {
 public:
  virtual ~IPlatformSettings() = default;

  /// Read a 32-bit integer setting.  Returns true on success.
  virtual bool GetInt(const char* key, int* out_value) = 0;

  /// Write a 32-bit integer setting.  Returns true on success.
  virtual bool SetInt(const char* key, int value) = 0;

  /// Read a UTF-8 string setting into `buf` (at most `buf_size` bytes
  /// including the NUL terminator).  Returns true on success.
  virtual bool GetString(const char* key, char* buf, int buf_size) = 0;

  /// Write a UTF-8 string setting.  Returns true on success.
  virtual bool SetString(const char* key, const char* value) = 0;

  /// Check whether the application is configured to launch at login.
  virtual bool IsAutoStartEnabled() = 0;

  /// Enable or disable launch-at-login for the current user.
  virtual void SetAutoStart(bool enable) = 0;

 protected:
  IPlatformSettings() = default;

 private:
  IPlatformSettings(const IPlatformSettings&) = delete;
  IPlatformSettings& operator=(const IPlatformSettings&) = delete;
};

/// Factory — returns the platform-specific implementation.
std::unique_ptr<IPlatformSettings> CreatePlatformSettings();

#endif  // PIXELGRAB_EXAMPLES_CORE_PLATFORM_SETTINGS_H_
