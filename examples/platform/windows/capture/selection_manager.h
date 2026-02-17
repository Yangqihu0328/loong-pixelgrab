// Copyright 2026 The PixelGrab Authors
// Selection mode + low-level hooks.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_SELECTION_MANAGER_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_SELECTION_MANAGER_H_

#ifdef _WIN32

#include "platform/windows/win_app_defs.h"

class SelectionManager {
 public:
  static LRESULT CALLBACK MouseHookProc(int code, WPARAM wp, LPARAM lp);
  static LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wp, LPARAM lp);

  void SyncHook();
  void BeginSelect();
  void EndSelect();
  void HandleCancel();
  void HandleClick(int x, int y);
  void HandleMove(int x, int y);
  void HandleRegionSelect(int x1, int y1, int x2, int y2);
  bool DispatchF1Mode(RECT rc);
  RECT GetVisibleWindowRect(HWND hwnd);

  bool IsSelecting() const { return selecting_; }
  HHOOK MouseHook() const { return mouse_hook_; }
  HHOOK KbdHook() const { return kbd_hook_; }
  POINT LastCursor() const { return last_cursor_; }
  void SetLastCursor(POINT p) { last_cursor_ = p; }
  HWND HighlightHwnd() const { return highlight_hwnd_; }
  void SetHighlightHwnd(HWND h) { highlight_hwnd_ = h; }
  bool IsSelectDragging() const { return select_dragging_; }
  void SetSelectDragging(bool v) { select_dragging_ = v; }
  POINT SelectStart() const { return select_start_; }
  void SetSelectStart(POINT p) { select_start_ = p; }
  DWORD ClickTime() const { return click_time_; }
  void SetClickTime(DWORD t) { click_time_ = t; }
  POINT ClickPt() const { return click_pt_; }
  void SetClickPt(POINT p) { click_pt_ = p; }

 private:
  bool  selecting_ = false;
  HHOOK mouse_hook_ = nullptr;
  HHOOK kbd_hook_ = nullptr;
  POINT last_cursor_ = {-1, -1};
  HWND  highlight_hwnd_ = nullptr;
  bool  select_dragging_ = false;
  POINT select_start_ = {};
  DWORD click_time_ = 0;
  POINT click_pt_ = {};
  bool  toolbar_click_ = false;
};

#endif  // _WIN32
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_SELECTION_MANAGER_H_
