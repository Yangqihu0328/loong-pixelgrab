// Copyright 2026 The PixelGrab Authors
// Pin border frame management.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_PIN_MANAGER_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_PIN_MANAGER_H_

#ifdef _WIN32

#include "platform/windows/win_app_defs.h"

class PinManager {
 public:
  static LRESULT CALLBACK PinBorderWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  void ShowBorderFor(PinEntry& entry, int x, int y, int w, int h);
  void HideBorderFor(PinEntry& entry);
  void SyncBorders();
  void CloseByHwnd(HWND hw);
  void PinCapture();
  void PinFromClipboard();
  void PinFromHistory(int history_id);
  void HandleDoubleClick(int x, int y);

  std::vector<PinEntry>& Pins() { return pins_; }
  const std::vector<PinEntry>& Pins() const { return pins_; }

 private:
  std::vector<PinEntry> pins_;
};

#endif  // _WIN32
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_PIN_MANAGER_H_
