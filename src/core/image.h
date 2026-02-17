// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_CORE_IMAGE_H_
#define PIXELGRAB_CORE_IMAGE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "pixelgrab/pixelgrab.h"

namespace pixelgrab {
namespace internal {

/// Internal image representation holding captured pixel data.
class Image {
 public:
  Image(int width, int height, int stride, PixelGrabPixelFormat format,
        std::vector<uint8_t> data);
  ~Image() = default;

  // Non-copyable, movable.
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;
  Image(Image&&) = default;
  Image& operator=(Image&&) = default;

  int width() const { return width_; }
  int height() const { return height_; }
  int stride() const { return stride_; }
  PixelGrabPixelFormat format() const { return format_; }
  const uint8_t* data() const { return data_.data(); }
  size_t data_size() const { return data_.size(); }

  /// Create an image with pre-allocated buffer (to be filled by caller).
  static std::unique_ptr<Image> Create(int width, int height,
                                       PixelGrabPixelFormat format);

  /// Create an image from existing data (takes ownership via move).
  static std::unique_ptr<Image> CreateFromData(int width, int height,
                                               int stride,
                                               PixelGrabPixelFormat format,
                                               std::vector<uint8_t> data);

  /// Get a mutable pointer to pixel data (for backends to fill).
  uint8_t* mutable_data() { return data_.data(); }

 private:
  int width_;
  int height_;
  int stride_;
  PixelGrabPixelFormat format_;
  std::vector<uint8_t> data_;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_CORE_IMAGE_H_
