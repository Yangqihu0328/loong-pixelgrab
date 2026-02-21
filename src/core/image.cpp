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

static constexpr size_t kMaxImageBytes = 256ULL * 1024 * 1024;  // 256 MB

// static
std::unique_ptr<Image> Image::Create(int width, int height,
                                     PixelGrabPixelFormat format) {
  if (width <= 0 || height <= 0) return nullptr;
  int bpp = BytesPerPixel(format);
  int stride = width * bpp;
  size_t total = static_cast<size_t>(stride) * static_cast<size_t>(height);
  if (total > kMaxImageBytes) return nullptr;
  std::vector<uint8_t> data(total, 0);
  return std::make_unique<Image>(width, height, stride, format,
                                 std::move(data));
}

// static
std::unique_ptr<Image> Image::CreateFromData(int width, int height, int stride,
                                             PixelGrabPixelFormat format,
                                             std::vector<uint8_t> data) {
  if (width <= 0 || height <= 0 || stride <= 0) return nullptr;
  size_t required = static_cast<size_t>(stride) * static_cast<size_t>(height);
  if (data.size() < required) return nullptr;
  return std::make_unique<Image>(width, height, stride, format,
                                 std::move(data));
}

std::unique_ptr<Image> Image::Clone() const {
  std::vector<uint8_t> data_copy(data_);
  return std::make_unique<Image>(width_, height_, stride_, format_,
                                 std::move(data_copy));
}

}  // namespace internal
}  // namespace pixelgrab
