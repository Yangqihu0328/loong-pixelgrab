// Copyright 2026 The loong-pixelgrab Authors
//
// Abstract OCR backend interface.

#ifndef PIXELGRAB_OCR_OCR_BACKEND_H_
#define PIXELGRAB_OCR_OCR_BACKEND_H_

#include <cstdint>
#include <memory>
#include <string>

namespace pixelgrab {
namespace internal {

class OcrBackend {
 public:
  virtual ~OcrBackend() = default;

  /// Check if OCR is supported on this platform.
  virtual bool IsSupported() const = 0;

  /// Recognize text from BGRA8 pixel data.
  /// @param bgra_data  Pointer to BGRA8 pixel buffer.
  /// @param width      Image width in pixels.
  /// @param height     Image height in pixels.
  /// @param stride     Row stride in bytes.
  /// @param language   BCP-47 language tag (e.g. "zh-Hans-CN", "en-US").
  ///                   Empty or null for auto-detect from user profile.
  /// @return Recognized text as UTF-8, or empty string on failure.
  virtual std::string RecognizeText(const uint8_t* bgra_data, int width,
                                    int height, int stride,
                                    const char* language) = 0;
};

/// Create the platform-specific OCR backend.
std::unique_ptr<OcrBackend> CreatePlatformOcrBackend();

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_OCR_OCR_BACKEND_H_
