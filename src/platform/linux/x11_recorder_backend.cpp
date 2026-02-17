// Copyright 2026 The loong-pixelgrab Authors
// Linux recording backend — GStreamer appsrc pipeline implementation.

#include "core/recorder_backend.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include "core/logger.h"
#include "watermark/watermark_renderer.h"

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

namespace pixelgrab {
namespace internal {

class X11RecorderBackend : public RecorderBackend {
 public:
  X11RecorderBackend() = default;

  ~X11RecorderBackend() override {
    StopCaptureLoop();
    if (state_ == RecordState::kRecording ||
        state_ == RecordState::kPaused) {
      Stop();
    }
    CleanupPipeline();
  }

  bool Initialize(const RecordConfig& config) override {
    config_ = config;

    // Initialize GStreamer (process-global, idempotent).
    gst_init(nullptr, nullptr);

    // Resolve dimensions — 0 means primary screen.
    frame_width_ = config.region_width;
    frame_height_ = config.region_height;
    if (frame_width_ <= 0 || frame_height_ <= 0) {
      // Default to 1920x1080 — real resolution will come from CaptureBackend.
      frame_width_ = 1920;
      frame_height_ = 1080;
    }
    // H.264 requires even dimensions.
    frame_width_ = (frame_width_ + 1) & ~1;
    frame_height_ = (frame_height_ + 1) & ~1;

    fps_ = config.fps;
    frame_duration_ns_ = GST_SECOND / fps_;

    // Build pipeline string.
    // appsrc → videoconvert → x264enc → h264parse → mp4mux → filesink
    std::string pipeline_str =
        "appsrc name=videosrc is-live=true format=time "
        "! video/x-raw,format=BGRA"
        ",width=" +
        std::to_string(frame_width_) +
        ",height=" + std::to_string(frame_height_) +
        ",framerate=" + std::to_string(fps_) +
        "/1 "
        "! videoconvert "
        "! x264enc tune=zerolatency bitrate=" +
        std::to_string(config.bitrate / 1000) +  // x264enc uses kbps
        " speed-preset=ultrafast "
        "! h264parse "
        "! mp4mux name=mux "
        "! filesink location=" +
        config.output_path;

    GError* error = nullptr;
    pipeline_ = gst_parse_launch(pipeline_str.c_str(), &error);
    if (!pipeline_ || error) {
      PIXELGRAB_LOG_ERROR("GStreamer pipeline creation failed: {}",
                         error ? error->message : "unknown");
      if (error) g_error_free(error);
      return false;
    }

    // Get appsrc element reference.
    appsrc_ = gst_bin_get_by_name(GST_BIN(pipeline_), "videosrc");
    if (!appsrc_) {
      PIXELGRAB_LOG_ERROR("Failed to get appsrc element from pipeline");
      CleanupPipeline();
      return false;
    }

    state_ = RecordState::kIdle;
    PIXELGRAB_LOG_INFO(
        "Linux Recorder initialized: {}x{} @{}fps, {}bps → {}",
        frame_width_, frame_height_, fps_, config.bitrate,
        config.output_path);
    return true;
  }

  bool Start() override {
    if (state_ != RecordState::kIdle) return false;

    GstStateChangeReturn ret =
        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
      PIXELGRAB_LOG_ERROR("Failed to set GStreamer pipeline to PLAYING");
      return false;
    }

    frame_count_ = 0;
    gst_pts_ = 0;
    start_time_ = std::chrono::steady_clock::now();
    state_ = RecordState::kRecording;
    PIXELGRAB_LOG_INFO("Linux Recording started");
    return true;
  }

  bool Pause() override {
    if (state_ != RecordState::kRecording) return false;

    GstStateChangeReturn ret =
        gst_element_set_state(pipeline_, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
      PIXELGRAB_LOG_ERROR("Failed to pause GStreamer pipeline");
      return false;
    }

    paused_.store(true, std::memory_order_release);
    state_ = RecordState::kPaused;
    PIXELGRAB_LOG_INFO("Linux Recording paused");
    return true;
  }

  bool Resume() override {
    if (state_ != RecordState::kPaused) return false;

    GstStateChangeReturn ret =
        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
      PIXELGRAB_LOG_ERROR("Failed to resume GStreamer pipeline");
      return false;
    }

    paused_.store(false, std::memory_order_release);
    state_ = RecordState::kRecording;
    PIXELGRAB_LOG_INFO("Linux Recording resumed");
    return true;
  }

  bool WriteFrame(const Image& frame) override {
    if (state_ != RecordState::kRecording) return false;

    std::lock_guard<std::mutex> lock(write_mutex_);

    // Allocate GstBuffer and copy BGRA pixel data.
    gsize buf_size =
        static_cast<gsize>(frame_width_) * frame_height_ * 4;
    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, buf_size, nullptr);
    if (!buffer) {
      PIXELGRAB_LOG_ERROR("gst_buffer_new_allocate failed");
      return false;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
      gst_buffer_unref(buffer);
      return false;
    }

    // Copy frame data row-by-row (strides may differ).
    const uint8_t* src = frame.data();
    int src_stride = frame.stride();
    int dst_stride = frame_width_ * 4;
    for (int row = 0; row < frame_height_; ++row) {
      std::memcpy(map.data + row * dst_stride,
                  src + row * src_stride,
                  static_cast<size_t>(dst_stride));
    }
    gst_buffer_unmap(buffer, &map);

    // Set timestamps.
    GST_BUFFER_PTS(buffer) = gst_pts_;
    GST_BUFFER_DURATION(buffer) = frame_duration_ns_;
    gst_pts_ += frame_duration_ns_;

    // Push buffer to appsrc (takes ownership).
    GstFlowReturn flow_ret =
        gst_app_src_push_buffer(GST_APP_SRC(appsrc_), buffer);
    if (flow_ret != GST_FLOW_OK) {
      PIXELGRAB_LOG_ERROR("gst_app_src_push_buffer failed: {}",
                         static_cast<int>(flow_ret));
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

    // Send EOS to let mp4mux write the moov atom.
    gst_app_src_end_of_stream(GST_APP_SRC(appsrc_));

    // Wait for EOS to propagate through the pipeline.
    GstBus* bus = gst_element_get_bus(pipeline_);
    if (bus) {
      GstMessage* msg = gst_bus_timed_pop_filtered(
          bus, 5 * GST_SECOND,
          static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
      if (msg) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
          GError* err = nullptr;
          gst_message_parse_error(msg, &err, nullptr);
          PIXELGRAB_LOG_ERROR("GStreamer pipeline error: {}",
                             err ? err->message : "unknown");
          if (err) g_error_free(err);
        }
        gst_message_unref(msg);
      }
      gst_object_unref(bus);
    }

    gst_element_set_state(pipeline_, GST_STATE_NULL);

    state_ = RecordState::kStopped;
    PIXELGRAB_LOG_INFO("Linux Recording stopped: {} frames, {}ms",
                       frame_count_, GetDurationMs());
    return true;
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
        std::thread(&X11RecorderBackend::CaptureLoopFunc, this);
    PIXELGRAB_LOG_INFO("Linux Capture loop started (auto mode)");
  }

  void StopCaptureLoop() override {
    if (!capture_running_.load(std::memory_order_acquire)) return;

    capture_running_.store(false, std::memory_order_release);
    if (capture_thread_.joinable()) {
      capture_thread_.join();
    }
    PIXELGRAB_LOG_INFO("Linux Capture loop stopped");
  }

 private:
  void CleanupPipeline() {
    if (appsrc_) {
      gst_object_unref(appsrc_);
      appsrc_ = nullptr;
    }
    if (pipeline_) {
      gst_element_set_state(pipeline_, GST_STATE_NULL);
      gst_object_unref(pipeline_);
      pipeline_ = nullptr;
    }
  }

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
  guint64 frame_duration_ns_ = 0;
  int64_t frame_count_ = 0;
  guint64 gst_pts_ = 0;  // PTS accumulator (nanoseconds)
  RecordState state_ = RecordState::kIdle;
  std::chrono::steady_clock::time_point start_time_;

  // GStreamer objects.
  GstElement* pipeline_ = nullptr;
  GstElement* appsrc_ = nullptr;

  // Thread safety.
  std::mutex write_mutex_;
  std::thread capture_thread_;
  std::atomic<bool> capture_running_{false};
  std::atomic<bool> paused_{false};
};

std::unique_ptr<RecorderBackend> CreatePlatformRecorder() {
  return std::make_unique<X11RecorderBackend>();
}

}  // namespace internal
}  // namespace pixelgrab
