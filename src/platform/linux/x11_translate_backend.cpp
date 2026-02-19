// Copyright 2026 The loong-pixelgrab Authors
//
// Linux translation backend — stub (not yet implemented).

#if defined(__linux__)

#include "translate/translate_backend.h"

namespace pixelgrab {
namespace internal {

class X11TranslateBackend : public TranslateBackend {
 protected:
  std::string HttpPost(const std::string& /*url*/,
                       const std::string& /*body*/) override {
    return {};
  }

  std::string ComputeMD5(const std::string& /*input*/) override { return {}; }
};

std::unique_ptr<TranslateBackend> CreatePlatformTranslateBackend() {
  return std::make_unique<X11TranslateBackend>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // __linux__
