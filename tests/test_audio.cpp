// Copyright 2026 The loong-pixelgrab Authors
// Tests for: Audio Device Query (3 APIs)

#include <cstring>

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

// ===========================================================================
// Audio API Tests
// ===========================================================================

class AudioTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ctx_ = pixelgrab_context_create();
    ASSERT_NE(ctx_, nullptr);
  }

  void TearDown() override {
    pixelgrab_context_destroy(ctx_);
  }

  PixelGrabContext* ctx_ = nullptr;
};

// ---------------------------------------------------------------------------
// pixelgrab_audio_is_supported
// ---------------------------------------------------------------------------

TEST_F(AudioTest, IsSupportedNullCtx) {
  EXPECT_EQ(pixelgrab_audio_is_supported(nullptr), 0);
}

TEST_F(AudioTest, IsSupportedValidCtx) {
  // On Windows, audio should be supported.
  int supported = pixelgrab_audio_is_supported(ctx_);
#if defined(_WIN32)
  EXPECT_NE(supported, 0);
#else
  // On other platforms we just verify it doesn't crash.
  (void)supported;
#endif
}

// ---------------------------------------------------------------------------
// pixelgrab_audio_enumerate_devices
// ---------------------------------------------------------------------------

TEST_F(AudioTest, EnumerateDevicesNullCtx) {
  PixelGrabAudioDeviceInfo devices[8];
  EXPECT_EQ(pixelgrab_audio_enumerate_devices(nullptr, devices, 8), -1);
}

TEST_F(AudioTest, EnumerateDevicesNullBuffer) {
  EXPECT_EQ(pixelgrab_audio_enumerate_devices(ctx_, nullptr, 8), -1);
}

TEST_F(AudioTest, EnumerateDevicesZeroCount) {
  PixelGrabAudioDeviceInfo devices[1];
  EXPECT_EQ(pixelgrab_audio_enumerate_devices(ctx_, devices, 0), -1);
}

TEST_F(AudioTest, EnumerateDevicesSuccess) {
  if (!pixelgrab_audio_is_supported(ctx_)) {
    GTEST_SKIP() << "Audio not supported on this platform";
  }

  PixelGrabAudioDeviceInfo devices[32];
  int count = pixelgrab_audio_enumerate_devices(ctx_, devices, 32);
  EXPECT_GE(count, 0);

  // Verify all returned devices have non-empty names.
  for (int i = 0; i < count; ++i) {
    EXPECT_GT(std::strlen(devices[i].name), 0u)
        << "Device " << i << " has empty name";
  }
}

// ---------------------------------------------------------------------------
// pixelgrab_audio_get_default_device
// ---------------------------------------------------------------------------

TEST_F(AudioTest, GetDefaultDeviceNullCtx) {
  PixelGrabAudioDeviceInfo device;
  EXPECT_EQ(pixelgrab_audio_get_default_device(nullptr, 1, &device),
            kPixelGrabErrorInvalidParam);
}

TEST_F(AudioTest, GetDefaultDeviceNullOutput) {
  EXPECT_EQ(pixelgrab_audio_get_default_device(ctx_, 1, nullptr),
            kPixelGrabErrorInvalidParam);
}

TEST_F(AudioTest, GetDefaultInputDevice) {
  if (!pixelgrab_audio_is_supported(ctx_)) {
    GTEST_SKIP() << "Audio not supported on this platform";
  }

  PixelGrabAudioDeviceInfo device;
  std::memset(&device, 0, sizeof(device));
  PixelGrabError err = pixelgrab_audio_get_default_device(ctx_, 1, &device);
  EXPECT_EQ(err, kPixelGrabOk);
  EXPECT_GT(std::strlen(device.name), 0u);
  EXPECT_NE(device.is_default, 0);
  EXPECT_NE(device.is_input, 0);
}

TEST_F(AudioTest, GetDefaultOutputDevice) {
  if (!pixelgrab_audio_is_supported(ctx_)) {
    GTEST_SKIP() << "Audio not supported on this platform";
  }

  PixelGrabAudioDeviceInfo device;
  std::memset(&device, 0, sizeof(device));
  PixelGrabError err = pixelgrab_audio_get_default_device(ctx_, 0, &device);
  EXPECT_EQ(err, kPixelGrabOk);
  EXPECT_GT(std::strlen(device.name), 0u);
  EXPECT_NE(device.is_default, 0);
  EXPECT_EQ(device.is_input, 0);
}

// ---------------------------------------------------------------------------
// PixelGrabRecordConfig audio fields — backward compatibility
// ---------------------------------------------------------------------------

TEST_F(AudioTest, RecordConfigAudioFieldsDefault) {
  // Verify that zero-initialized PixelGrabRecordConfig has audio_source = None.
  PixelGrabRecordConfig cfg;
  std::memset(&cfg, 0, sizeof(cfg));
  EXPECT_EQ(cfg.audio_source, kPixelGrabAudioNone);
  EXPECT_EQ(cfg.audio_device_id, nullptr);
  EXPECT_EQ(cfg.audio_sample_rate, 0);
}

TEST_F(AudioTest, RecordConfigAudioSourceEnum) {
  // Verify enum values are stable.
  EXPECT_EQ(kPixelGrabAudioNone, 0);
  EXPECT_EQ(kPixelGrabAudioMicrophone, 1);
  EXPECT_EQ(kPixelGrabAudioSystem, 2);
  EXPECT_EQ(kPixelGrabAudioBoth, 3);
}

// ---------------------------------------------------------------------------
// Recorder + audio_source integration (smoke test)
// ---------------------------------------------------------------------------

TEST_F(AudioTest, RecorderCreateWithAudioNone) {
  // Creating a recorder with audio_source = None should work as before.
  if (!pixelgrab_recorder_is_supported(ctx_)) {
    GTEST_SKIP() << "Recorder not supported";
  }

  char path[128];
  std::snprintf(path, sizeof(path), "test_audio_none_%p.mp4", ctx_);

  PixelGrabRecordConfig cfg;
  std::memset(&cfg, 0, sizeof(cfg));
  cfg.output_path = path;
  cfg.audio_source = kPixelGrabAudioNone;

  PixelGrabRecorder* rec = pixelgrab_recorder_create(ctx_, &cfg);
  EXPECT_NE(rec, nullptr);
  if (rec) {
    pixelgrab_recorder_destroy(rec);
  }
  std::remove(path);
}
