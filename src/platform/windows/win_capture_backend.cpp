// Copyright 2026 The loong-pixelgrab Authors

#include "platform/windows/win_capture_backend.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellscalingapi.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "core/image.h"

namespace pixelgrab {
namespace internal {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Convert a wide string to UTF-8.
std::string WideToUtf8(const wchar_t* wide) {
  if (!wide || !wide[0]) return "";
  int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr,
                                nullptr);
  if (len <= 0) return "";
  std::string result(len - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide, -1, &result[0], len, nullptr, nullptr);
  return result;
}

/// Safe string copy into a fixed-size char buffer.
void SafeCopy(char* dst, size_t dst_size, const std::string& src) {
  size_t copy_len = (std::min)(src.size(), dst_size - 1);
  std::memcpy(dst, src.data(), copy_len);
  dst[copy_len] = '\0';
}

/// Data passed to EnumDisplayMonitors callback.
struct MonitorEnumData {
  std::vector<PixelGrabScreenInfo>* screens;
};

BOOL CALLBACK MonitorEnumProc(HMONITOR monitor, HDC /*hdc*/,
                              LPRECT /*rect*/, LPARAM lparam) {
  auto* data = reinterpret_cast<MonitorEnumData*>(lparam);

  MONITORINFOEXW mi = {};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(monitor, &mi)) return TRUE;

  PixelGrabScreenInfo info = {};
  info.index = static_cast<int>(data->screens->size());
  info.x = mi.rcMonitor.left;
  info.y = mi.rcMonitor.top;
  info.width = mi.rcMonitor.right - mi.rcMonitor.left;
  info.height = mi.rcMonitor.bottom - mi.rcMonitor.top;
  info.is_primary = (mi.dwFlags & MONITORINFOF_PRIMARY) ? 1 : 0;

  std::string name = WideToUtf8(mi.szDevice);
  SafeCopy(info.name, sizeof(info.name), name);

  data->screens->push_back(info);
  return TRUE;
}

/// Data passed to EnumWindows callback.
struct WindowEnumData {
  std::vector<PixelGrabWindowInfo>* windows;
};

BOOL CALLBACK WindowEnumProc(HWND hwnd, LPARAM lparam) {
  auto* data = reinterpret_cast<WindowEnumData*>(lparam);

  // Skip invisible windows.
  if (!IsWindowVisible(hwnd)) return TRUE;

  // Skip windows with empty titles.
  wchar_t title_buf[256] = {};
  int title_len = GetWindowTextW(hwnd, title_buf, 256);
  if (title_len <= 0) return TRUE;

  // Skip tool windows and other non-app windows.
  LONG ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);
  if (ex_style & WS_EX_TOOLWINDOW) return TRUE;

  // Get window rect.
  RECT rect = {};
  GetWindowRect(hwnd, &rect);
  int width = rect.right - rect.left;
  int height = rect.bottom - rect.top;
  if (width <= 0 || height <= 0) return TRUE;

  // Get process name.
  std::string process_name;
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid != 0) {
    HANDLE process =
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process) {
      wchar_t exe_path[MAX_PATH] = {};
      DWORD path_size = MAX_PATH;
      if (QueryFullProcessImageNameW(process, 0, exe_path, &path_size)) {
        // Extract just the filename.
        std::wstring full_path(exe_path);
        size_t pos = full_path.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
          process_name = WideToUtf8(full_path.substr(pos + 1).c_str());
        } else {
          process_name = WideToUtf8(exe_path);
        }
      }
      CloseHandle(process);
    }
  }

  PixelGrabWindowInfo info = {};
  info.id = reinterpret_cast<uint64_t>(hwnd);
  info.x = rect.left;
  info.y = rect.top;
  info.width = width;
  info.height = height;
  info.is_visible = 1;

  SafeCopy(info.title, sizeof(info.title), WideToUtf8(title_buf));
  SafeCopy(info.process_name, sizeof(info.process_name), process_name);

  data->windows->push_back(info);
  return TRUE;
}

}  // namespace

// ---------------------------------------------------------------------------
// WinCaptureBackend implementation
// ---------------------------------------------------------------------------

WinCaptureBackend::WinCaptureBackend() = default;
WinCaptureBackend::~WinCaptureBackend() { Shutdown(); }

bool WinCaptureBackend::Initialize() {
  if (initialized_) return true;
  // GDI is always available; no special init needed.
  initialized_ = true;
  return true;
}

void WinCaptureBackend::Shutdown() {
  initialized_ = false;
}

std::vector<PixelGrabScreenInfo> WinCaptureBackend::GetScreens() {
  std::vector<PixelGrabScreenInfo> screens;
  MonitorEnumData data{&screens};
  EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc,
                      reinterpret_cast<LPARAM>(&data));
  return screens;
}

std::unique_ptr<Image> WinCaptureBackend::CaptureScreen(int screen_index) {
  auto screens = GetScreens();
  if (screen_index < 0 || screen_index >= static_cast<int>(screens.size())) {
    return nullptr;
  }
  const auto& s = screens[screen_index];
  return CaptureRegionGdi(s.x, s.y, s.width, s.height);
}

std::unique_ptr<Image> WinCaptureBackend::CaptureRegion(int x, int y,
                                                        int width,
                                                        int height) {
  return CaptureRegionGdi(x, y, width, height);
}

std::unique_ptr<Image> WinCaptureBackend::CaptureWindow(
    uint64_t window_handle) {
  return CaptureWindowGdi(window_handle);
}

std::vector<PixelGrabWindowInfo> WinCaptureBackend::EnumerateWindows() {
  std::vector<PixelGrabWindowInfo> windows;
  WindowEnumData data{&windows};
  ::EnumWindows(WindowEnumProc, reinterpret_cast<LPARAM>(&data));
  return windows;
}

// ---------------------------------------------------------------------------
// DPI awareness
// ---------------------------------------------------------------------------

bool WinCaptureBackend::EnableDpiAwareness() {
  if (dpi_aware_) return true;

  // Try Per-Monitor V2 first (Windows 10 1703+).
  // Use dynamic loading since DPI_AWARENESS_CONTEXT may not exist on older SDK.
  using SetProcessDpiAwarenessContextFunc = BOOL(WINAPI*)(HANDLE);
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  if (user32) {
    auto set_dpi_ctx = reinterpret_cast<SetProcessDpiAwarenessContextFunc>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (set_dpi_ctx) {
      // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = ((HANDLE)-4)
      if (set_dpi_ctx(reinterpret_cast<HANDLE>(static_cast<intptr_t>(-4)))) {
        dpi_aware_ = true;
        return true;
      }
    }
  }

  // Fallback: SetProcessDpiAwareness (Windows 8.1+).
  HMODULE shcore = LoadLibraryW(L"shcore.dll");
  if (shcore) {
    using SetProcessDpiAwarenessFunc = HRESULT(WINAPI*)(int);
    auto set_dpi = reinterpret_cast<SetProcessDpiAwarenessFunc>(
        GetProcAddress(shcore, "SetProcessDpiAwareness"));
    if (set_dpi) {
      // PROCESS_PER_MONITOR_DPI_AWARE = 2
      HRESULT hr = set_dpi(2);
      if (SUCCEEDED(hr) || hr == E_ACCESSDENIED) {
        // E_ACCESSDENIED means already set — that's fine.
        dpi_aware_ = true;
        FreeLibrary(shcore);
        return true;
      }
    }
    FreeLibrary(shcore);
  }

  // Last resort: SetProcessDPIAware (Vista+).
  if (SetProcessDPIAware()) {
    dpi_aware_ = true;
    return true;
  }

  return false;
}

bool WinCaptureBackend::GetDpiInfo(int screen_index,
                                   PixelGrabDpiInfo* out_info) {
  if (!out_info) return false;

  auto screens = GetScreens();
  if (screen_index < 0 || screen_index >= static_cast<int>(screens.size())) {
    return false;
  }

  const auto& scr = screens[screen_index];
  out_info->screen_index = screen_index;

  // Get the HMONITOR for this screen by hitting a point inside it.
  POINT pt = {scr.x + scr.width / 2, scr.y + scr.height / 2};
  HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

  // Try GetDpiForMonitor (Windows 8.1+, shcore.dll).
  HMODULE shcore = LoadLibraryW(L"shcore.dll");
  if (shcore) {
    using GetDpiForMonitorFunc = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    auto get_dpi = reinterpret_cast<GetDpiForMonitorFunc>(
        GetProcAddress(shcore, "GetDpiForMonitor"));
    if (get_dpi) {
      UINT dpi_x = 96, dpi_y = 96;
      // MDT_EFFECTIVE_DPI = 0
      if (SUCCEEDED(get_dpi(monitor, 0, &dpi_x, &dpi_y))) {
        out_info->dpi_x = static_cast<int>(dpi_x);
        out_info->dpi_y = static_cast<int>(dpi_y);
        out_info->scale_x = dpi_x / 96.0f;
        out_info->scale_y = dpi_y / 96.0f;
        FreeLibrary(shcore);
        return true;
      }
    }
    FreeLibrary(shcore);
  }

  // Fallback: use the primary DC DPI (system-wide).
  HDC hdc = GetDC(nullptr);
  if (hdc) {
    int dpi_x = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpi_y = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);
    out_info->dpi_x = dpi_x;
    out_info->dpi_y = dpi_y;
    out_info->scale_x = dpi_x / 96.0f;
    out_info->scale_y = dpi_y / 96.0f;
    return true;
  }

  // Ultimate fallback.
  out_info->dpi_x = 96;
  out_info->dpi_y = 96;
  out_info->scale_x = 1.0f;
  out_info->scale_y = 1.0f;
  return true;
}

// ---------------------------------------------------------------------------
// GDI capture implementation
// ---------------------------------------------------------------------------

std::unique_ptr<Image> WinCaptureBackend::CaptureRegionGdi(int x, int y,
                                                           int width,
                                                           int height) {
  HDC screen_dc = GetDC(nullptr);
  if (!screen_dc) return nullptr;

  HDC mem_dc = CreateCompatibleDC(screen_dc);
  if (!mem_dc) {
    ReleaseDC(nullptr, screen_dc);
    return nullptr;
  }

  HBITMAP bitmap = CreateCompatibleBitmap(screen_dc, width, height);
  if (!bitmap) {
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);
    return nullptr;
  }

  HGDIOBJ old_obj = SelectObject(mem_dc, bitmap);
  BitBlt(mem_dc, 0, 0, width, height, screen_dc, x, y, SRCCOPY);
  SelectObject(mem_dc, old_obj);

  // Read pixel data from the bitmap.
  BITMAPINFOHEADER bmi = {};
  bmi.biSize = sizeof(bmi);
  bmi.biWidth = width;
  bmi.biHeight = -height;  // Top-down.
  bmi.biPlanes = 1;
  bmi.biBitCount = 32;
  bmi.biCompression = BI_RGB;

  int stride = width * 4;
  std::vector<uint8_t> data(static_cast<size_t>(stride) * height);
  GetDIBits(mem_dc, bitmap, 0, height, data.data(),
            reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS);

  // GDI captures leave alpha=0x00; set to 0xFF (opaque) so that GDI+
  // SourceOver compositing works correctly in annotation rendering.
  for (size_t i = 3; i < data.size(); i += 4) {
    data[i] = 0xFF;
  }

  DeleteObject(bitmap);
  DeleteDC(mem_dc);
  ReleaseDC(nullptr, screen_dc);

  return Image::CreateFromData(width, height, stride, kPixelGrabFormatBgra8,
                               std::move(data));
}

std::unique_ptr<Image> WinCaptureBackend::CaptureWindowGdi(
    uint64_t window_handle) {
  HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(window_handle));
  if (!IsWindow(hwnd)) return nullptr;

  RECT rect = {};
  GetWindowRect(hwnd, &rect);
  int width = rect.right - rect.left;
  int height = rect.bottom - rect.top;
  if (width <= 0 || height <= 0) return nullptr;

  HDC window_dc = GetWindowDC(hwnd);
  if (!window_dc) return nullptr;

  HDC mem_dc = CreateCompatibleDC(window_dc);
  if (!mem_dc) {
    ReleaseDC(hwnd, window_dc);
    return nullptr;
  }

  HBITMAP bitmap = CreateCompatibleBitmap(window_dc, width, height);
  if (!bitmap) {
    DeleteDC(mem_dc);
    ReleaseDC(hwnd, window_dc);
    return nullptr;
  }

  HGDIOBJ old_obj = SelectObject(mem_dc, bitmap);

  // PrintWindow can capture windows that are partially off-screen.
  PrintWindow(hwnd, mem_dc, PW_RENDERFULLCONTENT);

  SelectObject(mem_dc, old_obj);

  // Read pixel data.
  BITMAPINFOHEADER bmi = {};
  bmi.biSize = sizeof(bmi);
  bmi.biWidth = width;
  bmi.biHeight = -height;
  bmi.biPlanes = 1;
  bmi.biBitCount = 32;
  bmi.biCompression = BI_RGB;

  int stride = width * 4;
  std::vector<uint8_t> data(static_cast<size_t>(stride) * height);
  GetDIBits(mem_dc, bitmap, 0, height, data.data(),
            reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS);

  // GDI captures leave alpha=0x00; set to 0xFF (opaque) so that GDI+
  // SourceOver compositing works correctly in annotation rendering.
  for (size_t i = 3; i < data.size(); i += 4) {
    data[i] = 0xFF;
  }

  DeleteObject(bitmap);
  DeleteDC(mem_dc);
  ReleaseDC(hwnd, window_dc);

  return Image::CreateFromData(width, height, stride, kPixelGrabFormatBgra8,
                               std::move(data));
}

// ---------------------------------------------------------------------------
// Factory function
// ---------------------------------------------------------------------------

std::unique_ptr<CaptureBackend> CreatePlatformBackend() {
  return std::make_unique<WinCaptureBackend>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // _WIN32
