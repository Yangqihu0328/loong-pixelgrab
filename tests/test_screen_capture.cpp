// Copyright 2026 The loong-pixelgrab Authors
// Tests for: Screen, Capture, Window enumeration, Image accessors

#include <cstring>

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

// Fixture: provides an initialized context.
class ScreenCaptureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ctx_ = pixelgrab_context_create();
    ASSERT_NE(ctx_, nullptr);
  }
  void TearDown() override { pixelgrab_context_destroy(ctx_); }

  bool HasDisplay() const { return pixelgrab_get_screen_count(ctx_) > 0; }

  PixelGrabContext* ctx_ = nullptr;
};

// ---------------------------------------------------------------------------
// Screen info
// ---------------------------------------------------------------------------

TEST_F(ScreenCaptureTest, ScreenCountAtLeastOne) {
  if (!HasDisplay()) GTEST_SKIP() << "No display available";
  int count = pixelgrab_get_screen_count(ctx_);
  EXPECT_GE(count, 1);
}

TEST_F(ScreenCaptureTest, ScreenCountNullCtx) {
  EXPECT_EQ(pixelgrab_get_screen_count(nullptr), -1);
}

TEST_F(ScreenCaptureTest, GetScreenInfoPrimary) {
  if (!HasDisplay()) GTEST_SKIP() << "No display available";
  PixelGrabScreenInfo info = {};
  PixelGrabError err = pixelgrab_get_screen_info(ctx_, 0, &info);
  EXPECT_EQ(err, kPixelGrabOk);
  EXPECT_GT(info.width, 0);
  EXPECT_GT(info.height, 0);
}

TEST_F(ScreenCaptureTest, GetScreenInfoOutOfRange) {
  PixelGrabScreenInfo info = {};
  PixelGrabError err = pixelgrab_get_screen_info(ctx_, 999, &info);
  EXPECT_NE(err, kPixelGrabOk);
}

TEST_F(ScreenCaptureTest, GetScreenInfoNullOut) {
  PixelGrabError err = pixelgrab_get_screen_info(ctx_, 0, nullptr);
  EXPECT_NE(err, kPixelGrabOk);
}

// ---------------------------------------------------------------------------
// Capture screen
// ---------------------------------------------------------------------------

TEST_F(ScreenCaptureTest, CaptureScreen0) {
  if (!HasDisplay()) GTEST_SKIP() << "No display available";
  PixelGrabImage* img = pixelgrab_capture_screen(ctx_, 0);
  ASSERT_NE(img, nullptr);
  EXPECT_GT(pixelgrab_image_get_width(img), 0);
  EXPECT_GT(pixelgrab_image_get_height(img), 0);
  EXPECT_GT(pixelgrab_image_get_stride(img), 0);
  EXPECT_NE(pixelgrab_image_get_data(img), nullptr);
  EXPECT_GT(pixelgrab_image_get_data_size(img), 0u);
  pixelgrab_image_destroy(img);
}

TEST_F(ScreenCaptureTest, CaptureScreenNullCtx) {
  PixelGrabImage* img = pixelgrab_capture_screen(nullptr, 0);
  EXPECT_EQ(img, nullptr);
}

// ---------------------------------------------------------------------------
// Capture region
// ---------------------------------------------------------------------------

TEST_F(ScreenCaptureTest, CaptureRegionValid) {
  if (!HasDisplay()) GTEST_SKIP() << "No display available";
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 100, 100);
  ASSERT_NE(img, nullptr);
  EXPECT_EQ(pixelgrab_image_get_width(img), 100);
  EXPECT_EQ(pixelgrab_image_get_height(img), 100);
  pixelgrab_image_destroy(img);
}

TEST_F(ScreenCaptureTest, CaptureRegionInvalidSize) {
  EXPECT_EQ(pixelgrab_capture_region(ctx_, 0, 0, 0, 100), nullptr);
  EXPECT_EQ(pixelgrab_capture_region(ctx_, 0, 0, 100, -1), nullptr);
}

TEST_F(ScreenCaptureTest, CaptureRegionNullCtx) {
  EXPECT_EQ(pixelgrab_capture_region(nullptr, 0, 0, 100, 100), nullptr);
}

// ---------------------------------------------------------------------------
// Capture window
// ---------------------------------------------------------------------------

TEST_F(ScreenCaptureTest, CaptureWindowInvalidHandle) {
  EXPECT_EQ(pixelgrab_capture_window(ctx_, 0), nullptr);
}

TEST_F(ScreenCaptureTest, CaptureWindowNullCtx) {
  EXPECT_EQ(pixelgrab_capture_window(nullptr, 1), nullptr);
}

// ---------------------------------------------------------------------------
// Window enumeration
// ---------------------------------------------------------------------------

TEST_F(ScreenCaptureTest, EnumerateWindows) {
  if (!HasDisplay()) GTEST_SKIP() << "No display available";
  PixelGrabWindowInfo windows[64] = {};
  int count = pixelgrab_enumerate_windows(ctx_, windows, 64);
  EXPECT_GE(count, 0);
}

TEST_F(ScreenCaptureTest, EnumerateWindowsNullOut) {
  EXPECT_EQ(pixelgrab_enumerate_windows(ctx_, nullptr, 10), -1);
}

TEST_F(ScreenCaptureTest, EnumerateWindowsNullCtx) {
  PixelGrabWindowInfo windows[1] = {};
  EXPECT_EQ(pixelgrab_enumerate_windows(nullptr, windows, 1), -1);
}

// ---------------------------------------------------------------------------
// Image accessors — NULL safety
// ---------------------------------------------------------------------------

TEST_F(ScreenCaptureTest, ImageAccessorsNullSafe) {
  EXPECT_EQ(pixelgrab_image_get_width(nullptr), 0);
  EXPECT_EQ(pixelgrab_image_get_height(nullptr), 0);
  EXPECT_EQ(pixelgrab_image_get_stride(nullptr), 0);
  EXPECT_EQ(pixelgrab_image_get_data(nullptr), nullptr);
  EXPECT_EQ(pixelgrab_image_get_data_size(nullptr), 0u);
}

TEST_F(ScreenCaptureTest, ImageDestroyNullSafe) {
  pixelgrab_image_destroy(nullptr);  // Must not crash.
}

TEST_F(ScreenCaptureTest, ImageFormatIsBgra8) {
  if (!HasDisplay()) GTEST_SKIP() << "No display available";
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 10, 10);
  ASSERT_NE(img, nullptr);
  PixelGrabPixelFormat fmt = pixelgrab_image_get_format(img);
  // Default should be BGRA8 or Native.
  EXPECT_TRUE(fmt == kPixelGrabFormatBgra8 || fmt == kPixelGrabFormatNative);
  pixelgrab_image_destroy(img);
}

TEST_F(ScreenCaptureTest, ImageDataConsistency) {
  if (!HasDisplay()) GTEST_SKIP() << "No display available";
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 50, 30);
  ASSERT_NE(img, nullptr);
  int w = pixelgrab_image_get_width(img);
  int h = pixelgrab_image_get_height(img);
  int stride = pixelgrab_image_get_stride(img);
  size_t data_size = pixelgrab_image_get_data_size(img);
  EXPECT_EQ(w, 50);
  EXPECT_EQ(h, 30);
  EXPECT_GE(stride, w * 4);  // At least 4 bytes per pixel.
  EXPECT_EQ(data_size, static_cast<size_t>(stride * h));
  pixelgrab_image_destroy(img);
}
