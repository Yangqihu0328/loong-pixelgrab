// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_PLATFORM_MACOS_MAC_CAPTURE_BACKEND_H_
#define PIXELGRAB_PLATFORM_MACOS_MAC_CAPTURE_BACKEND_H_

#include <memory>
#include <vector>

#include "core/capture_backend.h"
#include "core/image.h"

namespace pixelgrab {
namespace internal {

/// macOS capture backend using ScreenCaptureKit / CoreGraphics.
class MacCaptureBackend : public CaptureBackend {
 public:
  MacCaptureBackend();
  ~MacCaptureBackend() override;

  bool Initialize() override;
  void Shutdown() override;

  std::vector<PixelGrabScreenInfo> GetScreens() override;
  std::unique_ptr<Image> CaptureScreen(int screen_index) override;
  std::unique_ptr<Image> CaptureRegion(int x, int y, int width,
                                       int height) override;
  std::unique_ptr<Image> CaptureWindow(uint64_t window_handle) override;
  std::vector<PixelGrabWindowInfo> EnumerateWindows() override;

  bool EnableDpiAwareness() override;
  bool GetDpiInfo(int screen_index, PixelGrabDpiInfo* out_info) override;

 private:
  bool initialized_ = false;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PLATFORM_MACOS_MAC_CAPTURE_BACKEND_H_
