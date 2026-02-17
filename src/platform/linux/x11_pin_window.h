// Copyright 2024 PixelGrab Authors. All rights reserved.
// Linux X11 floating pin window backend (stub).

#ifndef PIXELGRAB_PLATFORM_LINUX_X11_PIN_WINDOW_H_
#define PIXELGRAB_PLATFORM_LINUX_X11_PIN_WINDOW_H_

#include "core/image.h"
#include "pin/pin_window_backend.h"

namespace pixelgrab {
namespace internal {

class X11PinWindowBackend : public PinWindowBackend {
 public:
  bool Create(const PinWindowConfig& config) override;
  void Destroy() override;
  bool IsValid() const override;
  bool SetImageContent(const Image* image) override;
  bool SetTextContent(const char* text) override;
  void SetPosition(int x, int y) override;
  void SetSize(int width, int height) override;
  void SetOpacity(float opacity) override;
  float GetOpacity() const override;
  void SetVisible(bool visible) override;
  bool IsVisible() const override;
  bool ProcessEvents() override;

 private:
  float opacity_ = 1.0f;
  bool valid_ = false;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PLATFORM_LINUX_X11_PIN_WINDOW_H_
