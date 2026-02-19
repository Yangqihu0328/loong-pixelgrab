// Copyright 2026 The loong-pixelgrab Authors
//
// Linux OCR backend — stub (not yet implemented).

#if defined(__linux__)

#include "ocr/ocr_backend.h"

namespace pixelgrab {
namespace internal {

class X11OcrBackend : public OcrBackend {
 public:
  bool IsSupported() const override { return false; }

  std::string RecognizeText(const uint8_t* /*bgra_data*/, int /*width*/,
                            int /*height*/, int /*stride*/,
                            const char* /*language*/) override {
    return {};
  }
};

std::unique_ptr<OcrBackend> CreatePlatformOcrBackend() {
  return std::make_unique<X11OcrBackend>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // __linux__
