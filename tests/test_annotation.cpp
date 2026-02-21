// Copyright 2026 The loong-pixelgrab Authors
// Tests for: Annotation engine (17 functions)

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

class AnnotationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ctx_ = pixelgrab_context_create();
    ASSERT_NE(ctx_, nullptr);
    base_img_ = pixelgrab_capture_region(ctx_, 0, 0, 64, 64);
    if (!base_img_) {
      GTEST_SKIP() << "Capture unavailable (no display)";
      return;
    }
    ann_ = pixelgrab_annotation_create(ctx_, base_img_);
    ASSERT_NE(ann_, nullptr);
  }

  void TearDown() override {
    pixelgrab_annotation_destroy(ann_);
    pixelgrab_image_destroy(base_img_);
    pixelgrab_context_destroy(ctx_);
  }

  PixelGrabContext* ctx_ = nullptr;
  PixelGrabImage* base_img_ = nullptr;
  PixelGrabAnnotation* ann_ = nullptr;

  PixelGrabShapeStyle DefaultStyle() {
    PixelGrabShapeStyle s = {};
    s.stroke_color = 0xFFFF0000;  // Red, fully opaque.
    s.fill_color = 0x00000000;
    s.stroke_width = 2.0f;
    s.filled = 0;
    return s;
  }
};

// ---------------------------------------------------------------------------
// Create / Destroy
// ---------------------------------------------------------------------------

TEST_F(AnnotationTest, CreateReturnsNonNull) {
  // Already validated in SetUp; verify ann_ is usable.
  EXPECT_NE(ann_, nullptr);
}

TEST_F(AnnotationTest, DestroyNullSafe) {
  pixelgrab_annotation_destroy(nullptr);
}

TEST_F(AnnotationTest, CreateWithNullImage) {
  PixelGrabAnnotation* a = pixelgrab_annotation_create(ctx_, nullptr);
  EXPECT_EQ(a, nullptr);
}

TEST_F(AnnotationTest, CreateWithNullCtx) {
  PixelGrabAnnotation* a = pixelgrab_annotation_create(nullptr, base_img_);
  EXPECT_EQ(a, nullptr);
}

// ---------------------------------------------------------------------------
// Shape addition (8 types)
// ---------------------------------------------------------------------------

TEST_F(AnnotationTest, AddRect) {
  PixelGrabShapeStyle s = DefaultStyle();
  int id = pixelgrab_annotation_add_rect(ann_, 5, 5, 20, 20, &s);
  EXPECT_GE(id, 0);
}

TEST_F(AnnotationTest, AddEllipse) {
  PixelGrabShapeStyle s = DefaultStyle();
  int id = pixelgrab_annotation_add_ellipse(ann_, 32, 32, 10, 15, &s);
  EXPECT_GE(id, 0);
}

TEST_F(AnnotationTest, AddLine) {
  PixelGrabShapeStyle s = DefaultStyle();
  int id = pixelgrab_annotation_add_line(ann_, 0, 0, 63, 63, &s);
  EXPECT_GE(id, 0);
}

TEST_F(AnnotationTest, AddArrow) {
  PixelGrabShapeStyle s = DefaultStyle();
  int id = pixelgrab_annotation_add_arrow(ann_, 10, 10, 50, 50, 8.0f, &s);
  EXPECT_GE(id, 0);
}

TEST_F(AnnotationTest, AddPencil) {
  PixelGrabShapeStyle s = DefaultStyle();
  int points[] = {5, 5, 10, 10, 15, 20, 20, 25};
  int id = pixelgrab_annotation_add_pencil(ann_, points, 4, &s);
  EXPECT_GE(id, 0);
}

TEST_F(AnnotationTest, AddText) {
  int id = pixelgrab_annotation_add_text(ann_, 5, 5, "Hello", "Arial", 12,
                                         0xFFFFFFFF);
  EXPECT_GE(id, 0);
}

TEST_F(AnnotationTest, AddMosaic) {
  int id = pixelgrab_annotation_add_mosaic(ann_, 10, 10, 30, 30, 5);
  EXPECT_GE(id, 0);
}

TEST_F(AnnotationTest, AddBlur) {
  int id = pixelgrab_annotation_add_blur(ann_, 10, 10, 30, 30, 3);
  EXPECT_GE(id, 0);
}

// ---------------------------------------------------------------------------
// Remove shape
// ---------------------------------------------------------------------------

TEST_F(AnnotationTest, RemoveShape) {
  PixelGrabShapeStyle s = DefaultStyle();
  int id = pixelgrab_annotation_add_rect(ann_, 5, 5, 10, 10, &s);
  ASSERT_GE(id, 0);
  PixelGrabError err = pixelgrab_annotation_remove_shape(ann_, id);
  EXPECT_EQ(err, kPixelGrabOk);
}

TEST_F(AnnotationTest, RemoveInvalidShape) {
  PixelGrabError err = pixelgrab_annotation_remove_shape(ann_, 9999);
  EXPECT_NE(err, kPixelGrabOk);
}

// ---------------------------------------------------------------------------
// Undo / Redo
// ---------------------------------------------------------------------------

TEST_F(AnnotationTest, UndoRedoStateMachine) {
  // Initially, nothing to undo/redo.
  EXPECT_EQ(pixelgrab_annotation_can_undo(ann_), 0);
  EXPECT_EQ(pixelgrab_annotation_can_redo(ann_), 0);

  // Add a shape.
  PixelGrabShapeStyle s = DefaultStyle();
  pixelgrab_annotation_add_rect(ann_, 5, 5, 10, 10, &s);

  // Now can undo but not redo.
  EXPECT_NE(pixelgrab_annotation_can_undo(ann_), 0);
  EXPECT_EQ(pixelgrab_annotation_can_redo(ann_), 0);

  // Undo.
  EXPECT_EQ(pixelgrab_annotation_undo(ann_), kPixelGrabOk);

  // Now can redo but (possibly) not undo.
  EXPECT_NE(pixelgrab_annotation_can_redo(ann_), 0);

  // Redo.
  EXPECT_EQ(pixelgrab_annotation_redo(ann_), kPixelGrabOk);
  EXPECT_EQ(pixelgrab_annotation_can_redo(ann_), 0);
}

TEST_F(AnnotationTest, UndoOnEmpty) {
  PixelGrabError err = pixelgrab_annotation_undo(ann_);
  EXPECT_NE(err, kPixelGrabOk);
}

TEST_F(AnnotationTest, RedoOnEmpty) {
  PixelGrabError err = pixelgrab_annotation_redo(ann_);
  EXPECT_NE(err, kPixelGrabOk);
}

// ---------------------------------------------------------------------------
// Result / Export
// ---------------------------------------------------------------------------

TEST_F(AnnotationTest, GetResult) {
  PixelGrabShapeStyle s = DefaultStyle();
  pixelgrab_annotation_add_rect(ann_, 5, 5, 10, 10, &s);

  const PixelGrabImage* result = pixelgrab_annotation_get_result(ann_);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(pixelgrab_image_get_width(result), 64);
  EXPECT_EQ(pixelgrab_image_get_height(result), 64);
}

TEST_F(AnnotationTest, Export) {
  PixelGrabShapeStyle s = DefaultStyle();
  pixelgrab_annotation_add_rect(ann_, 5, 5, 10, 10, &s);

  PixelGrabImage* exported = pixelgrab_annotation_export(ann_);
  ASSERT_NE(exported, nullptr);
  EXPECT_EQ(pixelgrab_image_get_width(exported), 64);
  pixelgrab_image_destroy(exported);
}

// ---------------------------------------------------------------------------
// NULL safety for annotation functions
// ---------------------------------------------------------------------------

TEST_F(AnnotationTest, AddRectNullAnn) {
  PixelGrabShapeStyle s = DefaultStyle();
  EXPECT_EQ(pixelgrab_annotation_add_rect(nullptr, 0, 0, 10, 10, &s), -1);
}

TEST_F(AnnotationTest, GetResultNullAnn) {
  EXPECT_EQ(pixelgrab_annotation_get_result(nullptr), nullptr);
}

TEST_F(AnnotationTest, ExportNullAnn) {
  EXPECT_EQ(pixelgrab_annotation_export(nullptr), nullptr);
}
