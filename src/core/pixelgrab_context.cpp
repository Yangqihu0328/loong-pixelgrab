// Copyright 2026 The loong-pixelgrab Authors

#include "core/pixelgrab_context.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include "core/logger.h"

namespace pixelgrab {
namespace internal {

PixelGrabContextImpl::PixelGrabContextImpl() = default;

PixelGrabContextImpl::~PixelGrabContextImpl() {
  if (backend_) {
    backend_->Shutdown();
  }
}

bool PixelGrabContextImpl::Initialize() {
  if (initialized_) {
    return true;
  }

  PIXELGRAB_LOG_INFO("Initializing pixelgrab context...");

  backend_ = CreatePlatformBackend();
  if (!backend_) {
    SetError(kPixelGrabErrorNotSupported,
             "Failed to create platform capture backend");
    return false;
  }

  if (!backend_->Initialize()) {
    SetError(kPixelGrabErrorCaptureFailed,
             "Failed to initialize platform capture backend");
    backend_.reset();
    return false;
  }

  PIXELGRAB_LOG_DEBUG("Platform capture backend initialized");

  // Initialize element detection (best-effort; failure is not fatal).
  element_detector_ = CreatePlatformElementDetector();
  if (element_detector_) {
    snap_engine_ = std::make_unique<SnapEngine>(element_detector_.get());
    PIXELGRAB_LOG_DEBUG("Element detector and snap engine initialized");
  } else {
    PIXELGRAB_LOG_WARN("Element detector unavailable on this platform");
  }

  initialized_ = true;
  PIXELGRAB_LOG_INFO("pixelgrab context initialized successfully");
  ClearError();
  return true;
}

void PixelGrabContextImpl::SetError(PixelGrabError code,
                                    const std::string& message) {
  last_error_ = code;
  last_error_message_ = message;
  PIXELGRAB_LOG_ERROR("Error {}: {}", static_cast<int>(code), message);
}

void PixelGrabContextImpl::ClearError() {
  last_error_ = kPixelGrabOk;
  last_error_message_ = "No error";
}

void PixelGrabContextImpl::RefreshScreens() {
  if (!backend_) return;
  cached_screens_ = backend_->GetScreens();
}

int PixelGrabContextImpl::GetScreenCount() {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return -1;
  }
  RefreshScreens();
  return static_cast<int>(cached_screens_.size());
}

PixelGrabError PixelGrabContextImpl::GetScreenInfo(
    int screen_index, PixelGrabScreenInfo* out_info) {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return kPixelGrabErrorNotInitialized;
  }
  if (!out_info) {
    SetError(kPixelGrabErrorInvalidParam, "out_info is NULL");
    return kPixelGrabErrorInvalidParam;
  }

  RefreshScreens();

  if (screen_index < 0 ||
      screen_index >= static_cast<int>(cached_screens_.size())) {
    SetError(kPixelGrabErrorInvalidParam, "Screen index out of range");
    return kPixelGrabErrorInvalidParam;
  }

  *out_info = cached_screens_[screen_index];
  ClearError();
  return kPixelGrabOk;
}

Image* PixelGrabContextImpl::CaptureScreen(int screen_index) {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return nullptr;
  }

  PIXELGRAB_LOG_DEBUG("CaptureScreen(screen_index={})", screen_index);

  auto image = backend_->CaptureScreen(screen_index);
  if (!image) {
    SetError(kPixelGrabErrorCaptureFailed, "Screen capture failed");
    return nullptr;
  }

  PIXELGRAB_LOG_INFO("Screen {} captured: {}x{}", screen_index,
                     image->width(), image->height());
  ClearError();
  return image.release();  // Transfer ownership to C API caller.
}

Image* PixelGrabContextImpl::CaptureRegion(int x, int y, int width,
                                           int height) {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return nullptr;
  }
  if (width <= 0 || height <= 0) {
    SetError(kPixelGrabErrorInvalidParam,
             "Region width and height must be positive");
    return nullptr;
  }

  PIXELGRAB_LOG_DEBUG("CaptureRegion(x={}, y={}, w={}, h={})", x, y, width,
                      height);

  auto image = backend_->CaptureRegion(x, y, width, height);
  if (!image) {
    SetError(kPixelGrabErrorCaptureFailed, "Region capture failed");
    return nullptr;
  }

  // Record this region in capture history.
  capture_history_.Record(x, y, width, height);

  PIXELGRAB_LOG_INFO("Region captured: ({},{}) {}x{}", x, y, width, height);
  ClearError();
  return image.release();
}

Image* PixelGrabContextImpl::CaptureWindow(uint64_t window_handle) {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return nullptr;
  }
  if (window_handle == 0) {
    SetError(kPixelGrabErrorInvalidParam, "Invalid window handle");
    return nullptr;
  }

  PIXELGRAB_LOG_DEBUG("CaptureWindow(handle=0x{:X})", window_handle);

  auto image = backend_->CaptureWindow(window_handle);
  if (!image) {
    SetError(kPixelGrabErrorCaptureFailed, "Window capture failed");
    return nullptr;
  }

  PIXELGRAB_LOG_INFO("Window 0x{:X} captured: {}x{}", window_handle,
                     image->width(), image->height());
  ClearError();
  return image.release();
}

int PixelGrabContextImpl::EnumerateWindows(PixelGrabWindowInfo* out_windows,
                                           int max_count) {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return -1;
  }
  if (!out_windows || max_count <= 0) {
    SetError(kPixelGrabErrorInvalidParam,
             "Invalid output buffer or max_count");
    return -1;
  }

  auto windows = backend_->EnumerateWindows();
  int count = std::min(static_cast<int>(windows.size()), max_count);
  for (int i = 0; i < count; ++i) {
    out_windows[i] = windows[i];
  }

  ClearError();
  return count;
}

// ---------------------------------------------------------------------------
// DPI support
// ---------------------------------------------------------------------------

PixelGrabError PixelGrabContextImpl::EnableDpiAwareness() {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return kPixelGrabErrorNotInitialized;
  }

  if (!backend_->EnableDpiAwareness()) {
    SetError(kPixelGrabErrorNotSupported, "DPI awareness not supported");
    return kPixelGrabErrorNotSupported;
  }

  // Refresh screens after DPI change — coordinates may have changed.
  RefreshScreens();
  ClearError();
  return kPixelGrabOk;
}

PixelGrabError PixelGrabContextImpl::GetDpiInfo(int screen_index,
                                                PixelGrabDpiInfo* out_info) {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return kPixelGrabErrorNotInitialized;
  }
  if (!out_info) {
    SetError(kPixelGrabErrorInvalidParam, "out_info is NULL");
    return kPixelGrabErrorInvalidParam;
  }

  if (!backend_->GetDpiInfo(screen_index, out_info)) {
    SetError(kPixelGrabErrorInvalidParam,
             "Failed to get DPI info for screen index");
    return kPixelGrabErrorInvalidParam;
  }

  ClearError();
  return kPixelGrabOk;
}

PixelGrabError PixelGrabContextImpl::LogicalToPhysical(
    int screen_index, int logical_x, int logical_y, int* out_physical_x,
    int* out_physical_y) {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return kPixelGrabErrorNotInitialized;
  }
  if (!out_physical_x || !out_physical_y) {
    SetError(kPixelGrabErrorInvalidParam, "Output pointers are NULL");
    return kPixelGrabErrorInvalidParam;
  }

  PixelGrabDpiInfo dpi = {};
  if (!backend_->GetDpiInfo(screen_index, &dpi)) {
    SetError(kPixelGrabErrorInvalidParam, "Invalid screen index for DPI");
    return kPixelGrabErrorInvalidParam;
  }

  *out_physical_x = static_cast<int>(std::round(logical_x * dpi.scale_x));
  *out_physical_y = static_cast<int>(std::round(logical_y * dpi.scale_y));
  ClearError();
  return kPixelGrabOk;
}

PixelGrabError PixelGrabContextImpl::PhysicalToLogical(
    int screen_index, int physical_x, int physical_y, int* out_logical_x,
    int* out_logical_y) {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return kPixelGrabErrorNotInitialized;
  }
  if (!out_logical_x || !out_logical_y) {
    SetError(kPixelGrabErrorInvalidParam, "Output pointers are NULL");
    return kPixelGrabErrorInvalidParam;
  }

  PixelGrabDpiInfo dpi = {};
  if (!backend_->GetDpiInfo(screen_index, &dpi)) {
    SetError(kPixelGrabErrorInvalidParam, "Invalid screen index for DPI");
    return kPixelGrabErrorInvalidParam;
  }

  if (dpi.scale_x < 1e-6f || dpi.scale_y < 1e-6f) {
    SetError(kPixelGrabErrorInvalidParam, "DPI scale is zero");
    return kPixelGrabErrorInvalidParam;
  }

  *out_logical_x = static_cast<int>(std::round(physical_x / dpi.scale_x));
  *out_logical_y = static_cast<int>(std::round(physical_y / dpi.scale_y));
  ClearError();
  return kPixelGrabOk;
}

// ---------------------------------------------------------------------------
// Color picker
// ---------------------------------------------------------------------------

PixelGrabError PixelGrabContextImpl::PickColor(int x, int y,
                                               PixelGrabColor* out_color) {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return kPixelGrabErrorNotInitialized;
  }
  if (!out_color) {
    SetError(kPixelGrabErrorInvalidParam, "out_color is NULL");
    return kPixelGrabErrorInvalidParam;
  }

  // Capture a 1x1 region at the specified coordinates.
  auto image = backend_->CaptureRegion(x, y, 1, 1);
  if (!image || image->data_size() < 4) {
    SetError(kPixelGrabErrorCaptureFailed, "Failed to capture pixel");
    return kPixelGrabErrorCaptureFailed;
  }

  const uint8_t* pixel = image->data();
  // Image is BGRA8 format.
  if (image->format() == kPixelGrabFormatBgra8 ||
      image->format() == kPixelGrabFormatNative) {
    out_color->b = pixel[0];
    out_color->g = pixel[1];
    out_color->r = pixel[2];
    out_color->a = pixel[3];
  } else {
    // RGBA8
    out_color->r = pixel[0];
    out_color->g = pixel[1];
    out_color->b = pixel[2];
    out_color->a = pixel[3];
  }

  ClearError();
  return kPixelGrabOk;
}

// ---------------------------------------------------------------------------
// Magnifier
// ---------------------------------------------------------------------------

Image* PixelGrabContextImpl::GetMagnifier(int x, int y, int radius,
                                          int magnification) {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return nullptr;
  }
  if (radius <= 0 || magnification < 2 || magnification > 32) {
    SetError(kPixelGrabErrorInvalidParam,
             "Invalid radius or magnification (radius>0, magnification 2-32)");
    return nullptr;
  }

  // Capture the source region around (x, y).
  int src_size = radius * 2 + 1;
  int src_x = x - radius;
  int src_y = y - radius;

  auto src = backend_->CaptureRegion(src_x, src_y, src_size, src_size);
  if (!src) {
    SetError(kPixelGrabErrorCaptureFailed, "Failed to capture magnifier region");
    return nullptr;
  }

  // Create the magnified output image using nearest-neighbor scaling.
  int out_size = src_size * magnification;
  auto out = Image::Create(out_size, out_size, kPixelGrabFormatBgra8);
  if (!out) {
    SetError(kPixelGrabErrorOutOfMemory, "Failed to allocate magnifier image");
    return nullptr;
  }

  const uint8_t* src_data = src->data();
  uint8_t* dst_data = out->mutable_data();
  int src_stride = src->stride();
  int dst_stride = out->stride();

  for (int dy = 0; dy < out_size; ++dy) {
    int sy = dy / magnification;
    if (sy >= src_size) sy = src_size - 1;
    for (int dx = 0; dx < out_size; ++dx) {
      int sx = dx / magnification;
      if (sx >= src_size) sx = src_size - 1;

      const uint8_t* sp = src_data + sy * src_stride + sx * 4;
      uint8_t* dp = dst_data + dy * dst_stride + dx * 4;
      dp[0] = sp[0];
      dp[1] = sp[1];
      dp[2] = sp[2];
      dp[3] = sp[3];
    }
  }

  ClearError();
  return out.release();
}

// ---------------------------------------------------------------------------
// Element detection / snapping
// ---------------------------------------------------------------------------

PixelGrabError PixelGrabContextImpl::DetectElement(
    int x, int y, PixelGrabElementRect* out_rect) {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return kPixelGrabErrorNotInitialized;
  }
  if (!out_rect) {
    SetError(kPixelGrabErrorInvalidParam, "out_rect is NULL");
    return kPixelGrabErrorInvalidParam;
  }
  if (!element_detector_) {
    SetError(kPixelGrabErrorNoElement, "Element detector not available");
    return kPixelGrabErrorNoElement;
  }

  PIXELGRAB_LOG_DEBUG("DetectElement(x={}, y={})", x, y);

  ElementInfo info;
  if (!element_detector_->DetectElement(x, y, &info)) {
    SetError(kPixelGrabErrorNoElement, "No element found at coordinates");
    return kPixelGrabErrorNoElement;
  }

  out_rect->x = info.x;
  out_rect->y = info.y;
  out_rect->width = info.width;
  out_rect->height = info.height;

  // Copy name (truncate if needed).
  std::memset(out_rect->name, 0, sizeof(out_rect->name));
  if (!info.name.empty()) {
    size_t copy_len =
        (std::min)(info.name.size(), sizeof(out_rect->name) - 1);
    std::memcpy(out_rect->name, info.name.c_str(), copy_len);
  }

  // Copy role.
  std::memset(out_rect->role, 0, sizeof(out_rect->role));
  if (!info.role.empty()) {
    size_t copy_len =
        (std::min)(info.role.size(), sizeof(out_rect->role) - 1);
    std::memcpy(out_rect->role, info.role.c_str(), copy_len);
  }

  ClearError();
  return kPixelGrabOk;
}

int PixelGrabContextImpl::DetectElements(int x, int y,
                                         PixelGrabElementRect* out_rects,
                                         int max_count) {
  if (!initialized_ || !out_rects || max_count <= 0 || !element_detector_) {
    return -1;
  }

  static constexpr int kMaxInternal = 10;
  ElementInfo infos[kMaxInternal];
  int actual_max = (std::min)(max_count, kMaxInternal);
  int count = element_detector_->DetectElements(x, y, infos, actual_max);
  if (count <= 0) return count;

  for (int i = 0; i < count; ++i) {
    out_rects[i].x = infos[i].x;
    out_rects[i].y = infos[i].y;
    out_rects[i].width = infos[i].width;
    out_rects[i].height = infos[i].height;

    std::memset(out_rects[i].name, 0, sizeof(out_rects[i].name));
    if (!infos[i].name.empty()) {
      size_t len =
          (std::min)(infos[i].name.size(), sizeof(out_rects[i].name) - 1);
      std::memcpy(out_rects[i].name, infos[i].name.c_str(), len);
    }

    std::memset(out_rects[i].role, 0, sizeof(out_rects[i].role));
    if (!infos[i].role.empty()) {
      size_t len =
          (std::min)(infos[i].role.size(), sizeof(out_rects[i].role) - 1);
      std::memcpy(out_rects[i].role, infos[i].role.c_str(), len);
    }
  }

  return count;
}

PixelGrabError PixelGrabContextImpl::SnapToElement(
    int x, int y, int snap_distance, PixelGrabElementRect* out_rect) {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return kPixelGrabErrorNotInitialized;
  }
  if (!out_rect) {
    SetError(kPixelGrabErrorInvalidParam, "out_rect is NULL");
    return kPixelGrabErrorInvalidParam;
  }
  if (!snap_engine_) {
    SetError(kPixelGrabErrorNoElement, "Snap engine not available");
    return kPixelGrabErrorNoElement;
  }

  if (snap_distance > 0) {
    snap_engine_->SetSnapDistance(snap_distance);
  }

  SnapResult result = snap_engine_->TrySnap(x, y);
  if (!result.snapped) {
    SetError(kPixelGrabErrorNoElement, "No element within snap distance");
    return kPixelGrabErrorNoElement;
  }

  out_rect->x = result.snapped_x;
  out_rect->y = result.snapped_y;
  out_rect->width = result.snapped_w;
  out_rect->height = result.snapped_h;

  std::memset(out_rect->name, 0, sizeof(out_rect->name));
  if (!result.element.name.empty()) {
    size_t len =
        (std::min)(result.element.name.size(), sizeof(out_rect->name) - 1);
    std::memcpy(out_rect->name, result.element.name.c_str(), len);
  }

  std::memset(out_rect->role, 0, sizeof(out_rect->role));
  if (!result.element.role.empty()) {
    size_t len =
        (std::min)(result.element.role.size(), sizeof(out_rect->role) - 1);
    std::memcpy(out_rect->role, result.element.role.c_str(), len);
  }

  ClearError();
  return kPixelGrabOk;
}

// ---------------------------------------------------------------------------
// Capture history
// ---------------------------------------------------------------------------

int PixelGrabContextImpl::HistoryCount() const {
  return capture_history_.Count();
}

PixelGrabError PixelGrabContextImpl::HistoryGetEntry(
    int index, PixelGrabHistoryEntry* out_entry) {
  if (!out_entry) {
    return kPixelGrabErrorInvalidParam;
  }

  HistoryEntry entry;
  if (!capture_history_.GetEntry(index, &entry)) {
    SetError(kPixelGrabErrorHistoryEmpty, "History index out of range");
    return kPixelGrabErrorHistoryEmpty;
  }

  out_entry->id = entry.id;
  out_entry->region_x = entry.region_x;
  out_entry->region_y = entry.region_y;
  out_entry->region_width = entry.region_width;
  out_entry->region_height = entry.region_height;
  out_entry->timestamp = entry.timestamp;

  ClearError();
  return kPixelGrabOk;
}

Image* PixelGrabContextImpl::HistoryRecapture(int history_id) {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return nullptr;
  }

  const HistoryEntry* entry = capture_history_.FindById(history_id);
  if (!entry) {
    SetError(kPixelGrabErrorHistoryEmpty, "History entry not found");
    return nullptr;
  }

  auto image = backend_->CaptureRegion(entry->region_x, entry->region_y,
                                       entry->region_width,
                                       entry->region_height);
  if (!image) {
    SetError(kPixelGrabErrorCaptureFailed, "Recapture failed");
    return nullptr;
  }

  ClearError();
  return image.release();
}

Image* PixelGrabContextImpl::RecaptureLast() {
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return nullptr;
  }

  if (capture_history_.Count() == 0) {
    SetError(kPixelGrabErrorHistoryEmpty, "No capture history");
    return nullptr;
  }

  HistoryEntry entry;
  if (!capture_history_.GetEntry(0, &entry)) {
    SetError(kPixelGrabErrorHistoryEmpty, "Failed to get last entry");
    return nullptr;
  }

  auto image = backend_->CaptureRegion(entry.region_x, entry.region_y,
                                       entry.region_width,
                                       entry.region_height);
  if (!image) {
    SetError(kPixelGrabErrorCaptureFailed, "Recapture failed");
    return nullptr;
  }

  ClearError();
  return image.release();
}

void PixelGrabContextImpl::HistoryClear() {
  capture_history_.Clear();
}

void PixelGrabContextImpl::HistorySetMaxCount(int max_count) {
  capture_history_.SetMaxCount(max_count);
}

// ---------------------------------------------------------------------------
// Clipboard reader (lazy init)
// ---------------------------------------------------------------------------

ClipboardReader* PixelGrabContextImpl::clipboard_reader() {
  if (!clipboard_reader_) {
    PIXELGRAB_LOG_DEBUG("Lazy-initializing clipboard reader");
    clipboard_reader_ = CreatePlatformClipboardReader();
  }
  return clipboard_reader_.get();
}

// ---------------------------------------------------------------------------
// Watermark renderer (lazy init)
// ---------------------------------------------------------------------------

WatermarkRenderer* PixelGrabContextImpl::watermark_renderer() {
  if (!watermark_renderer_) {
    PIXELGRAB_LOG_DEBUG("Lazy-initializing watermark renderer");
    watermark_renderer_ = CreatePlatformWatermarkRenderer();
  }
  return watermark_renderer_.get();
}

// ---------------------------------------------------------------------------
// OCR backend (lazy init)
// ---------------------------------------------------------------------------

OcrBackend* PixelGrabContextImpl::ocr_backend() {
  if (!ocr_backend_) {
    PIXELGRAB_LOG_DEBUG("Lazy-initializing OCR backend");
    ocr_backend_ = CreatePlatformOcrBackend();
  }
  return ocr_backend_.get();
}

// ---------------------------------------------------------------------------
// Translation backend (lazy init)
// ---------------------------------------------------------------------------

TranslateBackend* PixelGrabContextImpl::translate_backend() {
  if (!translate_backend_) {
    PIXELGRAB_LOG_DEBUG("Lazy-initializing translation backend");
    translate_backend_ = CreatePlatformTranslateBackend();
  }
  return translate_backend_.get();
}

// ---------------------------------------------------------------------------
// Audio backend (lazy init)
// ---------------------------------------------------------------------------

AudioBackend* PixelGrabContextImpl::audio_backend() {
  if (!audio_backend_) {
    PIXELGRAB_LOG_DEBUG("Lazy-initializing audio backend");
    audio_backend_ = CreatePlatformAudioBackend();
  }
  return audio_backend_.get();
}

}  // namespace internal
}  // namespace pixelgrab
