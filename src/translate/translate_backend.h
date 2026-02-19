// Copyright 2026 The loong-pixelgrab Authors
//
// Abstract translation backend with shared API logic (template method pattern).
// Platform subclasses implement HttpPost() and ComputeMD5().

#ifndef PIXELGRAB_TRANSLATE_TRANSLATE_BACKEND_H_
#define PIXELGRAB_TRANSLATE_TRANSLATE_BACKEND_H_

#include <cstdint>
#include <memory>
#include <string>

namespace pixelgrab {
namespace internal {

struct TranslateConfig {
  std::string provider = "baidu";
  std::string app_id;
  std::string secret_key;
};

class TranslateBackend {
 public:
  virtual ~TranslateBackend() = default;

  void SetConfig(const TranslateConfig& config) { config_ = config; }
  const TranslateConfig& config() const { return config_; }

  /// Last error detail (human-readable reason for the most recent failure).
  const std::string& last_error_detail() const { return last_error_detail_; }

  /// Check if translation is available (config must be set with valid keys).
  bool IsSupported() const;

  /// Translate text from one language to another.
  /// @param text   UTF-8 text to translate.
  /// @param from   Source language code (e.g. "en", "zh", "auto").
  /// @param to     Target language code (e.g. "zh", "en").
  /// @return Translated UTF-8 text, or empty string on failure.
  std::string Translate(const char* text, const char* from, const char* to);

 protected:
  /// Platform-specific HTTP POST. Returns response body or empty on error.
  virtual std::string HttpPost(const std::string& url,
                               const std::string& body) = 0;

  /// Platform-specific MD5 hash. Returns lowercase hex digest (32 chars).
  virtual std::string ComputeMD5(const std::string& input) = 0;

  TranslateConfig config_;
  std::string last_error_detail_;

 private:
  std::string BuildBaiduBody(const char* text, const char* from,
                             const char* to);
  static std::string UrlEncode(const std::string& str);
  std::string ParseBaiduResponse(const std::string& json);
  static std::string GenerateSalt();
};

/// Create the platform-specific translate backend.
std::unique_ptr<TranslateBackend> CreatePlatformTranslateBackend();

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_TRANSLATE_TRANSLATE_BACKEND_H_
