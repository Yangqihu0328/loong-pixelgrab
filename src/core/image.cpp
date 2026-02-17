// Copyright 2026 The loong-pixelgrab Authors

#include "core/image.h"

#include <cassert>
#include <utility>

namespace pixelgrab {
namespace internal {

namespace {

int BytesPerPixel(PixelGrabPixelFormat format) {
  switch (format) {
    case kPixelGrabFormatBgra8:
    case kPixelGrabFormatRgba8:
    case kPixelGrabFormatNative:
      return 4;
    default:
      return 4;
  }
}

}  // namespace

Image::Image(int width, int height, int stride, PixelGrabPixelFormat format,
             std::vector<uint8_t> data)
    : width_(width),
      height_(height),
      stride_(stride),
      format_(format),
      data_(std::move(data)) {}

// static
std::unique_ptr<Image> Image::Create(int width, int height,
                                     PixelGrabPixelFormat format) {
  int bpp = BytesPerPixel(format);
  int stride = width * bpp;
  std::vector<uint8_t> data(static_cast<size_t>(stride) * height, 0);
  return std::make_unique<Image>(width, height, stride, format,
                                 std::move(data));
}

// static
std::unique_ptr<Image> Image::CreateFromData(int width, int height, int stride,
                                             PixelGrabPixelFormat format,
                                             std::vector<uint8_t> data) {
  assert(data.size() >= static_cast<size_t>(stride) * height);
  return std::make_unique<Image>(width, height, stride, format,
                                 std::move(data));
}

}  // namespace internal
}  // namespace pixelgrab
