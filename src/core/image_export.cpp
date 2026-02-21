// Copyright 2026 The loong-pixelgrab Authors
//
// Image file export using stb_image_write (PNG, JPEG, BMP).

#include "pixelgrab/pixelgrab.h"

#include <cstring>
#include <memory>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)  // sprintf deprecation in stb
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "core/image.h"

using pixelgrab::internal::Image;

struct PixelGrabImage;  // Forward declaration (defined in pixelgrab_api.cpp).

// Access the internal Image from the opaque handle.
// Must match the layout in pixelgrab_api.cpp.
namespace {
const Image* GetImpl(const PixelGrabImage* img) {
  if (!img) return nullptr;
  // PixelGrabImage has a single member: std::unique_ptr<Image> impl.
  // We access it via the same struct layout.
  const auto* p = reinterpret_cast<const std::unique_ptr<Image>*>(img);
  return p->get();
}
}  // namespace

PixelGrabError pixelgrab_image_export(const PixelGrabImage* image,
                                      const char* path,
                                      PixelGrabImageFormat format,
                                      int quality) {
  if (!image || !path) return kPixelGrabErrorInvalidParam;
  const Image* img = GetImpl(image);
  if (!img) return kPixelGrabErrorInvalidParam;

  int w = img->width();
  int h = img->height();
  int stride = img->stride();
  const uint8_t* src = img->data();

  // Convert BGRA → RGBA for stb_image_write.
  std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
  for (int y = 0; y < h; ++y) {
    const uint8_t* row = src + y * stride;
    uint8_t* dst = rgba.data() + y * w * 4;
    for (int x = 0; x < w; ++x) {
      dst[x * 4 + 0] = row[x * 4 + 2];  // R ← B
      dst[x * 4 + 1] = row[x * 4 + 1];  // G
      dst[x * 4 + 2] = row[x * 4 + 0];  // B ← R
      dst[x * 4 + 3] = row[x * 4 + 3];  // A
    }
  }

  int result = 0;
  switch (format) {
    case kPixelGrabImageFormatPng:
      result = stbi_write_png(path, w, h, 4, rgba.data(), w * 4);
      break;
    case kPixelGrabImageFormatJpeg:
      if (quality <= 0 || quality > 100) quality = 90;
      result = stbi_write_jpg(path, w, h, 4, rgba.data(), quality);
      break;
    case kPixelGrabImageFormatBmp:
      result = stbi_write_bmp(path, w, h, 4, rgba.data());
      break;
    default:
      return kPixelGrabErrorInvalidParam;
  }

  return result ? kPixelGrabOk : kPixelGrabErrorCaptureFailed;
}
