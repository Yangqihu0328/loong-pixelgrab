// Copyright 2026 The loong-pixelgrab Authors
// Linux audio backend — PulseAudio Simple API implementation.

#include "core/audio_backend.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/logger.h"

#include <pulse/simple.h>
#include <pulse/error.h>

namespace pixelgrab {
namespace internal {

class X11AudioBackend : public AudioBackend {
 public:
  X11AudioBackend() = default;

  ~X11AudioBackend() override {
    Stop();
    if (pa_simple_) {
      pa_simple_free(pa_simple_);
      pa_simple_ = nullptr;
    }
  }

  bool IsSupported() const override {
    // Test by creating a temporary PulseAudio connection.
    pa_sample_spec spec = {};
    spec.format = PA_SAMPLE_S16LE;
    spec.rate = 44100;
    spec.channels = 2;

    int error = 0;
    pa_simple* test = pa_simple_new(
        nullptr, "pixelgrab", PA_STREAM_RECORD, nullptr,
        "support_check", &spec, nullptr, nullptr, &error);
    if (test) {
      pa_simple_free(test);
      return true;
    }
    return false;
  }

  std::vector<AudioDeviceInfo> EnumerateDevices() override {
    std::vector<AudioDeviceInfo> result;

    // PulseAudio Simple API doesn't support device enumeration.
    // Provide a default device entry.
    AudioDeviceInfo default_mic;
    default_mic.id = "";
    default_mic.name = "Default Microphone";
    default_mic.is_input = true;
    default_mic.is_default = true;
    result.push_back(std::move(default_mic));

    // System audio monitor (loopback).
    AudioDeviceInfo monitor;
    monitor.id = "@DEFAULT_MONITOR@";
    monitor.name = "System Audio (Monitor)";
    monitor.is_input = false;
    monitor.is_default = true;
    result.push_back(std::move(monitor));

    return result;
  }

  AudioDeviceInfo GetDefaultDevice(bool is_input) override {
    AudioDeviceInfo info;
    info.is_input = is_input;
    info.is_default = true;

    if (is_input) {
      info.id = "";
      info.name = "Default Microphone";
    } else {
      info.id = "@DEFAULT_MONITOR@";
      info.name = "System Audio (Monitor)";
    }

    return info;
  }

  bool Initialize(const std::string& device_id,
                  PixelGrabAudioSource source,
                  int sample_rate) override {
    source_ = source;
    sample_rate_ = (sample_rate > 0) ? sample_rate : 44100;
    channels_ = 2;

    pa_sample_spec spec = {};
    spec.format = PA_SAMPLE_S16LE;
    spec.rate = static_cast<uint32_t>(sample_rate_);
    spec.channels = static_cast<uint8_t>(channels_);

    // Determine the PulseAudio device name.
    const char* pa_device = nullptr;
    if (!device_id.empty()) {
      pa_device = device_id.c_str();
    } else if (source == kPixelGrabAudioSystem ||
               source == kPixelGrabAudioBoth) {
      // Use the default monitor source for system audio loopback.
      pa_device = "@DEFAULT_MONITOR@";
    }
    // nullptr = PulseAudio default source (microphone)

    int error = 0;
    pa_simple_ = pa_simple_new(
        nullptr,               // default server
        "pixelgrab",           // application name
        PA_STREAM_RECORD,      // direction
        pa_device,             // device (nullptr = default)
        "audio_capture",       // stream name
        &spec,                 // sample format
        nullptr,               // default channel map
        nullptr,               // default buffer attributes
        &error);

    if (!pa_simple_) {
      PIXELGRAB_LOG_ERROR("PulseAudio connection failed: {}",
                         pa_strerror(error));
      return false;
    }

    initialized_ = true;
    PIXELGRAB_LOG_INFO("PulseAudio audio initialized: {}Hz, {}ch, device={}",
                       sample_rate_, channels_,
                       pa_device ? pa_device : "default");
    return true;
  }

  bool Start() override {
    if (!initialized_ || !pa_simple_) return false;
    if (capturing_.load()) return true;

    capturing_.store(true, std::memory_order_release);
    capture_thread_ = std::thread(&X11AudioBackend::CaptureLoop, this);
    return true;
  }

  bool Stop() override {
    if (!capturing_.load()) return false;

    capturing_.store(false, std::memory_order_release);
    if (capture_thread_.joinable()) {
      capture_thread_.join();
    }
    return true;
  }

  AudioSamples ReadSamples() override {
    AudioSamples samples;
    samples.sample_rate = sample_rate_;
    samples.channels = channels_;

    std::lock_guard<std::mutex> lock(buffer_mutex_);
    samples.data = std::move(pending_samples_);
    pending_samples_.clear();

    return samples;
  }

  int GetSampleRate() const override { return sample_rate_; }
  int GetChannels() const override { return channels_; }

 private:
  /// Background thread that reads from PulseAudio.
  void CaptureLoop() {
    // Read buffer: 10ms of audio at a time.
    int frames_per_read = sample_rate_ / 100;
    int samples_per_read = frames_per_read * channels_;
    std::vector<int16_t> read_buf(samples_per_read);

    while (capturing_.load(std::memory_order_acquire)) {
      int error = 0;
      int ret = pa_simple_read(pa_simple_, read_buf.data(),
                               read_buf.size() * sizeof(int16_t), &error);
      if (ret < 0) {
        PIXELGRAB_LOG_ERROR("pa_simple_read failed: {}", pa_strerror(error));
        break;
      }

      std::lock_guard<std::mutex> lock(buffer_mutex_);
      pending_samples_.insert(pending_samples_.end(),
                              read_buf.begin(), read_buf.end());
    }
  }

  bool initialized_ = false;
  std::atomic<bool> capturing_{false};
  PixelGrabAudioSource source_ = kPixelGrabAudioNone;
  int sample_rate_ = 44100;
  int channels_ = 2;

  pa_simple* pa_simple_ = nullptr;

  // Background capture thread.
  std::thread capture_thread_;

  // Thread-safe sample buffer.
  std::mutex buffer_mutex_;
  std::vector<int16_t> pending_samples_;
};

std::unique_ptr<AudioBackend> CreatePlatformAudioBackend() {
  return std::make_unique<X11AudioBackend>();
}

}  // namespace internal
}  // namespace pixelgrab
