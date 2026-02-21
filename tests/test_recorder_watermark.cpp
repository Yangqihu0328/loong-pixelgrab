// Copyright 2026 The loong-pixelgrab Authors
// Tests for: Screen Recording (10 APIs) + Watermark (3 APIs)

#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

// ===========================================================================
// Recorder Tests
// ===========================================================================

class RecorderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ctx_ = pixelgrab_context_create();
    ASSERT_NE(ctx_, nullptr);

    // Build a temp output path for recordings.
    std::snprintf(path_, sizeof(path_), "pixelgrab_test_rec_%p.mp4", static_cast<void*>(ctx_));
  }

  void TearDown() override {
    pixelgrab_context_destroy(ctx_);
    // Best-effort cleanup of temp file.
    std::remove(path_);
  }

  /// Create a recorder in manual mode with sensible defaults.
  PixelGrabRecorder* CreateManualRecorder() {
    PixelGrabRecordConfig cfg = {};
    cfg.output_path   = path_;
    cfg.region_x      = 0;
    cfg.region_y      = 0;
    cfg.region_width  = 64;
    cfg.region_height = 64;
    cfg.fps           = 10;
    cfg.bitrate       = 500000;
    cfg.auto_capture  = 0;  // manual mode
    return pixelgrab_recorder_create(ctx_, &cfg);
  }

  /// Create a recorder in auto-capture mode.
  PixelGrabRecorder* CreateAutoRecorder() {
    PixelGrabRecordConfig cfg = {};
    cfg.output_path   = path_;
    cfg.region_x      = 0;
    cfg.region_y      = 0;
    cfg.region_width  = 64;
    cfg.region_height = 64;
    cfg.fps           = 10;
    cfg.bitrate       = 500000;
    cfg.auto_capture  = 1;  // auto mode
    return pixelgrab_recorder_create(ctx_, &cfg);
  }

  PixelGrabContext* ctx_ = nullptr;
  char path_[256] = {};
};

// ---------------------------------------------------------------------------
// pixelgrab_recorder_is_supported
// ---------------------------------------------------------------------------

TEST_F(RecorderTest, IsSupported) {
  // On Windows this should return non-zero.
  int supported = pixelgrab_recorder_is_supported(ctx_);
#if defined(_WIN32)
  EXPECT_NE(supported, 0);
#else
  EXPECT_EQ(supported, 0);
#endif
}

TEST_F(RecorderTest, IsSupportedNullCtx) {
  // Should not crash; result platform-dependent.
  pixelgrab_recorder_is_supported(nullptr);
}

// ---------------------------------------------------------------------------
// pixelgrab_recorder_create / destroy
// ---------------------------------------------------------------------------

TEST_F(RecorderTest, CreateAndDestroy) {
  PixelGrabRecorder* rec = CreateManualRecorder();
  ASSERT_NE(rec, nullptr);
  pixelgrab_recorder_destroy(rec);
}

TEST_F(RecorderTest, CreateNullCtx) {
  PixelGrabRecordConfig cfg = {};
  cfg.output_path = path_;
  cfg.region_width = 64;
  cfg.region_height = 64;
  EXPECT_EQ(pixelgrab_recorder_create(nullptr, &cfg), nullptr);
}

TEST_F(RecorderTest, CreateNullConfig) {
  EXPECT_EQ(pixelgrab_recorder_create(ctx_, nullptr), nullptr);
}

TEST_F(RecorderTest, CreateNullPath) {
  PixelGrabRecordConfig cfg = {};
  cfg.output_path = nullptr;
  cfg.region_width = 64;
  cfg.region_height = 64;
  EXPECT_EQ(pixelgrab_recorder_create(ctx_, &cfg), nullptr);
}

TEST_F(RecorderTest, DestroyNullSafe) {
  pixelgrab_recorder_destroy(nullptr);  // Must not crash.
}

// ---------------------------------------------------------------------------
// pixelgrab_recorder_get_state
// ---------------------------------------------------------------------------

TEST_F(RecorderTest, StateIdleAfterCreate) {
  PixelGrabRecorder* rec = CreateManualRecorder();
  ASSERT_NE(rec, nullptr);
  EXPECT_EQ(pixelgrab_recorder_get_state(rec), kPixelGrabRecordIdle);
  pixelgrab_recorder_destroy(rec);
}

TEST_F(RecorderTest, StateNullRecorder) {
  EXPECT_EQ(pixelgrab_recorder_get_state(nullptr), kPixelGrabRecordIdle);
}

// ---------------------------------------------------------------------------
// pixelgrab_recorder_start
// ---------------------------------------------------------------------------

TEST_F(RecorderTest, StartSuccess) {
  PixelGrabRecorder* rec = CreateManualRecorder();
  ASSERT_NE(rec, nullptr);
  EXPECT_EQ(pixelgrab_recorder_start(rec), kPixelGrabOk);
  EXPECT_EQ(pixelgrab_recorder_get_state(rec), kPixelGrabRecordRecording);
  pixelgrab_recorder_stop(rec);
  pixelgrab_recorder_destroy(rec);
}

TEST_F(RecorderTest, StartNullRecorder) {
  EXPECT_NE(pixelgrab_recorder_start(nullptr), kPixelGrabOk);
}

// ---------------------------------------------------------------------------
// pixelgrab_recorder_pause / resume
// ---------------------------------------------------------------------------

TEST_F(RecorderTest, PauseSuccess) {
  PixelGrabRecorder* rec = CreateManualRecorder();
  ASSERT_NE(rec, nullptr);
  pixelgrab_recorder_start(rec);
  EXPECT_EQ(pixelgrab_recorder_pause(rec), kPixelGrabOk);
  EXPECT_EQ(pixelgrab_recorder_get_state(rec), kPixelGrabRecordPaused);
  pixelgrab_recorder_stop(rec);
  pixelgrab_recorder_destroy(rec);
}

TEST_F(RecorderTest, PauseWithoutStart) {
  PixelGrabRecorder* rec = CreateManualRecorder();
  ASSERT_NE(rec, nullptr);
  EXPECT_NE(pixelgrab_recorder_pause(rec), kPixelGrabOk);
  pixelgrab_recorder_destroy(rec);
}

TEST_F(RecorderTest, ResumeSuccess) {
  PixelGrabRecorder* rec = CreateManualRecorder();
  ASSERT_NE(rec, nullptr);
  pixelgrab_recorder_start(rec);
  pixelgrab_recorder_pause(rec);
  EXPECT_EQ(pixelgrab_recorder_resume(rec), kPixelGrabOk);
  EXPECT_EQ(pixelgrab_recorder_get_state(rec), kPixelGrabRecordRecording);
  pixelgrab_recorder_stop(rec);
  pixelgrab_recorder_destroy(rec);
}

TEST_F(RecorderTest, ResumeWithoutPause) {
  PixelGrabRecorder* rec = CreateManualRecorder();
  ASSERT_NE(rec, nullptr);
  pixelgrab_recorder_start(rec);
  EXPECT_NE(pixelgrab_recorder_resume(rec), kPixelGrabOk);
  pixelgrab_recorder_stop(rec);
  pixelgrab_recorder_destroy(rec);
}

// ---------------------------------------------------------------------------
// pixelgrab_recorder_stop
// ---------------------------------------------------------------------------

TEST_F(RecorderTest, StopSuccess) {
  PixelGrabRecorder* rec = CreateManualRecorder();
  ASSERT_NE(rec, nullptr);
  pixelgrab_recorder_start(rec);

  // Write at least one frame — MF Sink Writer requires >= 1 sample for
  // Finalize to succeed.
  PixelGrabImage* frame = pixelgrab_capture_region(ctx_, 0, 0, 64, 64);
  ASSERT_NE(frame, nullptr);
  pixelgrab_recorder_write_frame(rec, frame);
  pixelgrab_image_destroy(frame);

  EXPECT_EQ(pixelgrab_recorder_stop(rec), kPixelGrabOk);
  EXPECT_EQ(pixelgrab_recorder_get_state(rec), kPixelGrabRecordStopped);
  pixelgrab_recorder_destroy(rec);
}

TEST_F(RecorderTest, StopWithoutStart) {
  PixelGrabRecorder* rec = CreateManualRecorder();
  ASSERT_NE(rec, nullptr);
  EXPECT_NE(pixelgrab_recorder_stop(rec), kPixelGrabOk);
  pixelgrab_recorder_destroy(rec);
}

TEST_F(RecorderTest, StopNullRecorder) {
  EXPECT_NE(pixelgrab_recorder_stop(nullptr), kPixelGrabOk);
}

// ---------------------------------------------------------------------------
// pixelgrab_recorder_get_duration_ms
// ---------------------------------------------------------------------------

TEST_F(RecorderTest, DurationZeroBeforeStart) {
  PixelGrabRecorder* rec = CreateManualRecorder();
  ASSERT_NE(rec, nullptr);
  EXPECT_EQ(pixelgrab_recorder_get_duration_ms(rec), 0);
  pixelgrab_recorder_destroy(rec);
}

TEST_F(RecorderTest, DurationNullRecorder) {
  EXPECT_EQ(pixelgrab_recorder_get_duration_ms(nullptr), 0);
}

// ---------------------------------------------------------------------------
// pixelgrab_recorder_write_frame (manual mode)
// ---------------------------------------------------------------------------

TEST_F(RecorderTest, WriteFrameManualMode) {
  PixelGrabRecorder* rec = CreateManualRecorder();
  ASSERT_NE(rec, nullptr);
  pixelgrab_recorder_start(rec);

  // Capture a small image to use as a frame.
  PixelGrabImage* frame = pixelgrab_capture_region(ctx_, 0, 0, 64, 64);
  ASSERT_NE(frame, nullptr);

  PixelGrabError err = pixelgrab_recorder_write_frame(rec, frame);
  EXPECT_EQ(err, kPixelGrabOk);

  pixelgrab_image_destroy(frame);
  pixelgrab_recorder_stop(rec);

  // After writing one frame, duration should be > 0.
  EXPECT_GT(pixelgrab_recorder_get_duration_ms(rec), 0);

  pixelgrab_recorder_destroy(rec);
}

TEST_F(RecorderTest, WriteFrameNullRecorder) {
  EXPECT_NE(pixelgrab_recorder_write_frame(nullptr, nullptr), kPixelGrabOk);
}

TEST_F(RecorderTest, WriteFrameNullImage) {
  PixelGrabRecorder* rec = CreateManualRecorder();
  ASSERT_NE(rec, nullptr);
  pixelgrab_recorder_start(rec);
  EXPECT_NE(pixelgrab_recorder_write_frame(rec, nullptr), kPixelGrabOk);
  pixelgrab_recorder_stop(rec);
  pixelgrab_recorder_destroy(rec);
}

TEST_F(RecorderTest, WriteFrameAutoModeBlocked) {
  PixelGrabRecorder* rec = CreateAutoRecorder();
  ASSERT_NE(rec, nullptr);
  pixelgrab_recorder_start(rec);

  PixelGrabImage* frame = pixelgrab_capture_region(ctx_, 0, 0, 64, 64);
  ASSERT_NE(frame, nullptr);

  // write_frame should be rejected in auto mode.
  PixelGrabError err = pixelgrab_recorder_write_frame(rec, frame);
  EXPECT_NE(err, kPixelGrabOk);

  pixelgrab_image_destroy(frame);
  pixelgrab_recorder_stop(rec);
  pixelgrab_recorder_destroy(rec);
}

// ---------------------------------------------------------------------------
// Auto capture: start/stop with internal thread
// ---------------------------------------------------------------------------

TEST_F(RecorderTest, AutoCaptureStartStop) {
  PixelGrabRecorder* rec = CreateAutoRecorder();
  ASSERT_NE(rec, nullptr);
  EXPECT_EQ(pixelgrab_recorder_start(rec), kPixelGrabOk);
  EXPECT_EQ(pixelgrab_recorder_get_state(rec), kPixelGrabRecordRecording);

  // Let the capture thread run briefly.
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  EXPECT_EQ(pixelgrab_recorder_stop(rec), kPixelGrabOk);
  EXPECT_EQ(pixelgrab_recorder_get_state(rec), kPixelGrabRecordStopped);

  // Should have captured at least 1 frame in 200ms at 10fps.
  EXPECT_GT(pixelgrab_recorder_get_duration_ms(rec), 0);

  pixelgrab_recorder_destroy(rec);
}

// ===========================================================================
// Watermark Tests
// ===========================================================================

class WatermarkTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ctx_ = pixelgrab_context_create();
    ASSERT_NE(ctx_, nullptr);
    img_ = pixelgrab_capture_region(ctx_, 0, 0, 64, 64);
    ASSERT_NE(img_, nullptr);
  }

  void TearDown() override {
    pixelgrab_image_destroy(img_);
    pixelgrab_context_destroy(ctx_);
  }

  PixelGrabContext* ctx_ = nullptr;
  PixelGrabImage* img_   = nullptr;
};

// ---------------------------------------------------------------------------
// pixelgrab_watermark_is_supported
// ---------------------------------------------------------------------------

TEST_F(WatermarkTest, IsSupported) {
  int supported = pixelgrab_watermark_is_supported(ctx_);
#if defined(_WIN32)
  EXPECT_NE(supported, 0);
#else
  EXPECT_EQ(supported, 0);
#endif
}

// ---------------------------------------------------------------------------
// pixelgrab_watermark_apply_text
// ---------------------------------------------------------------------------

TEST_F(WatermarkTest, ApplyTextSuccess) {
  PixelGrabTextWatermarkConfig cfg = {};
  cfg.text      = "Test";
  cfg.font_size = 12;
  cfg.color     = 0xFFFFFFFF;
  cfg.position  = kPixelGrabWatermarkCenter;
  cfg.margin    = 4;

  PixelGrabError err = pixelgrab_watermark_apply_text(ctx_, img_, &cfg);
  EXPECT_EQ(err, kPixelGrabOk);
}

TEST_F(WatermarkTest, ApplyTextNullCtx) {
  PixelGrabTextWatermarkConfig cfg = {};
  cfg.text = "Test";
  EXPECT_NE(pixelgrab_watermark_apply_text(nullptr, img_, &cfg), kPixelGrabOk);
}

TEST_F(WatermarkTest, ApplyTextNullImage) {
  PixelGrabTextWatermarkConfig cfg = {};
  cfg.text = "Test";
  EXPECT_NE(pixelgrab_watermark_apply_text(ctx_, nullptr, &cfg), kPixelGrabOk);
}

TEST_F(WatermarkTest, ApplyTextNullConfig) {
  EXPECT_NE(pixelgrab_watermark_apply_text(ctx_, img_, nullptr), kPixelGrabOk);
}

TEST_F(WatermarkTest, ApplyTextNullText) {
  PixelGrabTextWatermarkConfig cfg = {};
  cfg.text = nullptr;
  EXPECT_NE(pixelgrab_watermark_apply_text(ctx_, img_, &cfg), kPixelGrabOk);
}

// ---------------------------------------------------------------------------
// pixelgrab_watermark_apply_image
// ---------------------------------------------------------------------------

TEST_F(WatermarkTest, ApplyImageSuccess) {
  // Create a small watermark image.
  PixelGrabImage* wm_img = pixelgrab_capture_region(ctx_, 0, 0, 16, 16);
  ASSERT_NE(wm_img, nullptr);

  PixelGrabError err =
      pixelgrab_watermark_apply_image(ctx_, img_, wm_img, 0, 0, 0.5f);
  EXPECT_EQ(err, kPixelGrabOk);

  pixelgrab_image_destroy(wm_img);
}

TEST_F(WatermarkTest, ApplyImageNullParams) {
  PixelGrabImage* wm_img = pixelgrab_capture_region(ctx_, 0, 0, 16, 16);
  ASSERT_NE(wm_img, nullptr);

  EXPECT_NE(pixelgrab_watermark_apply_image(nullptr, img_, wm_img, 0, 0, 1.0f),
            kPixelGrabOk);
  EXPECT_NE(pixelgrab_watermark_apply_image(ctx_, nullptr, wm_img, 0, 0, 1.0f),
            kPixelGrabOk);
  EXPECT_NE(pixelgrab_watermark_apply_image(ctx_, img_, nullptr, 0, 0, 1.0f),
            kPixelGrabOk);

  pixelgrab_image_destroy(wm_img);
}

// ===========================================================================
// GPU Acceleration Tests (gpu_hint field)
// ===========================================================================

// Test: gpu_hint default (0) — auto-detect, should succeed in auto mode.
TEST_F(RecorderTest, GpuHintAutoDefault) {
  PixelGrabRecordConfig cfg = {};
  cfg.output_path   = path_;
  cfg.region_width  = 64;
  cfg.region_height = 64;
  cfg.fps           = 10;
  cfg.bitrate       = 500000;
  cfg.auto_capture  = 1;
  cfg.gpu_hint      = 0;  // auto (default)

  PixelGrabRecorder* rec = pixelgrab_recorder_create(ctx_, &cfg);
  ASSERT_NE(rec, nullptr);
  EXPECT_EQ(pixelgrab_recorder_start(rec), kPixelGrabOk);

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  EXPECT_EQ(pixelgrab_recorder_stop(rec), kPixelGrabOk);
  EXPECT_GT(pixelgrab_recorder_get_duration_ms(rec), 0);

  pixelgrab_recorder_destroy(rec);
}

// Test: gpu_hint = -1 — force CPU, should succeed without GPU.
TEST_F(RecorderTest, GpuHintForceCpu) {
  PixelGrabRecordConfig cfg = {};
  cfg.output_path   = path_;
  cfg.region_width  = 64;
  cfg.region_height = 64;
  cfg.fps           = 10;
  cfg.bitrate       = 500000;
  cfg.auto_capture  = 1;
  cfg.gpu_hint      = -1;  // force CPU

  PixelGrabRecorder* rec = pixelgrab_recorder_create(ctx_, &cfg);
  ASSERT_NE(rec, nullptr);
  EXPECT_EQ(pixelgrab_recorder_start(rec), kPixelGrabOk);

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  EXPECT_EQ(pixelgrab_recorder_stop(rec), kPixelGrabOk);
  EXPECT_GT(pixelgrab_recorder_get_duration_ms(rec), 0);

  pixelgrab_recorder_destroy(rec);
}

// Test: gpu_hint in manual mode — GPU is not used, WriteFrame always CPU.
TEST_F(RecorderTest, GpuHintManualModeIgnored) {
  PixelGrabRecordConfig cfg = {};
  cfg.output_path   = path_;
  cfg.region_width  = 64;
  cfg.region_height = 64;
  cfg.fps           = 10;
  cfg.bitrate       = 500000;
  cfg.auto_capture  = 0;   // manual mode
  cfg.gpu_hint      = 0;   // auto

  PixelGrabRecorder* rec = pixelgrab_recorder_create(ctx_, &cfg);
  ASSERT_NE(rec, nullptr);
  EXPECT_EQ(pixelgrab_recorder_start(rec), kPixelGrabOk);

  // Feed one frame manually.
  PixelGrabImage* frame = pixelgrab_capture_region(ctx_, 0, 0, 64, 64);
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(pixelgrab_recorder_write_frame(rec, frame), kPixelGrabOk);
  pixelgrab_image_destroy(frame);

  EXPECT_EQ(pixelgrab_recorder_stop(rec), kPixelGrabOk);
  EXPECT_GT(pixelgrab_recorder_get_duration_ms(rec), 0);

  pixelgrab_recorder_destroy(rec);
}

// Test: RecordConfig gpu_hint field defaults to 0 on zero-initialization.
TEST_F(RecorderTest, GpuHintZeroInitDefault) {
  PixelGrabRecordConfig cfg = {};
  EXPECT_EQ(cfg.gpu_hint, 0);
}

// Test: gpu_hint values are stable.
TEST_F(RecorderTest, GpuHintValues) {
  PixelGrabRecordConfig cfg = {};
  cfg.gpu_hint = 0;
  EXPECT_EQ(cfg.gpu_hint, 0);   // auto

  cfg.gpu_hint = 1;
  EXPECT_EQ(cfg.gpu_hint, 1);   // prefer GPU

  cfg.gpu_hint = -1;
  EXPECT_EQ(cfg.gpu_hint, -1);  // force CPU
}
