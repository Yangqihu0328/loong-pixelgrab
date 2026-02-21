// Copyright 2026 The loong-pixelgrab Authors
// Tests for: Translation API

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

class TranslateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ctx_ = pixelgrab_context_create();
    ASSERT_NE(ctx_, nullptr);
  }

  void TearDown() override { pixelgrab_context_destroy(ctx_); }

  PixelGrabContext* ctx_ = nullptr;
};

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

TEST_F(TranslateTest, SetConfigNullCtx) {
  PixelGrabError err =
      pixelgrab_translate_set_config(nullptr, "baidu", "id", "key");
  EXPECT_EQ(err, kPixelGrabErrorInvalidParam);
}

TEST_F(TranslateTest, SetConfigNullAppId) {
  PixelGrabError err =
      pixelgrab_translate_set_config(ctx_, "baidu", nullptr, "key");
  EXPECT_EQ(err, kPixelGrabErrorInvalidParam);
}

TEST_F(TranslateTest, SetConfigNullSecretKey) {
  PixelGrabError err =
      pixelgrab_translate_set_config(ctx_, "baidu", "id", nullptr);
  EXPECT_EQ(err, kPixelGrabErrorInvalidParam);
}

TEST_F(TranslateTest, SetConfigSuccess) {
  PixelGrabError err =
      pixelgrab_translate_set_config(ctx_, "baidu", "test_id", "test_key");
  EXPECT_EQ(err, kPixelGrabOk);
}

TEST_F(TranslateTest, SetConfigDefaultProvider) {
  PixelGrabError err =
      pixelgrab_translate_set_config(ctx_, nullptr, "test_id", "test_key");
  EXPECT_EQ(err, kPixelGrabOk);
}

// ---------------------------------------------------------------------------
// Is supported
// ---------------------------------------------------------------------------

TEST_F(TranslateTest, IsSupportedNullCtx) {
  EXPECT_EQ(pixelgrab_translate_is_supported(nullptr), 0);
}

TEST_F(TranslateTest, NotSupportedWithoutConfig) {
  EXPECT_EQ(pixelgrab_translate_is_supported(ctx_), 0);
}

TEST_F(TranslateTest, SupportedAfterConfig) {
  pixelgrab_translate_set_config(ctx_, "baidu", "id", "key");
#ifdef PIXELGRAB_HAS_TRANSLATE
  EXPECT_NE(pixelgrab_translate_is_supported(ctx_), 0);
#else
  EXPECT_EQ(pixelgrab_translate_is_supported(ctx_), 0);
#endif
}

// ---------------------------------------------------------------------------
// Translate text
// ---------------------------------------------------------------------------

TEST_F(TranslateTest, TranslateNullCtx) {
  char* result = nullptr;
  PixelGrabError err =
      pixelgrab_translate_text(nullptr, "hello", "en", "zh", &result);
  EXPECT_NE(err, kPixelGrabOk);
}

TEST_F(TranslateTest, TranslateNullText) {
  char* result = nullptr;
  PixelGrabError err =
      pixelgrab_translate_text(ctx_, nullptr, "en", "zh", &result);
  EXPECT_EQ(err, kPixelGrabErrorInvalidParam);
}

TEST_F(TranslateTest, TranslateNullTargetLang) {
  char* result = nullptr;
  PixelGrabError err =
      pixelgrab_translate_text(ctx_, "hello", "en", nullptr, &result);
  EXPECT_EQ(err, kPixelGrabErrorInvalidParam);
}

TEST_F(TranslateTest, TranslateNullOutPointer) {
  PixelGrabError err =
      pixelgrab_translate_text(ctx_, "hello", "en", "zh", nullptr);
  EXPECT_EQ(err, kPixelGrabErrorInvalidParam);
}

TEST_F(TranslateTest, TranslateWithoutConfig) {
  char* result = nullptr;
  PixelGrabError err =
      pixelgrab_translate_text(ctx_, "hello", "en", "zh", &result);
  EXPECT_EQ(err, kPixelGrabErrorNotSupported);
  EXPECT_EQ(result, nullptr);
}

TEST_F(TranslateTest, TranslateWithInvalidKeys) {
  pixelgrab_translate_set_config(ctx_, "baidu", "invalid", "invalid");
  char* result = nullptr;
  PixelGrabError err =
      pixelgrab_translate_text(ctx_, "hello", "en", "zh", &result);
  // Will fail due to network or auth error, but should not crash.
  if (err == kPixelGrabOk && result) {
    pixelgrab_free_string(result);
  }
}
