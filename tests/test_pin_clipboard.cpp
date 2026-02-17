// Copyright 2026 The loong-pixelgrab Authors
// Tests for: Pin windows (12 + 20 multi-pin) + Clipboard (4)

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

class PinClipboardTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ctx_ = pixelgrab_context_create();
    ASSERT_NE(ctx_, nullptr);
  }
  void TearDown() override {
    pixelgrab_pin_destroy_all(ctx_);
    pixelgrab_context_destroy(ctx_);
  }
  PixelGrabContext* ctx_ = nullptr;
};

// ---------------------------------------------------------------------------
// Pin — image
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, PinImageCreatesWindow) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 50, 50);
  ASSERT_NE(img, nullptr);

  PixelGrabPinWindow* pin = pixelgrab_pin_image(ctx_, img, 100, 100);
  EXPECT_NE(pin, nullptr);

  if (pin) {
    EXPECT_GE(pixelgrab_pin_count(ctx_), 1);
    pixelgrab_pin_destroy(pin);
  }

  pixelgrab_image_destroy(img);
}

TEST_F(PinClipboardTest, PinImageNullCtx) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 10, 10);
  ASSERT_NE(img, nullptr);
  EXPECT_EQ(pixelgrab_pin_image(nullptr, img, 0, 0), nullptr);
  pixelgrab_image_destroy(img);
}

TEST_F(PinClipboardTest, PinImageNullImage) {
  EXPECT_EQ(pixelgrab_pin_image(ctx_, nullptr, 0, 0), nullptr);
}

// ---------------------------------------------------------------------------
// Pin — text
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, PinTextCreatesWindow) {
  PixelGrabPinWindow* pin = pixelgrab_pin_text(ctx_, "Hello Test", 200, 200);
  EXPECT_NE(pin, nullptr);
  if (pin) {
    EXPECT_GE(pixelgrab_pin_count(ctx_), 1);
    pixelgrab_pin_destroy(pin);
  }
}

TEST_F(PinClipboardTest, PinTextNullCtx) {
  EXPECT_EQ(pixelgrab_pin_text(nullptr, "text", 0, 0), nullptr);
}

TEST_F(PinClipboardTest, PinTextNullText) {
  EXPECT_EQ(pixelgrab_pin_text(ctx_, nullptr, 0, 0), nullptr);
}

// ---------------------------------------------------------------------------
// Pin — clipboard
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, PinClipboardNullCtx) {
  EXPECT_EQ(pixelgrab_pin_clipboard(nullptr, 0, 0), nullptr);
}

// ---------------------------------------------------------------------------
// Pin — destroy
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, DestroyNullSafe) {
  pixelgrab_pin_destroy(nullptr);  // Must not crash.
}

// ---------------------------------------------------------------------------
// Pin — opacity
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, OpacitySetGet) {
  PixelGrabPinWindow* pin = pixelgrab_pin_text(ctx_, "Opacity", 0, 0);
  ASSERT_NE(pin, nullptr);

  PixelGrabError err = pixelgrab_pin_set_opacity(pin, 0.5f);
  EXPECT_EQ(err, kPixelGrabOk);

  float opacity = pixelgrab_pin_get_opacity(pin);
  EXPECT_NEAR(opacity, 0.5f, 0.05f);

  pixelgrab_pin_destroy(pin);
}

TEST_F(PinClipboardTest, OpacityNullPin) {
  EXPECT_NE(pixelgrab_pin_set_opacity(nullptr, 1.0f), kPixelGrabOk);
  // NULL pin returns default opacity (1.0) per implementation.
  float opacity = pixelgrab_pin_get_opacity(nullptr);
  (void)opacity;  // Just verify no crash; value is implementation-defined.
}

// ---------------------------------------------------------------------------
// Pin — position, size, visible
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, SetPositionNullPin) {
  EXPECT_NE(pixelgrab_pin_set_position(nullptr, 0, 0), kPixelGrabOk);
}

TEST_F(PinClipboardTest, SetSizeNullPin) {
  EXPECT_NE(pixelgrab_pin_set_size(nullptr, 100, 100), kPixelGrabOk);
}

TEST_F(PinClipboardTest, SetVisibleNullPin) {
  EXPECT_NE(pixelgrab_pin_set_visible(nullptr, 1), kPixelGrabOk);
}

TEST_F(PinClipboardTest, SetPositionValid) {
  PixelGrabPinWindow* pin = pixelgrab_pin_text(ctx_, "Pos", 0, 0);
  ASSERT_NE(pin, nullptr);
  EXPECT_EQ(pixelgrab_pin_set_position(pin, 300, 400), kPixelGrabOk);
  pixelgrab_pin_destroy(pin);
}

TEST_F(PinClipboardTest, SetSizeValid) {
  PixelGrabPinWindow* pin = pixelgrab_pin_text(ctx_, "Size", 0, 0);
  ASSERT_NE(pin, nullptr);
  EXPECT_EQ(pixelgrab_pin_set_size(pin, 200, 150), kPixelGrabOk);
  pixelgrab_pin_destroy(pin);
}

TEST_F(PinClipboardTest, SetVisibleValid) {
  PixelGrabPinWindow* pin = pixelgrab_pin_text(ctx_, "Vis", 0, 0);
  ASSERT_NE(pin, nullptr);
  EXPECT_EQ(pixelgrab_pin_set_visible(pin, 0), kPixelGrabOk);
  EXPECT_EQ(pixelgrab_pin_set_visible(pin, 1), kPixelGrabOk);
  pixelgrab_pin_destroy(pin);
}

// ---------------------------------------------------------------------------
// Pin — process events / count / destroy all
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, ProcessEventsReturnsCount) {
  int count = pixelgrab_pin_process_events(ctx_);
  EXPECT_GE(count, 0);
}

TEST_F(PinClipboardTest, PinCountInitiallyZero) {
  EXPECT_EQ(pixelgrab_pin_count(ctx_), 0);
}

TEST_F(PinClipboardTest, DestroyAllClearsCount) {
  PixelGrabPinWindow* pin = pixelgrab_pin_text(ctx_, "Temp", 0, 0);
  ASSERT_NE(pin, nullptr);
  EXPECT_GE(pixelgrab_pin_count(ctx_), 1);

  pixelgrab_pin_destroy_all(ctx_);
  EXPECT_EQ(pixelgrab_pin_count(ctx_), 0);
}

TEST_F(PinClipboardTest, ProcessEventsNullCtx) {
  EXPECT_EQ(pixelgrab_pin_process_events(nullptr), 0);
}

TEST_F(PinClipboardTest, PinCountNullCtx) {
  EXPECT_EQ(pixelgrab_pin_count(nullptr), 0);
}

TEST_F(PinClipboardTest, DestroyAllNullCtx) {
  pixelgrab_pin_destroy_all(nullptr);  // Must not crash.
}

// ---------------------------------------------------------------------------
// Pin — enumerate
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, EnumerateEmpty) {
  int ids[4] = {};
  EXPECT_EQ(pixelgrab_pin_enumerate(ctx_, ids, 4), 0);
}

TEST_F(PinClipboardTest, EnumerateMultiple) {
  PixelGrabPinWindow* p1 = pixelgrab_pin_text(ctx_, "A", 0, 0);
  PixelGrabPinWindow* p2 = pixelgrab_pin_text(ctx_, "B", 50, 50);
  ASSERT_NE(p1, nullptr);
  ASSERT_NE(p2, nullptr);

  int ids[8] = {};
  int count = pixelgrab_pin_enumerate(ctx_, ids, 8);
  EXPECT_EQ(count, 2);

  pixelgrab_pin_destroy(p1);
  pixelgrab_pin_destroy(p2);
}

TEST_F(PinClipboardTest, EnumerateNullCtx) {
  int ids[4] = {};
  EXPECT_EQ(pixelgrab_pin_enumerate(nullptr, ids, 4), -1);
}

TEST_F(PinClipboardTest, EnumerateNullArray) {
  EXPECT_EQ(pixelgrab_pin_enumerate(ctx_, nullptr, 4), -1);
}

// ---------------------------------------------------------------------------
// Pin — get info
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, GetInfoImagePin) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 64, 48);
  ASSERT_NE(img, nullptr);

  PixelGrabPinWindow* pin = pixelgrab_pin_image(ctx_, img, 120, 130);
  ASSERT_NE(pin, nullptr);

  PixelGrabPinInfo info = {};
  EXPECT_EQ(pixelgrab_pin_get_info(pin, &info), kPixelGrabOk);
  EXPECT_GT(info.id, 0);
  EXPECT_EQ(info.width, 64);
  EXPECT_EQ(info.height, 48);
  EXPECT_NEAR(info.opacity, 1.0f, 0.05f);
  EXPECT_NE(info.is_visible, 0);
  EXPECT_EQ(info.content_type, 0);  // image

  pixelgrab_pin_destroy(pin);
  pixelgrab_image_destroy(img);
}

TEST_F(PinClipboardTest, GetInfoTextPin) {
  PixelGrabPinWindow* pin = pixelgrab_pin_text(ctx_, "Info", 10, 20);
  ASSERT_NE(pin, nullptr);

  PixelGrabPinInfo info = {};
  EXPECT_EQ(pixelgrab_pin_get_info(pin, &info), kPixelGrabOk);
  EXPECT_EQ(info.content_type, 1);  // text

  pixelgrab_pin_destroy(pin);
}

TEST_F(PinClipboardTest, GetInfoNullPin) {
  PixelGrabPinInfo info = {};
  EXPECT_NE(pixelgrab_pin_get_info(nullptr, &info), kPixelGrabOk);
}

TEST_F(PinClipboardTest, GetInfoNullOut) {
  PixelGrabPinWindow* pin = pixelgrab_pin_text(ctx_, "X", 0, 0);
  ASSERT_NE(pin, nullptr);
  EXPECT_NE(pixelgrab_pin_get_info(pin, nullptr), kPixelGrabOk);
  pixelgrab_pin_destroy(pin);
}

// ---------------------------------------------------------------------------
// Pin — get/set image content
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, GetImageFromImagePin) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 32, 32);
  ASSERT_NE(img, nullptr);

  PixelGrabPinWindow* pin = pixelgrab_pin_image(ctx_, img, 0, 0);
  ASSERT_NE(pin, nullptr);

  PixelGrabImage* copy = pixelgrab_pin_get_image(pin);
  ASSERT_NE(copy, nullptr);
  EXPECT_EQ(pixelgrab_image_get_width(copy), 32);
  EXPECT_EQ(pixelgrab_image_get_height(copy), 32);

  pixelgrab_image_destroy(copy);
  pixelgrab_pin_destroy(pin);
  pixelgrab_image_destroy(img);
}

TEST_F(PinClipboardTest, GetImageFromTextPinReturnsNull) {
  PixelGrabPinWindow* pin = pixelgrab_pin_text(ctx_, "NoImage", 0, 0);
  ASSERT_NE(pin, nullptr);
  EXPECT_EQ(pixelgrab_pin_get_image(pin), nullptr);
  pixelgrab_pin_destroy(pin);
}

TEST_F(PinClipboardTest, GetImageNullPin) {
  EXPECT_EQ(pixelgrab_pin_get_image(nullptr), nullptr);
}

TEST_F(PinClipboardTest, SetImageUpdatesContent) {
  PixelGrabImage* img1 = pixelgrab_capture_region(ctx_, 0, 0, 40, 40);
  ASSERT_NE(img1, nullptr);

  PixelGrabPinWindow* pin = pixelgrab_pin_image(ctx_, img1, 0, 0);
  ASSERT_NE(pin, nullptr);

  // Capture a different region and update.
  PixelGrabImage* img2 = pixelgrab_capture_region(ctx_, 10, 10, 60, 60);
  ASSERT_NE(img2, nullptr);

  EXPECT_EQ(pixelgrab_pin_set_image(pin, img2), kPixelGrabOk);

  // Verify the new content.
  PixelGrabImage* got = pixelgrab_pin_get_image(pin);
  ASSERT_NE(got, nullptr);
  EXPECT_EQ(pixelgrab_image_get_width(got), 60);
  EXPECT_EQ(pixelgrab_image_get_height(got), 60);

  pixelgrab_image_destroy(got);
  pixelgrab_image_destroy(img2);
  pixelgrab_pin_destroy(pin);
  pixelgrab_image_destroy(img1);
}

TEST_F(PinClipboardTest, SetImageNullPin) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 10, 10);
  ASSERT_NE(img, nullptr);
  EXPECT_NE(pixelgrab_pin_set_image(nullptr, img), kPixelGrabOk);
  pixelgrab_image_destroy(img);
}

TEST_F(PinClipboardTest, SetImageOnTextPinFails) {
  PixelGrabPinWindow* pin = pixelgrab_pin_text(ctx_, "T", 0, 0);
  ASSERT_NE(pin, nullptr);
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 10, 10);
  ASSERT_NE(img, nullptr);
  EXPECT_NE(pixelgrab_pin_set_image(pin, img), kPixelGrabOk);
  pixelgrab_image_destroy(img);
  pixelgrab_pin_destroy(pin);
}

// ---------------------------------------------------------------------------
// Pin — set visible all
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, SetVisibleAllHideShow) {
  PixelGrabPinWindow* pin = pixelgrab_pin_text(ctx_, "VA", 0, 0);
  ASSERT_NE(pin, nullptr);

  // Hide all.
  EXPECT_EQ(pixelgrab_pin_set_visible_all(ctx_, 0), kPixelGrabOk);
  PixelGrabPinInfo info = {};
  EXPECT_EQ(pixelgrab_pin_get_info(pin, &info), kPixelGrabOk);
  EXPECT_EQ(info.is_visible, 0);

  // Show all.
  EXPECT_EQ(pixelgrab_pin_set_visible_all(ctx_, 1), kPixelGrabOk);
  EXPECT_EQ(pixelgrab_pin_get_info(pin, &info), kPixelGrabOk);
  EXPECT_NE(info.is_visible, 0);

  pixelgrab_pin_destroy(pin);
}

TEST_F(PinClipboardTest, SetVisibleAllNullCtx) {
  EXPECT_NE(pixelgrab_pin_set_visible_all(nullptr, 1), kPixelGrabOk);
}

// ---------------------------------------------------------------------------
// Pin — duplicate
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, DuplicateImagePin) {
  PixelGrabImage* img = pixelgrab_capture_region(ctx_, 0, 0, 50, 50);
  ASSERT_NE(img, nullptr);

  PixelGrabPinWindow* pin = pixelgrab_pin_image(ctx_, img, 100, 100);
  ASSERT_NE(pin, nullptr);

  PixelGrabPinWindow* dup = pixelgrab_pin_duplicate(pin, 30, 30);
  ASSERT_NE(dup, nullptr);
  EXPECT_EQ(pixelgrab_pin_count(ctx_), 2);

  // Verify the duplicate has the same image dimensions.
  PixelGrabImage* dup_img = pixelgrab_pin_get_image(dup);
  ASSERT_NE(dup_img, nullptr);
  EXPECT_EQ(pixelgrab_image_get_width(dup_img), 50);
  EXPECT_EQ(pixelgrab_image_get_height(dup_img), 50);

  pixelgrab_image_destroy(dup_img);
  pixelgrab_pin_destroy(dup);
  pixelgrab_pin_destroy(pin);
  pixelgrab_image_destroy(img);
}

TEST_F(PinClipboardTest, DuplicateNullPin) {
  EXPECT_EQ(pixelgrab_pin_duplicate(nullptr, 0, 0), nullptr);
}

// ---------------------------------------------------------------------------
// Capture excluding pins
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, CaptureScreenExcludePins) {
  // Just verify the function works without crashing.
  PixelGrabImage* img = pixelgrab_capture_screen_exclude_pins(ctx_, 0);
  EXPECT_NE(img, nullptr);
  if (img) pixelgrab_image_destroy(img);
}

TEST_F(PinClipboardTest, CaptureRegionExcludePins) {
  PixelGrabImage* img =
      pixelgrab_capture_region_exclude_pins(ctx_, 0, 0, 100, 100);
  EXPECT_NE(img, nullptr);
  if (img) pixelgrab_image_destroy(img);
}

TEST_F(PinClipboardTest, CaptureScreenExcludePinsNullCtx) {
  EXPECT_EQ(pixelgrab_capture_screen_exclude_pins(nullptr, 0), nullptr);
}

TEST_F(PinClipboardTest, CaptureRegionExcludePinsNullCtx) {
  EXPECT_EQ(pixelgrab_capture_region_exclude_pins(nullptr, 0, 0, 10, 10),
            nullptr);
}

// ---------------------------------------------------------------------------
// Clipboard reading
// ---------------------------------------------------------------------------

TEST_F(PinClipboardTest, ClipboardGetFormat) {
  PixelGrabClipboardFormat fmt = pixelgrab_clipboard_get_format(ctx_);
  // Format is platform-dependent; just verify it's a valid enum value.
  EXPECT_GE(static_cast<int>(fmt), 0);
  EXPECT_LE(static_cast<int>(fmt), 3);
}

TEST_F(PinClipboardTest, ClipboardGetFormatNullCtx) {
  PixelGrabClipboardFormat fmt = pixelgrab_clipboard_get_format(nullptr);
  EXPECT_EQ(fmt, kPixelGrabClipboardNone);
}

TEST_F(PinClipboardTest, ClipboardGetImageNullCtx) {
  EXPECT_EQ(pixelgrab_clipboard_get_image(nullptr), nullptr);
}

TEST_F(PinClipboardTest, ClipboardGetTextNullCtx) {
  EXPECT_EQ(pixelgrab_clipboard_get_text(nullptr), nullptr);
}

TEST_F(PinClipboardTest, FreeStringNullSafe) {
  pixelgrab_free_string(nullptr);  // Must not crash.
}

TEST_F(PinClipboardTest, ClipboardGetTextFreeRoundTrip) {
  // If clipboard has text, read and free it.
  char* text = pixelgrab_clipboard_get_text(ctx_);
  if (text) {
    // Just verify it's a valid string.
    EXPECT_GT(strlen(text), 0u);
    pixelgrab_free_string(text);
  }
  // If no text, that's fine too.
}
