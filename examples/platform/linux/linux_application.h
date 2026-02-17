// Copyright 2026 The PixelGrab Authors
// Linux application skeleton — GTK3 status icon + basic capture.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_LINUX_LINUX_APPLICATION_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_LINUX_LINUX_APPLICATION_H_

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include "pixelgrab/pixelgrab.h"
#include "core/platform_settings.h"
#include "core/platform_hotkey.h"
#include "core/platform_http.h"
#include "core/i18n.h"
#include "platform/linux/linux_color_picker.h"
#include <memory>

class LinuxApplication {
 public:
  static LinuxApplication& instance();

  bool Init();
  int  Run();
  void Shutdown();

  PixelGrabContext* Ctx() const { return ctx_; }
  ColorPicker&     GetColorPicker() { return color_picker_; }

 private:
  LinuxApplication() = default;

  PixelGrabContext* ctx_ = nullptr;
  std::unique_ptr<IPlatformSettings> settings_;
  std::unique_ptr<IPlatformHotkey>   hotkey_;
  std::unique_ptr<IPlatformHttp>     http_;
  ColorPicker color_picker_;
};

#endif  // __linux__
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_LINUX_LINUX_APPLICATION_H_
