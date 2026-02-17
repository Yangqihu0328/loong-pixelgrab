// Copyright 2026 The PixelGrab Authors
// Canvas window, toolbar, text dialog, and annotation helpers.

#include "platform/windows/capture/annotation_manager.h"

#ifdef _WIN32

#include "platform/windows/win_application.h"

// Edge bitmask for canvas resize hit-testing.
enum {
  kResizeNone   = 0,
  kResizeLeft   = 1,
  kResizeRight  = 2,
  kResizeTop    = 4,
  kResizeBottom = 8,
};

static int CanvasHitTestEdge(int cx, int cy, int cw, int ch) {
  const int t = kEdgeThreshold;
  int edge = kResizeNone;
  if (cx < t)      edge |= kResizeLeft;
  if (cx >= cw - t) edge |= kResizeRight;
  if (cy < t)      edge |= kResizeTop;
  if (cy >= ch - t) edge |= kResizeBottom;
  return edge;
}

LRESULT CALLBACK AnnotationManager::CanvasWndProc(HWND hwnd, UINT msg,
                                                    WPARAM wp, LPARAM lp) {
  auto& app = Application::instance();
  auto& self = app.Annotation();
  switch (msg) {
    case WM_PAINT: {
      if (!self.ann_) break;
      const PixelGrabImage* result = pixelgrab_annotation_get_result(self.ann_);
      if (!result) break;

      int img_w = pixelgrab_image_get_width(result);
      int img_h = pixelgrab_image_get_height(result);
      const uint8_t* data = pixelgrab_image_get_data(result);
      if (!data || img_w <= 0 || img_h <= 0) break;

      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);

      RECT cr;
      GetClientRect(hwnd, &cr);
      int cli_w = static_cast<int>(cr.right);
      int cli_h = static_cast<int>(cr.bottom);
      if (cli_w <= 0) cli_w = img_w;
      if (cli_h <= 0) cli_h = img_h;

      HDC mem_dc = CreateCompatibleDC(hdc);
      HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, cli_w, cli_h);
      HGDIOBJ old_bmp = SelectObject(mem_dc, mem_bmp);

      {
        HBRUSH bg = CreateSolidBrush(RGB(48, 48, 48));
        FillRect(mem_dc, &cr, bg);
        DeleteObject(bg);
      }

      {
        BITMAPINFO bmi;
        std::memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize        = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth       = img_w;
        bmi.bmiHeader.biHeight      = -img_h;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        SetDIBitsToDevice(mem_dc, 0, 0,
                           static_cast<DWORD>(img_w),
                           static_cast<DWORD>(img_h),
                           0, 0, 0, static_cast<UINT>(img_h),
                           data, &bmi, DIB_RGB_COLORS);
      }

      if (self.dragging_ && !self.canvas_resizing_ && self.current_tool_ != kToolText) {
        HPEN pen = CreatePen(PS_DASH, IntMax(1, static_cast<int>(self.current_width_)),
                             ArgbToColorref(self.current_color_));
        HGDIOBJ old_pen = SelectObject(mem_dc, pen);
        HGDIOBJ old_br  = SelectObject(mem_dc, GetStockObject(NULL_BRUSH));

        int x1 = self.drag_start_.x, y1 = self.drag_start_.y;
        int x2 = self.drag_end_.x,   y2 = self.drag_end_.y;

        switch (self.current_tool_) {
          case kToolRect:
          case kToolMosaic:
          case kToolBlur:
            Rectangle(mem_dc, IntMin(x1, x2), IntMin(y1, y2),
                      IntMax(x1, x2), IntMax(y1, y2));
            break;
          case kToolEllipse:
            Ellipse(mem_dc, IntMin(x1, x2), IntMin(y1, y2),
                    IntMax(x1, x2), IntMax(y1, y2));
            break;
          case kToolArrow:
          case kToolLine:
            MoveToEx(mem_dc, x1, y1, nullptr);
            LineTo(mem_dc, x2, y2);
            break;
          default:
            break;
        }

        SelectObject(mem_dc, old_pen);
        SelectObject(mem_dc, old_br);
        DeleteObject(pen);
      }

      {
        HBRUSH br = CreateSolidBrush(kConfirmColor);
        int b = kHighlightBorder;
        RECT rt = {0, 0, cli_w, b};
        RECT rb = {0, cli_h - b, cli_w, cli_h};
        RECT rl = {0, b, b, cli_h - b};
        RECT rr = {cli_w - b, b, cli_w, cli_h - b};
        FillRect(mem_dc, &rt, br);
        FillRect(mem_dc, &rb, br);
        FillRect(mem_dc, &rl, br);
        FillRect(mem_dc, &rr, br);
        DeleteObject(br);
      }

      {
        const int hs = kHandleSize, hh = hs / 2;
        HBRUSH fill = CreateSolidBrush(kHandleFill);
        HPEN   pen  = CreatePen(PS_SOLID, 1, kHandleBorder);
        HGDIOBJ oldBr2  = SelectObject(mem_dc, fill);
        HGDIOBJ oldPen2 = SelectObject(mem_dc, pen);

        const int cxp[] = {0,       cli_w/2, cli_w-1, cli_w-1,
                          cli_w-1, cli_w/2, 0,       0};
        const int cyp[] = {0,       0,       0,       cli_h/2,
                          cli_h-1, cli_h-1, cli_h-1, cli_h/2};
        for (int i = 0; i < 8; ++i) {
          int lx = cxp[i] - hh, ly = cyp[i] - hh;
          Rectangle(mem_dc, lx, ly, lx + hs, ly + hs);
        }

        SelectObject(mem_dc, oldPen2);
        SelectObject(mem_dc, oldBr2);
        DeleteObject(pen);
        DeleteObject(fill);
      }

      BitBlt(hdc, 0, 0, cli_w, cli_h, mem_dc, 0, 0, SRCCOPY);

      SelectObject(mem_dc, old_bmp);
      DeleteObject(mem_bmp);
      DeleteDC(mem_dc);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_LBUTTONDOWN: {
      int mx = LParamX(lp);
      int my = LParamY(lp);

      RECT cr;
      GetClientRect(hwnd, &cr);
      int edge = CanvasHitTestEdge(mx, my,
                                    static_cast<int>(cr.right),
                                    static_cast<int>(cr.bottom));
      if (edge != kResizeNone) {
        self.canvas_resizing_    = true;
        self.canvas_resize_edge_ = edge;
        POINT spt;
        GetCursorPos(&spt);
        self.canvas_resize_start_ = spt;
        GetWindowRect(hwnd, &self.canvas_resize_orig_);
        SetCapture(hwnd);

        SetWindowPos(hwnd, nullptr, -32000, -32000, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        if (self.toolbar_wnd_) ShowWindow(self.toolbar_wnd_, SW_HIDE);

        int ow = static_cast<int>(self.canvas_resize_orig_.right  - self.canvas_resize_orig_.left);
        int oh = static_cast<int>(self.canvas_resize_orig_.bottom - self.canvas_resize_orig_.top);
        app.Overlay().ShowHighlight(static_cast<int>(self.canvas_resize_orig_.left),
                      static_cast<int>(self.canvas_resize_orig_.top), ow, oh);
        if (app.Overlay().Overlay())
          SetWindowPos(app.Overlay().Overlay(), HWND_TOP, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return 0;
      }

      if (self.current_tool_ == kToolText) {
        int sx = static_cast<int>(self.canvas_rect_.left) + mx;
        int sy = static_cast<int>(self.canvas_rect_.top)  + my;
        char buf[256] = {};
        if (self.PromptText(sx, sy, buf, sizeof(buf))) {
          pixelgrab_annotation_add_text(self.ann_, mx, my, buf,
                                        "Arial", self.current_font_size_, self.current_color_);
          InvalidateRect(self.canvas_, nullptr, FALSE);
          self.UpdateToolbarButtons();
        }
      } else {
        self.drag_start_.x = mx;
        self.drag_start_.y = my;
        self.drag_end_     = self.drag_start_;
        self.dragging_     = true;
        SetCapture(hwnd);
      }
      return 0;
    }

    case WM_MOUSEMOVE:
      if (self.canvas_resizing_) {
        POINT spt;
        GetCursorPos(&spt);
        int dx = spt.x - self.canvas_resize_start_.x;
        int dy = spt.y - self.canvas_resize_start_.y;
        RECT nr = self.canvas_resize_orig_;
        if (self.canvas_resize_edge_ & kResizeLeft)   nr.left   += dx;
        if (self.canvas_resize_edge_ & kResizeRight)  nr.right  += dx;
        if (self.canvas_resize_edge_ & kResizeTop)    nr.top    += dy;
        if (self.canvas_resize_edge_ & kResizeBottom) nr.bottom += dy;
        if (nr.right - nr.left < 30) {
          if (self.canvas_resize_edge_ & kResizeLeft)  nr.left  = nr.right - 30;
          else                                         nr.right = nr.left  + 30;
        }
        if (nr.bottom - nr.top < 30) {
          if (self.canvas_resize_edge_ & kResizeTop)   nr.top    = nr.bottom - 30;
          else                                         nr.bottom = nr.top    + 30;
        }
        int ow = static_cast<int>(nr.right  - nr.left);
        int oh = static_cast<int>(nr.bottom - nr.top);
        app.Overlay().ShowHighlight(static_cast<int>(nr.left),
                      static_cast<int>(nr.top), ow, oh);
        if (app.Overlay().Overlay())
          SetWindowPos(app.Overlay().Overlay(), HWND_TOP, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return 0;
      }
      if (self.dragging_) {
        self.drag_end_.x = LParamX(lp);
        self.drag_end_.y = LParamY(lp);
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;

    case WM_LBUTTONUP:
      if (self.canvas_resizing_) {
        self.canvas_resizing_ = false;
        ReleaseCapture();

        POINT spt;
        GetCursorPos(&spt);
        int dx = spt.x - self.canvas_resize_start_.x;
        int dy = spt.y - self.canvas_resize_start_.y;
        RECT nr = self.canvas_resize_orig_;
        if (self.canvas_resize_edge_ & kResizeLeft)   nr.left   += dx;
        if (self.canvas_resize_edge_ & kResizeRight)  nr.right  += dx;
        if (self.canvas_resize_edge_ & kResizeTop)    nr.top    += dy;
        if (self.canvas_resize_edge_ & kResizeBottom) nr.bottom += dy;
        if (nr.right - nr.left < 30) {
          if (self.canvas_resize_edge_ & kResizeLeft)  nr.left  = nr.right - 30;
          else                                         nr.right = nr.left  + 30;
        }
        if (nr.bottom - nr.top < 30) {
          if (self.canvas_resize_edge_ & kResizeTop)   nr.top    = nr.bottom - 30;
          else                                         nr.bottom = nr.top    + 30;
        }

        if (app.Overlay().Overlay()) ShowWindow(app.Overlay().Overlay(), SW_HIDE);
        app.Overlay().ResetState();

        int nw = static_cast<int>(nr.right  - nr.left);
        int nh = static_cast<int>(nr.bottom - nr.top);

        if (app.Captured()) { pixelgrab_image_destroy(app.Captured()); app.SetCaptured(nullptr); }
        app.SetCaptured(pixelgrab_capture_region(app.Ctx(),
            static_cast<int>(nr.left), static_cast<int>(nr.top), nw, nh));

        if (self.ann_) { pixelgrab_annotation_destroy(self.ann_); self.ann_ = nullptr; }
        if (app.Captured()) {
          self.ann_ = pixelgrab_annotation_create(app.Ctx(), app.Captured());
        }

        self.canvas_rect_ = nr;
        ShowWindow(hwnd, SW_HIDE);
        SetWindowPos(hwnd, nullptr,
                     static_cast<int>(nr.left), static_cast<int>(nr.top),
                     nw, nh, SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);

        if (self.toolbar_wnd_) {
          RECT tbr;
          GetWindowRect(self.toolbar_wnd_, &tbr);
          int tbw = static_cast<int>(tbr.right - tbr.left);
          int tby = static_cast<int>(nr.bottom);
          int st = GetSystemMetrics(SM_YVIRTUALSCREEN);
          int sb = st + GetSystemMetrics(SM_CYVIRTUALSCREEN);
          if (tby + kToolbarH > sb) {
            tby = static_cast<int>(nr.top) - kToolbarH;
            if (tby < st) tby = sb - kToolbarH;
          }
          SetWindowPos(self.toolbar_wnd_, nullptr,
                       static_cast<int>(nr.left), tby,
                       tbw, kToolbarH, SWP_NOZORDER | SWP_NOACTIVATE);
          ShowWindow(self.toolbar_wnd_, SW_SHOWNOACTIVATE);
          if (self.color_panel_wnd_) {
            self.HideColorPanel();
            self.ShowColorPanel();
          }
        }

        self.UpdateDimRegion();
        self.RaiseToolbar();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (self.dragging_) {
        self.dragging_ = false;
        ReleaseCapture();
        int x2 = LParamX(lp);
        int y2 = LParamY(lp);
        if (IntAbs(x2 - self.drag_start_.x) > 2 ||
            IntAbs(y2 - self.drag_start_.y) > 2) {
          self.CommitShape(self.drag_start_.x, self.drag_start_.y, x2, y2);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        self.RaiseToolbar();
      }
      return 0;

    case WM_LBUTTONDBLCLK:
      self.CopyAnnotation();
      return 0;

    case WM_RBUTTONDOWN:
      if (self.ann_ && pixelgrab_annotation_can_undo(self.ann_)) {
        pixelgrab_annotation_undo(self.ann_);
        InvalidateRect(hwnd, nullptr, FALSE);
        self.UpdateToolbarButtons();
      } else {
        self.Cancel();
      }
      return 0;

    case WM_KEYDOWN:
      if (wp == VK_ESCAPE) {
        self.Cancel();
        return 0;
      }
      break;

    case WM_SETCURSOR: {
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hwnd, &pt);
      RECT cr2;
      GetClientRect(hwnd, &cr2);
      int edge = CanvasHitTestEdge(pt.x, pt.y,
                                    static_cast<int>(cr2.right),
                                    static_cast<int>(cr2.bottom));
      LPCSTR cur = IDC_CROSS;
      if (edge == (kResizeTop | kResizeLeft) ||
          edge == (kResizeBottom | kResizeRight))
        cur = IDC_SIZENWSE;
      else if (edge == (kResizeTop | kResizeRight) ||
               edge == (kResizeBottom | kResizeLeft))
        cur = IDC_SIZENESW;
      else if (edge & (kResizeLeft | kResizeRight))
        cur = IDC_SIZEWE;
      else if (edge & (kResizeTop | kResizeBottom))
        cur = IDC_SIZENS;
      SetCursor(LoadCursorA(nullptr, cur));
      return TRUE;
    }

    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK AnnotationManager::ToolbarWndProc(HWND hwnd, UINT msg,
                                                     WPARAM wp, LPARAM lp) {
  auto& app = Application::instance();
  auto& self = app.Annotation();
  if (msg == WM_COMMAND) {
    int id = LOWORD(wp);
    switch (id) {
      case kBtnRect:    self.current_tool_ = kToolRect;    break;
      case kBtnEllipse: self.current_tool_ = kToolEllipse; break;
      case kBtnArrow:   self.current_tool_ = kToolArrow;   break;
      case kBtnLine:    self.current_tool_ = kToolLine;    break;
      case kBtnText:    self.current_tool_ = kToolText;    break;
      case kBtnMosaic:  self.current_tool_ = kToolMosaic;  break;
      case kBtnBlur:    self.current_tool_ = kToolBlur;    break;

      case kBtnUndo:
        if (self.ann_ && pixelgrab_annotation_can_undo(self.ann_)) {
          pixelgrab_annotation_undo(self.ann_);
          InvalidateRect(self.canvas_, nullptr, FALSE);
        }
        break;
      case kBtnRedo:
        if (self.ann_ && pixelgrab_annotation_can_redo(self.ann_)) {
          pixelgrab_annotation_redo(self.ann_);
          InvalidateRect(self.canvas_, nullptr, FALSE);
        }
        break;
      case kBtnPin:
        self.PinAnnotation();
        break;
      case kBtnCopy:
        self.CopyAnnotation();
        break;
      case kBtnCancel:
        self.Cancel();
        break;

      default:
        break;
    }
    self.UpdateToolbarButtons();
    if (self.canvas_) {
      SetFocus(self.canvas_);
      self.RaiseToolbar();
    }
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

void AnnotationManager::CreateToolbarButtons() {
  if (!toolbar_wnd_) return;

  struct BtnDef { int id; const wchar_t* label; bool is_tool; };
  BtnDef defs[] = {
    {kBtnRect,    T(kStr_ToolRect),    true},
    {kBtnEllipse, T(kStr_ToolEllipse), true},
    {kBtnArrow,   T(kStr_ToolArrow),   true},
    {kBtnLine,    T(kStr_ToolLine),    true},
    {kBtnText,    T(kStr_ToolText),    true},
    {kBtnMosaic,  T(kStr_ToolMosaic),  true},
    {kBtnBlur,    T(kStr_ToolBlur),    true},
    {kBtnUndo,    T(kStr_ToolUndo),    false},
    {kBtnRedo,    T(kStr_ToolRedo),    false},
    {kBtnPin,     T(kStr_ToolPin),     false},
    {kBtnCopy,    T(kStr_ToolCopy),    false},
    {kBtnCancel,  T(kStr_ToolCancel),  false},
  };

  HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  int x = kBtnGap;
  bool prev_is_tool = true;

  for (const auto& d : defs) {
    if (prev_is_tool && !d.is_tool) x += kSepGap;
    prev_is_tool = d.is_tool;

    HWND btn = CreateWindowExW(
        0, L"BUTTON", d.label,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, kBtnMarginY, kBtnW, kBtnH,
        toolbar_wnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(d.id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessage(btn, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    x += kBtnW + kBtnGap;
  }
}

void AnnotationManager::UpdateToolbarButtons() {
  if (!toolbar_wnd_) return;

  int active_id = kBtnRect + static_cast<int>(current_tool_);

  struct ToolLabel { int id; const wchar_t* base; };
  ToolLabel tools[] = {
    {kBtnRect,    T(kStr_ToolRect)},
    {kBtnEllipse, T(kStr_ToolEllipse)},
    {kBtnArrow,   T(kStr_ToolArrow)},
    {kBtnLine,    T(kStr_ToolLine)},
    {kBtnText,    T(kStr_ToolText)},
    {kBtnMosaic,  T(kStr_ToolMosaic)},
    {kBtnBlur,    T(kStr_ToolBlur)},
  };
  for (const auto& t : tools) {
    HWND btn = GetDlgItem(toolbar_wnd_, t.id);
    if (!btn) continue;
    if (t.id == active_id) {
      wchar_t buf[32];
      wsprintfW(buf, L"\x25CF%s", t.base);
      SetWindowTextW(btn, buf);
    } else {
      SetWindowTextW(btn, t.base);
    }
  }

  HWND undo_btn = GetDlgItem(toolbar_wnd_, kBtnUndo);
  HWND redo_btn = GetDlgItem(toolbar_wnd_, kBtnRedo);
  if (undo_btn)
    EnableWindow(undo_btn, ann_ && pixelgrab_annotation_can_undo(ann_));
  if (redo_btn)
    EnableWindow(redo_btn, ann_ && pixelgrab_annotation_can_redo(ann_));

  // Auto-show/hide property bubble based on tool type
  bool needs_props = (current_tool_ >= kToolRect && current_tool_ <= kToolText);
  if (needs_props) {
    if (!color_panel_wnd_)
      ShowColorPanel();
    else
      InvalidateRect(color_panel_wnd_, nullptr, FALSE);
  } else {
    HideColorPanel();
  }
}

void AnnotationManager::Begin(RECT rc) {
  auto& app = Application::instance();
  int img_w = pixelgrab_image_get_width(app.Captured());
  int img_h = pixelgrab_image_get_height(app.Captured());
  std::printf("  Captured %dx%d -- entering annotation mode.\n", img_w, img_h);

  ann_ = pixelgrab_annotation_create(app.Ctx(), app.Captured());
  if (!ann_) {
    std::printf("  Annotation create failed: %s\n",
                pixelgrab_get_last_error_message(app.Ctx()));
    app.Overlay().HideHighlight();
    app.Overlay().SetColor(kHighlightColor);
    return;
  }

  canvas_rect_ = rc;

  canvas_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
      kCanvasClass, nullptr,
      WS_POPUP | WS_VISIBLE,
      static_cast<int>(rc.left), static_cast<int>(rc.top),
      img_w, img_h,
      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

  int toolbar_w = 7 * (kBtnW + kBtnGap) + kSepGap
                + 2 * (kBtnW + kBtnGap) + kSepGap
                + 3 * (kBtnW + kBtnGap) + kBtnGap;
  int toolbar_x = static_cast<int>(rc.left);
  int toolbar_y = static_cast<int>(rc.top) + img_h;

  int scr_top    = GetSystemMetrics(SM_YVIRTUALSCREEN);
  int scr_bottom = scr_top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (toolbar_y + kToolbarH > scr_bottom) {
    toolbar_y = static_cast<int>(rc.top) - kToolbarH;
    if (toolbar_y < scr_top)
      toolbar_y = scr_bottom - kToolbarH;
  }

  toolbar_wnd_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
      kToolbarClass, nullptr,
      WS_POPUP | WS_VISIBLE,
      toolbar_x, toolbar_y, toolbar_w, kToolbarH,
      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  CreateToolbarButtons();

  if (app.Overlay().Overlay()) ShowWindow(app.Overlay().Overlay(), SW_HIDE);
  app.Overlay().ResetState();
  app.Overlay().SetColor(kConfirmColor);

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
    if (dim)
      SetLayeredWindowAttributes(dim, 0, 100, LWA_ALPHA);
    app.Overlay().set_SelectDimWnd(dim);
  }
  if (app.Overlay().SelectDimWnd()) {
    UpdateDimRegion();
  }

  annotating_   = true;
  current_tool_ = kToolRect;
  dragging_     = false;
  SetFocus(canvas_);

  if (toolbar_wnd_) {
    SetWindowPos(toolbar_wnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
  UpdateToolbarButtons();

  std::printf("  Toolbar ready. Draw on canvas, right-click to undo.\n"
              "  [%s] pin  [%s] copy  [%s/%s] cancel\n",
              "\xe8\xb4\xb4\xe5\x9b\xbe",
              "\xe5\xa4\x8d\xe5\x88\xb6",
              "\xe5\x8f\x96\xe6\xb6\x88",
              "Esc");
}

void AnnotationManager::CommitShape(int x1, int y1, int x2, int y2) {
  if (!ann_) return;

  PixelGrabShapeStyle style;
  std::memset(&style, 0, sizeof(style));
  style.stroke_color = current_color_;
  style.fill_color   = 0;
  style.stroke_width = current_width_;
  style.filled       = 0;

  int left   = IntMin(x1, x2);
  int top    = IntMin(y1, y2);
  int right  = IntMax(x1, x2);
  int bottom = IntMax(y1, y2);
  int w      = right - left;
  int h      = bottom - top;

  switch (current_tool_) {
    case kToolRect:
      pixelgrab_annotation_add_rect(ann_, left, top, w, h, &style);
      break;
    case kToolEllipse: {
      int cx = (x1 + x2) / 2;
      int cy = (y1 + y2) / 2;
      int rx = w / 2;
      int ry = h / 2;
      pixelgrab_annotation_add_ellipse(ann_, cx, cy, rx, ry, &style);
      break;
    }
    case kToolArrow:
      pixelgrab_annotation_add_arrow(ann_, x1, y1, x2, y2,
                                      kArrowHead, &style);
      break;
    case kToolLine:
      pixelgrab_annotation_add_line(ann_, x1, y1, x2, y2, &style);
      break;
    case kToolMosaic:
      if (w > 0 && h > 0)
        pixelgrab_annotation_add_mosaic(ann_, left, top, w, h, kMosaicBlock);
      break;
    case kToolBlur:
      if (w > 0 && h > 0)
        pixelgrab_annotation_add_blur(ann_, left, top, w, h, kBlurRadius);
      break;
    case kToolText:
      break;
  }
  UpdateToolbarButtons();
}

LRESULT CALLBACK AnnotationManager::TextDlgWndProc(HWND hwnd, UINT msg,
                                                     WPARAM wp, LPARAM lp) {
  auto& self = Application::instance().Annotation();
  if (msg == WM_COMMAND && LOWORD(wp) == IDOK) {
    self.text_ok_   = true;
    self.text_done_ = true;
    return 0;
  }
  if (msg == WM_CLOSE) {
    self.text_done_ = true;
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

bool AnnotationManager::PromptText(int scr_x, int scr_y, char* buf, int buf_size) {
  text_ok_   = false;
  text_done_ = false;

  HWND dlg = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
      kTextDlgClass,
      T(kStr_TitleTextInput),
      WS_POPUP | WS_CAPTION | WS_VISIBLE,
      scr_x, scr_y, 320, 80,
      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

  text_edit_ctrl_ = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"",
      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      8, 10, 220, 24,
      dlg, nullptr, GetModuleHandleW(nullptr), nullptr);

  CreateWindowExW(
      0, L"BUTTON", L"OK",
      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
      236, 10, 60, 24,
      dlg,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)),
      GetModuleHandleW(nullptr), nullptr);

  HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  SendMessage(text_edit_ctrl_, WM_SETFONT,
              reinterpret_cast<WPARAM>(font), TRUE);

  SetForegroundWindow(dlg);
  SetFocus(text_edit_ctrl_);

  MSG tmsg;
  while (!text_done_ && GetMessage(&tmsg, nullptr, 0, 0)) {
    if (tmsg.hwnd == text_edit_ctrl_ && tmsg.message == WM_KEYDOWN) {
      if (tmsg.wParam == VK_RETURN) { text_ok_ = true; break; }
      if (tmsg.wParam == VK_ESCAPE) { break; }
    }
    TranslateMessage(&tmsg);
    DispatchMessage(&tmsg);
  }

  buf[0] = '\0';
  if (text_ok_ && text_edit_ctrl_ && IsWindow(text_edit_ctrl_)) {
    wchar_t wbuf[256] = {};
    GetWindowTextW(text_edit_ctrl_, wbuf, 256);
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, buf, buf_size, nullptr, nullptr);
  }

  if (IsWindow(dlg)) DestroyWindow(dlg);
  text_edit_ctrl_ = nullptr;
  return text_ok_ && buf[0] != '\0';
}

bool AnnotationManager::CopyToClipboard(const PixelGrabImage* img) {
  int w      = pixelgrab_image_get_width(img);
  int h      = pixelgrab_image_get_height(img);
  int stride = pixelgrab_image_get_stride(img);
  const uint8_t* data = pixelgrab_image_get_data(img);
  if (!data || w <= 0 || h <= 0 || stride <= 0) return false;

  DWORD img_bytes = static_cast<DWORD>(stride) * static_cast<DWORD>(h);
  SIZE_T dib_size = sizeof(BITMAPINFOHEADER) + img_bytes;

  HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, dib_size);
  if (!hMem) return false;

  void* ptr = GlobalLock(hMem);
  if (!ptr) { GlobalFree(hMem); return false; }

  auto* bih = static_cast<BITMAPINFOHEADER*>(ptr);
  std::memset(bih, 0, sizeof(*bih));
  bih->biSize        = sizeof(*bih);
  bih->biWidth       = w;
  bih->biHeight      = -h;
  bih->biPlanes      = 1;
  bih->biBitCount    = 32;
  bih->biCompression = BI_RGB;
  bih->biSizeImage   = img_bytes;
  std::memcpy(static_cast<uint8_t*>(ptr) + sizeof(*bih),
              data, static_cast<size_t>(img_bytes));
  GlobalUnlock(hMem);

  if (!OpenClipboard(nullptr)) { GlobalFree(hMem); return false; }
  EmptyClipboard();
  SetClipboardData(CF_DIB, hMem);
  CloseClipboard();
  return true;
}

void AnnotationManager::PinAnnotation() {
  auto& app = Application::instance();
  if (!ann_ || !annotating_) return;

  PixelGrabImage* exported = pixelgrab_annotation_export(ann_);
  if (!exported) {
    std::printf("  Export failed: %s\n",
                pixelgrab_get_last_error_message(app.Ctx()));
    return;
  }

  RECT pin_rc = canvas_rect_;
  int pin_w = pixelgrab_image_get_width(exported);
  int pin_h = pixelgrab_image_get_height(exported);

  PixelGrabPinWindow* new_pin = pixelgrab_pin_image(
      app.Ctx(), exported,
      static_cast<int>(pin_rc.left),
      static_cast<int>(pin_rc.top));
  if (new_pin) {
    pixelgrab_pin_set_opacity(new_pin, 0.95f);
    PinEntry entry;
    entry.pin = new_pin;
    app.Pins().Pins().push_back(entry);
    std::printf("  Pinned (%d total). Double-click to close.\n",
                static_cast<int>(app.Pins().Pins().size()));
  }

  if (app.Captured()) pixelgrab_image_destroy(app.Captured());
  app.SetCaptured(exported);

  Cleanup();
  app.Selection().SyncHook();

  if (new_pin && !app.Pins().Pins().empty()) {
    app.Pins().ShowBorderFor(app.Pins().Pins().back(),
                     static_cast<int>(pin_rc.left),
                     static_cast<int>(pin_rc.top), pin_w, pin_h);
  }
}

void AnnotationManager::CopyAnnotation() {
  auto& app = Application::instance();
  if (!ann_ || !annotating_) return;

  PixelGrabImage* exported = pixelgrab_annotation_export(ann_);
  if (!exported) {
    std::printf("  Export failed: %s\n",
                pixelgrab_get_last_error_message(app.Ctx()));
    return;
  }

  int ew = pixelgrab_image_get_width(exported);
  int eh = pixelgrab_image_get_height(exported);

  if (CopyToClipboard(exported)) {
    std::printf("  %dx%d copied to clipboard.\n", ew, eh);
  } else {
    std::printf("  Clipboard copy failed.\n");
  }

  if (app.Captured()) pixelgrab_image_destroy(app.Captured());
  app.SetCaptured(exported);

  Cleanup();
}

void AnnotationManager::Cleanup() {
  auto& app = Application::instance();
  if (app.Recording().IsRecording()) app.Recording().StopRecording();

  annotating_ = false;
  dragging_   = false;

  HideColorPanel();

  if (toolbar_wnd_) {
    DestroyWindow(toolbar_wnd_);
    toolbar_wnd_ = nullptr;
  }
  if (canvas_) {
    DestroyWindow(canvas_);
    canvas_ = nullptr;
  }
  if (ann_) {
    pixelgrab_annotation_destroy(ann_);
    ann_ = nullptr;
  }
  if (app.Overlay().SelectDimWnd()) {
    DestroyWindow(app.Overlay().SelectDimWnd());
    app.Overlay().set_SelectDimWnd(nullptr);
  }
  app.Overlay().HideHighlight();
  app.Overlay().SetColor(kHighlightColor);
  std::printf("  Annotation mode ended.\n");
  app.About().ShowPendingUpdate();
}

void AnnotationManager::Cancel() {
  auto& app = Application::instance();
  if (app.Recording().IsRecording()) app.Recording().StopRecording();

  if (app.Captured()) {
    pixelgrab_image_destroy(app.Captured());
    app.SetCaptured(nullptr);
  }
  Cleanup();
}

// ── Property bubble (color palette + width / font-size) ─────────

static constexpr int kBubblePad     = 6;
static constexpr int kBubbleMidGap  = 8;
static constexpr int kBubblePropW   = 40;
static constexpr int kBubblePropH   = 20;
static constexpr int kBubblePropGap = 2;

static int BubbleGridW() {
  return kPaletteCols * kSwatchSize + (kPaletteCols - 1) * kSwatchGap;
}
static int BubbleGridH() {
  return kPaletteRows * kSwatchSize + (kPaletteRows - 1) * kSwatchGap;
}
static int BubbleWidth() {
  return kBubblePad + BubbleGridW() + kBubbleMidGap + kBubblePropW + kBubblePad;
}
static int BubbleHeight() {
  return kBubblePad + BubbleGridH() + kBubblePad;
}

static int BubbleSwatchHitTest(int mx, int my) {
  int ox = kBubblePad, oy = kBubblePad;
  int lx = mx - ox, ly = my - oy;
  if (lx < 0 || ly < 0) return -1;
  int col = lx / (kSwatchSize + kSwatchGap);
  int row = ly / (kSwatchSize + kSwatchGap);
  if (col >= kPaletteCols || row >= kPaletteRows) return -1;
  int sx = col * (kSwatchSize + kSwatchGap);
  int sy = row * (kSwatchSize + kSwatchGap);
  if (lx >= sx && lx < sx + kSwatchSize && ly >= sy && ly < sy + kSwatchSize)
    return row * kPaletteCols + col;
  return -1;
}

static int BubblePropHitTest(int mx, int my) {
  int ox = kBubblePad + BubbleGridW() + kBubbleMidGap;
  int oy = kBubblePad;
  int lx = mx - ox, ly = my - oy;
  if (lx < 0 || lx >= kBubblePropW || ly < 0) return -1;
  int idx = ly / (kBubblePropH + kBubblePropGap);
  int top = idx * (kBubblePropH + kBubblePropGap);
  if (ly >= top && ly < top + kBubblePropH && idx < 3) return idx;
  return -1;
}

LRESULT CALLBACK AnnotationManager::ColorPanelWndProc(HWND hwnd, UINT msg,
                                                       WPARAM wp, LPARAM lp) {
  auto& self = Application::instance().Annotation();
  switch (msg) {
    case WM_ERASEBKGND:
      return TRUE;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);

      RECT cr;
      GetClientRect(hwnd, &cr);
      HBRUSH bg = CreateSolidBrush(RGB(50, 50, 50));
      FillRect(hdc, &cr, bg);
      DeleteObject(bg);

      // ── Color grid ──
      for (int i = 0; i < kPaletteCount; ++i) {
        int col = i % kPaletteCols;
        int row = i / kPaletteCols;
        int sx = kBubblePad + col * (kSwatchSize + kSwatchGap);
        int sy = kBubblePad + row * (kSwatchSize + kSwatchGap);

        RECT sr = {sx, sy, sx + kSwatchSize, sy + kSwatchSize};
        HBRUSH br = CreateSolidBrush(ArgbToColorref(kColorPalette[i]));
        FillRect(hdc, &sr, br);
        DeleteObject(br);

        if (kColorPalette[i] == self.current_color_) {
          HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
          HGDIOBJ old_pen = SelectObject(hdc, pen);
          HGDIOBJ old_br  = SelectObject(hdc, GetStockObject(NULL_BRUSH));
          Rectangle(hdc, sx - 1, sy - 1, sx + kSwatchSize + 1, sy + kSwatchSize + 1);
          SelectObject(hdc, old_br);
          SelectObject(hdc, old_pen);
          DeleteObject(pen);
        }
      }

      // ── Property buttons (right side) ──
      bool is_text = (self.current_tool_ == kToolText);
      int prop_ox = kBubblePad + BubbleGridW() + kBubbleMidGap;
      int prop_oy = kBubblePad;

      struct PropDef { const wchar_t* label; bool active; };
      PropDef props[3];
      if (is_text) {
        props[0] = {T(kStr_FontSmall),  self.current_font_size_ <= kFontSmall};
        props[1] = {T(kStr_FontMed),    self.current_font_size_ == kFontMedium};
        props[2] = {T(kStr_FontLarge),  self.current_font_size_ >= kFontLarge};
      } else {
        props[0] = {T(kStr_WidthThin),  self.current_width_ < 2.0f};
        props[1] = {T(kStr_WidthMed),   self.current_width_ >= 2.0f && self.current_width_ <= 4.0f};
        props[2] = {T(kStr_WidthThick), self.current_width_ > 4.0f};
      }

      HFONT font = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                L"Consolas");
      HFONT old_font = static_cast<HFONT>(SelectObject(hdc, font));
      SetBkMode(hdc, TRANSPARENT);

      for (int i = 0; i < 3; ++i) {
        int py = prop_oy + i * (kBubblePropH + kBubblePropGap);
        RECT pr = {prop_ox, py, prop_ox + kBubblePropW, py + kBubblePropH};

        COLORREF fill = props[i].active ? RGB(0, 120, 215) : RGB(80, 80, 80);
        HBRUSH pbr = CreateSolidBrush(fill);
        FillRect(hdc, &pr, pbr);
        DeleteObject(pbr);

        SetTextColor(hdc, RGB(240, 240, 240));
        DrawTextW(hdc, props[i].label, -1, &pr,
                  DT_CENTER | DT_SINGLELINE | DT_VCENTER);
      }

      SelectObject(hdc, old_font);
      DeleteObject(font);

      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_LBUTTONDOWN: {
      int mx = LParamX(lp);
      int my = LParamY(lp);

      int swatch = BubbleSwatchHitTest(mx, my);
      if (swatch >= 0 && swatch < kPaletteCount) {
        self.current_color_ = kColorPalette[swatch];
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      int prop = BubblePropHitTest(mx, my);
      if (prop >= 0) {
        bool is_text = (self.current_tool_ == kToolText);
        if (is_text) {
          const int sizes[] = {kFontSmall, kFontMedium, kFontLarge};
          self.current_font_size_ = sizes[prop];
        } else {
          const float widths[] = {kWidthThin, kWidthMedium, kWidthThick};
          self.current_width_ = widths[prop];
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      return 0;
    }

    case WM_NCHITTEST: {
      LRESULT ht = DefWindowProcW(hwnd, msg, wp, lp);
      return (ht == HTCLIENT) ? HTCLIENT : ht;
    }
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

void AnnotationManager::ShowColorPanel() {
  if (color_panel_wnd_ || !toolbar_wnd_) return;

  RECT tbr;
  GetWindowRect(toolbar_wnd_, &tbr);

  RECT cr = {};
  if (canvas_) GetWindowRect(canvas_, &cr);

  int pw = BubbleWidth();
  int ph = BubbleHeight();
  int px = static_cast<int>(tbr.left);

  int scr_top    = GetSystemMetrics(SM_YVIRTUALSCREEN);
  int scr_bottom = scr_top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

  int py;
  if (tbr.top >= cr.bottom) {
    // Toolbar below canvas
    py = static_cast<int>(tbr.bottom) + 2;
    if (py + ph > scr_bottom) {
      py = static_cast<int>(cr.top) - ph - 2;
      if (py < scr_top) py = scr_top;
    }
  } else if (tbr.bottom <= cr.top) {
    // Toolbar above canvas
    py = static_cast<int>(tbr.top) - ph - 2;
    if (py < scr_top) {
      py = static_cast<int>(cr.bottom) + 2;
      if (py + ph > scr_bottom) py = scr_bottom - ph;
    }
  } else {
    // Toolbar overlaps canvas (fullscreen) — bubble above toolbar
    py = static_cast<int>(tbr.top) - ph - 2;
    if (py < scr_top) py = scr_top;
  }

  int scr_right = GetSystemMetrics(SM_XVIRTUALSCREEN)
                + GetSystemMetrics(SM_CXVIRTUALSCREEN);
  if (px + pw > scr_right)
    px = scr_right - pw;

  color_panel_wnd_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      kColorPanelClass, nullptr,
      WS_POPUP | WS_VISIBLE,
      px, py, pw, ph,
      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}

void AnnotationManager::HideColorPanel() {
  if (color_panel_wnd_) {
    DestroyWindow(color_panel_wnd_);
    color_panel_wnd_ = nullptr;
  }
}

void AnnotationManager::UpdateDimRegion() {
  auto& app = Application::instance();
  HWND dim = app.Overlay().SelectDimWnd();
  if (!dim) return;

  int vs_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
  int vs_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
  int vs_w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  int vs_h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

  RECT cr = {};
  if (canvas_) GetWindowRect(canvas_, &cr);

  HRGN full = CreateRectRgn(0, 0, vs_w, vs_h);
  HRGN hole = CreateRectRgn(
      static_cast<int>(cr.left)  - vs_x,
      static_cast<int>(cr.top)   - vs_y,
      static_cast<int>(cr.right) - vs_x,
      static_cast<int>(cr.bottom) - vs_y);
  int rgn_type = CombineRgn(full, full, hole, RGN_DIFF);
  DeleteObject(hole);

  if (rgn_type == NULLREGION) {
    DeleteObject(full);
    ShowWindow(dim, SW_HIDE);
  } else {
    SetWindowRgn(dim, full, TRUE);
    ShowWindow(dim, SW_SHOWNOACTIVATE);
    SetWindowPos(canvas_, dim, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
}

void AnnotationManager::RaiseToolbar() {
  if (toolbar_wnd_)
    SetWindowPos(toolbar_wnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  if (color_panel_wnd_)
    SetWindowPos(color_panel_wnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

#endif
