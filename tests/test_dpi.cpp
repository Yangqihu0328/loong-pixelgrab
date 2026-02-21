// Copyright 2026 The loong-pixelgrab Authors
// Tests for: pixelgrab_enable_dpi_awareness, pixelgrab_get_dpi_info,
//            pixelgrab_logical_to_physical, pixelgrab_physical_to_logical

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

class DpiTest : public ::testing::Test {
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
// DPI awareness
// ---------------------------------------------------------------------------

TEST_F(DpiTest, EnableDpiAwareness) {
  if (!HasDisplay()) GTEST_SKIP() << "No display available";
  PixelGrabError err = pixelgrab_enable_dpi_awareness(ctx_);
  EXPECT_EQ(err, kPixelGrabOk);
}

TEST_F(DpiTest, EnableDpiAwarenessNullCtx) {
  PixelGrabError err = pixelgrab_enable_dpi_awareness(nullptr);
  EXPECT_NE(err, kPixelGrabOk);
}

// ---------------------------------------------------------------------------
// DPI info
// ---------------------------------------------------------------------------

TEST_F(DpiTest, GetDpiInfoScreen0) {
  if (!HasDisplay()) GTEST_SKIP() << "No display available";
  pixelgrab_enable_dpi_awareness(ctx_);
  PixelGrabDpiInfo info = {};
  PixelGrabError err = pixelgrab_get_dpi_info(ctx_, 0, &info);
  EXPECT_EQ(err, kPixelGrabOk);
  EXPECT_GT(info.scale_x, 0.0f);
  EXPECT_GT(info.scale_y, 0.0f);
  EXPECT_GT(info.dpi_x, 0);
  EXPECT_GT(info.dpi_y, 0);
}

TEST_F(DpiTest, GetDpiInfoNullOut) {
  PixelGrabError err = pixelgrab_get_dpi_info(ctx_, 0, nullptr);
  EXPECT_NE(err, kPixelGrabOk);
}

TEST_F(DpiTest, GetDpiInfoInvalidScreen) {
  PixelGrabDpiInfo info = {};
  PixelGrabError err = pixelgrab_get_dpi_info(ctx_, 999, &info);
  EXPECT_NE(err, kPixelGrabOk);
}

// ---------------------------------------------------------------------------
// Coordinate conversion
// ---------------------------------------------------------------------------

TEST_F(DpiTest, LogicalToPhysical) {
  if (!HasDisplay()) GTEST_SKIP() << "No display available";
  pixelgrab_enable_dpi_awareness(ctx_);
  int px = 0, py = 0;
  PixelGrabError err =
      pixelgrab_logical_to_physical(ctx_, 0, 100, 200, &px, &py);
  EXPECT_EQ(err, kPixelGrabOk);
  EXPECT_GT(px, 0);
  EXPECT_GT(py, 0);
}

TEST_F(DpiTest, PhysicalToLogical) {
  if (!HasDisplay()) GTEST_SKIP() << "No display available";
  pixelgrab_enable_dpi_awareness(ctx_);
  int lx = 0, ly = 0;
  PixelGrabError err =
      pixelgrab_physical_to_logical(ctx_, 0, 100, 200, &lx, &ly);
  EXPECT_EQ(err, kPixelGrabOk);
  EXPECT_GT(lx, 0);
  EXPECT_GT(ly, 0);
}

TEST_F(DpiTest, RoundTripConversion) {
  if (!HasDisplay()) GTEST_SKIP() << "No display available";
  pixelgrab_enable_dpi_awareness(ctx_);
  int px = 0, py = 0;
  int lx = 0, ly = 0;

  pixelgrab_logical_to_physical(ctx_, 0, 500, 300, &px, &py);
  pixelgrab_physical_to_logical(ctx_, 0, px, py, &lx, &ly);

  // Round-trip should be approximately consistent (within rounding).
  EXPECT_NEAR(lx, 500, 1);
  EXPECT_NEAR(ly, 300, 1);
}

TEST_F(DpiTest, LogicalToPhysicalNullOut) {
  PixelGrabError err =
      pixelgrab_logical_to_physical(ctx_, 0, 100, 200, nullptr, nullptr);
  EXPECT_NE(err, kPixelGrabOk);
}

TEST_F(DpiTest, PhysicalToLogicalNullOut) {
  PixelGrabError err =
      pixelgrab_physical_to_logical(ctx_, 0, 100, 200, nullptr, nullptr);
  EXPECT_NE(err, kPixelGrabOk);
}
