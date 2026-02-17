// Copyright 2026 The PixelGrab Authors
// F1 toolbar (top-center mode selector).

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_F1_TOOLBAR_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_F1_TOOLBAR_H_

#ifdef _WIN32

#include "platform/windows/win_app_defs.h"

class F1Toolbar {
 public:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  void Dismiss();
  void UpdateButtons();
  void ShowMenu();

  HWND Toolbar() const { return toolbar_; }
  int ActiveId() const { return active_id_; }
  void SetActiveId(int id) { active_id_ = id; }

 private:
  HWND toolbar_ = nullptr;
  int  active_id_ = kF1Capture;
};

#endif  // _WIN32
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_F1_TOOLBAR_H_
