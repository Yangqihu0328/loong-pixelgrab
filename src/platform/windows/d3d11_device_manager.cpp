// Copyright 2026 The loong-pixelgrab Authors
//
// D3D11 device manager implementation.

#include "platform/windows/d3d11_device_manager.h"

#ifdef _WIN32

#include "core/logger.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

namespace pixelgrab {
namespace internal {

// ---------------------------------------------------------------------------
// D3D11DeviceManager::Create
// ---------------------------------------------------------------------------

std::unique_ptr<D3D11DeviceManager> D3D11DeviceManager::Create() {
  // Feature levels to try, in descending order of preference.
  D3D_FEATURE_LEVEL feature_levels[] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
  };

  // Flags: BGRA support is required for D2D interop.
  UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  D3D_FEATURE_LEVEL achieved_level{};

  HRESULT hr = D3D11CreateDevice(
      nullptr,                    // Default adapter
      D3D_DRIVER_TYPE_HARDWARE,   // Hardware GPU
      nullptr,                    // No software rasterizer
      flags,                      // Creation flags
      feature_levels,             // Feature levels to try
      ARRAYSIZE(feature_levels),  // Number of feature levels
      D3D11_SDK_VERSION,          // SDK version
      &device,                    // [out] Device
      &achieved_level,            // [out] Achieved feature level
      &context);                  // [out] Immediate context

  if (FAILED(hr)) {
    PIXELGRAB_LOG_WARN("D3D11CreateDevice failed: 0x{:08X}", hr);
    return nullptr;
  }

  PIXELGRAB_LOG_INFO("D3D11 device created (feature level: 0x{:04X})",
                     static_cast<int>(achieved_level));

  // Get the DXGI adapter from the device.
  ComPtr<IDXGIDevice> dxgi_device;
  hr = device.As(&dxgi_device);
  if (FAILED(hr)) {
    PIXELGRAB_LOG_WARN("Failed to get IDXGIDevice: 0x{:08X}", hr);
    return nullptr;
  }

  ComPtr<IDXGIAdapter> adapter;
  hr = dxgi_device->GetAdapter(&adapter);
  if (FAILED(hr)) {
    PIXELGRAB_LOG_WARN("Failed to get DXGI adapter: 0x{:08X}", hr);
    return nullptr;
  }

  auto mgr = std::unique_ptr<D3D11DeviceManager>(new D3D11DeviceManager());
  mgr->device_ = std::move(device);
  mgr->context_ = std::move(context);
  mgr->adapter_ = std::move(adapter);
  mgr->feature_level_ = achieved_level;

  return mgr;
}

// ---------------------------------------------------------------------------
// CreateOutputDuplication
// ---------------------------------------------------------------------------

ComPtr<IDXGIOutputDuplication> D3D11DeviceManager::CreateOutputDuplication(
    int output_index) {
  if (!adapter_) return nullptr;

  // Get the specified output from the adapter.
  ComPtr<IDXGIOutput> output;
  HRESULT hr = adapter_->EnumOutputs(static_cast<UINT>(output_index), &output);
  if (FAILED(hr)) {
    PIXELGRAB_LOG_WARN("EnumOutputs({}) failed: 0x{:08X}", output_index, hr);
    return nullptr;
  }

  // Desktop Duplication requires IDXGIOutput1 (Windows 8+).
  ComPtr<IDXGIOutput1> output1;
  hr = output.As(&output1);
  if (FAILED(hr)) {
    PIXELGRAB_LOG_WARN("IDXGIOutput1 not available (pre-Win8?): 0x{:08X}", hr);
    return nullptr;
  }

  ComPtr<IDXGIOutputDuplication> duplication;
  hr = output1->DuplicateOutput(device_.Get(), &duplication);
  if (FAILED(hr)) {
    // Common failures: E_ACCESSDENIED (remote desktop), DXGI_ERROR_NOT_CURRENTLY_AVAILABLE
    PIXELGRAB_LOG_WARN("DuplicateOutput failed: 0x{:08X} "
                       "(may be remote desktop or VM)", hr);
    return nullptr;
  }

  DXGI_OUTDUPL_DESC desc{};
  duplication->GetDesc(&desc);
  PIXELGRAB_LOG_INFO("DXGI Output Duplication created: {}x{} (output {})",
                     desc.ModeDesc.Width, desc.ModeDesc.Height, output_index);

  return duplication;
}

// ---------------------------------------------------------------------------
// CreateStagingTexture
// ---------------------------------------------------------------------------

ComPtr<ID3D11Texture2D> D3D11DeviceManager::CreateStagingTexture(int width,
                                                                  int height) {
  if (!device_) return nullptr;

  D3D11_TEXTURE2D_DESC desc{};
  desc.Width = static_cast<UINT>(width);
  desc.Height = static_cast<UINT>(height);
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

  ComPtr<ID3D11Texture2D> texture;
  HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &texture);
  if (FAILED(hr)) {
    PIXELGRAB_LOG_ERROR("CreateStagingTexture failed: 0x{:08X}", hr);
    return nullptr;
  }

  return texture;
}

// ---------------------------------------------------------------------------
// CreateRenderTargetTexture
// ---------------------------------------------------------------------------

ComPtr<ID3D11Texture2D> D3D11DeviceManager::CreateRenderTargetTexture(
    int width, int height) {
  if (!device_) return nullptr;

  D3D11_TEXTURE2D_DESC desc{};
  desc.Width = static_cast<UINT>(width);
  desc.Height = static_cast<UINT>(height);
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET;
  desc.MiscFlags = 0;

  ComPtr<ID3D11Texture2D> texture;
  HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &texture);
  if (FAILED(hr)) {
    PIXELGRAB_LOG_ERROR("CreateRenderTargetTexture failed: 0x{:08X}", hr);
    return nullptr;
  }

  return texture;
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // _WIN32
