// Copyright 2026 The loong-pixelgrab Authors
// Windows audio backend — WASAPI implementation.

#include "core/audio_backend.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "core/logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// initguid.h must come before functiondiscoverykeys_devpkey.h to provide
// the DEFINE_PROPERTYKEY macro implementation.
#include <initguid.h>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include <wrl/client.h>

#pragma comment(lib, "ole32.lib")

using Microsoft::WRL::ComPtr;

namespace pixelgrab {
namespace internal {

namespace {

/// Convert a wide string to UTF-8.
std::string WideToUtf8(const std::wstring& wide) {
  if (wide.empty()) return {};
  int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                static_cast<int>(wide.size()), nullptr, 0,
                                nullptr, nullptr);
  std::string utf8(static_cast<size_t>(len), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                      static_cast<int>(wide.size()), utf8.data(), len,
                      nullptr, nullptr);
  return utf8;
}

/// Get a device's friendly name from its property store.
std::string GetDeviceName(IMMDevice* device) {
  ComPtr<IPropertyStore> props;
  if (FAILED(device->OpenPropertyStore(STGM_READ, &props))) return "Unknown";

  PROPVARIANT name_var;
  PropVariantInit(&name_var);
  HRESULT hr = props->GetValue(PKEY_Device_FriendlyName, &name_var);
  std::string name;
  if (SUCCEEDED(hr) && name_var.vt == VT_LPWSTR && name_var.pwszVal) {
    name = WideToUtf8(name_var.pwszVal);
  }
  PropVariantClear(&name_var);
  return name.empty() ? "Unknown" : name;
}

/// Get a device's ID string.
std::string GetDeviceId(IMMDevice* device) {
  LPWSTR id_str = nullptr;
  if (FAILED(device->GetId(&id_str)) || !id_str) return "";
  std::string id = WideToUtf8(id_str);
  CoTaskMemFree(id_str);
  return id;
}

}  // namespace

class WinAudioBackend : public AudioBackend {
 public:
  WinAudioBackend() {
    // COM must be initialized for WASAPI.
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    com_initialized_ = SUCCEEDED(hr) || hr == S_FALSE || hr == RPC_E_CHANGED_MODE;
  }

  ~WinAudioBackend() override {
    Stop();
  }

  bool IsSupported() const override { return com_initialized_; }

  std::vector<AudioDeviceInfo> EnumerateDevices() override {
    std::vector<AudioDeviceInfo> result;
    if (!com_initialized_) return result;

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(enumerator.GetAddressOf()));
    if (FAILED(hr)) return result;

    // Enumerate capture (input) devices.
    EnumerateDeviceType(enumerator.Get(), eCapture, true, result);
    // Enumerate render (output/loopback) devices.
    EnumerateDeviceType(enumerator.Get(), eRender, false, result);

    return result;
  }

  AudioDeviceInfo GetDefaultDevice(bool is_input) override {
    AudioDeviceInfo info;
    info.is_input = is_input;
    info.is_default = true;

    if (!com_initialized_) {
      info.name = "Default";
      return info;
    }

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(enumerator.GetAddressOf()));
    if (FAILED(hr)) {
      info.name = "Default";
      return info;
    }

    ComPtr<IMMDevice> device;
    EDataFlow flow = is_input ? eCapture : eRender;
    hr = enumerator->GetDefaultAudioEndpoint(flow, eConsole, &device);
    if (SUCCEEDED(hr) && device) {
      info.id = GetDeviceId(device.Get());
      info.name = GetDeviceName(device.Get());
    } else {
      info.name = "Default";
    }

    return info;
  }

  bool Initialize(const std::string& device_id,
                  PixelGrabAudioSource source,
                  int sample_rate) override {
    if (!com_initialized_) return false;

    source_ = source;
    sample_rate_ = (sample_rate > 0) ? sample_rate : 44100;
    channels_ = 2;

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(enumerator.GetAddressOf()));
    if (FAILED(hr)) return false;

    // For loopback capture, we open a render device.
    // For microphone, we open a capture device.
    bool use_loopback = (source == kPixelGrabAudioSystem ||
                         source == kPixelGrabAudioBoth);
    EDataFlow flow = use_loopback ? eRender : eCapture;

    if (device_id.empty()) {
      hr = enumerator->GetDefaultAudioEndpoint(flow, eConsole, &device_);
    } else {
      // Convert device_id to wide string.
      int wlen = MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1,
                                     nullptr, 0);
      std::wstring wid(static_cast<size_t>(wlen), L'\0');
      MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1, wid.data(),
                          wlen);
      hr = enumerator->GetDevice(wid.c_str(), &device_);
    }
    if (FAILED(hr) || !device_) {
      PIXELGRAB_LOG_ERROR("Failed to get audio device: 0x{:08X}", hr);
      return false;
    }

    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                           reinterpret_cast<void**>(audio_client_.GetAddressOf()));
    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("IAudioClient activation failed: 0x{:08X}", hr);
      return false;
    }

    // Get device mix format and configure.
    WAVEFORMATEX* mix_format = nullptr;
    hr = audio_client_->GetMixFormat(&mix_format);
    if (FAILED(hr) || !mix_format) return false;

    // Use the device's native format for best compatibility.
    channels_ = mix_format->nChannels;
    sample_rate_ = static_cast<int>(mix_format->nSamplesPerSec);

    DWORD stream_flags = use_loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0;
    REFERENCE_TIME buffer_duration = 1'000'000;  // 100ms

    hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, stream_flags,
                                   buffer_duration, 0, mix_format, nullptr);
    CoTaskMemFree(mix_format);
    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("IAudioClient::Initialize failed: 0x{:08X}", hr);
      return false;
    }

    hr = audio_client_->GetService(
        __uuidof(IAudioCaptureClient),
        reinterpret_cast<void**>(capture_client_.GetAddressOf()));
    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("GetService(IAudioCaptureClient) failed: 0x{:08X}",
                         hr);
      return false;
    }

    initialized_ = true;
    PIXELGRAB_LOG_INFO("WASAPI audio initialized: {}Hz, {}ch, {}",
                       sample_rate_, channels_,
                       use_loopback ? "loopback" : "capture");
    return true;
  }

  bool Start() override {
    if (!initialized_ || !audio_client_) return false;
    HRESULT hr = audio_client_->Start();
    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("IAudioClient::Start failed: 0x{:08X}", hr);
      return false;
    }
    capturing_ = true;
    return true;
  }

  bool Stop() override {
    if (!capturing_ || !audio_client_) return false;
    audio_client_->Stop();
    capturing_ = false;
    return true;
  }

  AudioSamples ReadSamples() override {
    AudioSamples samples;
    samples.sample_rate = sample_rate_;
    samples.channels = channels_;

    if (!capturing_ || !capture_client_) return samples;

    UINT32 packet_length = 0;
    capture_client_->GetNextPacketSize(&packet_length);

    while (packet_length > 0) {
      BYTE* data = nullptr;
      UINT32 frames = 0;
      DWORD flags = 0;
      UINT64 device_position = 0;

      HRESULT hr = capture_client_->GetBuffer(&data, &frames, &flags,
                                              &device_position, nullptr);
      if (FAILED(hr)) break;

      if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data) {
        // Convert float samples to S16LE.
        int total_samples = static_cast<int>(frames) * channels_;
        const float* fdata = reinterpret_cast<const float*>(data);
        size_t prev_size = samples.data.size();
        samples.data.resize(prev_size + total_samples);

        for (int i = 0; i < total_samples; ++i) {
          float val = fdata[i];
          val = (std::max)(-1.0f, (std::min)(1.0f, val));
          samples.data[prev_size + i] =
              static_cast<int16_t>(val * 32767.0f);
        }
      }

      capture_client_->ReleaseBuffer(frames);
      capture_client_->GetNextPacketSize(&packet_length);
    }

    return samples;
  }

  int GetSampleRate() const override { return sample_rate_; }
  int GetChannels() const override { return channels_; }

 private:
  void EnumerateDeviceType(IMMDeviceEnumerator* enumerator,
                           EDataFlow flow, bool is_input,
                           std::vector<AudioDeviceInfo>& out) {
    ComPtr<IMMDeviceCollection> collection;
    HRESULT hr = enumerator->EnumAudioEndpoints(
        flow, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr) || !collection) return;

    UINT count = 0;
    collection->GetCount(&count);

    // Get default device ID for comparison.
    std::string default_id;
    {
      ComPtr<IMMDevice> def_dev;
      if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(
              flow, eConsole, &def_dev)) && def_dev) {
        default_id = GetDeviceId(def_dev.Get());
      }
    }

    for (UINT i = 0; i < count; ++i) {
      ComPtr<IMMDevice> device;
      if (FAILED(collection->Item(i, &device)) || !device) continue;

      AudioDeviceInfo info;
      info.id = GetDeviceId(device.Get());
      info.name = GetDeviceName(device.Get());
      info.is_input = is_input;
      info.is_default = (!default_id.empty() && info.id == default_id);
      out.push_back(std::move(info));
    }
  }

  bool com_initialized_ = false;
  bool initialized_ = false;
  bool capturing_ = false;

  PixelGrabAudioSource source_ = kPixelGrabAudioNone;
  int sample_rate_ = 44100;
  int channels_ = 2;

  ComPtr<IMMDevice> device_;
  ComPtr<IAudioClient> audio_client_;
  ComPtr<IAudioCaptureClient> capture_client_;
};

std::unique_ptr<AudioBackend> CreatePlatformAudioBackend() {
  return std::make_unique<WinAudioBackend>();
}

}  // namespace internal
}  // namespace pixelgrab
