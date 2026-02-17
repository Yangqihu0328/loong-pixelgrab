// Copyright 2026 The PixelGrab Authors
// Pin border frame management.

#include "platform/windows/capture/pin_manager.h"

#ifdef _WIN32

#include "platform/windows/win_application.h"

LRESULT CALLBACK PinManager::PinBorderWndProc(HWND hwnd, UINT msg,
                                               WPARAM wp, LPARAM lp) {
  if (msg == WM_ERASEBKGND) {
    HDC hdc = reinterpret_cast<HDC>(wp);
    RECT rc;
    GetClientRect(hwnd, &rc);
    HBRUSH br = CreateSolidBrush(kConfirmColor);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
    return 1;
  }
  if (msg == WM_NCHITTEST) return HTTRANSPARENT;
  return DefWindowProcW(hwnd, msg, wp, lp);
}

void PinManager::ShowBorderFor(PinEntry& entry, int x, int y, int w, int h) {
  if (entry.border) { DestroyWindow(entry.border); entry.border = nullptr; }

  int b = kHighlightBorder;
  int bx = x - b, by = y - b, bw = w + 2 * b, bh = h + 2 * b;

  entry.border = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      kPinBorderClass, nullptr, WS_POPUP,
      bx, by, bw, bh,
      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (!entry.border) return;

  ShowWindow(entry.border, SW_SHOWNOACTIVATE);

  entry.hwnd = static_cast<HWND>(
      pixelgrab_pin_get_native_handle(entry.pin));

  if (entry.hwnd) {
    SetWindowPos(entry.hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
}

void PinManager::HideBorderFor(PinEntry& entry) {
  if (entry.border) {
    DestroyWindow(entry.border);
    entry.border = nullptr;
  }
  entry.hwnd = nullptr;
}

void PinManager::SyncBorders() {
  auto& app = Application::instance();
  bool changed = false;

  for (auto it = pins_.begin(); it != pins_.end(); ) {
    PinEntry& e = *it;

    if (e.hwnd && !IsWindow(e.hwnd)) {
      HideBorderFor(e);
      if (e.pin) {
        pixelgrab_pin_destroy(e.pin);
        e.pin = nullptr;
      }
      it = pins_.erase(it);
      changed = true;
      continue;
    }

    if (e.hwnd) {
      RECT pr;
      if (GetWindowRect(e.hwnd, &pr) && e.border) {
        int b = kHighlightBorder;
        SetWindowPos(e.border, nullptr,
                     static_cast<int>(pr.left) - b,
                     static_cast<int>(pr.top) - b,
                     static_cast<int>(pr.right - pr.left) + 2 * b,
                     static_cast<int>(pr.bottom - pr.top) + 2 * b,
                     SWP_NOZORDER | SWP_NOACTIVATE);
      }
    }
    ++it;
  }

  if (changed) app.Selection().SyncHook();
}

void PinManager::CloseByHwnd(HWND target) {
  auto& app = Application::instance();
  for (auto it = pins_.begin(); it != pins_.end(); ++it) {
    if (it->hwnd == target) {
      HideBorderFor(*it);
      if (it->pin) pixelgrab_pin_destroy(it->pin);
      pins_.erase(it);
      std::printf("  Pin closed. (%d remaining)\n",
                  static_cast<int>(pins_.size()));
      app.Selection().SyncHook();
      return;
    }
  }
}

void PinManager::PinCapture() {
  auto& app = Application::instance();
  if (!app.Captured()) {
    std::printf("  [F3] Nothing captured yet. Press F1 to capture first.\n");
    return;
  }
  int w = pixelgrab_image_get_width(app.Captured());
  int h = pixelgrab_image_get_height(app.Captured());

  int offset = static_cast<int>(pins_.size()) * 30;
  PixelGrabPinWindow* new_pin = pixelgrab_pin_image(
      app.Ctx(), app.Captured(), 100 + offset, 100 + offset);
  if (new_pin) {
    pixelgrab_pin_set_opacity(new_pin, 0.95f);
    PinEntry entry;
    entry.pin = new_pin;
    pins_.push_back(entry);
    std::printf("  [F3] Pinned %dx%d at (%d,%d) -- "
                "double-click to close. (%d total)\n",
                w, h, 100 + offset, 100 + offset,
                static_cast<int>(pins_.size()));
    app.Selection().SyncHook();
    ShowBorderFor(pins_.back(), 100 + offset, 100 + offset, w, h);
  } else {
    std::printf("  [F3] Pin failed: %s\n",
                pixelgrab_get_last_error_message(app.Ctx()));
  }
}

void PinManager::PinFromClipboard() {
  auto& app = Application::instance();
  PixelGrabClipboardFormat fmt = pixelgrab_clipboard_get_format(app.Ctx());

  if (fmt == kPixelGrabClipboardImage) {
    PixelGrabImage* img = pixelgrab_clipboard_get_image(app.Ctx());
    if (img) {
      int w = pixelgrab_image_get_width(img);
      int h = pixelgrab_image_get_height(img);
      int offset = static_cast<int>(pins_.size()) * 30;
      PixelGrabPinWindow* pin = pixelgrab_pin_image(
          app.Ctx(), img, 120 + offset, 120 + offset);
      if (pin) {
        pixelgrab_pin_set_opacity(pin, 0.95f);
        PinEntry entry;
        entry.pin = pin;
        pins_.push_back(entry);
        app.Selection().SyncHook();
        ShowBorderFor(pins_.back(), 120 + offset, 120 + offset, w, h);
        std::printf("  [Clipboard] Pinned image %dx%d from clipboard.\n", w, h);
      }
      pixelgrab_image_destroy(img);
    } else {
      std::printf("  [Clipboard] Failed to read image.\n");
    }
  } else if (fmt == kPixelGrabClipboardText) {
    char* text = pixelgrab_clipboard_get_text(app.Ctx());
    if (text) {
      std::printf("  [Clipboard] Text: %.80s%s\n",
                  text, (std::strlen(text) > 80 ? "..." : ""));
      pixelgrab_free_string(text);
    }
  } else {
    std::printf("  [Clipboard] No image or text in clipboard.\n");
  }
}

void PinManager::PinFromHistory(int history_id) {
  auto& app = Application::instance();
  PixelGrabImage* img = pixelgrab_history_recapture(app.Ctx(), history_id);
  if (!img) {
    std::printf("  [History] Recapture failed for id=%d.\n", history_id);
    return;
  }
  int w = pixelgrab_image_get_width(img);
  int h = pixelgrab_image_get_height(img);
  int offset = static_cast<int>(pins_.size()) * 30;
  PixelGrabPinWindow* pin = pixelgrab_pin_image(
      app.Ctx(), img, 100 + offset, 100 + offset);
  if (pin) {
    pixelgrab_pin_set_opacity(pin, 0.95f);
    PinEntry entry;
    entry.pin = pin;
    pins_.push_back(entry);
    app.Selection().SyncHook();
    ShowBorderFor(pins_.back(), 100 + offset, 100 + offset, w, h);
    std::printf("  [History] Recaptured id=%d (%dx%d).\n", history_id, w, h);
  }
  pixelgrab_image_destroy(img);
}

void PinManager::HandleDoubleClick(int x, int y) {
  if (pins_.empty()) return;

  POINT pt = {x, y};
  HWND hw = WindowFromPoint(pt);
  if (!hw) return;

  DWORD pid = 0;
  GetWindowThreadProcessId(hw, &pid);
  if (pid != GetCurrentProcessId()) return;

  for (const auto& e : pins_) {
    if (e.hwnd == hw) {
      CloseByHwnd(hw);
      return;
    }
  }
}

#endif
