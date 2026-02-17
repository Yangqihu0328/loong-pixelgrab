// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_CORE_CAPTURE_BACKEND_H_
#define PIXELGRAB_CORE_CAPTURE_BACKEND_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/image.h"
#include "pixelgrab/pixelgrab.h"

namespace pixelgrab {
namespace internal {

/// Abstract interface for platform-specific screen capture backends.
///
/// Each platform (Windows, macOS, Linux) provides a concrete implementation.
/// Only the implementation for the current build platform is compiled.
class CaptureBackend {
 public:
  virtual ~CaptureBackend() = default;

  // Non-copyable.
  CaptureBackend(const CaptureBackend&) = delete;
  CaptureBackend& operator=(const CaptureBackend&) = delete;

  /// Initialize the capture backend. Must be called before any capture
  /// operations.
  /// @return true on success.
  virtual bool Initialize() = 0;

  /// Shut down and release platform resources.
  virtual void Shutdown() = 0;

  // -- Screen information --

  /// Refresh and return the list of connected screens.
  virtual std::vector<PixelGrabScreenInfo> GetScreens() = 0;

  // -- Capture operations --

  /// Capture the full contents of a screen.
  virtual std::unique_ptr<Image> CaptureScreen(int screen_index) = 0;

  /// Capture a rectangular region in virtual screen coordinates.
  virtual std::unique_ptr<Image> CaptureRegion(int x, int y, int width,
                                               int height) = 0;

  /// Capture the contents of a specific window.
  virtual std::unique_ptr<Image> CaptureWindow(uint64_t window_handle) = 0;

  // -- Window enumeration --

  /// Enumerate visible top-level windows.
  virtual std::vector<PixelGrabWindowInfo> EnumerateWindows() = 0;

  // -- DPI support --

  /// Enable system DPI awareness. Called once.
  /// Returns true if DPI awareness was successfully enabled.
  virtual bool EnableDpiAwareness() = 0;

  /// Get DPI information for a specific screen.
  virtual bool GetDpiInfo(int screen_index, PixelGrabDpiInfo* out_info) = 0;

 protected:
  CaptureBackend() = default;
};

/// Factory function implemented per-platform (one per build target).
/// Defined in platform/<os>/xxx_capture_backend.cpp.
std::unique_ptr<CaptureBackend> CreatePlatformBackend();

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_CORE_CAPTURE_BACKEND_H_
