// Copyright 2026 The PixelGrab Authors
// Selection mode + low-level hooks.

#include "platform/windows/capture/selection_manager.h"

#ifdef _WIN32

#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "platform/windows/win_application.h"

LRESULT CALLBACK SelectionManager::MouseHookProc(int code, WPARAM wp, LPARAM lp) {
  auto& app = Application::instance();
  auto& self = app.Selection();
  if (code >= 0) {
    auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lp);

    if (self.selecting_) {
      if (app.GetF1Toolbar().Toolbar()) {
        RECT tbrc;
        GetWindowRect(app.GetF1Toolbar().Toolbar(), &tbrc);
        POINT tpt = ms->pt;
        if (PtInRect(&tbrc, tpt)) {
          // If we are actively dragging a selection rectangle, the
          // button-up must still reach the selection logic so the drag
          // state does not get stuck – even when the cursor is above
          // the toolbar.
          if (!(self.select_dragging_ && wp == WM_LBUTTONUP)) {
            if (wp == WM_LBUTTONDOWN)
              self.toolbar_click_ = true;
            if (wp == WM_LBUTTONUP)
              self.toolbar_click_ = false;
            return CallNextHookEx(self.mouse_hook_, code, wp, lp);
          }
        }
      }

      // Mousedown started on the toolbar but the mouse slipped outside
      // before release.  Swallow the event so it never reaches the
      // selection logic, and release any lingering button capture so
      // the button control resets to its normal visual state.
      if (self.toolbar_click_ && wp == WM_LBUTTONUP) {
        self.toolbar_click_ = false;
        ReleaseCapture();
        return 1;
      }

      if (wp == WM_LBUTTONDOWN) {
        self.toolbar_click_ = false;
        PostThreadMessage(app.MainThread(), kMsgLeftDown,
                          static_cast<WPARAM>(ms->pt.x),
                          static_cast<LPARAM>(ms->pt.y));
        return 1;
      } else if (wp == WM_LBUTTONUP) {
        PostThreadMessage(app.MainThread(), kMsgLeftUp,
                          static_cast<WPARAM>(ms->pt.x),
                          static_cast<LPARAM>(ms->pt.y));
        return 1;
      } else if (wp == WM_RBUTTONDOWN) {
        PostThreadMessage(app.MainThread(), kMsgRightClick, 0, 0);
        return 1;
      }
    } else if (!app.Pins().Pins().empty() && wp == WM_LBUTTONDOWN) {
      DWORD now = ms->time;
      int dx = IntAbs(static_cast<int>(ms->pt.x - self.click_pt_.x));
      int dy = IntAbs(static_cast<int>(ms->pt.y - self.click_pt_.y));
      int thresh_x = GetSystemMetrics(SM_CXDOUBLECLK) / 2;
      int thresh_y = GetSystemMetrics(SM_CYDOUBLECLK) / 2;

      if ((now - self.click_time_) <= GetDoubleClickTime() &&
          dx <= thresh_x && dy <= thresh_y) {
        PostThreadMessage(app.MainThread(), kMsgDoubleClick,
                          static_cast<WPARAM>(ms->pt.x),
                          static_cast<LPARAM>(ms->pt.y));
        self.click_time_ = 0;
      } else {
        self.click_time_ = now;
        self.click_pt_   = ms->pt;
      }
    }
  }
  return CallNextHookEx(self.mouse_hook_, code, wp, lp);
}

LRESULT CALLBACK SelectionManager::KeyboardHookProc(int code, WPARAM wp, LPARAM lp) {
  auto& app = Application::instance();
  auto& self = app.Selection();
  static bool ctrl_held = false;

  if (code >= 0) {
    auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);
    bool down = (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN);

    // Track Ctrl state (low-level hook reports VK_LCONTROL / VK_RCONTROL)
    if (kb->vkCode == VK_LCONTROL || kb->vkCode == VK_RCONTROL)
      ctrl_held = down;

    if (self.selecting_ && down) {
      if (kb->vkCode == VK_ESCAPE) {
        PostThreadMessage(app.MainThread(), kMsgKeyEscape, 0, 0);
        return 1;
      }
      if (kb->vkCode == 'C' && ctrl_held) {
        PostThreadMessage(app.MainThread(), kMsgCopyColor, 0, 0);
        return 1;
      }
      if (kb->vkCode == VK_LSHIFT || kb->vkCode == VK_RSHIFT) {
        PostThreadMessage(app.MainThread(), kMsgToggleColor, 0, 0);
        return 1;
      }
    }
  }
  return CallNextHookEx(self.kbd_hook_, code, wp, lp);
}

void SelectionManager::SyncHook() {
  bool need = selecting_ || !Application::instance().Pins().Pins().empty();
  if (need && !mouse_hook_) {
    mouse_hook_ = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc,
                                    GetModuleHandle(nullptr), 0);
  } else if (!need && mouse_hook_) {
    UnhookWindowsHookEx(mouse_hook_);
    mouse_hook_ = nullptr;
  }

  bool need_kbd = selecting_;
  if (need_kbd && !kbd_hook_) {
    kbd_hook_ = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc,
                                  GetModuleHandle(nullptr), 0);
  } else if (!need_kbd && kbd_hook_) {
    UnhookWindowsHookEx(kbd_hook_);
    kbd_hook_ = nullptr;
  }
}

void SelectionManager::BeginSelect() {
  auto& app = Application::instance();
  if (selecting_ || app.Annotation().IsAnnotating()) return;
  selecting_      = true;
  highlight_hwnd_ = nullptr;
  last_cursor_    = {-1, -1};
  SyncHook();

  app.Overlay().SetSelectionCursors();

  if (!app.Overlay().SelectDimWnd()) {
    int vs_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vs_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vs_w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vs_h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HWND dim = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
        WS_EX_LAYERED | WS_EX_TRANSPARENT,
        kRecDimClass, nullptr, WS_POPUP,
        vs_x, vs_y, vs_w, vs_h,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (dim) {
      SetLayeredWindowAttributes(dim, 0, 100, LWA_ALPHA);
      ShowWindow(dim, SW_SHOWNOACTIVATE);
    }
    app.Overlay().set_SelectDimWnd(dim);
  }

  app.GetColorPicker().Show();

  if (app.GetF1Toolbar().Toolbar()) {
    SetWindowPos(app.GetF1Toolbar().Toolbar(), HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }

  std::printf("  [F1] Window selection mode -- "
              "left-click to select, right-click to cancel.\n");

  // Immediately trigger highlight so the screen is not fully gray on entry.
  POINT cur;
  GetCursorPos(&cur);
  HandleMove(static_cast<int>(cur.x), static_cast<int>(cur.y));
}

void SelectionManager::EndSelect() {
  auto& app = Application::instance();
  selecting_       = false;
  highlight_hwnd_  = nullptr;
  select_dragging_ = false;
  app.GetColorPicker().Dismiss();
  app.Overlay().SetColor(kHighlightColor);
  app.Overlay().HideHighlight();
  app.Overlay().RestoreSystemCursors();
  if (app.Overlay().SelectDimWnd()) {
    DestroyWindow(app.Overlay().SelectDimWnd());
    app.Overlay().set_SelectDimWnd(nullptr);
  }
  SyncHook();
}

RECT SelectionManager::GetVisibleWindowRect(HWND hw) {
  RECT rc = {};
  if (FAILED(DwmGetWindowAttribute(hw, DWMWA_EXTENDED_FRAME_BOUNDS,
                                    &rc, sizeof(rc)))) {
    GetWindowRect(hw, &rc);
  }
  return rc;
}

// Returns true for desktop shell windows that should not be highlighted.
static bool IsShellWindow(HWND h) {
  if (!h) return false;
  if (h == GetDesktopWindow()) return true;
  wchar_t cls[64] = {};
  GetClassNameW(h, cls, 64);
  return wcscmp(cls, L"Progman") == 0 ||
         wcscmp(cls, L"WorkerW") == 0 ||
         wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0;
}

void SelectionManager::HandleMove(int x, int y) {
  if (!selecting_) return;
  auto& app = Application::instance();

  POINT pt = {x, y};
  HWND hw = WindowFromPoint(pt);

  auto should_skip = [&](HWND h) -> bool {
    if (!h) return false;
    if (h == app.Overlay().Overlay() ||
        h == app.Overlay().SelectDimWnd() ||
        h == app.GetColorPicker().PickerWnd() ||
        h == app.MenuHost() ||
        h == app.Recording().RecBorder())
      return true;
    if (app.GetF1Toolbar().Toolbar() &&
        (h == app.GetF1Toolbar().Toolbar() ||
         GetParent(h) == app.GetF1Toolbar().Toolbar()))
      return true;
    for (const auto& pe : app.Pins().Pins()) {
      if (h == pe.border) return true;
    }
    if (IsShellWindow(h)) return true;
    // Also skip children of shell windows (e.g. SHELLDLL_DefView,
    // SysListView32 which are children of Progman / WorkerW).
    HWND root = GetAncestor(h, GA_ROOT);
    if (root && root != h && IsShellWindow(root)) return true;
    return false;
  };

  if (should_skip(hw)) {
    HWND found = nullptr;
    HWND cur = GetTopWindow(nullptr);
    while (cur) {
      if (!should_skip(cur) && IsWindowVisible(cur)) {
        // Skip DWM-cloaked windows: UWP/Store apps and system components
        // that report as visible but are actually hidden by the compositor.
        DWORD cloaked = 0;
        DwmGetWindowAttribute(cur, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
        if (!cloaked) {
          RECT rc;
          if (GetWindowRect(cur, &rc) && PtInRect(&rc, pt)) {
            found = cur;
            break;
          }
        }
      }
      cur = GetNextWindow(cur, GW_HWNDNEXT);
    }
    if (!found) {
      HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi = {};
      mi.cbSize = sizeof(mi);
      GetMonitorInfoW(mon, &mi);
      RECT mr = mi.rcMonitor;
      if (highlight_hwnd_ != reinterpret_cast<HWND>(static_cast<uintptr_t>(-1))) {
        highlight_hwnd_ = reinterpret_cast<HWND>(static_cast<uintptr_t>(-1));
        app.Overlay().ShowHighlight(
            static_cast<int>(mr.left), static_cast<int>(mr.top),
            static_cast<int>(mr.right  - mr.left),
            static_cast<int>(mr.bottom - mr.top));
      }
      return;
    }
    hw = found;
  }

  if (IsShellWindow(hw)) {
    HWND root = hw ? GetAncestor(hw, GA_ROOT) : nullptr;
    if (!root || IsShellWindow(root)) {
      highlight_hwnd_ = nullptr;
      app.Overlay().HideHighlight();
      return;
    }
  }

  for (const auto& pe : app.Pins().Pins()) {
    if (hw == pe.border) return;
  }
  if (app.GetF1Toolbar().Toolbar() &&
      (hw == app.GetF1Toolbar().Toolbar() ||
       GetParent(hw) == app.GetF1Toolbar().Toolbar()))
    return;

  if (hw) {
    HWND root = GetAncestor(hw, GA_ROOT);
    if (root) hw = root;
    if (IsShellWindow(hw)) {
      highlight_hwnd_ = nullptr;
      app.Overlay().HideHighlight();
      return;
    }
    if (hw == highlight_hwnd_) return;
    highlight_hwnd_ = hw;
    RECT rc = GetVisibleWindowRect(hw);
    app.Overlay().ShowHighlight(static_cast<int>(rc.left), static_cast<int>(rc.top),
                  static_cast<int>(rc.right  - rc.left),
                  static_cast<int>(rc.bottom - rc.top));
  } else {
    highlight_hwnd_ = nullptr;
    app.Overlay().HideHighlight();
  }
}

bool SelectionManager::DispatchF1Mode(RECT rc) {
  auto& app = Application::instance();
  if (app.GetF1Toolbar().ActiveId() == kF1Record) {
    app.GetF1Toolbar().Dismiss();
    app.Recording().ShowSettings(rc);
    return true;
  }
  if (app.GetF1Toolbar().ActiveId() == kF1OCR) {
    app.GetF1Toolbar().Dismiss();
    PerformOcr(app, rc);
    return true;
  }
  app.GetF1Toolbar().Dismiss();
  return false;
}

void SelectionManager::HandleClick(int x, int y) {
  auto& app = Application::instance();
  EndSelect();

  POINT pt = {x, y};
  HWND hw = WindowFromPoint(pt);
  if (hw) {
    HWND root = GetAncestor(hw, GA_ROOT);
    if (root) hw = root;
  }
  if (!hw) {
    app.GetF1Toolbar().Dismiss();
    std::printf("  No window at (%d,%d).\n", x, y);
    return;
  }

  char title[256] = {};
  GetWindowTextA(hw, title, sizeof(title));
  RECT rc = GetVisibleWindowRect(hw);
  int cw = static_cast<int>(rc.right  - rc.left);
  int ch = static_cast<int>(rc.bottom - rc.top);
  std::printf("  Selected: \"%s\" (%ld,%ld) %dx%d\n",
              title, rc.left, rc.top, cw, ch);

  if (DispatchF1Mode(rc)) return;

  if (app.Captured()) { pixelgrab_image_destroy(app.Captured()); app.SetCaptured(nullptr); }
  app.SetCaptured(pixelgrab_capture_region(app.Ctx(),
      static_cast<int>(rc.left), static_cast<int>(rc.top), cw, ch));
  if (!app.Captured()) {
    std::printf("  Capture failed: %s\n",
                pixelgrab_get_last_error_message(app.Ctx()));
    return;
  }

  app.Annotation().Begin(rc);
}

void SelectionManager::HandleRegionSelect(int x1, int y1, int x2, int y2) {
  auto& app = Application::instance();
  EndSelect();

  RECT rc;
  rc.left   = static_cast<LONG>(IntMin(x1, x2));
  rc.top    = static_cast<LONG>(IntMin(y1, y2));
  rc.right  = static_cast<LONG>(IntMax(x1, x2));
  rc.bottom = static_cast<LONG>(IntMax(y1, y2));
  int w = static_cast<int>(rc.right  - rc.left);
  int h = static_cast<int>(rc.bottom - rc.top);

  std::printf("  Region: (%ld,%ld) %dx%d\n", rc.left, rc.top, w, h);

  if (DispatchF1Mode(rc)) return;

  if (app.Captured()) { pixelgrab_image_destroy(app.Captured()); app.SetCaptured(nullptr); }
  app.SetCaptured(pixelgrab_capture_region(app.Ctx(),
      static_cast<int>(rc.left), static_cast<int>(rc.top), w, h));
  if (!app.Captured()) {
    std::printf("  Capture failed: %s\n",
                pixelgrab_get_last_error_message(app.Ctx()));
    return;
  }

  app.Annotation().Begin(rc);
}

void SelectionManager::HandleCancel() {
  auto& app = Application::instance();
  EndSelect();
  app.GetF1Toolbar().Dismiss();
  std::printf("  Selection cancelled.\n");
  app.About().ShowPendingUpdate();
}

void SelectionManager::PerformOcr(Application& app, RECT rc) {
  int w = static_cast<int>(rc.right - rc.left);
  int h = static_cast<int>(rc.bottom - rc.top);
  if (w <= 0 || h <= 0) return;

  SetCursor(LoadCursor(nullptr, IDC_WAIT));

  PixelGrabImage* img = pixelgrab_capture_region(
      app.Ctx(), static_cast<int>(rc.left), static_cast<int>(rc.top), w, h);
  if (!img) {
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
    MessageBoxW(nullptr, T(kStr_MsgOCRFailed),
                L"PixelGrab", MB_OK | MB_ICONERROR | MB_TOPMOST);
    return;
  }

  char* text = nullptr;
  PixelGrabError err = pixelgrab_ocr_recognize(app.Ctx(), img, nullptr, &text);
  pixelgrab_image_destroy(img);
  SetCursor(LoadCursor(nullptr, IDC_ARROW));

  if (err != kPixelGrabOk || !text || text[0] == '\0') {
    if (text) pixelgrab_free_string(text);
    MessageBoxW(nullptr, T(kStr_MsgOCRNoText),
                L"PixelGrab", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    return;
  }

  int wlen = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
  std::vector<wchar_t> wtext(wlen);
  MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext.data(), wlen);
  pixelgrab_free_string(text);

  if (OpenClipboard(nullptr)) {
    EmptyClipboard();
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(wchar_t));
    if (hg) {
      std::memcpy(GlobalLock(hg), wtext.data(), wlen * sizeof(wchar_t));
      GlobalUnlock(hg);
      SetClipboardData(CF_UNICODETEXT, hg);
    }
    CloseClipboard();
  }

  std::wstring msg = wtext.data();
  msg += L"\n\n(";
  msg += T(kStr_MsgOCRCopied);
  msg += L")";

  // Append hint: "Yes" = translate, "No" = close.
  msg += L"\n\n[";
  msg += T(kStr_BtnTranslate);
  msg += L" → Yes]";

  int choice = MessageBoxW(nullptr, msg.c_str(), L"PixelGrab OCR",
                           MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST |
                               MB_DEFBUTTON2);
  if (choice == IDYES) {
    // Convert wtext back to UTF-8 for translation API.
    int u8len = WideCharToMultiByte(CP_UTF8, 0, wtext.data(), -1, nullptr, 0,
                                    nullptr, nullptr);
    std::string u8text(u8len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wtext.data(), -1, &u8text[0], u8len,
                        nullptr, nullptr);
    PerformTranslate(app, u8text);
  }
}

void SelectionManager::PerformTranslate(Application& app,
                                        const std::string& text) {
  if (!pixelgrab_translate_is_supported(app.Ctx())) {
    MessageBoxW(nullptr, T(kStr_MsgTranslateNotConfigured), L"PixelGrab",
                MB_OK | MB_ICONWARNING | MB_TOPMOST);
    return;
  }

  SetCursor(LoadCursor(nullptr, IDC_WAIT));

  // Auto-detect: if text contains CJK characters, translate to English;
  // otherwise translate to Chinese.
  bool has_cjk = false;
  for (size_t i = 0; i < text.size(); ++i) {
    auto c = static_cast<unsigned char>(text[i]);
    // UTF-8 CJK Unified Ideographs start with 0xE4..0xE9 first byte.
    if (c >= 0xE4 && c <= 0xE9 && i + 2 < text.size()) {
      has_cjk = true;
      break;
    }
  }
  const char* target = has_cjk ? "en" : "zh";

  char* translated = nullptr;
  PixelGrabError err = kPixelGrabOk;

  std::thread worker([&] {
    err = pixelgrab_translate_text(app.Ctx(), text.c_str(), "auto", target,
                                  &translated);
  });
  worker.join();

  SetCursor(LoadCursor(nullptr, IDC_ARROW));

  if (err != kPixelGrabOk || !translated || translated[0] == '\0') {
    if (translated) pixelgrab_free_string(translated);
    const char* detail = pixelgrab_get_last_error_message(app.Ctx());
    std::wstring errmsg = T(kStr_MsgTranslateFailed);
    if (detail && detail[0]) {
      int dlen = MultiByteToWideChar(CP_UTF8, 0, detail, -1, nullptr, 0);
      std::vector<wchar_t> wdetail(dlen);
      MultiByteToWideChar(CP_UTF8, 0, detail, -1, wdetail.data(), dlen);
      errmsg += L"\n\n";
      errmsg += wdetail.data();
    }
    MessageBoxW(nullptr, errmsg.c_str(), L"PixelGrab",
                MB_OK | MB_ICONERROR | MB_TOPMOST);
    return;
  }

  int wlen = MultiByteToWideChar(CP_UTF8, 0, translated, -1, nullptr, 0);
  std::vector<wchar_t> wtranslated(wlen);
  MultiByteToWideChar(CP_UTF8, 0, translated, -1, wtranslated.data(), wlen);
  pixelgrab_free_string(translated);

  if (OpenClipboard(nullptr)) {
    EmptyClipboard();
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(wchar_t));
    if (hg) {
      std::memcpy(GlobalLock(hg), wtranslated.data(), wlen * sizeof(wchar_t));
      GlobalUnlock(hg);
      SetClipboardData(CF_UNICODETEXT, hg);
    }
    CloseClipboard();
  }

  std::wstring msg = wtranslated.data();
  msg += L"\n\n(";
  msg += T(kStr_MsgOCRCopied);
  msg += L")";
  MessageBoxW(nullptr, msg.c_str(), L"PixelGrab", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

#endif
