// Copyright 2026 The loong-pixelgrab Authors
// Linux X11 floating pin window backend.

#ifndef PIXELGRAB_PLATFORM_LINUX_X11_PIN_WINDOW_H_
#define PIXELGRAB_PLATFORM_LINUX_X11_PIN_WINDOW_H_

#include <memory>

#include "core/image.h"
#include "pin/pin_window_backend.h"

typedef struct _XDisplay Display;
typedef unsigned long XID;

namespace pixelgrab {
namespace internal {

class X11PinWindowBackend : public PinWindowBackend {
 public:
  X11PinWindowBackend();
  ~X11PinWindowBackend() override;

  bool Create(const PinWindowConfig& config) override;
  void Destroy() override;
  bool IsValid() const override;
  bool SetImageContent(const Image* image) override;
  bool SetTextContent(const char* text) override;
  std::unique_ptr<Image> GetImageContent() const override;
  void SetPosition(int x, int y) override;
  void SetSize(int width, int height) override;
  void SetOpacity(float opacity) override;
  float GetOpacity() const override;
  void SetVisible(bool visible) override;
  bool IsVisible() const override;
  void GetPosition(int* out_x, int* out_y) const override;
  void GetSize(int* out_width, int* out_height) const override;
  void* GetNativeHandle() const override;
  bool ProcessEvents() override;

 private:
  void Repaint();

  Display* display_ = nullptr;
  XID window_ = 0;
  void* gc_ = nullptr;
  int x_ = 0, y_ = 0;
  int width_ = 1, height_ = 1;
  float opacity_ = 1.0f;
  bool visible_ = false;
  bool valid_ = false;
  std::unique_ptr<Image> content_;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PLATFORM_LINUX_X11_PIN_WINDOW_H_
