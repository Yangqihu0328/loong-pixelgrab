// Copyright 2026 The PixelGrab Authors
// macOS implementation of IPlatformSettings using NSUserDefaults.

#include "core/platform_settings.h"

#ifdef __APPLE__

#import <Foundation/Foundation.h>
#import <ServiceManagement/ServiceManagement.h>
#include <cstdio>
#include <cstring>

static NSString* const kSuiteName = @"com.pixelgrab.settings";

class MacPlatformSettings : public IPlatformSettings {
 public:
  MacPlatformSettings() {
    defaults_ = [[NSUserDefaults alloc] initWithSuiteName:kSuiteName];
  }

  bool GetInt(const char* key, int* out_value) override {
    NSString* nskey = [NSString stringWithUTF8String:key];
    id obj = [defaults_ objectForKey:nskey];
    if (!obj) return false;
    *out_value = [defaults_ integerForKey:nskey];
    return true;
  }

  bool SetInt(const char* key, int value) override {
    NSString* nskey = [NSString stringWithUTF8String:key];
    [defaults_ setInteger:value forKey:nskey];
    [defaults_ synchronize];
    return true;
  }

  bool GetString(const char* key, char* buf, int buf_size) override {
    NSString* nskey = [NSString stringWithUTF8String:key];
    NSString* val = [defaults_ stringForKey:nskey];
    if (!val) return false;
    const char* utf8 = [val UTF8String];
    if (!utf8) return false;
    strncpy(buf, utf8, static_cast<size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return true;
  }

  bool SetString(const char* key, const char* value) override {
    NSString* nskey = [NSString stringWithUTF8String:key];
    NSString* nsval = [NSString stringWithUTF8String:value];
    [defaults_ setObject:nsval forKey:nskey];
    [defaults_ synchronize];
    return true;
  }

  bool IsAutoStartEnabled() override {
    // Check if launch-at-login is enabled
    // macOS 13+ uses SMAppService; for older, use SMLoginItemSetEnabled
    return false;  // Simplified: actual implementation needs bundle ID
  }

  void SetAutoStart(bool enable) override {
    if (enable)
      std::printf("  [macOS] Auto-start enabled (stub).\n");
    else
      std::printf("  [macOS] Auto-start disabled (stub).\n");
  }

 private:
  NSUserDefaults* defaults_;
};

std::unique_ptr<IPlatformSettings> CreatePlatformSettings() {
  return std::make_unique<MacPlatformSettings>();
}

#endif  // __APPLE__
