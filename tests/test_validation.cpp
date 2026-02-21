// Copyright 2026 The loong-pixelgrab Authors
// Tests for: Phase 1 input validation, error handling, and boundary conditions.

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

class ValidationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ctx_ = pixelgrab_context_create();
    ASSERT_NE(ctx_, nullptr);
  }

  void TearDown() override { pixelgrab_context_destroy(ctx_); }

  PixelGrabContext* ctx_ = nullptr;

  PixelGrabShapeStyle DefaultStyle() {
    PixelGrabShapeStyle s = {};
    s.stroke_color = 0xFFFF0000;
    s.stroke_width = 2.0f;
    return s;
  }
};

// ---------------------------------------------------------------------------
// Annotation input validation
// ---------------------------------------------------------------------------

TEST_F(ValidationTest, RectZeroWidth) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 64, 64);
  if (!img) GTEST_SKIP() << "Capture unavailable";
  PixelGrabAnnotation* ann = pixelgrab_annotation_create(ctx_, img);
  ASSERT_NE(ann, nullptr);

  PixelGrabShapeStyle s = DefaultStyle();
  EXPECT_EQ(pixelgrab_annotation_add_rect(ann, 0, 0, 0, 10, &s), -1);
  EXPECT_EQ(pixelgrab_annotation_add_rect(ann, 0, 0, 10, 0, &s), -1);
  EXPECT_EQ(pixelgrab_annotation_add_rect(ann, 0, 0, -5, 10, &s), -1);

  pixelgrab_annotation_destroy(ann);
  pixelgrab_image_destroy(img);
}

TEST_F(ValidationTest, EllipseZeroRadii) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 64, 64);
  if (!img) GTEST_SKIP() << "Capture unavailable";
  PixelGrabAnnotation* ann = pixelgrab_annotation_create(ctx_, img);
  ASSERT_NE(ann, nullptr);

  PixelGrabShapeStyle s = DefaultStyle();
  EXPECT_EQ(pixelgrab_annotation_add_ellipse(ann, 32, 32, 0, 10, &s), -1);
  EXPECT_EQ(pixelgrab_annotation_add_ellipse(ann, 32, 32, 10, 0, &s), -1);

  pixelgrab_annotation_destroy(ann);
  pixelgrab_image_destroy(img);
}

TEST_F(ValidationTest, PencilTooFewPoints) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 64, 64);
  if (!img) GTEST_SKIP() << "Capture unavailable";
  PixelGrabAnnotation* ann = pixelgrab_annotation_create(ctx_, img);
  ASSERT_NE(ann, nullptr);

  PixelGrabShapeStyle s = DefaultStyle();
  int points[] = {5, 5};
  EXPECT_EQ(pixelgrab_annotation_add_pencil(ann, points, 1, &s), -1);
  EXPECT_EQ(pixelgrab_annotation_add_pencil(ann, nullptr, 4, &s), -1);
  EXPECT_EQ(pixelgrab_annotation_add_pencil(ann, points, 0, &s), -1);

  pixelgrab_annotation_destroy(ann);
  pixelgrab_image_destroy(img);
}

TEST_F(ValidationTest, TextNullString) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 64, 64);
  if (!img) GTEST_SKIP() << "Capture unavailable";
  PixelGrabAnnotation* ann = pixelgrab_annotation_create(ctx_, img);
  ASSERT_NE(ann, nullptr);

  EXPECT_EQ(pixelgrab_annotation_add_text(ann, 0, 0, nullptr, nullptr, 12,
                                          0xFFFFFFFF),
            -1);

  pixelgrab_annotation_destroy(ann);
  pixelgrab_image_destroy(img);
}

TEST_F(ValidationTest, TextZeroFontSize) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 64, 64);
  if (!img) GTEST_SKIP() << "Capture unavailable";
  PixelGrabAnnotation* ann = pixelgrab_annotation_create(ctx_, img);
  ASSERT_NE(ann, nullptr);

  // font_size <= 0 should auto-default to 16, not fail.
  int id = pixelgrab_annotation_add_text(ann, 5, 5, "Test", nullptr, 0,
                                         0xFFFFFFFF);
  EXPECT_GE(id, 0);

  pixelgrab_annotation_destroy(ann);
  pixelgrab_image_destroy(img);
}

TEST_F(ValidationTest, MosaicZeroDimensions) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 64, 64);
  if (!img) GTEST_SKIP() << "Capture unavailable";
  PixelGrabAnnotation* ann = pixelgrab_annotation_create(ctx_, img);
  ASSERT_NE(ann, nullptr);

  EXPECT_EQ(pixelgrab_annotation_add_mosaic(ann, 0, 0, 0, 10, 5), -1);
  EXPECT_EQ(pixelgrab_annotation_add_mosaic(ann, 0, 0, 10, 0, 5), -1);
  EXPECT_EQ(pixelgrab_annotation_add_mosaic(ann, 0, 0, 10, 10, 0), -1);

  pixelgrab_annotation_destroy(ann);
  pixelgrab_image_destroy(img);
}

TEST_F(ValidationTest, BlurZeroDimensions) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 64, 64);
  if (!img) GTEST_SKIP() << "Capture unavailable";
  PixelGrabAnnotation* ann = pixelgrab_annotation_create(ctx_, img);
  ASSERT_NE(ann, nullptr);

  EXPECT_EQ(pixelgrab_annotation_add_blur(ann, 0, 0, 0, 10, 3), -1);
  EXPECT_EQ(pixelgrab_annotation_add_blur(ann, 0, 0, 10, 0, 3), -1);
  EXPECT_EQ(pixelgrab_annotation_add_blur(ann, 0, 0, 10, 10, 0), -1);

  pixelgrab_annotation_destroy(ann);
  pixelgrab_image_destroy(img);
}

// ---------------------------------------------------------------------------
// Magnifier validation
// ---------------------------------------------------------------------------

TEST_F(ValidationTest, MagnifierRadiusTooLarge) {
  PixelGrabImage* img = pixelgrab_get_magnifier(ctx_, 100, 100, 501, 4);
  EXPECT_EQ(img, nullptr);
  EXPECT_NE(pixelgrab_get_last_error(ctx_), kPixelGrabOk);
}

TEST_F(ValidationTest, MagnifierRadiusZero) {
  PixelGrabImage* img = pixelgrab_get_magnifier(ctx_, 100, 100, 0, 4);
  EXPECT_EQ(img, nullptr);
}

TEST_F(ValidationTest, MagnifierMagnificationOutOfRange) {
  PixelGrabImage* img = pixelgrab_get_magnifier(ctx_, 100, 100, 5, 1);
  EXPECT_EQ(img, nullptr);
  img = pixelgrab_get_magnifier(ctx_, 100, 100, 5, 33);
  EXPECT_EQ(img, nullptr);
}

TEST_F(ValidationTest, MagnifierValidRange) {
  PixelGrabImage* img = pixelgrab_get_magnifier(ctx_, 100, 100, 5, 2);
  if (img) {
    EXPECT_GT(pixelgrab_image_get_width(img), 0);
    pixelgrab_image_destroy(img);
  }
}

// ---------------------------------------------------------------------------
// Error state propagation
// ---------------------------------------------------------------------------

TEST_F(ValidationTest, ErrorStateAfterAnnotationCreate) {
  PixelGrabAnnotation* ann = pixelgrab_annotation_create(ctx_, nullptr);
  EXPECT_EQ(ann, nullptr);
  EXPECT_EQ(pixelgrab_get_last_error(ctx_), kPixelGrabErrorInvalidParam);
  const char* msg = pixelgrab_get_last_error_message(ctx_);
  EXPECT_NE(msg, nullptr);
  EXPECT_GT(strlen(msg), 0u);
}

TEST_F(ValidationTest, ErrorStateAfterPinImageNull) {
  PixelGrabPinWindow* pin = pixelgrab_pin_image(ctx_, nullptr, 0, 0);
  EXPECT_EQ(pin, nullptr);
  EXPECT_EQ(pixelgrab_get_last_error(ctx_), kPixelGrabErrorInvalidParam);
}

TEST_F(ValidationTest, ErrorStateAfterPinTextNull) {
  PixelGrabPinWindow* pin = pixelgrab_pin_text(ctx_, nullptr, 0, 0);
  EXPECT_EQ(pin, nullptr);
  EXPECT_EQ(pixelgrab_get_last_error(ctx_), kPixelGrabErrorInvalidParam);
}

// ---------------------------------------------------------------------------
// History validation
// ---------------------------------------------------------------------------

TEST_F(ValidationTest, HistorySetMaxCountZero) {
  pixelgrab_history_set_max_count(ctx_, 0);
  // Should be silently ignored; count remains unchanged.
  pixelgrab_history_set_max_count(ctx_, -5);
}

TEST_F(ValidationTest, HistorySetMaxCountPositive) {
  pixelgrab_history_set_max_count(ctx_, 10);
  // Capture multiple regions to verify max is enforced.
  for (int i = 0; i < 15; ++i) {
    PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 32, 32);
    if (img) pixelgrab_image_destroy(img);
  }
  EXPECT_LE(pixelgrab_history_count(ctx_), 10);
}

// ---------------------------------------------------------------------------
// Capture region validation
// ---------------------------------------------------------------------------

TEST_F(ValidationTest, CaptureRegionZeroSize) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 0, 10);
  EXPECT_EQ(img, nullptr);
  img = pixelgrab_capture_region(ctx_, 0, 0, 10, 0);
  EXPECT_EQ(img, nullptr);
}

TEST_F(ValidationTest, CaptureRegionNegativeSize) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, -10, 10);
  EXPECT_EQ(img, nullptr);
  img = pixelgrab_capture_region(ctx_, 0, 0, 10, -10);
  EXPECT_EQ(img, nullptr);
}

// ---------------------------------------------------------------------------
// Image accessor null safety
// ---------------------------------------------------------------------------

TEST_F(ValidationTest, ImageAccessorsNull) {
  EXPECT_EQ(pixelgrab_image_get_width(nullptr), 0);
  EXPECT_EQ(pixelgrab_image_get_height(nullptr), 0);
  EXPECT_EQ(pixelgrab_image_get_stride(nullptr), 0);
  EXPECT_EQ(pixelgrab_image_get_data(nullptr), nullptr);
  EXPECT_EQ(pixelgrab_image_get_data_size(nullptr), 0u);
}
