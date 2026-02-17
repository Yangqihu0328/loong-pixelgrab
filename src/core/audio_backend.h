// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_CORE_AUDIO_BACKEND_H_
#define PIXELGRAB_CORE_AUDIO_BACKEND_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "pixelgrab/pixelgrab.h"

namespace pixelgrab {
namespace internal {

/// Internal representation of an audio device.
struct AudioDeviceInfo {
  std::string id;         // Platform device ID
  std::string name;       // Human-readable name
  bool is_default;        // Whether this is the default device
  bool is_input;          // true = microphone, false = loopback/system audio
};

/// Audio samples buffer (interleaved S16LE PCM).
struct AudioSamples {
  std::vector<int16_t> data;
  int sample_rate = 44100;
  int channels = 2;
  int64_t timestamp_ns = 0;  // Presentation timestamp in nanoseconds
};

/// Abstract interface for platform-specific audio capture backends.
///
/// Each platform implements this using its native audio API:
///   Windows: WASAPI  |  macOS: CoreAudio  |  Linux: PulseAudio
class AudioBackend {
 public:
  virtual ~AudioBackend() = default;

  // Non-copyable.
  AudioBackend(const AudioBackend&) = delete;
  AudioBackend& operator=(const AudioBackend&) = delete;

  /// Check if audio capture is available on this platform.
  virtual bool IsSupported() const = 0;

  /// Enumerate available audio devices.
  virtual std::vector<AudioDeviceInfo> EnumerateDevices() = 0;

  /// Get the default audio device.
  /// @param is_input  true = default microphone, false = default system audio.
  virtual AudioDeviceInfo GetDefaultDevice(bool is_input) = 0;

  /// Initialize audio capture with the given parameters.
  /// @param device_id     Device ID (empty = default device).
  /// @param source        Audio source type.
  /// @param sample_rate   Sample rate (0 = default 44100).
  /// @return true on success.
  virtual bool Initialize(const std::string& device_id,
                          PixelGrabAudioSource source,
                          int sample_rate) = 0;

  /// Start capturing audio.
  virtual bool Start() = 0;

  /// Stop capturing audio.
  virtual bool Stop() = 0;

  /// Read captured audio samples.
  /// Returns a buffer of PCM samples captured since the last call.
  /// May return an empty buffer if no new samples are available.
  virtual AudioSamples ReadSamples() = 0;

  /// Get the configured sample rate.
  virtual int GetSampleRate() const = 0;

  /// Get the configured number of channels.
  virtual int GetChannels() const = 0;

 protected:
  AudioBackend() = default;
};

/// Factory function — returns the platform-native audio backend.
/// Defined per-platform in platform/<os>/xxx_audio_backend.cpp.
std::unique_ptr<AudioBackend> CreatePlatformAudioBackend();

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_CORE_AUDIO_BACKEND_H_
