// Copyright 2026 The PixelGrab Authors
// Color picker overlay — shows magnifier + real-time color info at cursor.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_COLOR_PICKER_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_COLOR_PICKER_H_

#ifdef _WIN32

#include "platform/windows/win_app_defs.h"

class ColorPicker {
 public:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  void Show();
  void Dismiss();
  void CopyColor();
  void ToggleDisplay();
  bool IsActive() const { return active_; }
  HWND PickerWnd() const { return picker_wnd_; }

 private:
  void UpdateAtCursor();
  void Paint(HDC hdc);

  bool  active_ = false;
  bool  show_hex_ = true;
  HWND  picker_wnd_ = nullptr;

  // Current color state
  PixelGrabColor  cur_color_ = {};
  PixelGrabColorHsv cur_hsv_ = {};
  char  hex_buf_[16] = {};
  char  rgb_buf_[32] = {};
  int   cursor_x_ = 0;
  int   cursor_y_ = 0;
};

#endif  // _WIN32
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_COLOR_PICKER_H_
