// Copyright 2026 The loong-pixelgrab Authors
// Tests for: Element detection (3) + Capture history (6)

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

class DetectionHistoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ctx_ = pixelgrab_context_create();
    ASSERT_NE(ctx_, nullptr);
  }
  void TearDown() override { pixelgrab_context_destroy(ctx_); }
  PixelGrabContext* ctx_ = nullptr;
};

// ---------------------------------------------------------------------------
// Element detection — mostly NULL/param protection
// (Real detection requires interactive UI environment)
// ---------------------------------------------------------------------------

TEST_F(DetectionHistoryTest, DetectElementNullCtx) {
  PixelGrabElementRect rect = {};
  PixelGrabError err = pixelgrab_detect_element(nullptr, 100, 100, &rect);
  EXPECT_NE(err, kPixelGrabOk);
}

TEST_F(DetectionHistoryTest, DetectElementNullOut) {
  PixelGrabError err = pixelgrab_detect_element(ctx_, 100, 100, nullptr);
  EXPECT_NE(err, kPixelGrabOk);
}

TEST_F(DetectionHistoryTest, DetectElementsNullCtx) {
  PixelGrabElementRect rects[4] = {};
  EXPECT_EQ(pixelgrab_detect_elements(nullptr, 0, 0, rects, 4), -1);
}

TEST_F(DetectionHistoryTest, DetectElementsNullOut) {
  EXPECT_EQ(pixelgrab_detect_elements(ctx_, 0, 0, nullptr, 4), -1);
}

TEST_F(DetectionHistoryTest, DetectElementsZeroMax) {
  PixelGrabElementRect rects[1] = {};
  EXPECT_EQ(pixelgrab_detect_elements(ctx_, 0, 0, rects, 0), -1);
}

TEST_F(DetectionHistoryTest, SnapToElementNullCtx) {
  PixelGrabElementRect rect = {};
  EXPECT_NE(pixelgrab_snap_to_element(nullptr, 0, 0, 10, &rect), kPixelGrabOk);
}

TEST_F(DetectionHistoryTest, SnapToElementNullOut) {
  EXPECT_NE(pixelgrab_snap_to_element(ctx_, 0, 0, 10, nullptr), kPixelGrabOk);
}

// ---------------------------------------------------------------------------
// Capture history
// ---------------------------------------------------------------------------

TEST_F(DetectionHistoryTest, HistoryInitiallyEmpty) {
  EXPECT_EQ(pixelgrab_history_count(ctx_), 0);
}

TEST_F(DetectionHistoryTest, HistoryCountNullCtx) {
  EXPECT_EQ(pixelgrab_history_count(nullptr), 0);
}

TEST_F(DetectionHistoryTest, HistoryGetEntryEmpty) {
  PixelGrabHistoryEntry entry = {};
  PixelGrabError err = pixelgrab_history_get_entry(ctx_, 0, &entry);
  EXPECT_NE(err, kPixelGrabOk);
}

TEST_F(DetectionHistoryTest, HistoryRecaptureEmpty) {
  PixelGrabImage* img = pixelgrab_history_recapture(ctx_, 1);
  EXPECT_EQ(img, nullptr);
}

TEST_F(DetectionHistoryTest, RecaptureLastEmpty) {
  PixelGrabImage* img = pixelgrab_recapture_last(ctx_);
  EXPECT_EQ(img, nullptr);
}

TEST_F(DetectionHistoryTest, HistoryAfterCapture) {
  // Capture a region — this should add a history entry.
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 10, 20, 30, 40);
  ASSERT_NE(img, nullptr);
  pixelgrab_image_destroy(img);

  EXPECT_EQ(pixelgrab_history_count(ctx_), 1);

  PixelGrabHistoryEntry entry = {};
  PixelGrabError err = pixelgrab_history_get_entry(ctx_, 0, &entry);
  EXPECT_EQ(err, kPixelGrabOk);
  EXPECT_EQ(entry.region_x, 10);
  EXPECT_EQ(entry.region_y, 20);
  EXPECT_EQ(entry.region_width, 30);
  EXPECT_EQ(entry.region_height, 40);
}

TEST_F(DetectionHistoryTest, HistoryClear) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 10, 10);
  ASSERT_NE(img, nullptr);
  pixelgrab_image_destroy(img);

  EXPECT_GE(pixelgrab_history_count(ctx_), 1);
  pixelgrab_history_clear(ctx_);
  EXPECT_EQ(pixelgrab_history_count(ctx_), 0);
}

TEST_F(DetectionHistoryTest, HistorySetMaxCount) {
  pixelgrab_history_set_max_count(ctx_, 2);

  // Capture 3 regions — only 2 should remain.
  for (int i = 0; i < 3; ++i) {
    PixelGrabImage* img = pixelgrab_capture_region(ctx_, i * 10, 0, 10, 10);
    ASSERT_NE(img, nullptr);
    pixelgrab_image_destroy(img);
  }

  EXPECT_EQ(pixelgrab_history_count(ctx_), 2);
}

TEST_F(DetectionHistoryTest, RecaptureLast) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 5, 5, 20, 20);
  ASSERT_NE(img, nullptr);
  pixelgrab_image_destroy(img);

  PixelGrabImage* recap = pixelgrab_recapture_last(ctx_);
  ASSERT_NE(recap, nullptr);
  EXPECT_EQ(pixelgrab_image_get_width(recap), 20);
  EXPECT_EQ(pixelgrab_image_get_height(recap), 20);
  pixelgrab_image_destroy(recap);
}

TEST_F(DetectionHistoryTest, HistoryClearNullCtx) {
  pixelgrab_history_clear(nullptr);  // Must not crash.
}

TEST_F(DetectionHistoryTest, HistorySetMaxCountNullCtx) {
  pixelgrab_history_set_max_count(nullptr, 10);  // Must not crash.
}
