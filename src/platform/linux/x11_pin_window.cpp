// Copyright 2024 PixelGrab Authors. All rights reserved.
// Linux X11 floating pin window backend (stub implementation).

#include "platform/linux/x11_pin_window.h"

namespace pixelgrab {
namespace internal {

bool X11PinWindowBackend::Create(const PinWindowConfig& config) {
  // TODO: XCreateWindow + _NET_WM_STATE_ABOVE.
  opacity_ = config.opacity;
  valid_ = true;
  return true;
}

void X11PinWindowBackend::Destroy() { valid_ = false; }
bool X11PinWindowBackend::IsValid() const { return valid_; }

bool X11PinWindowBackend::SetImageContent(const Image* image) {
  // TODO: XPutImage
  (void)image;
  return valid_;
}

bool X11PinWindowBackend::SetTextContent(const char* text) {
  // TODO: Cairo text rendering
  (void)text;
  return valid_;
}

void X11PinWindowBackend::SetPosition(int x, int y) { (void)x; (void)y; }
void X11PinWindowBackend::SetSize(int w, int h) { (void)w; (void)h; }
void X11PinWindowBackend::SetOpacity(float o) { opacity_ = o; }
float X11PinWindowBackend::GetOpacity() const { return opacity_; }
void X11PinWindowBackend::SetVisible(bool v) { (void)v; }
bool X11PinWindowBackend::IsVisible() const { return valid_; }
bool X11PinWindowBackend::ProcessEvents() { return valid_; }

std::unique_ptr<PinWindowBackend> CreatePlatformPinWindowBackend() {
  return std::make_unique<X11PinWindowBackend>();
}

}  // namespace internal
}  // namespace pixelgrab
