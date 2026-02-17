// Copyright 2024 PixelGrab Authors. All rights reserved.
// Pin window (floating overlay) abstract interface.

#ifndef PIXELGRAB_PIN_PIN_WINDOW_BACKEND_H_
#define PIXELGRAB_PIN_PIN_WINDOW_BACKEND_H_

#include <memory>

namespace pixelgrab {
namespace internal {

class Image;

/// Content type for a pin window.
enum class PinContentType {
  kImage,
  kText,
  kHtml,
};

/// Configuration for creating a pin window.
struct PinWindowConfig {
  int x = 0;
  int y = 0;
  int width = 0;          // 0 = auto-size to content
  int height = 0;         // 0 = auto-size to content
  float opacity = 1.0f;   // 0.0 = transparent, 1.0 = opaque
  bool topmost = true;
};

/// Abstract interface for platform-specific pin window operations.
class PinWindowBackend {
 public:
  virtual ~PinWindowBackend() = default;

  // -- Lifecycle --

  virtual bool Create(const PinWindowConfig& config) = 0;
  virtual void Destroy() = 0;
  virtual bool IsValid() const = 0;

  // -- Content --

  virtual bool SetImageContent(const Image* image) = 0;
  virtual bool SetTextContent(const char* text) = 0;

  /// Get a copy of the image content.  Returns nullptr for text pins.
  virtual std::unique_ptr<Image> GetImageContent() const = 0;

  // -- Attributes --

  virtual void SetPosition(int x, int y) = 0;
  virtual void SetSize(int width, int height) = 0;
  virtual void SetOpacity(float opacity) = 0;
  virtual float GetOpacity() const = 0;
  virtual void SetVisible(bool visible) = 0;
  virtual bool IsVisible() const = 0;

  /// Get the current window position (screen coordinates).
  virtual void GetPosition(int* out_x, int* out_y) const = 0;

  /// Get the current window size in pixels.
  virtual void GetSize(int* out_width, int* out_height) const = 0;

  // -- Native handle --

  /// Get the platform-specific native window handle.
  /// Returns nullptr if the window is not valid.
  virtual void* GetNativeHandle() const = 0;

  // -- Events --

  /// Process pending system events for this window (non-blocking).
  /// Returns false if the window has been closed by the user.
  virtual bool ProcessEvents() = 0;

 protected:
  PinWindowBackend() = default;
};

/// Factory: creates the platform-specific pin window backend.
std::unique_ptr<PinWindowBackend> CreatePlatformPinWindowBackend();

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PIN_PIN_WINDOW_BACKEND_H_
