// Copyright 2026 The PixelGrab Authors
// macOS application skeleton — NSApplication + NSStatusItem + basic capture.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_MACOS_MAC_APPLICATION_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_MACOS_MAC_APPLICATION_H_

#ifdef __APPLE__

#include "pixelgrab/pixelgrab.h"
#include "core/platform_settings.h"
#include "core/platform_hotkey.h"
#include "core/platform_http.h"
#include "core/i18n.h"
#include "platform/macos/mac_color_picker.h"
#include <memory>

class MacApplication {
 public:
  static MacApplication& instance();

  bool Init();
  int  Run();
  void Shutdown();

  PixelGrabContext* Ctx() const { return ctx_; }
  ColorPicker&     GetColorPicker() { return color_picker_; }

 private:
  MacApplication() = default;

  PixelGrabContext* ctx_ = nullptr;
  std::unique_ptr<IPlatformSettings> settings_;
  std::unique_ptr<IPlatformHotkey>   hotkey_;
  std::unique_ptr<IPlatformHttp>     http_;
  ColorPicker color_picker_;
};

#endif  // __APPLE__
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_MACOS_MAC_APPLICATION_H_
