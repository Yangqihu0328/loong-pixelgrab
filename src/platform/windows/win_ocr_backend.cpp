// Copyright 2026 The loong-pixelgrab Authors
//
// Windows OCR backend using Windows.Media.Ocr (WinRT).

#ifdef _WIN32

#include "ocr/ocr_backend.h"

#include <windows.h>
#include <roapi.h>
#include <winstring.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>

#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include "core/logger.h"

// IMemoryBufferByteAccess — standard COM interface for accessing raw bytes
// of an IMemoryBufferReference. Declared inline to avoid header dependency.
struct __declspec(uuid("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d"))
    __declspec(novtable) IMemoryBufferByteAccess : ::IUnknown {
  virtual HRESULT __stdcall GetBuffer(uint8_t** value,
                                      uint32_t* capacity) = 0;
};

namespace pixelgrab {
namespace internal {

namespace {
namespace wf = winrt::Windows::Foundation;
namespace wg = winrt::Windows::Globalization;
namespace wgi = winrt::Windows::Graphics::Imaging;
namespace wmo = winrt::Windows::Media::Ocr;
}  // namespace

class WinOcrBackend : public OcrBackend {
 public:
  bool IsSupported() const override {
    try {
      auto engine = wmo::OcrEngine::TryCreateFromUserProfileLanguages();
      return engine != nullptr;
    } catch (...) {
      return false;
    }
  }

  std::string RecognizeText(const uint8_t* bgra_data, int width, int height,
                            int stride, const char* language) override {
    if (!bgra_data || width <= 0 || height <= 0 || stride <= 0) {
      return {};
    }

    // Run OCR on a dedicated MTA thread to avoid STA deadlock on the
    // Win32 UI thread. The .get() call blocks the worker thread while
    // WinRT completes the async recognition.
    std::string result;
    std::thread worker([&] {
      try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);

        wgi::SoftwareBitmap bitmap = CreateBitmap(bgra_data, width, height,
                                                  stride);

        wmo::OcrEngine engine{nullptr};
        if (language && language[0]) {
          int wlen = MultiByteToWideChar(CP_UTF8, 0, language, -1, nullptr, 0);
          std::wstring wlang(wlen - 1, L'\0');
          MultiByteToWideChar(CP_UTF8, 0, language, -1, wlang.data(), wlen);
          wg::Language lang(wlang);
          engine = wmo::OcrEngine::TryCreateFromLanguage(lang);
        }
        if (!engine) {
          engine = wmo::OcrEngine::TryCreateFromUserProfileLanguages();
        }
        if (!engine) {
          PIXELGRAB_LOG_ERROR("Failed to create OCR engine");
          return;
        }

        auto ocr_result = engine.RecognizeAsync(bitmap).get();
        result = winrt::to_string(ocr_result.Text());
      } catch (const winrt::hresult_error& e) {
        PIXELGRAB_LOG_ERROR("WinRT OCR error: {}",
                            winrt::to_string(e.message()));
      } catch (...) {
        PIXELGRAB_LOG_ERROR("OCR recognition failed with unknown error");
      }
    });
    worker.join();
    return result;
  }

 private:
  static wgi::SoftwareBitmap CreateBitmap(const uint8_t* bgra_data, int width,
                                          int height, int stride) {
    wgi::SoftwareBitmap bitmap(wgi::BitmapPixelFormat::Bgra8, width, height,
                               wgi::BitmapAlphaMode::Premultiplied);
    {
      auto buffer = bitmap.LockBuffer(wgi::BitmapBufferAccessMode::Write);
      auto ref = buffer.CreateReference();
      auto byte_access = ref.as<::IMemoryBufferByteAccess>();

      uint8_t* dest = nullptr;
      uint32_t capacity = 0;
      winrt::check_hresult(byte_access->GetBuffer(&dest, &capacity));

      int row_bytes = width * 4;
      for (int y = 0; y < height; ++y) {
        std::memcpy(dest + y * row_bytes, bgra_data + y * stride, row_bytes);
      }
    }
    return bitmap;
  }
};

std::unique_ptr<OcrBackend> CreatePlatformOcrBackend() {
  return std::make_unique<WinOcrBackend>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // _WIN32
