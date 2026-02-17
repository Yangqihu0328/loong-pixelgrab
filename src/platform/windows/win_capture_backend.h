// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_PLATFORM_WINDOWS_WIN_CAPTURE_BACKEND_H_
#define PIXELGRAB_PLATFORM_WINDOWS_WIN_CAPTURE_BACKEND_H_

#include <memory>
#include <vector>

#include "core/capture_backend.h"
#include "core/image.h"

// Forward declare Windows types to avoid including windows.h in the header.
struct IDXGIOutputDuplication;
struct ID3D11Device;
struct ID3D11DeviceContext;

namespace pixelgrab {
namespace internal {

/// Windows capture backend using DXGI Desktop Duplication (primary) with
/// GDI BitBlt fallback.
class WinCaptureBackend : public CaptureBackend {
 public:
  WinCaptureBackend();
  ~WinCaptureBackend() override;

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
  /// Capture a region using GDI BitBlt.
  std::unique_ptr<Image> CaptureRegionGdi(int x, int y, int width, int height);

  /// Capture a specific window using GDI.
  std::unique_ptr<Image> CaptureWindowGdi(uint64_t window_handle);

  bool initialized_ = false;
  bool dpi_aware_ = false;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PLATFORM_WINDOWS_WIN_CAPTURE_BACKEND_H_
