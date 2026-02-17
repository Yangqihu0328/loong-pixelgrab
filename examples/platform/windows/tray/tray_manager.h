// Copyright 2026 The PixelGrab Authors
// System tray WndProc and menu.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_TRAY_TRAY_MANAGER_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_TRAY_TRAY_MANAGER_H_

#ifdef _WIN32

#include "platform/windows/win_app_defs.h"

class TrayManager {
 public:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  void ShowMenu();

  HWND TrayHwnd() const { return tray_hwnd_; }
  void SetTrayHwnd(HWND h) { tray_hwnd_ = h; }
  NOTIFYICONDATAW& Nid() { return nid_; }
  const NOTIFYICONDATAW& Nid() const { return nid_; }

 private:
  HWND            tray_hwnd_ = nullptr;
  NOTIFYICONDATAW nid_ = {};
};

#endif  // _WIN32
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_TRAY_TRAY_MANAGER_H_
