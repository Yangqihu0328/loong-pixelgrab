// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_PLATFORM_LINUX_X11_CAPTURE_BACKEND_H_
#define PIXELGRAB_PLATFORM_LINUX_X11_CAPTURE_BACKEND_H_

#include <memory>
#include <vector>

#include "core/capture_backend.h"
#include "core/image.h"

namespace pixelgrab {
namespace internal {

/// Linux capture backend using X11 (XShmGetImage).
class X11CaptureBackend : public CaptureBackend {
 public:
  X11CaptureBackend();
  ~X11CaptureBackend() override;

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
  void* display_ = nullptr;  // Display* from X11
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PLATFORM_LINUX_X11_CAPTURE_BACKEND_H_
