// Copyright 2026 The loong-pixelgrab Authors

#include "platform/linux/x11_capture_backend.h"

#if defined(__linux__)

#include <cstring>
#include <string>

#include "core/image.h"

// TODO(linux): Include X11 headers and implement.
// #include <X11/Xlib.h>
// #include <X11/Xutil.h>
// #include <X11/extensions/XShm.h>

namespace pixelgrab {
namespace internal {

X11CaptureBackend::X11CaptureBackend() = default;
X11CaptureBackend::~X11CaptureBackend() { Shutdown(); }

bool X11CaptureBackend::Initialize() {
  // TODO(linux): Open X11 display connection.
  initialized_ = true;
  return true;
}

void X11CaptureBackend::Shutdown() {
  // TODO(linux): Close X11 display connection.
  initialized_ = false;
}

std::vector<PixelGrabScreenInfo> X11CaptureBackend::GetScreens() {
  // TODO(linux): Implement using Xinerama or XRandR.
  return {};
}

std::unique_ptr<Image> X11CaptureBackend::CaptureScreen(int screen_index) {
  (void)screen_index;
  // TODO(linux): Implement using XShmGetImage.
  return nullptr;
}

std::unique_ptr<Image> X11CaptureBackend::CaptureRegion(int x, int y,
                                                        int width,
                                                        int height) {
  (void)x; (void)y; (void)width; (void)height;
  // TODO(linux): Implement using XShmGetImage with sub-region.
  return nullptr;
}

std::unique_ptr<Image> X11CaptureBackend::CaptureWindow(
    uint64_t window_handle) {
  (void)window_handle;
  // TODO(linux): Implement using XGetImage for the window.
  return nullptr;
}

std::vector<PixelGrabWindowInfo> X11CaptureBackend::EnumerateWindows() {
  // TODO(linux): Implement using XQueryTree + XGetWindowAttributes.
  return {};
}

bool X11CaptureBackend::EnableDpiAwareness() {
  // TODO(linux): Read Xft.dpi from X resources or GDK_SCALE env var.
  return true;
}

bool X11CaptureBackend::GetDpiInfo(int screen_index,
                                   PixelGrabDpiInfo* out_info) {
  if (!out_info) return false;
  // TODO(linux): Use Xft.dpi from XGetDefault or parse GDK_SCALE.
  out_info->screen_index = screen_index;
  out_info->scale_x = 1.0f;
  out_info->scale_y = 1.0f;
  out_info->dpi_x = 96;
  out_info->dpi_y = 96;
  return true;
}

// Factory function.
std::unique_ptr<CaptureBackend> CreatePlatformBackend() {
  return std::make_unique<X11CaptureBackend>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // __linux__
