// Copyright 2024 PixelGrab Authors. All rights reserved.
// Windows floating pin window implementation.

#include "platform/windows/win_pin_window.h"

#include <cstring>
#include <string>

namespace pixelgrab {
namespace internal {

static const wchar_t kPinWindowClassName[] = L"PixelGrabPinWindow";

bool WinPinWindowBackend::class_registered_ = false;

// Helper: UTF-8 to wide string.
static std::wstring Utf8ToWide(const char* utf8) {
  if (!utf8 || !*utf8) return L"";
  int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
  if (len <= 0) return L"";
  std::wstring result(len, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &result[0], len);
  result.pop_back();  // Remove trailing null.
  return result;
}

bool WinPinWindowBackend::RegisterWindowClass() {
  if (class_registered_) return true;

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WinPinWindowBackend::WndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
  wc.lpszClassName = kPinWindowClassName;

  if (RegisterClassExW(&wc) == 0) {
    // Class might already be registered from a previous load.
    if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
  }

  class_registered_ = true;
  return true;
}

LRESULT CALLBACK WinPinWindowBackend::WndProc(HWND hwnd, UINT msg,
                                               WPARAM wparam, LPARAM lparam) {
  WinPinWindowBackend* self = nullptr;

  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
    self = static_cast<WinPinWindowBackend*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<WinPinWindowBackend*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      if (self) self->Paint(hdc);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_LBUTTONDOWN: {
      // Begin drag: capture mouse, record start position.
      if (self) {
        self->dragging_ = true;
        self->drag_start_.x = LOWORD(lparam);
        self->drag_start_.y = HIWORD(lparam);
        SetCapture(hwnd);
      }
      return 0;
    }

    case WM_MOUSEMOVE: {
      if (self && self->dragging_) {
        POINT cursor;
        GetCursorPos(&cursor);
        int new_x = cursor.x - self->drag_start_.x;
        int new_y = cursor.y - self->drag_start_.y;
        SetWindowPos(hwnd, nullptr, new_x, new_y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      }
      return 0;
    }

    case WM_LBUTTONUP: {
      if (self && self->dragging_) {
        self->dragging_ = false;
        ReleaseCapture();
      }
      return 0;
    }

    case WM_MOUSEWHEEL: {
      // Scroll wheel: adjust opacity.
      if (self) {
        int delta = GET_WHEEL_DELTA_WPARAM(wparam);
        float step = (delta > 0) ? 0.05f : -0.05f;
        float new_opacity = self->opacity_ + step;
        if (new_opacity < 0.1f) new_opacity = 0.1f;
        if (new_opacity > 1.0f) new_opacity = 1.0f;
        self->SetOpacity(new_opacity);
      }
      return 0;
    }

    case WM_RBUTTONUP: {
      // Right-click: close window.
      if (self) {
        DestroyWindow(hwnd);
      }
      return 0;
    }

    case WM_KEYDOWN: {
      if (wparam == VK_ESCAPE && self) {
        DestroyWindow(hwnd);
      }
      return 0;
    }

    case WM_DESTROY: {
      if (self) {
        self->hwnd_ = nullptr;  // Mark as invalid.
      }
      return 0;
    }

    default:
      break;
  }

  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

WinPinWindowBackend::WinPinWindowBackend() = default;

WinPinWindowBackend::~WinPinWindowBackend() {
  Destroy();
}

bool WinPinWindowBackend::Create(const PinWindowConfig& config) {
  if (!RegisterWindowClass()) return false;

  DWORD ex_style = WS_EX_TOOLWINDOW | WS_EX_LAYERED;
  if (config.topmost) {
    ex_style |= WS_EX_TOPMOST;
  }

  int w = (config.width > 0) ? config.width : 200;
  int h = (config.height > 0) ? config.height : 200;

  hwnd_ = CreateWindowExW(
      ex_style, kPinWindowClassName, L"PixelGrab Pin",
      WS_POPUP, config.x, config.y, w, h,
      nullptr, nullptr, GetModuleHandleW(nullptr),
      this  // Pass 'this' via CREATESTRUCT.
  );

  if (!hwnd_) return false;

  opacity_ = config.opacity;
  SetLayeredWindowAttributes(hwnd_, 0,
                             static_cast<BYTE>(opacity_ * 255.0f),
                             LWA_ALPHA);
  ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
  visible_ = true;

  return true;
}

void WinPinWindowBackend::Destroy() {
  if (bitmap_) {
    DeleteObject(bitmap_);
    bitmap_ = nullptr;
  }
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}

bool WinPinWindowBackend::IsValid() const {
  return hwnd_ != nullptr && IsWindow(hwnd_);
}

bool WinPinWindowBackend::SetImageContent(const Image* image) {
  if (!image || !hwnd_) return false;

  // Clean up previous bitmap.
  if (bitmap_) {
    DeleteObject(bitmap_);
    bitmap_ = nullptr;
  }

  int w = image->width();
  int h = image->height();

  // Create a DIB section from the image pixel data.
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;  // Top-down.
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HDC screen_dc = GetDC(nullptr);
  bitmap_ = CreateDIBSection(screen_dc, &bmi, DIB_RGB_COLORS, &bits, nullptr,
                             0);
  ReleaseDC(nullptr, screen_dc);

  if (!bitmap_ || !bits) return false;

  // Copy pixel data (BGRA8 → DIB BGRA).
  int src_stride = image->stride();
  int dst_stride = w * 4;
  const uint8_t* src = image->data();
  uint8_t* dst = static_cast<uint8_t*>(bits);

  for (int y = 0; y < h; ++y) {
    std::memcpy(dst + y * dst_stride, src + y * src_stride, w * 4);
  }

  bitmap_width_ = w;
  bitmap_height_ = h;
  content_type_ = PinContentType::kImage;

  // Cache a copy of the image data for GetImageContent().
  image_copy_width_ = w;
  image_copy_height_ = h;
  image_copy_stride_ = image->stride();
  image_copy_format_ = image->format();
  image_copy_data_.assign(image->data(),
                          image->data() + image->data_size());

  // Resize window to match image.
  SetWindowPos(hwnd_, nullptr, 0, 0, w, h,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

  InvalidateRect(hwnd_, nullptr, TRUE);
  return true;
}

bool WinPinWindowBackend::SetTextContent(const char* text) {
  if (!text || !hwnd_) return false;
  text_content_ = text;
  content_type_ = PinContentType::kText;
  // Clear image cache when switching to text.
  image_copy_data_.clear();
  image_copy_width_ = 0;
  image_copy_height_ = 0;
  InvalidateRect(hwnd_, nullptr, TRUE);
  return true;
}

std::unique_ptr<Image> WinPinWindowBackend::GetImageContent() const {
  if (content_type_ != PinContentType::kImage || image_copy_data_.empty()) {
    return nullptr;
  }
  // Return a deep copy of the cached image data.
  std::vector<uint8_t> copy(image_copy_data_);
  return Image::CreateFromData(image_copy_width_, image_copy_height_,
                               image_copy_stride_, image_copy_format_,
                               std::move(copy));
}

void WinPinWindowBackend::GetPosition(int* out_x, int* out_y) const {
  if (!out_x || !out_y) return;
  if (hwnd_ && IsWindow(hwnd_)) {
    RECT rect;
    GetWindowRect(hwnd_, &rect);
    *out_x = rect.left;
    *out_y = rect.top;
  } else {
    *out_x = 0;
    *out_y = 0;
  }
}

void WinPinWindowBackend::GetSize(int* out_width, int* out_height) const {
  if (!out_width || !out_height) return;
  if (hwnd_ && IsWindow(hwnd_)) {
    RECT rect;
    GetWindowRect(hwnd_, &rect);
    *out_width = rect.right - rect.left;
    *out_height = rect.bottom - rect.top;
  } else {
    *out_width = 0;
    *out_height = 0;
  }
}

void* WinPinWindowBackend::GetNativeHandle() const {
  return hwnd_;
}

void WinPinWindowBackend::SetPosition(int x, int y) {
  if (hwnd_) {
    SetWindowPos(hwnd_, nullptr, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
}

void WinPinWindowBackend::SetSize(int width, int height) {
  if (hwnd_ && width > 0 && height > 0) {
    SetWindowPos(hwnd_, nullptr, 0, 0, width, height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
}

void WinPinWindowBackend::SetOpacity(float opacity) {
  if (opacity < 0.0f) opacity = 0.0f;
  if (opacity > 1.0f) opacity = 1.0f;
  opacity_ = opacity;
  if (hwnd_) {
    SetLayeredWindowAttributes(hwnd_, 0,
                               static_cast<BYTE>(opacity_ * 255.0f),
                               LWA_ALPHA);
  }
}

float WinPinWindowBackend::GetOpacity() const {
  return opacity_;
}

void WinPinWindowBackend::SetVisible(bool visible) {
  visible_ = visible;
  if (hwnd_) {
    ShowWindow(hwnd_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
  }
}

bool WinPinWindowBackend::IsVisible() const {
  return visible_ && IsValid();
}

bool WinPinWindowBackend::ProcessEvents() {
  if (!IsValid()) return false;

  MSG msg;
  while (PeekMessageW(&msg, hwnd_, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  return IsValid();
}

void WinPinWindowBackend::Paint(HDC hdc) {
  if (!hwnd_) return;

  RECT rc;
  GetClientRect(hwnd_, &rc);

  if (content_type_ == PinContentType::kImage && bitmap_) {
    HDC mem_dc = CreateCompatibleDC(hdc);
    HGDIOBJ old = SelectObject(mem_dc, bitmap_);

    // Stretch image to fill window.
    StretchBlt(hdc, 0, 0, rc.right, rc.bottom,
               mem_dc, 0, 0, bitmap_width_, bitmap_height_, SRCCOPY);

    SelectObject(mem_dc, old);
    DeleteDC(mem_dc);
  } else if (content_type_ == PinContentType::kText) {
    // Fill background.
    FillRect(hdc, &rc, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    // Draw text.
    if (!text_content_.empty()) {
      std::wstring wide_text = Utf8ToWide(text_content_.c_str());
      RECT text_rc = rc;
      text_rc.left += 8;
      text_rc.top += 8;
      text_rc.right -= 8;
      text_rc.bottom -= 8;
      DrawTextW(hdc, wide_text.c_str(), static_cast<int>(wide_text.size()),
                &text_rc, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
  }
}

// Factory implementation.
std::unique_ptr<PinWindowBackend> CreatePlatformPinWindowBackend() {
  return std::make_unique<WinPinWindowBackend>();
}

}  // namespace internal
}  // namespace pixelgrab
