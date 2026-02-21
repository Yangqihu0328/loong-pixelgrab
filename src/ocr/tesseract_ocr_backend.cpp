// Copyright 2026 The loong-pixelgrab Authors
//
// Tesseract OCR backend — cross-platform, high-accuracy text recognition.

#include "ocr/ocr_backend.h"

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/logger.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace pixelgrab {
namespace internal {

namespace {

// Locate the tessdata directory next to the running executable.
std::string FindTessDataPath() {
  std::string exe_dir;

#ifdef _WIN32
  wchar_t wpath[MAX_PATH] = {};
  DWORD len = GetModuleFileNameW(nullptr, wpath, MAX_PATH);
  if (len > 0) {
    char utf8[MAX_PATH * 3] = {};
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, utf8, sizeof(utf8),
                        nullptr, nullptr);
    exe_dir = utf8;
  }
#elif defined(__APPLE__)
  char path[PATH_MAX] = {};
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0) {
    char resolved[PATH_MAX] = {};
    if (realpath(path, resolved)) exe_dir = resolved;
    else exe_dir = path;
  }
#else
  char path[PATH_MAX] = {};
  ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (n > 0) {
    path[n] = '\0';
    exe_dir = path;
  }
#endif

  // Strip filename, keep directory.
  auto pos = exe_dir.find_last_of("/\\");
  if (pos != std::string::npos) exe_dir.resize(pos);

  std::string tessdata = exe_dir + "/tessdata";

  // Reject suspicious path components (path traversal defense).
  if (tessdata.find("..") != std::string::npos) {
    PIXELGRAB_LOG_WARN("Rejected tessdata path with '..' component: {}",
                       tessdata);
    return {};
  }

  // Check if the directory exists.
#ifdef _WIN32
  DWORD attr = GetFileAttributesA(tessdata.c_str());
  if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
    return tessdata;
#else
  if (access(tessdata.c_str(), F_OK) == 0) return tessdata;
#endif

  // Fallback: TESSDATA_PREFIX environment variable.
#ifdef _WIN32
  char* env = nullptr;
  size_t env_len = 0;
  if (_dupenv_s(&env, &env_len, "TESSDATA_PREFIX") == 0 && env) {
    std::string result(env);
    free(env);
    if (!result.empty()) return result;
  }
#else
  const char* env = std::getenv("TESSDATA_PREFIX");
  if (env && env[0]) return env;
#endif

  // Fallback: common system paths.
#ifdef _WIN32
  return exe_dir;
#else
  return "/usr/share/tesseract-ocr/5/tessdata";
#endif
}

// Map BCP-47 language tag to Tesseract language code(s).
// Returns "chi_sim+eng" for Chinese, "eng" for English, etc.
std::string MapLanguage(const char* bcp47) {
  if (!bcp47 || !bcp47[0]) return "chi_sim+eng";

  std::string lang(bcp47);
  std::transform(lang.begin(), lang.end(), lang.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (lang.find("zh") != std::string::npos ||
      lang.find("chi") != std::string::npos ||
      lang.find("cn") != std::string::npos) {
    if (lang.find("tra") != std::string::npos ||
        lang.find("hant") != std::string::npos) {
      return "chi_tra+eng";
    }
    return "chi_sim+eng";
  }
  if (lang.find("ja") == 0 || lang.find("jpn") != std::string::npos)
    return "jpn+eng";
  if (lang.find("ko") == 0 || lang.find("kor") != std::string::npos)
    return "kor+eng";
  if (lang.find("en") == 0 || lang.find("eng") != std::string::npos)
    return "eng";

  return "chi_sim+eng";
}

// Convert BGRA8 pixel data to 8-bit grayscale.
std::vector<uint8_t> BgraToGray(const uint8_t* bgra, int width, int height,
                                int stride) {
  if (width <= 0 || height <= 0 || stride < width * 4) return {};
  size_t total = static_cast<size_t>(width) * static_cast<size_t>(height);
  if (total > 256ULL * 1024 * 1024) return {};
  std::vector<uint8_t> gray(total);
  for (int y = 0; y < height; ++y) {
    const uint8_t* row = bgra + y * stride;
    uint8_t* dst = gray.data() + y * width;
    for (int x = 0; x < width; ++x) {
      int b = row[x * 4 + 0];
      int g = row[x * 4 + 1];
      int r = row[x * 4 + 2];
      dst[x] = static_cast<uint8_t>((r * 77 + g * 150 + b * 29) >> 8);
    }
  }
  return gray;
}

}  // namespace

class TesseractOcrBackend : public OcrBackend {
 public:
  TesseractOcrBackend() : tessdata_path_(FindTessDataPath()) {
    PIXELGRAB_LOG_INFO("Tesseract tessdata path: {}", tessdata_path_);
  }

  ~TesseractOcrBackend() override = default;

  bool IsSupported() const override {
    // Check if at least eng.traineddata exists.
    std::string eng_path = tessdata_path_ + "/eng.traineddata";
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(eng_path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES;
#else
    return access(eng_path.c_str(), F_OK) == 0;
#endif
  }

  std::string RecognizeText(const uint8_t* bgra_data, int width, int height,
                            int stride, const char* language) override {
    if (!bgra_data || width <= 0 || height <= 0 || stride <= 0) return {};

    std::string tess_lang = MapLanguage(language);

    auto api = std::make_unique<tesseract::TessBaseAPI>();
    int init_result = api->Init(tessdata_path_.c_str(), tess_lang.c_str(),
                                tesseract::OEM_LSTM_ONLY);
    if (init_result != 0) {
      PIXELGRAB_LOG_ERROR("Tesseract Init failed (lang={}, path={})",
                          tess_lang, tessdata_path_);
      return {};
    }

    // PSM 6: Assume a single uniform block of text — ideal for screenshots.
    api->SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);

    std::vector<uint8_t> gray = BgraToGray(bgra_data, width, height, stride);
    api->SetImage(gray.data(), width, height, 1, width);

    std::unique_ptr<char[]> text(api->GetUTF8Text());
    api->End();

    if (!text) return {};

    std::string result(text.get());

    // Trim trailing whitespace/newlines.
    while (!result.empty() &&
           (result.back() == '\n' || result.back() == '\r' ||
            result.back() == ' ')) {
      result.pop_back();
    }

    PIXELGRAB_LOG_DEBUG("Tesseract recognized {} chars", result.size());
    return result;
  }

 private:
  std::string tessdata_path_;
};

std::unique_ptr<OcrBackend> CreatePlatformOcrBackend() {
  return std::make_unique<TesseractOcrBackend>();
}

}  // namespace internal
}  // namespace pixelgrab
