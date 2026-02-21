// Copyright 2026 The loong-pixelgrab Authors
// Tests for: OCR API

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

class OcrTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ctx_ = pixelgrab_context_create();
    ASSERT_NE(ctx_, nullptr);
  }

  void TearDown() override { pixelgrab_context_destroy(ctx_); }

  PixelGrabContext* ctx_ = nullptr;
};

TEST_F(OcrTest, IsSupportedDoesNotCrash) {
  int result = pixelgrab_ocr_is_supported(ctx_);
  EXPECT_GE(result, 0);  // 0 or 1
  EXPECT_LE(result, 1);
}

TEST_F(OcrTest, IsSupportedNullCtx) {
  EXPECT_EQ(pixelgrab_ocr_is_supported(nullptr), 0);
}

TEST_F(OcrTest, RecognizeNullCtx) {
  char* text = nullptr;
  PixelGrabError err = pixelgrab_ocr_recognize(nullptr, nullptr, nullptr, &text);
  EXPECT_NE(err, kPixelGrabOk);
  EXPECT_EQ(text, nullptr);
}

TEST_F(OcrTest, RecognizeNullImage) {
  char* text = nullptr;
  PixelGrabError err = pixelgrab_ocr_recognize(ctx_, nullptr, nullptr, &text);
  EXPECT_EQ(err, kPixelGrabErrorInvalidParam);
  EXPECT_EQ(text, nullptr);
}

TEST_F(OcrTest, RecognizeNullOutText) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 32, 32);
  if (!img) return;  // capture may fail on CI
  PixelGrabError err = pixelgrab_ocr_recognize(ctx_, img, nullptr, nullptr);
  EXPECT_EQ(err, kPixelGrabErrorInvalidParam);
  pixelgrab_image_destroy(img);
}

TEST_F(OcrTest, RecognizeWithImage) {
  if (!pixelgrab_ocr_is_supported(ctx_)) {
    GTEST_SKIP() << "OCR not supported on this system";
  }

  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 200, 100);
  if (!img) {
    GTEST_SKIP() << "Screen capture not available";
  }

  char* text = nullptr;
  PixelGrabError err = pixelgrab_ocr_recognize(ctx_, img, "en-US", &text);
  // May succeed or fail depending on image content; just verify no crash.
  if (err == kPixelGrabOk) {
    EXPECT_NE(text, nullptr);
    pixelgrab_free_string(text);
  }
  pixelgrab_image_destroy(img);
}

TEST_F(OcrTest, RecognizeWithLanguageNull) {
  if (!pixelgrab_ocr_is_supported(ctx_)) {
    GTEST_SKIP() << "OCR not supported on this system";
  }

  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 100, 50);
  if (!img) {
    GTEST_SKIP() << "Screen capture not available";
  }

  char* text = nullptr;
  PixelGrabError err = pixelgrab_ocr_recognize(ctx_, img, nullptr, &text);
  if (err == kPixelGrabOk && text) {
    pixelgrab_free_string(text);
  }
  pixelgrab_image_destroy(img);
}
