// Copyright 2026 The loong-pixelgrab Authors
//
// macOS translation backend — stub (not yet implemented).

#ifdef __APPLE__

#include "translate/translate_backend.h"

namespace pixelgrab {
namespace internal {

class MacTranslateBackend : public TranslateBackend {
 protected:
  std::string HttpPost(const std::string& /*url*/,
                       const std::string& /*body*/) override {
    return {};
  }

  std::string ComputeMD5(const std::string& /*input*/) override { return {}; }
};

std::unique_ptr<TranslateBackend> CreatePlatformTranslateBackend() {
  return std::make_unique<MacTranslateBackend>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // __APPLE__
