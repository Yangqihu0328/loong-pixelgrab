// Copyright 2026 The PixelGrab Authors
// Overlay highlight window + cursor management.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_OVERLAY_MANAGER_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_OVERLAY_MANAGER_H_

#ifdef _WIN32

#include "platform/windows/win_app_defs.h"

class OverlayManager {
 public:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static LRESULT CALLBACK DimWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  void SetSelectionCursors();
  void RestoreSystemCursors();
  void ShowHighlight(int x, int y, int w, int h);
  void HideHighlight();
  void SetColor(COLORREF color);
  void ResetState();

  HWND Overlay() const { return overlay_; }
  void SetOverlay(HWND h) { overlay_ = h; }
  COLORREF Color() const { return overlay_color_; }
  HWND SelectDimWnd() const { return select_dim_wnd_; }
  void set_SelectDimWnd(HWND h) { select_dim_wnd_ = h; }

 private:
  HWND overlay_ = nullptr;
  COLORREF overlay_color_ = kHighlightColor;
  HWND select_dim_wnd_ = nullptr;
  RECT last_highlight_ = {};
};

#endif  // _WIN32
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_OVERLAY_MANAGER_H_
