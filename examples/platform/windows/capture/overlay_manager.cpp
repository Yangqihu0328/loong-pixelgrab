// Copyright 2026 The PixelGrab Authors
// Overlay highlight window + cursor management.

#include "platform/windows/capture/overlay_manager.h"

#ifdef _WIN32

#include "platform/windows/win_application.h"

LRESULT CALLBACK OverlayManager::DimWndProc(HWND hwnd, UINT msg,
                                             WPARAM wp, LPARAM lp) {
  if (msg == WM_NCHITTEST) return HTTRANSPARENT;
  return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK OverlayManager::WndProc(HWND hwnd, UINT msg,
                                         WPARAM wp, LPARAM lp) {
  if (msg == WM_NCHITTEST) return HTTRANSPARENT;
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    auto& app = Application::instance();
    HBRUSH br = CreateSolidBrush(app.Overlay().Color());
    FillRect(hdc, &ps.rcPaint, br);
    DeleteObject(br);
    EndPaint(hwnd, &ps);
    return 0;
  }
  if (msg == WM_ERASEBKGND) return TRUE;
  return DefWindowProcW(hwnd, msg, wp, lp);
}

void OverlayManager::SetSelectionCursors() {
  HCURSOR hCross = LoadCursor(nullptr, IDC_CROSS);
  if (!hCross) return;
  static const DWORD kIds[] = {
    32512, 32513, 32514, 32515, 32516,
    32642, 32643, 32644, 32645, 32646,
    32648, 32649, 32650,
  };
  for (DWORD id : kIds) {
    HCURSOR hCopy = CopyCursor(hCross);
    if (hCopy) SetSystemCursor(hCopy, id);
  }
}

void OverlayManager::RestoreSystemCursors() {
  SystemParametersInfo(SPI_SETCURSORS, 0, nullptr, 0);
}

void OverlayManager::ShowHighlight(int x, int y, int w, int h) {
  if (!overlay_) return;
  if (w < kHighlightBorder * 2 || h < kHighlightBorder * 2) return;

  RECT nr = {static_cast<LONG>(x), static_cast<LONG>(y),
             static_cast<LONG>(x + w), static_cast<LONG>(y + h)};
  if (nr.left == last_highlight_.left && nr.top == last_highlight_.top &&
      nr.right == last_highlight_.right &&
      nr.bottom == last_highlight_.bottom &&
      IsWindowVisible(overlay_))
    return;
  last_highlight_ = nr;

  int brd = kHighlightBorder;

  // 1. Build a per-pixel-alpha bitmap: opaque border, transparent interior.
  //    This draws the border ON TOP of the content instead of replacing it,
  //    so the bright area always equals the full highlight rect.
  HDC hdc_screen = GetDC(nullptr);
  HDC hdc_mem = CreateCompatibleDC(hdc_screen);
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth       = w;
  bmi.bmiHeader.biHeight      = -h;  // top-down
  bmi.bmiHeader.biPlanes      = 1;
  bmi.bmiHeader.biBitCount    = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP hbmp = CreateDIBSection(hdc_mem, &bmi, DIB_RGB_COLORS,
                                  &bits, nullptr, 0);
  if (!hbmp) { DeleteDC(hdc_mem); ReleaseDC(nullptr, hdc_screen); return; }
  HBITMAP old_bmp = static_cast<HBITMAP>(SelectObject(hdc_mem, hbmp));

  // Clear to fully transparent.
  std::memset(bits, 0, static_cast<size_t>(w) * h * 4);

  // Pre-multiplied ARGB pixel for the border colour (alpha = 255).
  BYTE cr = GetRValue(overlay_color_);
  BYTE cg = GetGValue(overlay_color_);
  BYTE cb = GetBValue(overlay_color_);
  uint32_t pix = (255u << 24) | (static_cast<uint32_t>(cr) << 16) |
                 (static_cast<uint32_t>(cg) << 8) | cb;

  uint32_t* px = static_cast<uint32_t*>(bits);
  // Top rows.
  for (int r = 0; r < brd; r++)
    for (int c = 0; c < w; c++)
      px[r * w + c] = pix;
  // Bottom rows.
  for (int r = h - brd; r < h; r++)
    for (int c = 0; c < w; c++)
      px[r * w + c] = pix;
  // Left & right columns (between top/bottom strips).
  for (int r = brd; r < h - brd; r++) {
    for (int c = 0; c < brd; c++)
      px[r * w + c] = pix;
    for (int c = w - brd; c < w; c++)
      px[r * w + c] = pix;
  }

  POINT pt_src = {0, 0};
  POINT pt_dst = {x, y};
  SIZE  sz     = {w, h};
  BLENDFUNCTION blend = {};
  blend.BlendOp             = AC_SRC_OVER;
  blend.SourceConstantAlpha = 255;
  blend.AlphaFormat         = AC_SRC_ALPHA;

  // Remove any previous window region so the full bitmap is rendered.
  SetWindowRgn(overlay_, nullptr, FALSE);
  UpdateLayeredWindow(overlay_, hdc_screen, &pt_dst, &sz,
                      hdc_mem, &pt_src, 0, &blend, ULW_ALPHA);

  SelectObject(hdc_mem, old_bmp);
  DeleteObject(hbmp);
  DeleteDC(hdc_mem);
  ReleaseDC(nullptr, hdc_screen);

  // 2. Z-order: overlay just behind the F1 toolbar.
  HWND tb = Application::instance().GetF1Toolbar().Toolbar();
  SetWindowPos(overlay_, tb ? tb : HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);

  // 3. Update dim region BEFORE the overlay becomes visible, so the old
  //    dim hole does not linger during rapid highlight transitions.
  if (select_dim_wnd_) {
    auto& sel = Application::instance().Selection();
    if (sel.IsSelectDragging()) {
      if (IsWindowVisible(select_dim_wnd_))
        ShowWindow(select_dim_wnd_, SW_HIDE);
    } else {
      int vs_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
      int vs_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
      int vs_w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
      int vs_h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

      HRGN full = CreateRectRgn(0, 0, vs_w, vs_h);
      HRGN hole = CreateRectRgn(x - vs_x, y - vs_y,
                                 x + w - vs_x, y + h - vs_y);
      int rgn_type = CombineRgn(full, full, hole, RGN_DIFF);
      DeleteObject(hole);

      if (rgn_type == NULLREGION) {
        DeleteObject(full);
        if (IsWindowVisible(select_dim_wnd_))
          ShowWindow(select_dim_wnd_, SW_HIDE);
      } else {
        SetWindowRgn(select_dim_wnd_, full, TRUE);

        if (!IsWindowVisible(select_dim_wnd_))
          ShowWindow(select_dim_wnd_, SW_SHOWNOACTIVATE);

        SetWindowPos(select_dim_wnd_, overlay_, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        RedrawWindow(select_dim_wnd_, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_UPDATENOW);
      }
    }
  }

  // 4. Ensure the overlay is visible.
  if (!IsWindowVisible(overlay_))
    ShowWindow(overlay_, SW_SHOWNOACTIVATE);
}

void OverlayManager::HideHighlight() {
  auto& app = Application::instance();
  if (overlay_) {
    ShowWindow(overlay_, SW_HIDE);
  }
  if (select_dim_wnd_) {
    if (app.Selection().IsSelecting()) {
      SetWindowRgn(select_dim_wnd_, nullptr, TRUE);
    } else {
      ShowWindow(select_dim_wnd_, SW_HIDE);
    }
  }
  std::memset(&last_highlight_, 0, sizeof(last_highlight_));
}

void OverlayManager::SetColor(COLORREF color) {
  overlay_color_ = color;
  if (overlay_ && IsWindowVisible(overlay_)) {
    RECT r = last_highlight_;
    std::memset(&last_highlight_, 0, sizeof(last_highlight_));
    ShowHighlight(static_cast<int>(r.left), static_cast<int>(r.top),
                   static_cast<int>(r.right - r.left),
                   static_cast<int>(r.bottom - r.top));
  }
}

void OverlayManager::ResetState() {
  std::memset(&last_highlight_, 0, sizeof(last_highlight_));
}

#endif
