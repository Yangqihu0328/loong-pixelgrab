// Copyright 2024 PixelGrab Authors. All rights reserved.
// Windows clipboard reader implementation.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "platform/windows/win_clipboard.h"

#include <cstring>
#include <string>

#include "core/image.h"

namespace pixelgrab {
namespace internal {

PixelGrabClipboardFormat WinClipboardReader::GetAvailableFormat() const {
  if (IsClipboardFormatAvailable(CF_DIB) ||
      IsClipboardFormatAvailable(CF_DIBV5)) {
    return kPixelGrabClipboardImage;
  }
  if (IsClipboardFormatAvailable(CF_UNICODETEXT) ||
      IsClipboardFormatAvailable(CF_TEXT)) {
    return kPixelGrabClipboardText;
  }
  // Check for HTML format.
  static UINT cf_html = RegisterClipboardFormatW(L"HTML Format");
  if (cf_html && IsClipboardFormatAvailable(cf_html)) {
    return kPixelGrabClipboardHtml;
  }
  return kPixelGrabClipboardNone;
}

std::unique_ptr<Image> WinClipboardReader::ReadImage() {
  if (!OpenClipboard(nullptr)) return nullptr;

  HANDLE hdata = GetClipboardData(CF_DIB);
  if (!hdata) {
    CloseClipboard();
    return nullptr;
  }

  auto* bmi = static_cast<BITMAPINFO*>(GlobalLock(hdata));
  if (!bmi) {
    CloseClipboard();
    return nullptr;
  }

  int width = bmi->bmiHeader.biWidth;
  int height = bmi->bmiHeader.biHeight;
  bool top_down = (height < 0);
  if (height < 0) height = -height;

  int bit_count = bmi->bmiHeader.biBitCount;

  std::unique_ptr<Image> result;

  if (bit_count == 32) {
    // 32-bit BGRA.
    result = Image::Create(width, height, kPixelGrabFormatBgra8);
    if (result) {
      // Pixel data starts after the header.
      int header_size = bmi->bmiHeader.biSize;
      // Account for color masks if BI_BITFIELDS.
      if (bmi->bmiHeader.biCompression == BI_BITFIELDS) {
        header_size += 12;  // 3 DWORDs for color masks.
      }
      const uint8_t* src =
          reinterpret_cast<const uint8_t*>(bmi) + header_size;

      int src_stride = ((width * 32 + 31) / 32) * 4;
      uint8_t* dst = result->mutable_data();
      int dst_stride = result->stride();

      for (int y = 0; y < height; ++y) {
        int src_y = top_down ? y : (height - 1 - y);
        std::memcpy(dst + y * dst_stride, src + src_y * src_stride, width * 4);
      }
    }
  } else if (bit_count == 24) {
    // 24-bit BGR → BGRA.
    result = Image::Create(width, height, kPixelGrabFormatBgra8);
    if (result) {
      int header_size = bmi->bmiHeader.biSize;
      const uint8_t* src =
          reinterpret_cast<const uint8_t*>(bmi) + header_size;

      int src_stride = ((width * 24 + 31) / 32) * 4;
      uint8_t* dst = result->mutable_data();
      int dst_stride = result->stride();

      for (int y = 0; y < height; ++y) {
        int src_y = top_down ? y : (height - 1 - y);
        const uint8_t* sp = src + src_y * src_stride;
        uint8_t* dp = dst + y * dst_stride;
        for (int x = 0; x < width; ++x) {
          dp[x * 4 + 0] = sp[x * 3 + 0];  // B
          dp[x * 4 + 1] = sp[x * 3 + 1];  // G
          dp[x * 4 + 2] = sp[x * 3 + 2];  // R
          dp[x * 4 + 3] = 255;             // A
        }
      }
    }
  }

  GlobalUnlock(hdata);
  CloseClipboard();
  return result;
}

std::string WinClipboardReader::ReadText() {
  if (!OpenClipboard(nullptr)) return "";

  // Try Unicode first.
  HANDLE hdata = GetClipboardData(CF_UNICODETEXT);
  if (hdata) {
    const wchar_t* wstr = static_cast<const wchar_t*>(GlobalLock(hdata));
    if (wstr) {
      int utf8_len =
          WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr,
                              nullptr);
      std::string result;
      if (utf8_len > 0) {
        result.resize(utf8_len - 1);  // Exclude null terminator.
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], utf8_len,
                            nullptr, nullptr);
      }
      GlobalUnlock(hdata);
      CloseClipboard();
      return result;
    }
    GlobalUnlock(hdata);
  }

  // Fall back to ANSI text.
  hdata = GetClipboardData(CF_TEXT);
  if (hdata) {
    const char* str = static_cast<const char*>(GlobalLock(hdata));
    std::string result;
    if (str) {
      result = str;
    }
    GlobalUnlock(hdata);
    CloseClipboard();
    return result;
  }

  CloseClipboard();
  return "";
}

// Factory implementation.
std::unique_ptr<ClipboardReader> CreatePlatformClipboardReader() {
  return std::make_unique<WinClipboardReader>();
}

}  // namespace internal
}  // namespace pixelgrab
