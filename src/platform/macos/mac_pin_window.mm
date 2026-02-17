// Copyright 2024 PixelGrab Authors. All rights reserved.
// macOS floating pin window backend (stub implementation).

#include "platform/macos/mac_pin_window.h"

namespace pixelgrab {
namespace internal {

bool MacPinWindowBackend::Create(const PinWindowConfig& config) {
  // TODO: NSWindow with borderless style, floating level.
  opacity_ = config.opacity;
  valid_ = true;
  return true;
}

void MacPinWindowBackend::Destroy() { valid_ = false; }
bool MacPinWindowBackend::IsValid() const { return valid_; }

bool MacPinWindowBackend::SetImageContent(const Image* image) {
  // TODO: NSImageView → contentView
  (void)image;
  return valid_;
}

bool MacPinWindowBackend::SetTextContent(const char* text) {
  // TODO: NSTextField → contentView
  (void)text;
  return valid_;
}

void MacPinWindowBackend::SetPosition(int x, int y) { (void)x; (void)y; }
void MacPinWindowBackend::SetSize(int w, int h) { (void)w; (void)h; }
void MacPinWindowBackend::SetOpacity(float o) { opacity_ = o; }
float MacPinWindowBackend::GetOpacity() const { return opacity_; }
void MacPinWindowBackend::SetVisible(bool v) { (void)v; }
bool MacPinWindowBackend::IsVisible() const { return valid_; }
bool MacPinWindowBackend::ProcessEvents() { return valid_; }

std::unique_ptr<PinWindowBackend> CreatePlatformPinWindowBackend() {
  return std::make_unique<MacPinWindowBackend>();
}

}  // namespace internal
}  // namespace pixelgrab
