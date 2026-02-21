// Copyright 2026 The loong-pixelgrab Authors
//
// Stub translation backend — used when PIXELGRAB_ENABLE_TRANSLATE is OFF.

#include "translate/translate_backend.h"

namespace pixelgrab {
namespace internal {

bool TranslateBackend::IsSupported() const { return false; }

std::string TranslateBackend::Translate(const char*, const char*,
                                        const char*) {
  last_error_detail_ = "Translation support not compiled";
  return {};
}

class StubTranslateBackend : public TranslateBackend {
 protected:
  std::string HttpPost(const std::string&, const std::string&) override {
    return {};
  }
  std::string ComputeMD5(const std::string&) override { return {}; }
};

std::unique_ptr<TranslateBackend> CreatePlatformTranslateBackend() {
  return std::make_unique<StubTranslateBackend>();
}

}  // namespace internal
}  // namespace pixelgrab
