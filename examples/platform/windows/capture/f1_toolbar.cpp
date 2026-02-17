// Copyright 2026 The PixelGrab Authors
// F1 toolbar (top-center mode selector).

#include "platform/windows/capture/f1_toolbar.h"

#ifdef _WIN32

#include "platform/windows/win_application.h"

void F1Toolbar::Dismiss() {
  if (toolbar_) {
    DestroyWindow(toolbar_);
    toolbar_ = nullptr;
  }
  Application::instance().About().ShowPendingUpdate();
}

void F1Toolbar::UpdateButtons() {
  if (!toolbar_) return;

  struct F1Label { int id; const wchar_t* base; };
  F1Label labels[] = {
    {kF1Capture,   T(kStr_F1Capture)},
    {kF1Record,    T(kStr_F1Record)},
    {kF1OCR,       L"OCR"},
  };

  for (const auto& l : labels) {
    HWND btn = GetDlgItem(toolbar_, l.id);
    if (!btn) continue;
    if (l.id == active_id_) {
      wchar_t buf[32];
      wsprintfW(buf, L"\x25CF%s", l.base);
      SetWindowTextW(btn, buf);
    } else {
      SetWindowTextW(btn, l.base);
    }
  }
}

// Find the nearest button at a client-area point, using inflated rects
// that cover the gaps between buttons and the top/bottom margins.
static HWND FindNearestButton(HWND toolbar, POINT pt) {
  for (HWND child = GetWindow(toolbar, GW_CHILD); child;
       child = GetWindow(child, GW_HWNDNEXT)) {
    RECT rc;
    GetWindowRect(child, &rc);
    MapWindowPoints(nullptr, toolbar, reinterpret_cast<POINT*>(&rc), 2);
    InflateRect(&rc, kF1BtnGap / 2 + 1, (kF1BarH - kF1BtnH) / 2);
    if (PtInRect(&rc, pt)) return child;
  }
  return nullptr;
}

LRESULT CALLBACK F1Toolbar::WndProc(HWND hwnd, UINT msg,
                                     WPARAM wp, LPARAM lp) {
  auto& self = Application::instance().GetF1Toolbar();
  switch (msg) {
    case WM_COMMAND: {
      int id = LOWORD(wp);
      self.active_id_ = id;
      self.UpdateButtons();
      return 0;
    }
    case WM_CLOSE:
      self.Dismiss();
      return 0;
    case WM_NCHITTEST: {
      POINTS pts = MAKEPOINTS(lp);
      POINT pt = {pts.x, pts.y};
      ScreenToClient(hwnd, &pt);
      if (FindNearestButton(hwnd, pt)) return HTCLIENT;
      return HTCAPTION;
    }
    case WM_LBUTTONDOWN: {
      // Click landed in a gap/margin (not directly on a button child).
      // Find the nearest button and fire its command.
      POINT pt = {static_cast<int>(static_cast<short>(LOWORD(lp))),
                  static_cast<int>(static_cast<short>(HIWORD(lp)))};
      HWND btn = FindNearestButton(hwnd, pt);
      if (btn) {
        SendMessage(hwnd, WM_COMMAND,
                    MAKEWPARAM(GetDlgCtrlID(btn), BN_CLICKED),
                    reinterpret_cast<LPARAM>(btn));
        return 0;
      }
      break;
    }
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

void F1Toolbar::ShowMenu() {
  auto& app = Application::instance();
  if (app.Selection().IsSelecting()) {
    app.Selection().HandleCancel();
    return;
  }

  Dismiss();

  int scr_w = GetSystemMetrics(SM_CXSCREEN);
  int bar_x = (scr_w - kF1BarW) / 2;
  int bar_y = 0;

  toolbar_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      kF1ToolbarClass, nullptr,
      WS_POPUP | WS_VISIBLE,
      bar_x, bar_y, kF1BarW, kF1BarH,
      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

  HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  int margin_y = (kF1BarH - kF1BtnH) / 2;
  int x = 8;

  struct BtnDef { int id; const wchar_t* label; };
  BtnDef defs[] = {
    {kF1Capture,   T(kStr_F1Capture)},
    {kF1Record,    T(kStr_F1Record)},
    {kF1OCR,       L"OCR"},
  };

  for (const auto& d : defs) {
    HWND btn = CreateWindowExW(
        0, L"BUTTON", d.label,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, margin_y, kF1BtnW, kF1BtnH,
        toolbar_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(d.id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessage(btn, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    x += kF1BtnW + kF1BtnGap;
  }

  UpdateButtons();
  app.Selection().BeginSelect();
}

#endif
