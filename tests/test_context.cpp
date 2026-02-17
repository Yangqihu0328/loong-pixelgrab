// Copyright 2026 The loong-pixelgrab Authors
// Tests for: pixelgrab_context_create, pixelgrab_context_destroy,
//            pixelgrab_get_last_error, pixelgrab_get_last_error_message

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

// ---------------------------------------------------------------------------
// Context lifecycle
// ---------------------------------------------------------------------------

TEST(ContextTest, CreateReturnsNonNull) {
  PixelGrabContext* ctx = pixelgrab_context_create();
  ASSERT_NE(ctx, nullptr);
  pixelgrab_context_destroy(ctx);
}

TEST(ContextTest, DestroyNullIsSafe) {
  // Must not crash.
  pixelgrab_context_destroy(nullptr);
}

TEST(ContextTest, CreateMultipleContexts) {
  PixelGrabContext* a = pixelgrab_context_create();
  PixelGrabContext* b = pixelgrab_context_create();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_NE(a, b);
  pixelgrab_context_destroy(b);
  pixelgrab_context_destroy(a);
}

// ---------------------------------------------------------------------------
// Error state
// ---------------------------------------------------------------------------

TEST(ContextTest, InitialErrorIsOk) {
  PixelGrabContext* ctx = pixelgrab_context_create();
  ASSERT_NE(ctx, nullptr);
  EXPECT_EQ(pixelgrab_get_last_error(ctx), kPixelGrabOk);
  pixelgrab_context_destroy(ctx);
}

TEST(ContextTest, ErrorMessageIsNotNull) {
  PixelGrabContext* ctx = pixelgrab_context_create();
  ASSERT_NE(ctx, nullptr);
  const char* msg = pixelgrab_get_last_error_message(ctx);
  EXPECT_NE(msg, nullptr);
  pixelgrab_context_destroy(ctx);
}

TEST(ContextTest, GetLastErrorWithNullCtx) {
  // Defined behavior: should not crash; return value is implementation-defined.
  // We just verify no crash.
  pixelgrab_get_last_error(nullptr);
}

TEST(ContextTest, GetLastErrorMessageWithNullCtx) {
  pixelgrab_get_last_error_message(nullptr);
}
