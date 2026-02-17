// Copyright 2026 The PixelGrab Authors
// Color picker overlay — magnifier + coordinate / color display.

#include "platform/windows/capture/color_picker.h"

#ifdef _WIN32

#include "core/i18n.h"
#include "platform/windows/win_application.h"

static constexpr int kPkMagRadius = 8;
static constexpr int kPkMagZoom   = 10;
static constexpr int kPkMagDraw   = kPkMagRadius * 2 * kPkMagZoom;  // 160
static constexpr int kPkPad       = 6;
static constexpr int kPkRowH      = 18;
static constexpr int kPkSepGap    = 5;
static constexpr int kPkHintH     = 16;
static constexpr int kPkW         = kPkMagDraw;                     // 160
static constexpr int kPkH         = kPkMagDraw + kPkSepGap
                                    + kPkRowH * 2 + kPkSepGap
                                    + kPkHintH + kPkPad;            // 228
static constexpr UINT_PTR kPkTimerId = 10;

// Grid/separator visual colors
static constexpr COLORREF kPkBg       = RGB(30, 30, 30);
static constexpr COLORREF kPkGrid     = RGB(55, 55, 55);
static constexpr COLORREF kPkSep      = RGB(60, 60, 60);
static constexpr COLORREF kPkText     = RGB(230, 230, 230);
static constexpr COLORREF kPkDim      = RGB(140, 140, 140);
static constexpr COLORREF kPkHint     = RGB(100, 100, 100);

LRESULT CALLBACK ColorPicker::WndProc(HWND hwnd, UINT msg,
                                       WPARAM wp, LPARAM lp) {
  auto& self = Application::instance().GetColorPicker();
  if (msg == WM_NCHITTEST) return HTTRANSPARENT;
  switch (msg) {
    case WM_TIMER:
      if (wp == kPkTimerId &&
          !Application::instance().Selection().IsSelectDragging()) {
        self.UpdateAtCursor();
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;

    case WM_ERASEBKGND:
      return TRUE;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);

      HDC memDC = CreateCompatibleDC(hdc);
      HBITMAP memBmp = CreateCompatibleBitmap(hdc, kPkW, kPkH);
      HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

      self.Paint(memDC);

      BitBlt(hdc, 0, 0, kPkW, kPkH, memDC, 0, 0, SRCCOPY);

      SelectObject(memDC, oldBmp);
      DeleteObject(memBmp);
      DeleteDC(memDC);

      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_CLOSE:
      self.Dismiss();
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

void ColorPicker::Show() {
  if (active_) return;
  active_   = true;
  show_hex_ = false;

  HINSTANCE hInst = GetModuleHandleW(nullptr);
  picker_wnd_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
      L"PGColorPicker", nullptr,
      WS_POPUP | WS_VISIBLE,
      0, 0, kPkW, kPkH,
      nullptr, nullptr, hInst, nullptr);

  SetTimer(picker_wnd_, kPkTimerId, 30, nullptr);
  UpdateAtCursor();

  std::printf("  [ColorPicker] Active. Ctrl+C copy, Shift toggle, Esc cancel.\n");
}

void ColorPicker::Dismiss() {
  if (!active_) return;
  active_ = false;
  if (picker_wnd_) {
    KillTimer(picker_wnd_, kPkTimerId);
    DestroyWindow(picker_wnd_);
    picker_wnd_ = nullptr;
  }
}

void ColorPicker::CopyColor() {
  if (!active_ || !picker_wnd_) return;
  const char* text = show_hex_ ? hex_buf_ : rgb_buf_;
  if (OpenClipboard(picker_wnd_)) {
    EmptyClipboard();
    size_t len = std::strlen(text) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hMem) {
      char* p = static_cast<char*>(GlobalLock(hMem));
      std::memcpy(p, text, len);
      GlobalUnlock(hMem);
      SetClipboardData(CF_TEXT, hMem);
    }
    CloseClipboard();
  }
  std::printf("  Color copied: %s\n", text);
}

void ColorPicker::ToggleDisplay() {
  if (!active_) return;
  show_hex_ = !show_hex_;
  if (picker_wnd_)
    InvalidateRect(picker_wnd_, nullptr, FALSE);
}

void ColorPicker::UpdateAtCursor() {
  auto& app = Application::instance();
  POINT pt;
  GetCursorPos(&pt);
  cursor_x_ = pt.x;
  cursor_y_ = pt.y;

  pixelgrab_pick_color(app.Ctx(), cursor_x_, cursor_y_, &cur_color_);
  pixelgrab_color_rgb_to_hsv(&cur_color_, &cur_hsv_);
  pixelgrab_color_to_hex(&cur_color_, hex_buf_, sizeof(hex_buf_), 0);
  std::snprintf(rgb_buf_, sizeof(rgb_buf_), "RGB(%d,%d,%d)",
                cur_color_.r, cur_color_.g, cur_color_.b);

  int scr_w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  int scr_h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  int scr_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
  int scr_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
  int wx = cursor_x_ + 20;
  int wy = cursor_y_ + 20;
  if (wx + kPkW > scr_x + scr_w) wx = cursor_x_ - kPkW - 10;
  if (wy + kPkH > scr_y + scr_h) wy = cursor_y_ - kPkH - 10;
  SetWindowPos(picker_wnd_, HWND_TOPMOST, wx, wy, kPkW, kPkH,
               SWP_NOACTIVATE | SWP_NOREDRAW);
}

void ColorPicker::Paint(HDC hdc) {
  auto& app = Application::instance();

  // ── Background ──
  RECT rc = {0, 0, kPkW, kPkH};
  HBRUSH bg = CreateSolidBrush(kPkBg);
  FillRect(hdc, &rc, bg);
  DeleteObject(bg);

  // ── Magnifier (full width, flush at top) ──
  PixelGrabImage* mag = pixelgrab_get_magnifier(
      app.Ctx(), cursor_x_, cursor_y_, kPkMagRadius, kPkMagZoom);
  if (mag) {
    int mw = pixelgrab_image_get_width(mag);
    int mh = pixelgrab_image_get_height(mag);
    const uint8_t* data = pixelgrab_image_get_data(mag);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = mw;
    bmi.bmiHeader.biHeight      = -mh;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(hdc,
                  0, 0, kPkMagDraw, kPkMagDraw,
                  0, 0, mw, mh,
                  data, &bmi, DIB_RGB_COLORS, SRCCOPY);

    pixelgrab_image_destroy(mag);
  }

  // ── 2x2 grid (one vertical + one horizontal through center) ──
  int cx = kPkMagDraw / 2;
  int cy = kPkMagDraw / 2;
  int half = kPkMagZoom / 2;

  HPEN gridPen = CreatePen(PS_SOLID, 1, kPkGrid);
  HPEN oldPen  = static_cast<HPEN>(SelectObject(hdc, gridPen));
  MoveToEx(hdc, cx, 0, nullptr);
  LineTo(hdc, cx, kPkMagDraw);
  MoveToEx(hdc, 0, cy, nullptr);
  LineTo(hdc, kPkMagDraw, cy);
  SelectObject(hdc, oldPen);
  DeleteObject(gridPen);

  // ── Crosshair: highlight center pixel (black outer + white inner) ──
  HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));

  HPEN darkPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
  oldPen = static_cast<HPEN>(SelectObject(hdc, darkPen));
  HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
  Rectangle(hdc, cx - half - 1, cy - half - 1, cx + half + 2, cy + half + 2);
  SelectObject(hdc, oldBr);
  SelectObject(hdc, oldPen);
  DeleteObject(darkPen);

  HPEN whitePen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
  oldPen = static_cast<HPEN>(SelectObject(hdc, whitePen));
  oldBr  = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
  Rectangle(hdc, cx - half, cy - half, cx + half + 1, cy + half + 1);
  SelectObject(hdc, oldBr);
  SelectObject(hdc, oldPen);
  DeleteObject(whitePen);

  // ── Black border around magnifier ──
  HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
  oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
  oldBr  = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
  Rectangle(hdc, 0, 0, kPkMagDraw, kPkMagDraw);
  SelectObject(hdc, oldBr);
  SelectObject(hdc, oldPen);
  DeleteObject(borderPen);

  // ── Separator 1 (below magnifier) ──
  int sep1_y = kPkMagDraw + kPkSepGap / 2;
  HPEN sepPen = CreatePen(PS_SOLID, 1, kPkSep);
  oldPen = static_cast<HPEN>(SelectObject(hdc, sepPen));
  MoveToEx(hdc, kPkPad, sep1_y, nullptr);
  LineTo(hdc, kPkW - kPkPad, sep1_y);

  // ── Text rows ──
  int ty = kPkMagDraw + kPkSepGap;

  SetBkMode(hdc, TRANSPARENT);
  HFONT font = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                            L"Consolas");
  HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));

  wchar_t wbuf[64];

  // Row 1: coordinates
  _snwprintf_s(wbuf, _TRUNCATE, T(kStr_PkCoordFmt), cursor_x_, cursor_y_);
  SetTextColor(hdc, kPkText);
  RECT r1 = {kPkPad, ty, kPkW - kPkPad, ty + kPkRowH};
  DrawTextW(hdc, wbuf, -1, &r1, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

  // Row 2: ■ + RGB/HEX
  ty += kPkRowH;
  int sw_y = ty + (kPkRowH - 10) / 2;
  RECT swatch = {kPkPad, sw_y, kPkPad + 10, sw_y + 10};
  HBRUSH swBr = CreateSolidBrush(
      RGB(cur_color_.r, cur_color_.g, cur_color_.b));
  FillRect(hdc, &swatch, swBr);
  DeleteObject(swBr);
  FrameRect(hdc, &swatch,
            static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

  if (show_hex_) {
    wchar_t hex_w[16];
    MultiByteToWideChar(CP_UTF8, 0, hex_buf_, -1, hex_w, 16);
    _snwprintf_s(wbuf, _TRUNCATE, T(kStr_PkHEXFmt), hex_w);
  } else {
    _snwprintf_s(wbuf, _TRUNCATE, T(kStr_PkRGBFmt),
                 cur_color_.r, cur_color_.g, cur_color_.b);
  }
  SetTextColor(hdc, kPkText);
  RECT r2 = {kPkPad + 14, ty, kPkW - kPkPad, ty + kPkRowH};
  DrawTextW(hdc, wbuf, -1, &r2, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

  // ── Separator 2 ──
  ty += kPkRowH;
  int sep2_y = ty + kPkSepGap / 2;
  MoveToEx(hdc, kPkPad, sep2_y, nullptr);
  LineTo(hdc, kPkW - kPkPad, sep2_y);

  SelectObject(hdc, oldPen);
  DeleteObject(sepPen);

  // Row 3: hint (merged single row)
  ty += kPkSepGap;
  HFONT hintFont = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                L"Consolas");
  SelectObject(hdc, hintFont);
  SetTextColor(hdc, kPkText);
  RECT r3 = {kPkPad, ty, kPkW - kPkPad, ty + kPkHintH};
  DrawTextW(hdc, T(kStr_PkHint), -1, &r3,
            DT_CENTER | DT_SINGLELINE | DT_VCENTER);

  SelectObject(hdc, oldFont);
  DeleteObject(hintFont);
  DeleteObject(font);
}

#endif
