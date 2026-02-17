// Copyright 2026 The loong-pixelgrab Authors
// macOS recording backend — AVAssetWriter + AVFoundation implementation.

#include "core/recorder_backend.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

#include "core/logger.h"
#include "watermark/watermark_renderer.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>

namespace pixelgrab {
namespace internal {

class MacRecorderBackend : public RecorderBackend {
 public:
  MacRecorderBackend() = default;

  ~MacRecorderBackend() override {
    StopCaptureLoop();
    if (state_ == RecordState::kRecording ||
        state_ == RecordState::kPaused) {
      Stop();
    }
  }

  bool Initialize(const RecordConfig& config) override {
    config_ = config;

    // Resolve dimensions — 0 means primary screen.
    frame_width_ = config.region_width;
    frame_height_ = config.region_height;
    if (frame_width_ <= 0 || frame_height_ <= 0) {
      // Use main screen dimensions.
      NSScreen* main_screen = [NSScreen mainScreen];
      NSRect frame = [main_screen frame];
      frame_width_ = static_cast<int>(frame.size.width);
      frame_height_ = static_cast<int>(frame.size.height);
    }
    // H.264 requires even dimensions.
    frame_width_ = (frame_width_ + 1) & ~1;
    frame_height_ = (frame_height_ + 1) & ~1;

    fps_ = config.fps;
    frame_duration_ns_ = 1'000'000'000LL / fps_;

    // Create output URL.
    NSString* ns_path =
        [NSString stringWithUTF8String:config.output_path.c_str()];
    NSURL* output_url = [NSURL fileURLWithPath:ns_path];

    // Delete existing file if present (AVAssetWriter fails otherwise).
    [[NSFileManager defaultManager] removeItemAtURL:output_url error:nil];

    NSError* error = nil;
    asset_writer_ =
        [[AVAssetWriter alloc] initWithURL:output_url
                                  fileType:AVFileTypeMPEG4
                                     error:&error];
    if (!asset_writer_ || error) {
      PIXELGRAB_LOG_ERROR("AVAssetWriter creation failed: {}",
                         error ? [[error localizedDescription] UTF8String]
                               : "unknown");
      return false;
    }

    // Configure video output settings (H.264).
    NSDictionary* video_settings = @{
      AVVideoCodecKey : AVVideoCodecTypeH264,
      AVVideoWidthKey : @(frame_width_),
      AVVideoHeightKey : @(frame_height_),
      AVVideoCompressionPropertiesKey : @{
        AVVideoAverageBitRateKey : @(config.bitrate),
        AVVideoExpectedSourceFrameRateKey : @(fps_),
        AVVideoMaxKeyFrameIntervalKey : @(fps_ * 2),
      },
    };

    writer_input_ = [[AVAssetWriterInput alloc]
        initWithMediaType:AVMediaTypeVideo
           outputSettings:video_settings];
    writer_input_.expectsMediaDataInRealTime = YES;

    // Pixel buffer adaptor for CVPixelBuffer submission.
    NSDictionary* pixel_attrs = @{
      (NSString*)kCVPixelBufferPixelFormatTypeKey :
          @(kCVPixelFormatType_32BGRA),
      (NSString*)kCVPixelBufferWidthKey : @(frame_width_),
      (NSString*)kCVPixelBufferHeightKey : @(frame_height_),
    };
    adaptor_ = [[AVAssetWriterInputPixelBufferAdaptor alloc]
        initWithAssetWriterInput:writer_input_
        sourcePixelBufferAttributes:pixel_attrs];

    if (![asset_writer_ canAddInput:writer_input_]) {
      PIXELGRAB_LOG_ERROR("Cannot add video input to AVAssetWriter");
      return false;
    }
    [asset_writer_ addInput:writer_input_];

    state_ = RecordState::kIdle;
    PIXELGRAB_LOG_INFO("macOS Recorder initialized: {}x{} @{}fps, {}bps → {}",
                       frame_width_, frame_height_, fps_, config.bitrate,
                       config.output_path);
    return true;
  }

  bool Start() override {
    if (state_ != RecordState::kIdle) return false;

    if (![asset_writer_ startWriting]) {
      PIXELGRAB_LOG_ERROR("AVAssetWriter startWriting failed: {}",
                         [[asset_writer_.error localizedDescription] UTF8String]);
      return false;
    }
    [asset_writer_ startSessionAtSourceTime:kCMTimeZero];

    frame_count_ = 0;
    presentation_time_ns_ = 0;
    start_time_ = std::chrono::steady_clock::now();
    state_ = RecordState::kRecording;
    PIXELGRAB_LOG_INFO("macOS Recording started");
    return true;
  }

  bool Pause() override {
    if (state_ != RecordState::kRecording) return false;
    paused_.store(true, std::memory_order_release);
    pause_start_ = std::chrono::steady_clock::now();
    state_ = RecordState::kPaused;
    PIXELGRAB_LOG_INFO("macOS Recording paused");
    return true;
  }

  bool Resume() override {
    if (state_ != RecordState::kPaused) return false;
    auto pause_end = std::chrono::steady_clock::now();
    paused_duration_ns_ += std::chrono::duration_cast<std::chrono::nanoseconds>(
                               pause_end - pause_start_)
                               .count();
    paused_.store(false, std::memory_order_release);
    state_ = RecordState::kRecording;
    PIXELGRAB_LOG_INFO("macOS Recording resumed");
    return true;
  }

  bool WriteFrame(const Image& frame) override {
    if (state_ != RecordState::kRecording) return false;

    std::lock_guard<std::mutex> lock(write_mutex_);

    // Wait for writer input to be ready.
    if (![writer_input_ isReadyForMoreMediaData]) {
      PIXELGRAB_LOG_WARN("AVAssetWriterInput not ready, dropping frame");
      return false;
    }

    // Create CVPixelBuffer from BGRA image data.
    CVPixelBufferRef pixel_buffer = nullptr;
    CVReturn cv_ret = CVPixelBufferCreate(
        kCFAllocatorDefault, frame_width_, frame_height_,
        kCVPixelFormatType_32BGRA, nullptr, &pixel_buffer);
    if (cv_ret != kCVReturnSuccess || !pixel_buffer) {
      PIXELGRAB_LOG_ERROR("CVPixelBufferCreate failed: {}", cv_ret);
      return false;
    }

    CVPixelBufferLockBaseAddress(pixel_buffer, 0);
    uint8_t* dest =
        static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixel_buffer));
    size_t dest_stride = CVPixelBufferGetBytesPerRow(pixel_buffer);

    const uint8_t* src = frame.data();
    int src_stride = frame.stride();

    // Copy row by row (strides may differ).
    int copy_width = frame_width_ * 4;
    for (int row = 0; row < frame_height_; ++row) {
      std::memcpy(dest + row * dest_stride, src + row * src_stride,
                  static_cast<size_t>(copy_width));
    }
    CVPixelBufferUnlockBaseAddress(pixel_buffer, 0);

    // Compute presentation time.
    CMTime pts = CMTimeMake(presentation_time_ns_, 1'000'000'000);
    presentation_time_ns_ += frame_duration_ns_;

    BOOL ok = [adaptor_ appendPixelBuffer:pixel_buffer
                     withPresentationTime:pts];
    CVPixelBufferRelease(pixel_buffer);

    if (!ok) {
      PIXELGRAB_LOG_ERROR("appendPixelBuffer failed");
      return false;
    }

    ++frame_count_;
    return true;
  }

  bool Stop() override {
    if (state_ != RecordState::kRecording &&
        state_ != RecordState::kPaused) {
      return false;
    }

    StopCaptureLoop();

    [writer_input_ markAsFinished];

    // Synchronous finish.
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    [asset_writer_ finishWritingWithCompletionHandler:^{
      dispatch_semaphore_signal(sem);
    }];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

    bool success = (asset_writer_.status == AVAssetWriterStatusCompleted);
    if (!success) {
      PIXELGRAB_LOG_ERROR("AVAssetWriter finishWriting failed: {}",
                         asset_writer_.error
                             ? [[asset_writer_.error localizedDescription]
                                   UTF8String]
                             : "unknown");
    }

    state_ = RecordState::kStopped;
    PIXELGRAB_LOG_INFO("macOS Recording stopped: {} frames, {}ms",
                       frame_count_, GetDurationMs());
    return success;
  }

  RecordState GetState() const override { return state_; }

  int64_t GetDurationMs() const override {
    if (frame_count_ == 0) return 0;
    return (frame_count_ * 1000LL) / fps_;
  }

  int64_t GetFrameCount() const override { return frame_count_; }

  bool IsAutoCapture() const override { return config_.auto_capture; }

  void StartCaptureLoop() override {
    if (!config_.auto_capture) return;
    if (!config_.capture_backend) {
      PIXELGRAB_LOG_ERROR("Auto capture enabled but no capture backend set");
      return;
    }
    if (capture_running_.load(std::memory_order_acquire)) return;

    capture_running_.store(true, std::memory_order_release);
    capture_thread_ =
        std::thread(&MacRecorderBackend::CaptureLoopFunc, this);
    PIXELGRAB_LOG_INFO("macOS Capture loop started (auto mode)");
  }

  void StopCaptureLoop() override {
    if (!capture_running_.load(std::memory_order_acquire)) return;

    capture_running_.store(false, std::memory_order_release);
    if (capture_thread_.joinable()) {
      capture_thread_.join();
    }
    PIXELGRAB_LOG_INFO("macOS Capture loop stopped");
  }

 private:
  /// Background thread: capture → watermark → encode loop.
  void CaptureLoopFunc() {
    const auto interval = std::chrono::microseconds(1'000'000 / fps_);

    while (capture_running_.load(std::memory_order_acquire)) {
      auto tick_start = std::chrono::steady_clock::now();

      if (!paused_.load(std::memory_order_acquire)) {
        auto frame_img = config_.capture_backend->CaptureRegion(
            config_.region_x, config_.region_y,
            config_.region_width, config_.region_height);

        if (frame_img) {
          if (config_.has_watermark && config_.watermark_renderer) {
            config_.watermark_renderer->ApplyTextWatermark(
                frame_img.get(), config_.watermark_config);
          }
          WriteFrame(*frame_img);
        }
      }

      auto elapsed = std::chrono::steady_clock::now() - tick_start;
      if (elapsed < interval) {
        std::this_thread::sleep_for(interval - elapsed);
      }
    }
  }

  RecordConfig config_;
  int frame_width_ = 0;
  int frame_height_ = 0;
  int fps_ = 30;
  int64_t frame_duration_ns_ = 0;
  int64_t frame_count_ = 0;
  int64_t presentation_time_ns_ = 0;
  int64_t paused_duration_ns_ = 0;
  RecordState state_ = RecordState::kIdle;
  std::chrono::steady_clock::time_point start_time_;
  std::chrono::steady_clock::time_point pause_start_;

  // AVFoundation objects.
  AVAssetWriter* asset_writer_ = nil;
  AVAssetWriterInput* writer_input_ = nil;
  AVAssetWriterInputPixelBufferAdaptor* adaptor_ = nil;

  // Thread safety.
  std::mutex write_mutex_;
  std::thread capture_thread_;
  std::atomic<bool> capture_running_{false};
  std::atomic<bool> paused_{false};
};

std::unique_ptr<RecorderBackend> CreatePlatformRecorder() {
  return std::make_unique<MacRecorderBackend>();
}

}  // namespace internal
}  // namespace pixelgrab
