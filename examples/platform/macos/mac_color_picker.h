// Copyright 2026 The PixelGrab Authors
// Color picker overlay — shows magnifier + real-time color info at cursor.
// macOS implementation using Cocoa + Core Graphics.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_MACOS_MAC_COLOR_PICKER_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_MACOS_MAC_COLOR_PICKER_H_

#ifdef __APPLE__

#include "pixelgrab/pixelgrab.h"

#ifdef __OBJC__
@class NSWindow;
@class NSTimer;
@class NSView;
#else
typedef void NSWindow;
typedef void NSTimer;
typedef void NSView;
#endif

class ColorPicker {
 public:
  void Show();
  void Dismiss();
  bool IsActive() const { return active_; }

 private:
  bool       active_ = false;
  NSWindow*  window_ = nullptr;
  NSView*    content_view_ = nullptr;
  NSTimer*   timer_ = nullptr;

  PixelGrabColor    cur_color_ = {};
  PixelGrabColorHsv cur_hsv_ = {};
  char              hex_buf_[16] = {};
  int               cursor_x_ = 0;
  int               cursor_y_ = 0;
};

#endif  // __APPLE__
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_MACOS_MAC_COLOR_PICKER_H_
