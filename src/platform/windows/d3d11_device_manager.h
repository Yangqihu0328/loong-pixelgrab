// Copyright 2026 The loong-pixelgrab Authors
//
// D3D11 device manager — shared D3D11 device creation and management.
// Used by WinRecorderBackend for GPU-accelerated recording pipeline.

#ifndef PIXELGRAB_PLATFORM_WINDOWS_D3D11_DEVICE_MANAGER_H_
#define PIXELGRAB_PLATFORM_WINDOWS_D3D11_DEVICE_MANAGER_H_

#ifdef _WIN32

#include <cstdint>
#include <memory>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <d3d11.h>
#include <dxgi1_2.h>

#include <wrl/client.h>

namespace pixelgrab {
namespace internal {

/// Manages a shared D3D11 device for GPU-accelerated operations.
///
/// The device is created with BGRA support flag (required for D2D interop)
/// and can be shared across DXGI Desktop Duplication, Direct2D watermarking,
/// and Media Foundation hardware encoding.
class D3D11DeviceManager {
 public:
  /// Create a D3D11 device manager. Returns nullptr if D3D11 is not available.
  static std::unique_ptr<D3D11DeviceManager> Create();

  ~D3D11DeviceManager() = default;

  // Non-copyable.
  D3D11DeviceManager(const D3D11DeviceManager&) = delete;
  D3D11DeviceManager& operator=(const D3D11DeviceManager&) = delete;

  /// Get the D3D11 device.
  ID3D11Device* device() const { return device_.Get(); }

  /// Get the immediate device context.
  ID3D11DeviceContext* context() const { return context_.Get(); }

  /// Get the DXGI adapter used to create the device.
  IDXGIAdapter* adapter() const { return adapter_.Get(); }

  /// Get the D3D feature level of the created device.
  D3D_FEATURE_LEVEL feature_level() const { return feature_level_; }

  /// Create a DXGI Output Duplication for the specified output.
  /// @param output_index  Zero-based output index (0 = primary monitor).
  /// @return OutputDuplication interface, or nullptr on failure.
  Microsoft::WRL::ComPtr<IDXGIOutputDuplication> CreateOutputDuplication(
      int output_index = 0);

  /// Create a staging texture for GPU→CPU readback.
  /// @param width   Texture width.
  /// @param height  Texture height.
  /// @return Staging texture, or nullptr on failure.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateStagingTexture(int width,
                                                                int height);

  /// Create a render target texture (for D2D watermark rendering).
  /// @param width   Texture width.
  /// @param height  Texture height.
  /// @return Render target texture with DXGI surface binding, or nullptr.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateRenderTargetTexture(
      int width, int height);

 private:
  D3D11DeviceManager() = default;

  Microsoft::WRL::ComPtr<ID3D11Device> device_;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_;
  D3D_FEATURE_LEVEL feature_level_ = static_cast<D3D_FEATURE_LEVEL>(0);
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // _WIN32

#endif  // PIXELGRAB_PLATFORM_WINDOWS_D3D11_DEVICE_MANAGER_H_
