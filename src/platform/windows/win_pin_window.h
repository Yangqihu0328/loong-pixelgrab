// Copyright 2024 PixelGrab Authors. All rights reserved.
// Windows floating pin window backend.

#ifndef PIXELGRAB_PLATFORM_WINDOWS_WIN_PIN_WINDOW_H_
#define PIXELGRAB_PLATFORM_WINDOWS_WIN_PIN_WINDOW_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/image.h"
#include "pin/pin_window_backend.h"

namespace pixelgrab {
namespace internal {

/// Windows implementation of PinWindowBackend using Win32 HWND.
class WinPinWindowBackend : public PinWindowBackend {
 public:
  WinPinWindowBackend();
  ~WinPinWindowBackend() override;

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
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                  LPARAM lparam);
  static bool RegisterWindowClass();
  static bool class_registered_;

  void Paint(HDC hdc);

  HWND hwnd_ = nullptr;
  float opacity_ = 1.0f;
  bool visible_ = true;

  // Content storage.
  PinContentType content_type_ = PinContentType::kImage;
  HBITMAP bitmap_ = nullptr;
  int bitmap_width_ = 0;
  int bitmap_height_ = 0;
  std::string text_content_;

  // Cached copy of image data for GetImageContent().
  int image_copy_width_ = 0;
  int image_copy_height_ = 0;
  int image_copy_stride_ = 0;
  PixelGrabPixelFormat image_copy_format_ = kPixelGrabFormatBgra8;
  std::vector<uint8_t> image_copy_data_;

  // Drag support.
  bool dragging_ = false;
  POINT drag_start_ = {};
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PLATFORM_WINDOWS_WIN_PIN_WINDOW_H_
