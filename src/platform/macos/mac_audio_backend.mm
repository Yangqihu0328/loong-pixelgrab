// Copyright 2026 The loong-pixelgrab Authors
// macOS audio backend — CoreAudio implementation.

#include "core/audio_backend.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>

#include "core/logger.h"

#import <AudioToolbox/AudioToolbox.h>
#import <CoreAudio/CoreAudio.h>
#import <Foundation/Foundation.h>

namespace pixelgrab {
namespace internal {

namespace {

/// Get the name of an AudioDevice.
std::string GetAudioDeviceName(AudioDeviceID device_id) {
  CFStringRef name_ref = nullptr;
  AudioObjectPropertyAddress addr = {
      kAudioDevicePropertyDeviceNameCFString,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};

  UInt32 size = sizeof(CFStringRef);
  OSStatus status = AudioObjectGetPropertyData(device_id, &addr, 0, nullptr,
                                               &size, &name_ref);
  if (status != noErr || !name_ref) return "Unknown";

  NSString* ns_name = (__bridge_transfer NSString*)name_ref;
  return [ns_name UTF8String] ?: "Unknown";
}

/// Get the UID string of an AudioDevice.
std::string GetAudioDeviceUID(AudioDeviceID device_id) {
  CFStringRef uid_ref = nullptr;
  AudioObjectPropertyAddress addr = {
      kAudioDevicePropertyDeviceUID,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};

  UInt32 size = sizeof(CFStringRef);
  OSStatus status = AudioObjectGetPropertyData(device_id, &addr, 0, nullptr,
                                               &size, &uid_ref);
  if (status != noErr || !uid_ref) return "";

  NSString* ns_uid = (__bridge_transfer NSString*)uid_ref;
  return [ns_uid UTF8String] ?: "";
}

/// Check if a device has input (capture) channels.
bool DeviceHasInputChannels(AudioDeviceID device_id) {
  AudioObjectPropertyAddress addr = {
      kAudioDevicePropertyStreamConfiguration,
      kAudioDevicePropertyScopeInput,
      kAudioObjectPropertyElementMain};

  UInt32 size = 0;
  OSStatus status =
      AudioObjectGetPropertyDataSize(device_id, &addr, 0, nullptr, &size);
  if (status != noErr || size == 0) return false;

  std::vector<uint8_t> buf(size);
  auto* list = reinterpret_cast<AudioBufferList*>(buf.data());
  status = AudioObjectGetPropertyData(device_id, &addr, 0, nullptr,
                                      &size, list);
  if (status != noErr) return false;

  for (UInt32 i = 0; i < list->mNumberBuffers; ++i) {
    if (list->mBuffers[i].mNumberChannels > 0) return true;
  }
  return false;
}

}  // namespace

class MacAudioBackend : public AudioBackend {
 public:
  MacAudioBackend() = default;

  ~MacAudioBackend() override {
    Stop();
    if (audio_queue_) {
      AudioQueueDispose(audio_queue_, true);
      audio_queue_ = nullptr;
    }
  }

  bool IsSupported() const override { return true; }

  std::vector<AudioDeviceInfo> EnumerateDevices() override {
    std::vector<AudioDeviceInfo> result;

    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};

    UInt32 size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject, &addr, 0, nullptr, &size);
    if (status != noErr || size == 0) return result;

    UInt32 count = size / sizeof(AudioDeviceID);
    std::vector<AudioDeviceID> devices(count);
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr,
                                        0, nullptr, &size, devices.data());
    if (status != noErr) return result;

    // Get default input/output devices.
    AudioDeviceID default_input = 0, default_output = 0;
    {
      UInt32 dev_size = sizeof(AudioDeviceID);
      AudioObjectPropertyAddress input_addr = {
          kAudioHardwarePropertyDefaultInputDevice,
          kAudioObjectPropertyScopeGlobal,
          kAudioObjectPropertyElementMain};
      AudioObjectGetPropertyData(kAudioObjectSystemObject, &input_addr,
                                 0, nullptr, &dev_size, &default_input);

      AudioObjectPropertyAddress output_addr = {
          kAudioHardwarePropertyDefaultOutputDevice,
          kAudioObjectPropertyScopeGlobal,
          kAudioObjectPropertyElementMain};
      AudioObjectGetPropertyData(kAudioObjectSystemObject, &output_addr,
                                 0, nullptr, &dev_size, &default_output);
    }

    for (auto dev_id : devices) {
      bool has_input = DeviceHasInputChannels(dev_id);
      // Only list input devices (microphones).
      // macOS doesn't have a standard loopback API without ScreenCaptureKit.
      if (!has_input) continue;

      AudioDeviceInfo info;
      info.id = GetAudioDeviceUID(dev_id);
      info.name = GetAudioDeviceName(dev_id);
      info.is_input = true;
      info.is_default = (dev_id == default_input);
      result.push_back(std::move(info));
    }

    return result;
  }

  AudioDeviceInfo GetDefaultDevice(bool is_input) override {
    AudioDeviceInfo info;
    info.is_input = is_input;
    info.is_default = true;

    AudioObjectPropertyAddress addr = {
        is_input ? kAudioHardwarePropertyDefaultInputDevice
                 : kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};

    AudioDeviceID dev_id = 0;
    UInt32 size = sizeof(AudioDeviceID);
    OSStatus status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &addr, 0, nullptr, &size, &dev_id);
    if (status == noErr && dev_id != 0) {
      info.id = GetAudioDeviceUID(dev_id);
      info.name = GetAudioDeviceName(dev_id);
    } else {
      info.name = "Default";
    }

    return info;
  }

  bool Initialize(const std::string& device_id,
                  PixelGrabAudioSource source,
                  int sample_rate) override {
    source_ = source;
    sample_rate_ = (sample_rate > 0) ? sample_rate : 44100;
    channels_ = 2;

    // Set up Audio Queue for input recording.
    AudioStreamBasicDescription format = {};
    format.mSampleRate = static_cast<Float64>(sample_rate_);
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger |
                          kLinearPCMFormatFlagIsPacked;
    format.mBitsPerChannel = 16;
    format.mChannelsPerFrame = static_cast<UInt32>(channels_);
    format.mBytesPerFrame = format.mChannelsPerFrame * 2;
    format.mFramesPerPacket = 1;
    format.mBytesPerPacket = format.mBytesPerFrame;

    OSStatus status = AudioQueueNewInput(
        &format, AudioInputCallback, this, nullptr,
        kCFRunLoopCommonModes, 0, &audio_queue_);
    if (status != noErr) {
      PIXELGRAB_LOG_ERROR("AudioQueueNewInput failed: {}", status);
      return false;
    }

    // Allocate and enqueue buffers.
    UInt32 buffer_size =
        static_cast<UInt32>(sample_rate_) * channels_ * 2 / 10;  // 100ms
    for (int i = 0; i < kNumBuffers; ++i) {
      AudioQueueBufferRef buf = nullptr;
      status = AudioQueueAllocateBuffer(audio_queue_, buffer_size, &buf);
      if (status != noErr) {
        PIXELGRAB_LOG_ERROR("AudioQueueAllocateBuffer failed: {}", status);
        return false;
      }
      AudioQueueEnqueueBuffer(audio_queue_, buf, 0, nullptr);
    }

    initialized_ = true;
    PIXELGRAB_LOG_INFO("macOS Audio initialized: {}Hz, {}ch", sample_rate_,
                       channels_);
    return true;
  }

  bool Start() override {
    if (!initialized_ || !audio_queue_) return false;
    OSStatus status = AudioQueueStart(audio_queue_, nullptr);
    if (status != noErr) {
      PIXELGRAB_LOG_ERROR("AudioQueueStart failed: {}", status);
      return false;
    }
    capturing_ = true;
    return true;
  }

  bool Stop() override {
    if (!capturing_ || !audio_queue_) return false;
    AudioQueueStop(audio_queue_, true);
    capturing_ = false;
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
  static constexpr int kNumBuffers = 3;

  static void AudioInputCallback(void* user_data,
                                 AudioQueueRef queue,
                                 AudioQueueBufferRef buffer,
                                 const AudioTimeStamp* /* start_time */,
                                 UInt32 num_packets,
                                 const AudioStreamPacketDescription* /* descs */) {
    auto* self = static_cast<MacAudioBackend*>(user_data);
    if (num_packets > 0 && buffer->mAudioData) {
      const int16_t* data =
          static_cast<const int16_t*>(buffer->mAudioData);
      int total_samples =
          static_cast<int>(buffer->mAudioDataByteSize / sizeof(int16_t));

      std::lock_guard<std::mutex> lock(self->buffer_mutex_);
      self->pending_samples_.insert(self->pending_samples_.end(),
                                    data, data + total_samples);
    }
    // Re-enqueue the buffer.
    AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
  }

  bool initialized_ = false;
  bool capturing_ = false;
  PixelGrabAudioSource source_ = kPixelGrabAudioNone;
  int sample_rate_ = 44100;
  int channels_ = 2;

  AudioQueueRef audio_queue_ = nullptr;

  // Thread-safe sample buffer.
  std::mutex buffer_mutex_;
  std::vector<int16_t> pending_samples_;
};

std::unique_ptr<AudioBackend> CreatePlatformAudioBackend() {
  return std::make_unique<MacAudioBackend>();
}

}  // namespace internal
}  // namespace pixelgrab
