// Copyright 2026 The loong-pixelgrab Authors

#include "platform/macos/mac_capture_backend.h"

#ifdef __APPLE__

#include <CoreGraphics/CoreGraphics.h>

#include <cstring>
#include <string>

#include "core/image.h"

namespace pixelgrab {
namespace internal {

MacCaptureBackend::MacCaptureBackend() = default;
MacCaptureBackend::~MacCaptureBackend() { Shutdown(); }

bool MacCaptureBackend::Initialize() {
  initialized_ = true;
  return true;
}

void MacCaptureBackend::Shutdown() {
  initialized_ = false;
}

std::vector<PixelGrabScreenInfo> MacCaptureBackend::GetScreens() {
  std::vector<PixelGrabScreenInfo> screens;
  // TODO(macos): Implement using CGGetActiveDisplayList.
  return screens;
}

std::unique_ptr<Image> MacCaptureBackend::CaptureScreen(int screen_index) {
  // TODO(macos): Implement using CGDisplayCreateImage or ScreenCaptureKit.
  return nullptr;
}

std::unique_ptr<Image> MacCaptureBackend::CaptureRegion(int x, int y,
                                                        int width,
                                                        int height) {
  // TODO(macos): Implement using CGWindowListCreateImage with region.
  return nullptr;
}

std::unique_ptr<Image> MacCaptureBackend::CaptureWindow(
    uint64_t window_handle) {
  // TODO(macos): Implement using CGWindowListCreateImage with window ID.
  return nullptr;
}

std::vector<PixelGrabWindowInfo> MacCaptureBackend::EnumerateWindows() {
  // TODO(macos): Implement using CGWindowListCopyWindowInfo.
  return {};
}

bool MacCaptureBackend::EnableDpiAwareness() {
  // macOS supports Retina automatically — no-op.
  return true;
}

bool MacCaptureBackend::GetDpiInfo(int screen_index,
                                   PixelGrabDpiInfo* out_info) {
  if (!out_info) return false;
  // TODO(macos): Use NSScreen.backingScaleFactor for the given screen.
  out_info->screen_index = screen_index;
  out_info->scale_x = 2.0f;  // Default Retina assumption.
  out_info->scale_y = 2.0f;
  out_info->dpi_x = 144;
  out_info->dpi_y = 144;
  return true;
}

// Factory function.
std::unique_ptr<CaptureBackend> CreatePlatformBackend() {
  return std::make_unique<MacCaptureBackend>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // __APPLE__
