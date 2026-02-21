// Copyright 2026 The loong-pixelgrab Authors
//
// Stub OCR backend — used when PIXELGRAB_ENABLE_OCR is OFF.

#include "ocr/ocr_backend.h"

namespace pixelgrab {
namespace internal {

class StubOcrBackend : public OcrBackend {
 public:
  bool IsSupported() const override { return false; }

  std::string RecognizeText(const uint8_t*, int, int, int,
                            const char*) override {
    return {};
  }
};

std::unique_ptr<OcrBackend> CreatePlatformOcrBackend() {
  return std::make_unique<StubOcrBackend>();
}

}  // namespace internal
}  // namespace pixelgrab
