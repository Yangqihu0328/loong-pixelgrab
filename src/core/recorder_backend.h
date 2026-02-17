// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_CORE_RECORDER_BACKEND_H_
#define PIXELGRAB_CORE_RECORDER_BACKEND_H_

#include <cstdint>
#include <memory>
#include <string>

#include "core/audio_backend.h"
#include "core/capture_backend.h"
#include "core/image.h"
#include "pixelgrab/pixelgrab.h"

namespace pixelgrab {
namespace internal {

/// Recording state (maps to PixelGrabRecordState).
enum class RecordState {
  kIdle = 0,
  kRecording = 1,
  kPaused = 2,
  kStopped = 3,
};

// Forward declaration.
class WatermarkRenderer;

/// Internal recording configuration (parsed from PixelGrabRecordConfig).
struct RecordConfig {
  std::string output_path;
  int region_x = 0;
  int region_y = 0;
  int region_width = 0;   // 0 = full screen
  int region_height = 0;  // 0 = full screen
  int fps = 30;
  int bitrate = 4000000;  // 4 Mbps
  bool auto_capture = false;

  // Non-owning pointers — set by API layer when auto_capture is true.
  CaptureBackend* capture_backend = nullptr;
  WatermarkRenderer* watermark_renderer = nullptr;

  // System watermark (branding, bottom-right, always-on).
  PixelGrabTextWatermarkConfig watermark_config = {};
  bool has_watermark = false;

  // User-defined watermark (rendered at multiple positions).
  PixelGrabTextWatermarkConfig user_watermark_config = {};
  bool has_user_watermark = false;

  // Audio configuration.
  PixelGrabAudioSource audio_source = kPixelGrabAudioNone;
  std::string audio_device_id;
  int audio_sample_rate = 44100;
  AudioBackend* audio_backend = nullptr;  // Non-owning, set by API layer.

  // GPU acceleration hint.
  // 0 = auto (try GPU, fallback CPU), 1 = prefer GPU, -1 = force CPU.
  int gpu_hint = 0;
};

/// Abstract interface for platform-specific screen recording backends.
///
/// Each platform (Windows, macOS, Linux) provides a concrete implementation.
/// Only the implementation for the current build platform is compiled.
///
/// Windows: Media Foundation Sink Writer (H.264 MFT)
/// macOS:   AVFoundation AVAssetWriter (future)
/// Linux:   PipeWire + GStreamer (future)
class RecorderBackend {
 public:
  virtual ~RecorderBackend() = default;

  // Non-copyable.
  RecorderBackend(const RecorderBackend&) = delete;
  RecorderBackend& operator=(const RecorderBackend&) = delete;

  /// Initialize the recorder with the given configuration.
  /// @param config  Recording parameters.
  /// @return true on success.
  virtual bool Initialize(const RecordConfig& config) = 0;

  /// Start recording.
  virtual bool Start() = 0;

  /// Pause recording (frames are not written, but the session stays open).
  virtual bool Pause() = 0;

  /// Resume a paused recording.
  virtual bool Resume() = 0;

  /// Write a single frame to the recording.
  /// The frame image must be in BGRA8 format.
  /// @param frame  Frame image data.
  /// @return true on success.
  virtual bool WriteFrame(const Image& frame) = 0;

  /// Stop recording and finalize the output file.
  virtual bool Stop() = 0;

  /// Get the current recording state.
  virtual RecordState GetState() const = 0;

  /// Get the total recorded duration in milliseconds.
  virtual int64_t GetDurationMs() const = 0;

  /// Get the total number of frames written.
  virtual int64_t GetFrameCount() const = 0;

  /// Whether this recorder was configured for auto capture.
  virtual bool IsAutoCapture() const = 0;

  /// Start the internal capture loop (auto mode only).
  /// Called by the API layer after Start(). No-op if auto_capture is false.
  virtual void StartCaptureLoop() = 0;

  /// Stop the internal capture loop and join the background thread.
  /// Called by the API layer before Stop(). No-op if auto_capture is false.
  virtual void StopCaptureLoop() = 0;

 protected:
  RecorderBackend() = default;
};

/// Factory function implemented per-platform (one per build target).
/// Defined in platform/<os>/xxx_recorder_backend.cpp.
std::unique_ptr<RecorderBackend> CreatePlatformRecorder();

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_CORE_RECORDER_BACKEND_H_
