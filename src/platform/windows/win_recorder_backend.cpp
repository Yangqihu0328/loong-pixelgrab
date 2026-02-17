// Copyright 2026 The loong-pixelgrab Authors
//
// Windows recorder backend — Media Foundation H.264 encoding with optional
// GPU-accelerated pipeline (DXGI Desktop Duplication + Direct2D watermark +
// MF DXGI texture encoding).

#include "core/recorder_backend.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>

#include "core/logger.h"
#include "platform/windows/d3d11_device_manager.h"
#include "watermark/watermark_renderer.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Media Foundation
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

// Direct3D 11 / DXGI (for GPU pipeline)
#include <d3d11.h>
#include <dxgi1_2.h>

// Direct2D / DirectWrite (for GPU watermark)
#include <d2d1.h>
#include <dwrite.h>

// GDI+ (for pre-rendering user watermark bitmap)
#include <gdiplus.h>

#include <wrl/client.h>  // ComPtr

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "gdiplus.lib")

using Microsoft::WRL::ComPtr;

namespace pixelgrab {
namespace internal {

namespace {

/// Convert a UTF-8 string to a wide string.
std::wstring Utf8ToWide(const std::string& utf8) {
  if (utf8.empty()) return {};
  int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring wide(static_cast<size_t>(len), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                      static_cast<int>(utf8.size()), wide.data(), len);
  return wide;
}

}  // namespace

// ===========================================================================
// WinRecorderBackend
// ===========================================================================

class WinRecorderBackend : public RecorderBackend {
 public:
  WinRecorderBackend() = default;

  ~WinRecorderBackend() override {
    StopCaptureLoop();

    if (state_ == RecordState::kRecording ||
        state_ == RecordState::kPaused) {
      Stop();
    }

    // Release GPU resources before MF shutdown.
    ShutdownGpu();

    if (mf_started_) {
      MFShutdown();
      mf_started_ = false;
    }
  }

  // -------------------------------------------------------------------------
  // RecorderBackend interface
  // -------------------------------------------------------------------------

  bool Initialize(const RecordConfig& config) override {
    config_ = config;

    // Initialize Media Foundation.
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("MFStartup failed: 0x{:08X}", hr);
      return false;
    }
    mf_started_ = true;

    // Resolve dimensions — 0 means primary screen.
    frame_width_ = config.region_width;
    frame_height_ = config.region_height;
    if (frame_width_ <= 0 || frame_height_ <= 0) {
      frame_width_ = GetSystemMetrics(SM_CXSCREEN);
      frame_height_ = GetSystemMetrics(SM_CYSCREEN);
    }
    // MF requires even dimensions for H.264.
    frame_width_ = (frame_width_ + 1) & ~1;
    frame_height_ = (frame_height_ + 1) & ~1;

    fps_ = config.fps;
    frame_duration_ = 10'000'000LL / fps_;  // in 100-ns units

    // Try GPU pipeline initialization (auto mode recording only).
    if (config.auto_capture && config.gpu_hint >= 0) {
      InitializeGpu();
    }

    // If user requires GPU and it's not available, fail.
    if (config.gpu_hint > 0 && !gpu_available_) {
      PIXELGRAB_LOG_ERROR("GPU acceleration requested but not available");
      return false;
    }

    // Initialize audio backend if requested.
    if (config.audio_source != kPixelGrabAudioNone && config.audio_backend) {
      if (config.audio_backend->Initialize(
              config.audio_device_id, config.audio_source,
              config.audio_sample_rate)) {
        audio_backend_ = config.audio_backend;
        audio_sample_rate_ = config.audio_backend->GetSampleRate();
        audio_channels_ = config.audio_backend->GetChannels();
        PIXELGRAB_LOG_INFO("Audio backend initialized: {}Hz, {}ch",
                           audio_sample_rate_, audio_channels_);
      } else {
        PIXELGRAB_LOG_WARN("Audio backend init failed — recording without audio");
      }
    }

    // Create MF Sink Writer (includes audio stream if audio_backend_ set).
    if (!InitializeSinkWriter(config)) {
      return false;
    }

    state_ = RecordState::kIdle;
    PIXELGRAB_LOG_INFO(
        "Recorder initialized: {}x{} @{}fps, {}bps, gpu={}, audio={} → {}",
        frame_width_, frame_height_, fps_, config.bitrate,
        gpu_available_ ? "yes" : "no",
        audio_backend_ ? "yes" : "no",
        config.output_path);
    return true;
  }

  bool Start() override {
    if (state_ != RecordState::kIdle) return false;

    HRESULT hr = sink_writer_->BeginWriting();
    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("BeginWriting failed: 0x{:08X}", hr);
      return false;
    }

    // Start audio capture.
    if (audio_backend_) {
      if (!audio_backend_->Start()) {
        PIXELGRAB_LOG_WARN("Audio start failed — continuing without audio");
        audio_backend_ = nullptr;
      }
    }

    frame_count_ = 0;
    audio_samples_written_ = 0;
    start_time_ = std::chrono::steady_clock::now();
    state_ = RecordState::kRecording;
    PIXELGRAB_LOG_INFO("Recording started (audio={})",
                       audio_backend_ ? "on" : "off");
    return true;
  }

  bool Pause() override {
    if (state_ != RecordState::kRecording) return false;
    paused_.store(true, std::memory_order_release);
    state_ = RecordState::kPaused;
    PIXELGRAB_LOG_INFO("Recording paused");
    return true;
  }

  bool Resume() override {
    if (state_ != RecordState::kPaused) return false;
    paused_.store(false, std::memory_order_release);
    state_ = RecordState::kRecording;
    PIXELGRAB_LOG_INFO("Recording resumed");
    return true;
  }

  bool WriteFrame(const Image& frame) override {
    if (state_ != RecordState::kRecording) return false;

    std::lock_guard<std::mutex> lock(write_mutex_);

    // CPU path: create MF sample from Image pixel data.
    const DWORD buffer_size =
        static_cast<DWORD>(frame_width_ * frame_height_ * 4);
    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(buffer_size, &buffer);
    if (FAILED(hr)) return false;

    BYTE* dest = nullptr;
    hr = buffer->Lock(&dest, nullptr, nullptr);
    if (FAILED(hr)) return false;

    // Copy frame data. MF RGB32 is bottom-up; our Image is top-down BGRA.
    const uint8_t* src = frame.data();
    int src_stride = frame.stride();
    int dst_stride = frame_width_ * 4;
    for (int row = 0; row < frame_height_; ++row) {
      int src_row = row;
      int dst_row = frame_height_ - 1 - row;
      const uint8_t* s = src + src_row * src_stride;
      BYTE* d = dest + dst_row * dst_stride;
      std::memcpy(d, s, static_cast<size_t>(dst_stride));
    }

    buffer->Unlock();
    buffer->SetCurrentLength(buffer_size);

    ComPtr<IMFSample> sample;
    hr = MFCreateSample(&sample);
    if (FAILED(hr)) return false;

    sample->AddBuffer(buffer.Get());
    sample->SetSampleTime(frame_count_ * frame_duration_);
    sample->SetSampleDuration(frame_duration_);

    hr = sink_writer_->WriteSample(stream_index_, sample.Get());
    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("WriteSample failed: 0x{:08X}", hr);
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

    // Stop audio capture.
    if (audio_backend_) {
      // Flush remaining audio samples before finalizing.
      FlushAudioSamples();
      audio_backend_->Stop();
    }

    HRESULT hr = sink_writer_->Finalize();
    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("Finalize failed: 0x{:08X}", hr);
    }

    state_ = RecordState::kStopped;
    PIXELGRAB_LOG_INFO("Recording stopped: {} frames, {} audio samples, {}ms",
                       frame_count_, audio_samples_written_, GetDurationMs());
    return SUCCEEDED(hr);
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

    // GPU path does not need external capture_backend.
    if (!gpu_available_ && !config_.capture_backend) {
      PIXELGRAB_LOG_ERROR("Auto capture enabled but no capture backend and "
                          "no GPU available");
      return;
    }
    if (capture_running_.load(std::memory_order_acquire)) return;

    capture_running_.store(true, std::memory_order_release);
    capture_thread_ =
        std::thread(&WinRecorderBackend::CaptureLoopFunc, this);
    PIXELGRAB_LOG_INFO("Capture loop started (auto mode, gpu={})",
                       gpu_available_ ? "yes" : "no");
  }

  void StopCaptureLoop() override {
    if (!capture_running_.load(std::memory_order_acquire)) return;

    capture_running_.store(false, std::memory_order_release);
    if (capture_thread_.joinable()) {
      capture_thread_.join();
    }
    PIXELGRAB_LOG_INFO("Capture loop stopped");
  }

 private:
  // =========================================================================
  // GPU initialization
  // =========================================================================

  /// Try to initialize the full GPU pipeline.
  /// Sets gpu_available_ = true on success.
  void InitializeGpu() {
    PIXELGRAB_LOG_DEBUG("Attempting GPU pipeline initialization...");

    // Step 1: Create D3D11 device.
    d3d11_mgr_ = D3D11DeviceManager::Create();
    if (!d3d11_mgr_) {
      PIXELGRAB_LOG_INFO("D3D11 not available — using CPU path");
      return;
    }

    // Step 2: Create DXGI Output Duplication.
    dxgi_dup_ = d3d11_mgr_->CreateOutputDuplication(0);
    if (!dxgi_dup_) {
      PIXELGRAB_LOG_INFO("DXGI DD not available — using CPU path");
      d3d11_mgr_.reset();
      return;
    }

    // Step 3: Create D2D1 factory.
    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory),
        reinterpret_cast<void**>(d2d_factory_.GetAddressOf()));
    if (FAILED(hr)) {
      PIXELGRAB_LOG_WARN("D2D1CreateFactory failed: 0x{:08X}", hr);
      ShutdownGpu();
      return;
    }

    // Step 4: Create DirectWrite factory.
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwrite_factory_.GetAddressOf()));
    if (FAILED(hr)) {
      PIXELGRAB_LOG_WARN("DWriteCreateFactory failed: 0x{:08X}", hr);
      ShutdownGpu();
      return;
    }

    // Step 5: Create staging/render target textures.
    gpu_frame_texture_ =
        d3d11_mgr_->CreateRenderTargetTexture(frame_width_, frame_height_);
    if (!gpu_frame_texture_) {
      PIXELGRAB_LOG_WARN("Failed to create render target texture");
      ShutdownGpu();
      return;
    }

    // Step 6: Create MF DXGI device manager for hardware encoding.
    UINT reset_token = 0;
    hr = MFCreateDXGIDeviceManager(&reset_token, &mf_dxgi_manager_);
    if (FAILED(hr)) {
      PIXELGRAB_LOG_WARN("MFCreateDXGIDeviceManager failed: 0x{:08X}", hr);
      ShutdownGpu();
      return;
    }
    hr = mf_dxgi_manager_->ResetDevice(d3d11_mgr_->device(), reset_token);
    if (FAILED(hr)) {
      PIXELGRAB_LOG_WARN("IMFDXGIDeviceManager::ResetDevice failed: "
                         "0x{:08X}", hr);
      ShutdownGpu();
      return;
    }
    mf_reset_token_ = reset_token;

    gpu_available_ = true;
    PIXELGRAB_LOG_INFO("GPU pipeline initialized successfully "
                       "(D3D11 + DXGI DD + D2D + MF DXGI)");
  }

  /// Release all GPU resources.
  void ShutdownGpu() {
    // Release in reverse order of creation.
    mf_dxgi_manager_.Reset();
    gpu_staging_texture_.Reset();
    gpu_frame_texture_.Reset();
    dwrite_factory_.Reset();
    d2d_factory_.Reset();
    dxgi_dup_.Reset();
    d3d11_mgr_.reset();
    gpu_available_ = false;
    gpu_has_valid_frame_ = false;
  }

  // =========================================================================
  // Sink Writer initialization
  // =========================================================================

  bool InitializeSinkWriter(const RecordConfig& config) {
    std::wstring wpath = Utf8ToWide(config.output_path);

    ComPtr<IMFAttributes> attrs;
    HRESULT hr = MFCreateAttributes(&attrs, 2);
    if (FAILED(hr)) return false;

    hr = attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    if (FAILED(hr)) return false;

    // Note: We intentionally do NOT pass IMFDXGIDeviceManager to SinkWriter
    // here. The SinkWriter always receives CPU-side RGB32 buffers, which
    // keeps the input format consistent between GPU and CPU paths. The
    // MF hardware H.264 encoder is still used via ENABLE_HARDWARE_TRANSFORMS.
    // A future optimization can add DXGI texture-to-SinkWriter zero-copy.

    hr = MFCreateSinkWriterFromURL(wpath.c_str(), nullptr, attrs.Get(),
                                   &sink_writer_);
    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("MFCreateSinkWriterFromURL failed: 0x{:08X}", hr);
      return false;
    }

    // Output media type: H.264.
    ComPtr<IMFMediaType> out_type;
    hr = MFCreateMediaType(&out_type);
    if (FAILED(hr)) return false;

    out_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    out_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    out_type->SetUINT32(MF_MT_AVG_BITRATE,
                        static_cast<UINT32>(config.bitrate));
    MFSetAttributeSize(out_type.Get(), MF_MT_FRAME_SIZE,
                       static_cast<UINT32>(frame_width_),
                       static_cast<UINT32>(frame_height_));
    MFSetAttributeRatio(out_type.Get(), MF_MT_FRAME_RATE,
                        static_cast<UINT32>(fps_), 1);
    MFSetAttributeRatio(out_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    out_type->SetUINT32(MF_MT_INTERLACE_MODE,
                        MFVideoInterlace_Progressive);

    hr = sink_writer_->AddStream(out_type.Get(), &stream_index_);
    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("AddStream failed: 0x{:08X}", hr);
      return false;
    }

    // Input media type: always RGB32 (bottom-up BGRA).
    // Both GPU and CPU paths produce CPU-side Image data that is row-flipped
    // and written via WriteFrame(). This keeps the format consistent.
    ComPtr<IMFMediaType> in_type;
    hr = MFCreateMediaType(&in_type);
    if (FAILED(hr)) return false;

    in_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    in_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    MFSetAttributeSize(in_type.Get(), MF_MT_FRAME_SIZE,
                       static_cast<UINT32>(frame_width_),
                       static_cast<UINT32>(frame_height_));
    MFSetAttributeRatio(in_type.Get(), MF_MT_FRAME_RATE,
                        static_cast<UINT32>(fps_), 1);
    MFSetAttributeRatio(in_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    in_type->SetUINT32(MF_MT_INTERLACE_MODE,
                       MFVideoInterlace_Progressive);

    hr = sink_writer_->SetInputMediaType(stream_index_, in_type.Get(),
                                         nullptr);
    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("SetInputMediaType (video) failed: 0x{:08X}", hr);
      return false;
    }

    // --- Audio stream (AAC output, PCM input) ---
    if (audio_backend_) {
      // Output type: AAC.
      ComPtr<IMFMediaType> audio_out;
      hr = MFCreateMediaType(&audio_out);
      if (FAILED(hr)) return false;

      audio_out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
      audio_out->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
      audio_out->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
      audio_out->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,
                           static_cast<UINT32>(audio_sample_rate_));
      audio_out->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS,
                           static_cast<UINT32>(audio_channels_));
      audio_out->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000);

      hr = sink_writer_->AddStream(audio_out.Get(), &audio_stream_index_);
      if (FAILED(hr)) {
        PIXELGRAB_LOG_WARN("AddStream (audio) failed: 0x{:08X} — "
                           "recording without audio", hr);
        audio_backend_ = nullptr;
      } else {
        // Input type: PCM S16LE via WAVEFORMATEX (avoids GUID compat issues).
        WAVEFORMATEX wfx = {};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = static_cast<WORD>(audio_channels_);
        wfx.nSamplesPerSec = static_cast<DWORD>(audio_sample_rate_);
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        wfx.cbSize = 0;

        ComPtr<IMFMediaType> audio_in;
        hr = MFCreateMediaType(&audio_in);
        if (FAILED(hr)) return false;

        hr = MFInitMediaTypeFromWaveFormatEx(
            audio_in.Get(), &wfx, sizeof(wfx));
        if (FAILED(hr)) {
          PIXELGRAB_LOG_WARN("MFInitMediaTypeFromWaveFormatEx failed: "
                             "0x{:08X}", hr);
          audio_backend_ = nullptr;
        } else {
          hr = sink_writer_->SetInputMediaType(audio_stream_index_,
                                               audio_in.Get(), nullptr);
          if (FAILED(hr)) {
            PIXELGRAB_LOG_WARN("SetInputMediaType (audio) failed: 0x{:08X} — "
                               "recording without audio", hr);
            audio_backend_ = nullptr;
          } else {
            PIXELGRAB_LOG_INFO("Audio stream added: {}Hz, {}ch, AAC",
                               audio_sample_rate_, audio_channels_);
          }
        }
      }
    }

    return true;
  }

  // =========================================================================
  // GPU frame capture (DXGI Desktop Duplication)
  // =========================================================================

  /// Acquire a desktop frame via DXGI DD and copy the recording region into
  /// gpu_frame_texture_.
  /// @return true if a new frame was acquired.
  bool AcquireDesktopFrame() {
    if (!dxgi_dup_ || !d3d11_mgr_) return false;

    DXGI_OUTDUPL_FRAME_INFO frame_info{};
    ComPtr<IDXGIResource> desktop_resource;

    // Timeout: 0ms for non-blocking, or small value to avoid stalling.
    HRESULT hr = dxgi_dup_->AcquireNextFrame(
        100,  // 100ms timeout (> 1 frame at 30fps)
        &frame_info, &desktop_resource);

    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
      return false;  // No new frame available yet.
    }

    if (hr == DXGI_ERROR_ACCESS_LOST) {
      // Desktop switched (e.g., secure desktop, UAC). Try to recreate.
      PIXELGRAB_LOG_WARN("DXGI DD access lost, attempting recreation");
      dxgi_dup_.Reset();
      dxgi_dup_ = d3d11_mgr_->CreateOutputDuplication(0);
      return false;
    }

    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("AcquireNextFrame failed: 0x{:08X}", hr);
      return false;
    }

    // Get the desktop texture.
    ComPtr<ID3D11Texture2D> desktop_texture;
    hr = desktop_resource.As(&desktop_texture);
    if (FAILED(hr)) {
      dxgi_dup_->ReleaseFrame();
      return false;
    }

    // Copy the recording region from the desktop texture to our render target.
    D3D11_BOX src_box{};
    src_box.left = static_cast<UINT>((std::max)(config_.region_x, 0));
    src_box.top = static_cast<UINT>((std::max)(config_.region_y, 0));
    src_box.right = src_box.left + static_cast<UINT>(frame_width_);
    src_box.bottom = src_box.top + static_cast<UINT>(frame_height_);
    src_box.front = 0;
    src_box.back = 1;

    d3d11_mgr_->context()->CopySubresourceRegion(
        gpu_frame_texture_.Get(), 0,    // dest texture, subresource 0
        0, 0, 0,                        // dest x, y, z
        desktop_texture.Get(), 0,       // src texture, subresource 0
        &src_box);

    // Release the desktop frame immediately (required before next Acquire).
    dxgi_dup_->ReleaseFrame();

    return true;
  }

  // =========================================================================
  // GPU watermark (Direct2D + DirectWrite)
  // =========================================================================

  /// Apply a text watermark to the gpu_frame_texture_ using Direct2D.
  bool ApplyGpuTextWatermark(const PixelGrabTextWatermarkConfig& wm) {
    if (!d2d_factory_ || !dwrite_factory_) return false;
    if (!wm.text || !wm.text[0]) return true;

    // Get DXGI surface from our render target texture.
    ComPtr<IDXGISurface> surface;
    HRESULT hr = gpu_frame_texture_.As(&surface);
    if (FAILED(hr)) return false;

    // Create D2D render target from the DXGI surface.
    // Force 96 DPI so that D2D DIP coordinates equal physical pixels.
    // Without this, the system DPI is inherited, and watermark positions
    // (computed in pixels) are scaled by DPI/96, pushing the watermark
    // off-screen on high-DPI displays (e.g. 150% scaling).
    D2D1_RENDER_TARGET_PROPERTIES rt_props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                          D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);

    ComPtr<ID2D1RenderTarget> rt;
    hr = d2d_factory_->CreateDxgiSurfaceRenderTarget(
        surface.Get(), &rt_props, &rt);
    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("CreateDxgiSurfaceRenderTarget failed: 0x{:08X}",
                          hr);
      return false;
    }

    // Create DirectWrite text format.
    int font_size = (wm.font_size > 0) ? wm.font_size : 16;
    const char* font_name = wm.font_name ? wm.font_name : "Arial";
    std::wstring wfont = Utf8ToWide(font_name);

    ComPtr<IDWriteTextFormat> text_format;
    hr = dwrite_factory_->CreateTextFormat(
        wfont.c_str(), nullptr,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, static_cast<FLOAT>(font_size),
        L"", &text_format);
    if (FAILED(hr)) return false;

    // Convert text to wide string.
    std::wstring wtext = Utf8ToWide(wm.text);

    // Create text layout to measure bounds.
    ComPtr<IDWriteTextLayout> text_layout;
    hr = dwrite_factory_->CreateTextLayout(
        wtext.c_str(), static_cast<UINT32>(wtext.size()),
        text_format.Get(),
        static_cast<FLOAT>(frame_width_),
        static_cast<FLOAT>(frame_height_),
        &text_layout);
    if (FAILED(hr)) return false;

    DWRITE_TEXT_METRICS metrics{};
    text_layout->GetMetrics(&metrics);
    int text_w = static_cast<int>(metrics.width + 0.5f);
    int text_h = static_cast<int>(metrics.height + 0.5f);

    // Resolve position.
    int px = 0, py = 0;
    int margin = (wm.margin > 0) ? wm.margin : 10;
    switch (wm.position) {
      case kPixelGrabWatermarkTopLeft:
        px = margin; py = margin; break;
      case kPixelGrabWatermarkTopRight:
        px = frame_width_ - text_w - margin; py = margin; break;
      case kPixelGrabWatermarkBottomLeft:
        px = margin; py = frame_height_ - text_h - margin; break;
      case kPixelGrabWatermarkBottomRight:
        px = frame_width_ - text_w - margin;
        py = frame_height_ - text_h - margin; break;
      case kPixelGrabWatermarkCenter:
        px = (frame_width_ - text_w) / 2;
        py = (frame_height_ - text_h) / 2; break;
      case kPixelGrabWatermarkCustom:
      default:
        px = wm.x; py = wm.y; break;
    }

    // Parse ARGB — only alpha channel is used; text is black + white outline.
    uint32_t argb = wm.color;
    if (argb == 0) argb = 0x80FFFFFF;
    float a = ((argb >> 24) & 0xFF) / 255.0f;

    ComPtr<ID2D1SolidColorBrush> outline_brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, a),
                              &outline_brush);
    ComPtr<ID2D1SolidColorBrush> fill_brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, a),
                              &fill_brush);
    if (!outline_brush || !fill_brush) return false;

    // Draw: white outline (8-direction offset) then black fill.
    rt->BeginDraw();
    {
      FLOAT fx = static_cast<FLOAT>(px);
      FLOAT fy = static_cast<FLOAT>(py);

      // Apply rotation around text center if rotation != 0.
      bool rotated = (wm.rotation != 0.0f);
      if (rotated) {
        FLOAT cx = fx + metrics.width * 0.5f;
        FLOAT cy = fy + metrics.height * 0.5f;
        rt->SetTransform(D2D1::Matrix3x2F::Rotation(wm.rotation,
                         D2D1::Point2F(cx, cy)));
      }

      constexpr FLOAT kOff = 1.5f;
      constexpr FLOAT offsets[][2] = {
          {-kOff, -kOff}, {0, -kOff}, {kOff, -kOff},
          {-kOff,     0},             {kOff,     0},
          {-kOff,  kOff}, {0,  kOff}, {kOff,  kOff},
      };
      for (const auto& off : offsets) {
        rt->DrawTextLayout(D2D1::Point2F(fx + off[0], fy + off[1]),
                           text_layout.Get(), outline_brush.Get());
      }
      rt->DrawTextLayout(D2D1::Point2F(fx, fy),
                         text_layout.Get(), fill_brush.Get());

      if (rotated) {
        rt->SetTransform(D2D1::Matrix3x2F::Identity());
      }
    }
    hr = rt->EndDraw();
    if (FAILED(hr)) {
      PIXELGRAB_LOG_ERROR("D2D EndDraw failed: 0x{:08X}", hr);
      return false;
    }

    return true;
  }

  // =========================================================================
  // Pre-rendered user watermark bitmap  (render once → blend every frame)
  // =========================================================================

  /// Pre-render the user watermark text (with rotation + black/white outline)
  /// to an RGBA bitmap using GDI+.  Called once when recording starts.
  void PreRenderUserWatermarkBitmap() {
    if (uwm_bitmap_ready_) return;
    if (!config_.has_user_watermark) return;
    const auto& wm = config_.user_watermark_config;
    if (!wm.text || !wm.text[0]) return;

    // --- GDI+ startup (reference-counted, safe to call again) ---
    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR gdi_token = 0;
    Gdiplus::GdiplusStartup(&gdi_token, &gsi, nullptr);

    // --- Font ---
    int font_size = (wm.font_size > 0) ? wm.font_size : 16;
    const char* fn = wm.font_name ? wm.font_name : "Arial";
    int fnw = MultiByteToWideChar(CP_UTF8, 0, fn, -1, nullptr, 0);
    std::wstring wfont(static_cast<size_t>(fnw), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, fn, -1, wfont.data(), fnw);
    Gdiplus::Font font(wfont.c_str(),
                       static_cast<Gdiplus::REAL>(font_size),
                       Gdiplus::FontStyleRegular);

    // --- Text ---
    int tw = MultiByteToWideChar(CP_UTF8, 0, wm.text, -1, nullptr, 0);
    std::wstring wtext(static_cast<size_t>(tw), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, wm.text, -1, wtext.data(), tw);

    // --- Measure text extents ---
    Gdiplus::Bitmap dummy(1, 1, PixelFormat32bppARGB);
    Gdiplus::Graphics dg(&dummy);
    Gdiplus::RectF tr;
    dg.MeasureString(wtext.c_str(), -1, &font, Gdiplus::PointF(0, 0), &tr);
    float text_w = tr.Width + 8.0f;   // padding for outline
    float text_h = tr.Height + 8.0f;

    // Rotated bounding box.
    float rad = std::abs(kUserWmRotation) * 3.14159265f / 180.0f;
    float ca = std::cos(rad), sa = std::sin(rad);
    int bw = static_cast<int>(text_w * ca + text_h * sa + 12);
    int bh = static_cast<int>(text_w * sa + text_h * ca + 12);

    // --- Render to bitmap ---
    Gdiplus::Bitmap bmp(bw, bh, PixelFormat32bppARGB);
    {
      Gdiplus::Graphics g(&bmp);
      g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
      g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
      g.Clear(Gdiplus::Color(0, 0, 0, 0));

      // Rotate around bitmap center.
      float cx = bw * 0.5f, cy = bh * 0.5f;
      g.TranslateTransform(cx, cy);
      g.RotateTransform(kUserWmRotation);
      g.TranslateTransform(-cx, -cy);

      float dx = (bw - tr.Width) * 0.5f;
      float dy = (bh - tr.Height) * 0.5f;

      // Text path for outline rendering.
      Gdiplus::GraphicsPath path;
      Gdiplus::FontFamily family;
      font.GetFamily(&family);
      path.AddString(wtext.c_str(), -1, &family,
                     font.GetStyle(), font.GetSize(),
                     Gdiplus::PointF(dx, dy), nullptr);

      uint32_t argb = wm.color;
      if (argb == 0) argb = 0x80FFFFFF;
      uint8_t a = static_cast<uint8_t>((argb >> 24) & 0xFF);

      // White outline.
      Gdiplus::Pen pen(Gdiplus::Color(a, 255, 255, 255), 3.0f);
      pen.SetLineJoin(Gdiplus::LineJoinRound);
      g.DrawPath(&pen, &path);

      // Black fill.
      Gdiplus::SolidBrush fb(Gdiplus::Color(a, 0, 0, 0));
      g.FillPath(&fb, &path);
    }

    // --- Extract pixels ---
    Gdiplus::Rect rc(0, 0, bw, bh);
    Gdiplus::BitmapData bd;
    bmp.LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd);
    uwm_bmp_w_ = bw;
    uwm_bmp_h_ = bh;
    uwm_bmp_pixels_.resize(static_cast<size_t>(bw) * bh);
    for (int y = 0; y < bh; ++y) {
      std::memcpy(&uwm_bmp_pixels_[static_cast<size_t>(y) * bw],
                  static_cast<const uint8_t*>(bd.Scan0) + y * bd.Stride,
                  static_cast<size_t>(bw) * 4);
    }
    bmp.UnlockBits(&bd);

    Gdiplus::GdiplusShutdown(gdi_token);
    uwm_bitmap_ready_ = true;

    PIXELGRAB_LOG_INFO("Pre-rendered user watermark bitmap: {}x{}", bw, bh);
  }

  /// Alpha-blend the pre-rendered watermark bitmap onto a CPU frame at (px,py).
  /// This is extremely cheap: just a tight pixel loop, no COM/D2D/GDI+ calls.
  void BlendWatermarkOntoFrame(uint8_t* pixels, int stride,
                               int fw, int fh,
                               int px, int py) const {
    if (!uwm_bitmap_ready_) return;

    // Clip source and destination rectangles.
    int sx0 = (px < 0) ? -px : 0;
    int sy0 = (py < 0) ? -py : 0;
    int dx0 = (px < 0) ? 0 : px;
    int dy0 = (py < 0) ? 0 : py;
    int cw = std::min(uwm_bmp_w_ - sx0, fw - dx0);
    int ch = std::min(uwm_bmp_h_ - sy0, fh - dy0);
    if (cw <= 0 || ch <= 0) return;

    for (int y = 0; y < ch; ++y) {
      const uint32_t* src =
          &uwm_bmp_pixels_[static_cast<size_t>(sy0 + y) * uwm_bmp_w_ + sx0];
      uint8_t* dst = pixels + static_cast<size_t>(dy0 + y) * stride +
                     static_cast<size_t>(dx0) * 4;

      for (int x = 0; x < cw; ++x) {
        uint32_t sp = src[x];
        uint32_t sa = (sp >> 24) & 0xFF;
        if (sa == 0) { continue; }  // fully transparent — skip

        uint8_t* dp = dst + x * 4;
        if (sa == 255) {
          // Fully opaque — direct copy (fastest path).
          dp[0] = static_cast<uint8_t>(sp & 0xFF);         // B
          dp[1] = static_cast<uint8_t>((sp >> 8) & 0xFF);  // G
          dp[2] = static_cast<uint8_t>((sp >> 16) & 0xFF); // R
          dp[3] = 255;
        } else {
          uint32_t da = 255 - sa;
          dp[0] = static_cast<uint8_t>(((sp & 0xFF) * sa + dp[0] * da) / 255);
          dp[1] = static_cast<uint8_t>((((sp >> 8) & 0xFF) * sa + dp[1] * da) / 255);
          dp[2] = static_cast<uint8_t>((((sp >> 16) & 0xFF) * sa + dp[2] * da) / 255);
          dp[3] = static_cast<uint8_t>(std::min<uint32_t>(255, sa + dp[3]));
        }
      }
    }
  }

  /// Apply all visible user watermarks onto a CPU Image via pre-rendered blend.
  void ApplyUserWatermarksToImage(Image* image) {
    if (!image || !config_.has_user_watermark) return;
    PreRenderUserWatermarkBitmap();  // no-op after first call
    if (!uwm_bitmap_ready_) return;

    int wx[kUserWmCount], wy[kUserWmCount];
    bool vis[kUserWmCount];
    ComputeUserWmPositions(wx, wy, vis);

    uint8_t* pixels = image->mutable_data();
    int fw = image->width(), fh = image->height(), fs = image->stride();
    for (int i = 0; i < kUserWmCount; ++i) {
      if (!vis[i]) continue;
      BlendWatermarkOntoFrame(pixels, fs, fw, fh, wx[i], wy[i]);
    }
  }

  /// Apply a tiled (diagonal rain-like) text watermark using Direct2D.
  bool ApplyGpuTiledTextWatermark(const PixelGrabTextWatermarkConfig& wm,
                                  float angle_deg) {
    if (!d2d_factory_ || !dwrite_factory_) return false;
    if (!wm.text || !wm.text[0]) return true;

    ComPtr<IDXGISurface> surface;
    HRESULT hr = gpu_frame_texture_.As(&surface);
    if (FAILED(hr)) return false;

    // Force 96 DPI — coordinates are in physical pixels (see
    // ApplyGpuTextWatermark for full explanation).
    D2D1_RENDER_TARGET_PROPERTIES rt_props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                          D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);

    ComPtr<ID2D1RenderTarget> rt;
    hr = d2d_factory_->CreateDxgiSurfaceRenderTarget(
        surface.Get(), &rt_props, &rt);
    if (FAILED(hr)) return false;

    // Font.
    int font_size = (wm.font_size > 0) ? wm.font_size : 16;
    const char* font_name = wm.font_name ? wm.font_name : "Arial";
    std::wstring wfont = Utf8ToWide(font_name);

    ComPtr<IDWriteTextFormat> text_format;
    hr = dwrite_factory_->CreateTextFormat(
        wfont.c_str(), nullptr,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, static_cast<FLOAT>(font_size),
        L"", &text_format);
    if (FAILED(hr)) return false;

    std::wstring wtext = Utf8ToWide(wm.text);

    // Measure text.
    ComPtr<IDWriteTextLayout> text_layout;
    hr = dwrite_factory_->CreateTextLayout(
        wtext.c_str(), static_cast<UINT32>(wtext.size()),
        text_format.Get(), 10000.0f, 10000.0f, &text_layout);
    if (FAILED(hr)) return false;

    DWRITE_TEXT_METRICS metrics{};
    text_layout->GetMetrics(&metrics);
    int text_w = static_cast<int>(metrics.width + 0.5f);
    int text_h = static_cast<int>(metrics.height + 0.5f);

    // Alpha from color.
    uint32_t argb = wm.color;
    if (argb == 0) argb = 0x80FFFFFF;
    float a = ((argb >> 24) & 0xFF) / 255.0f;

    ComPtr<ID2D1SolidColorBrush> outline_brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, a),
                              &outline_brush);
    ComPtr<ID2D1SolidColorBrush> fill_brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, a),
                              &fill_brush);
    if (!outline_brush || !fill_brush) return false;

    // Tile spacing.
    int sx = text_w + 80;
    int sy = text_h + 60;

    // Expand tiling area to cover full image after rotation.
    FLOAT fw = static_cast<FLOAT>(frame_width_);
    FLOAT fh = static_cast<FLOAT>(frame_height_);
    FLOAT diag = std::sqrt(fw * fw + fh * fh);
    int start_x = -static_cast<int>((diag - fw) / 2);
    int start_y = -static_cast<int>((diag - fh) / 2);
    int end_x = frame_width_ + static_cast<int>((diag - fw) / 2);
    int end_y = frame_height_ + static_cast<int>((diag - fh) / 2);

    rt->BeginDraw();

    // Rotate around image center.
    D2D1_POINT_2F center = D2D1::Point2F(fw / 2, fh / 2);
    rt->SetTransform(D2D1::Matrix3x2F::Rotation(angle_deg, center));

    // Tile.
    constexpr FLOAT kOff = 1.5f;
    constexpr FLOAT offsets[][2] = {
        {-kOff, -kOff}, {0, -kOff}, {kOff, -kOff},
        {-kOff,     0},             {kOff,     0},
        {-kOff,  kOff}, {0,  kOff}, {kOff,  kOff},
    };

    for (int ty = start_y; ty < end_y; ty += sy) {
      for (int tx = start_x; tx < end_x; tx += sx) {
        FLOAT fx = static_cast<FLOAT>(tx);
        FLOAT fy = static_cast<FLOAT>(ty);
        // White outline (8-direction offset).
        for (const auto& off : offsets) {
          rt->DrawTextLayout(D2D1::Point2F(fx + off[0], fy + off[1]),
                             text_layout.Get(), outline_brush.Get());
        }
        // Black fill.
        rt->DrawTextLayout(D2D1::Point2F(fx, fy),
                           text_layout.Get(), fill_brush.Get());
      }
    }

    rt->SetTransform(D2D1::Matrix3x2F::Identity());
    hr = rt->EndDraw();
    return SUCCEEDED(hr);
  }

  // =========================================================================
  // GPU → CPU readback
  // =========================================================================

  /// Read gpu_frame_texture_ back to CPU memory and produce an Image.
  /// Uses a staging texture for the GPU→CPU copy.
  std::unique_ptr<Image> ReadbackGpuFrame() {
    if (!d3d11_mgr_ || !gpu_frame_texture_) return nullptr;

    auto* ctx = d3d11_mgr_->context();

    // Create staging texture on first use (or recreate if size changed).
    if (!gpu_staging_texture_) {
      gpu_staging_texture_ =
          d3d11_mgr_->CreateStagingTexture(frame_width_, frame_height_);
      if (!gpu_staging_texture_) return nullptr;
    }

    // GPU→GPU copy to staging texture.
    ctx->CopyResource(gpu_staging_texture_.Get(), gpu_frame_texture_.Get());

    // Map staging texture for CPU read.
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = ctx->Map(gpu_staging_texture_.Get(), 0, D3D11_MAP_READ,
                          0, &mapped);
    if (FAILED(hr)) return false;

    int stride = frame_width_ * 4;
    std::vector<uint8_t> data(static_cast<size_t>(stride) * frame_height_);

    // Copy row by row (mapped pitch may differ from our stride).
    for (int row = 0; row < frame_height_; ++row) {
      const uint8_t* src =
          static_cast<const uint8_t*>(mapped.pData) + row * mapped.RowPitch;
      uint8_t* dst = data.data() + row * stride;
      std::memcpy(dst, src, static_cast<size_t>(stride));
    }

    ctx->Unmap(gpu_staging_texture_.Get(), 0);

    // DXGI surfaces have alpha=0xFF by default for opaque desktop content.
    return Image::CreateFromData(frame_width_, frame_height_, stride,
                                 kPixelGrabFormatBgra8, std::move(data));
  }

  // =========================================================================
  // Capture loop (auto mode)
  // =========================================================================

  void CaptureLoopFunc() {
    const auto interval = std::chrono::microseconds(1'000'000 / fps_);

    while (capture_running_.load(std::memory_order_acquire)) {
      auto tick_start = std::chrono::steady_clock::now();

      if (!paused_.load(std::memory_order_acquire)) {
        if (gpu_available_) {
          // GPU path: DXGI DD → D2D watermark → MF texture encode.
          GpuCaptureOneFrame();
        } else {
          // CPU path: CaptureBackend → WatermarkRenderer → MF CPU encode.
          CpuCaptureOneFrame();
        }

        // Capture and write audio samples alongside video.
        FlushAudioSamples();
      }

      // Sleep for remainder of frame interval.
      auto elapsed = std::chrono::steady_clock::now() - tick_start;
      if (elapsed < interval) {
        std::this_thread::sleep_for(interval - elapsed);
      }
    }
  }

  /// GPU path: acquire → watermark → readback → encode.
  /// Falls back to CPU capture if DXGI DD fails and no previous frame exists.
  void GpuCaptureOneFrame() {
    bool new_frame = AcquireDesktopFrame();

    if (new_frame) {
      gpu_has_valid_frame_ = true;

      // Apply system watermark on GPU texture (bottom-right).
      if (config_.has_watermark) {
        ApplyGpuTextWatermark(config_.watermark_config);
      }
      // User watermarks are applied AFTER readback (fast pixel blend).
    }

    if (gpu_has_valid_frame_) {
      // Readback GPU texture → CPU Image.
      auto image = ReadbackGpuFrame();
      if (image) {
        // Apply user watermarks on CPU image (pre-rendered bitmap blend).
        ApplyUserWatermarksToImage(image.get());
        WriteFrame(*image);
      }
    } else if (config_.capture_backend) {
      // No GPU frame yet (first frame, desktop hasn't changed).
      // Fall back to CPU capture for this frame.
      CpuCaptureOneFrame();
    }
  }

  // =========================================================================
  // Animated user watermark — up to 5 drifting instances
  // =========================================================================

  /// Maximum simultaneous floating watermarks.
  static constexpr int kUserWmCount = 5;

  /// Text rotation angle (degrees, negative = tilt left).
  static constexpr float kUserWmRotation = -25.0f;

  /// Compute the (x, y) positions of 5 floating watermarks for the current
  /// frame.  Watermarks enter from the **top edge** at different horizontal
  /// positions and drift **diagonally** toward the bottom-left (斜着).
  /// Only positions that overlap the visible area are marked valid.
  ///
  /// @param[out] out_x      Array of kUserWmCount X positions.
  /// @param[out] out_y      Array of kUserWmCount Y positions.
  /// @param[out] out_vis    Array of kUserWmCount visibility flags.
  /// @return Number of currently visible watermarks.
  int ComputeUserWmPositions(int* out_x, int* out_y, bool* out_vis) const {
    const float w = static_cast<float>(frame_width_);
    const float h = static_cast<float>(frame_height_);

    // Diagonal movement: leftward + downward (roughly -40° from vertical).
    // At 30 fps → ~36 px/s left, ~45 px/s down.
    constexpr float kDx = -1.2f;   // leftward
    constexpr float kDy =  1.5f;   // downward (primary)

    // Cycle based on vertical travel (top → below bottom).
    const float y_entry = -80.0f;
    const float y_range = h + 200.0f;
    const float cycle   = y_range / kDy;

    // Entry X positions spread across screen width; phase offsets irregular.
    static constexpr float x_frac[5]     = { 0.12f, 0.74f, 0.40f, 0.88f, 0.55f };
    static constexpr float phase_frac[5] = { 0.00f, 0.38f, 0.65f, 0.20f, 0.82f };

    // Margin for visibility check (text may be ~250 px wide, ~40 px tall).
    constexpr int kMarginX = 300;
    constexpr int kMarginY = 80;

    int visible = 0;
    for (int i = 0; i < kUserWmCount; ++i) {
      float phase = phase_frac[i] * cycle;
      float t = std::fmod(
          static_cast<float>(frame_count_) + phase, cycle);
      int x = static_cast<int>(w * x_frac[i] + kDx * t);
      int y = static_cast<int>(y_entry + kDy * t);
      out_x[i] = x;
      out_y[i] = y;
      // Visible if any part of the text could overlap the frame.
      out_vis[i] = (x > -kMarginX && x < frame_width_ + 50 &&
                    y > -kMarginY && y < frame_height_ + 50);
      if (out_vis[i]) ++visible;
    }
    return visible;
  }

  /// CPU path: capture → watermark → encode (existing logic).
  void CpuCaptureOneFrame() {
    auto frame = config_.capture_backend->CaptureRegion(
        config_.region_x, config_.region_y,
        config_.region_width, config_.region_height);

    if (frame) {
      // System watermark (bottom-right).
      if (config_.has_watermark && config_.watermark_renderer) {
        config_.watermark_renderer->ApplyTextWatermark(
            frame.get(), config_.watermark_config);
      }
      // User watermark — pre-rendered bitmap blend (fast).
      ApplyUserWatermarksToImage(frame.get());
      WriteFrame(*frame);
    }
  }

  // =========================================================================
  // Audio capture helpers
  // =========================================================================

  /// Read available audio samples from the backend and write to sink writer.
  void FlushAudioSamples() {
    if (!audio_backend_ || !sink_writer_) return;

    auto samples = audio_backend_->ReadSamples();
    if (samples.data.empty()) return;

    DWORD data_bytes = static_cast<DWORD>(samples.data.size() * sizeof(int16_t));

    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(data_bytes, &buffer);
    if (FAILED(hr)) return;

    BYTE* dest = nullptr;
    hr = buffer->Lock(&dest, nullptr, nullptr);
    if (FAILED(hr)) return;

    std::memcpy(dest, samples.data.data(), data_bytes);
    buffer->Unlock();
    buffer->SetCurrentLength(data_bytes);

    ComPtr<IMFSample> sample;
    hr = MFCreateSample(&sample);
    if (FAILED(hr)) return;

    sample->AddBuffer(buffer.Get());

    // Compute presentation time from total samples written.
    // Time is in 100-ns units (MF reference time).
    int64_t time_100ns = (audio_samples_written_ * 10'000'000LL) /
                         audio_sample_rate_;
    sample->SetSampleTime(time_100ns);

    // Duration of this chunk.
    int num_frames = static_cast<int>(samples.data.size()) / audio_channels_;
    int64_t dur_100ns = (static_cast<int64_t>(num_frames) * 10'000'000LL) /
                        audio_sample_rate_;
    sample->SetSampleDuration(dur_100ns);

    {
      std::lock_guard<std::mutex> lock(write_mutex_);
      hr = sink_writer_->WriteSample(audio_stream_index_, sample.Get());
    }

    if (SUCCEEDED(hr)) {
      audio_samples_written_ += num_frames;
    } else {
      PIXELGRAB_LOG_ERROR("Audio WriteSample failed: 0x{:08X}", hr);
    }
  }

  // =========================================================================
  // Member variables
  // =========================================================================

  RecordConfig config_;
  ComPtr<IMFSinkWriter> sink_writer_;
  DWORD stream_index_ = 0;
  int frame_width_ = 0;
  int frame_height_ = 0;
  int fps_ = 30;
  int64_t frame_duration_ = 0;
  int64_t frame_count_ = 0;
  RecordState state_ = RecordState::kIdle;
  bool mf_started_ = false;
  std::chrono::steady_clock::time_point start_time_;

  // Thread safety.
  std::mutex write_mutex_;
  std::thread capture_thread_;
  std::atomic<bool> capture_running_{false};
  std::atomic<bool> paused_{false};

  // -- Audio --
  AudioBackend* audio_backend_ = nullptr;  // Non-owning, set during Initialize.
  DWORD audio_stream_index_ = 0;
  int audio_sample_rate_ = 44100;
  int audio_channels_ = 2;
  int64_t audio_samples_written_ = 0;  // Total audio frames written (for PTS).

  // -- GPU pipeline (all owned by Recorder, per Creative Phase 1 decision) --
  bool gpu_available_ = false;

  // D3D11 device (shared across DXGI DD, D2D, MF).
  std::unique_ptr<D3D11DeviceManager> d3d11_mgr_;

  // DXGI Desktop Duplication.
  ComPtr<IDXGIOutputDuplication> dxgi_dup_;

  // Render target texture (capture → watermark → readback).
  ComPtr<ID3D11Texture2D> gpu_frame_texture_;
  ComPtr<ID3D11Texture2D> gpu_staging_texture_;  // For GPU→CPU readback.
  bool gpu_has_valid_frame_ = false;  // True after first successful DXGI DD.

  // Direct2D + DirectWrite (GPU watermark).
  ComPtr<ID2D1Factory> d2d_factory_;
  ComPtr<IDWriteFactory> dwrite_factory_;

  // Pre-rendered user watermark bitmap (rendered once, blended every frame).
  bool uwm_bitmap_ready_ = false;
  std::vector<uint32_t> uwm_bmp_pixels_;  // BGRA pixels
  int uwm_bmp_w_ = 0;
  int uwm_bmp_h_ = 0;

  // MF DXGI device manager (GPU texture → H.264 encoding).
  ComPtr<IMFDXGIDeviceManager> mf_dxgi_manager_;
  UINT mf_reset_token_ = 0;
};

// ===========================================================================
// Factory
// ===========================================================================

std::unique_ptr<RecorderBackend> CreatePlatformRecorder() {
  return std::make_unique<WinRecorderBackend>();
}

}  // namespace internal
}  // namespace pixelgrab
