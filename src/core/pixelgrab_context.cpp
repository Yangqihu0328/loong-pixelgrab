// Copyright 2026 The loong-pixelgrab Authors

#include "core/pixelgrab_context.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
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
  std::lock_guard<std::mutex> lock(mu_);
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

static constexpr auto kScreensCacheTtl = std::chrono::seconds(1);

void PixelGrabContextImpl::RefreshScreens() {
  if (!backend_) return;
  auto now = std::chrono::steady_clock::now();
  if (!cached_screens_.empty() && (now - screens_cache_time_) < kScreensCacheTtl)
    return;
  cached_screens_ = backend_->GetScreens();
  screens_cache_time_ = now;
}

int PixelGrabContextImpl::GetScreenCount() {
  std::lock_guard<std::mutex> lock(mu_);
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return -1;
  }
  RefreshScreens();
  return static_cast<int>(cached_screens_.size());
}

PixelGrabError PixelGrabContextImpl::GetScreenInfo(
    int screen_index, PixelGrabScreenInfo* out_info) {
  std::lock_guard<std::mutex> lock(mu_);
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
  std::lock_guard<std::mutex> lock(mu_);
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
  std::lock_guard<std::mutex> lock(mu_);
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

  capture_history_.Record(x, y, width, height, image->Clone());

  PIXELGRAB_LOG_INFO("Region captured: ({},{}) {}x{}", x, y, width, height);
  ClearError();
  return image.release();
}

Image* PixelGrabContextImpl::CaptureWindow(uint64_t window_handle) {
  std::lock_guard<std::mutex> lock(mu_);
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

  // Invalidate cache and refresh screens after DPI change.
  screens_cache_time_ = {};
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
  std::lock_guard<std::mutex> lock(mu_);
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return kPixelGrabErrorNotInitialized;
  }
  if (!out_color) {
    SetError(kPixelGrabErrorInvalidParam, "out_color is NULL");
    return kPixelGrabErrorInvalidParam;
  }

  uint8_t bgra[4];
  if (!backend_->GetPixelColor(x, y, bgra)) {
    SetError(kPixelGrabErrorCaptureFailed, "Failed to capture pixel");
    return kPixelGrabErrorCaptureFailed;
  }

  out_color->b = bgra[0];
  out_color->g = bgra[1];
  out_color->r = bgra[2];
  out_color->a = bgra[3];

  ClearError();
  return kPixelGrabOk;
}

// ---------------------------------------------------------------------------
// Magnifier
// ---------------------------------------------------------------------------

static constexpr int kMinMagnification = 2;
static constexpr int kMaxMagnification = 32;
static constexpr int kMaxMagnifierRadius = 500;

Image* PixelGrabContextImpl::GetMagnifier(int x, int y, int radius,
                                          int magnification) {
  std::lock_guard<std::mutex> lock(mu_);
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return nullptr;
  }
  if (radius <= 0 || radius > kMaxMagnifierRadius) {
    SetError(kPixelGrabErrorInvalidParam,
             "Magnifier radius must be in range [1, 500]");
    return nullptr;
  }
  if (magnification < kMinMagnification ||
      magnification > kMaxMagnification) {
    SetError(kPixelGrabErrorInvalidParam,
             "Magnification must be in range [2, 32]");
    return nullptr;
  }

  int src_size = radius * 2 + 1;
  int64_t out_size_64 = static_cast<int64_t>(src_size) * magnification;
  if (out_size_64 > 16384) {
    SetError(kPixelGrabErrorInvalidParam,
             "Magnifier output exceeds 16384 pixel limit");
    return nullptr;
  }

  int src_x = x - radius;
  int src_y = y - radius;

  auto src = backend_->CaptureRegion(src_x, src_y, src_size, src_size);
  if (!src) {
    SetError(kPixelGrabErrorCaptureFailed, "Failed to capture magnifier region");
    return nullptr;
  }

  int out_size = static_cast<int>(out_size_64);
  auto out = Image::Create(out_size, out_size, kPixelGrabFormatBgra8);
  if (!out) {
    SetError(kPixelGrabErrorOutOfMemory, "Failed to allocate magnifier image");
    return nullptr;
  }

  const uint8_t* src_data = src->data();
  uint8_t* dst_data = out->mutable_data();
  int src_stride = src->stride();
  int dst_stride = out->stride();

  // Build the first output row for each source row, then memcpy duplicates.
  for (int sy = 0; sy < src_size; ++sy) {
    const uint8_t* src_row = src_data + sy * src_stride;
    int first_dy = sy * magnification;
    uint32_t* dst_row =
        reinterpret_cast<uint32_t*>(dst_data + first_dy * dst_stride);

    // Fill the first output row: each source pixel → magnification copies.
    for (int sx = 0; sx < src_size; ++sx) {
      uint32_t pixel;
      std::memcpy(&pixel, src_row + sx * 4, 4);
      int dx_start = sx * magnification;
      for (int k = 0; k < magnification; ++k) {
        dst_row[dx_start + k] = pixel;
      }
    }

    // Duplicate this row for the remaining (magnification - 1) output rows.
    uint8_t* first_row_ptr = dst_data + first_dy * dst_stride;
    for (int dup = 1; dup < magnification; ++dup) {
      int target_dy = first_dy + dup;
      if (target_dy >= out_size) break;
      std::memcpy(dst_data + target_dy * dst_stride, first_row_ptr,
                  static_cast<size_t>(out_size) * 4);
    }
  }

  ClearError();
  return out.release();
}

// ---------------------------------------------------------------------------
// Element detection / snapping
// ---------------------------------------------------------------------------

namespace {

void FillElementRect(PixelGrabElementRect* out, int x, int y, int w, int h,
                     const std::string& name, const std::string& role) {
  out->x = x;
  out->y = y;
  out->width = w;
  out->height = h;

  std::memset(out->name, 0, sizeof(out->name));
  if (!name.empty()) {
    size_t len = (std::min)(name.size(), sizeof(out->name) - 1);
    std::memcpy(out->name, name.c_str(), len);
  }

  std::memset(out->role, 0, sizeof(out->role));
  if (!role.empty()) {
    size_t len = (std::min)(role.size(), sizeof(out->role) - 1);
    std::memcpy(out->role, role.c_str(), len);
  }
}

}  // namespace

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

  FillElementRect(out_rect, info.x, info.y, info.width, info.height,
                  info.name, info.role);

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
    FillElementRect(&out_rects[i], infos[i].x, infos[i].y, infos[i].width,
                    infos[i].height, infos[i].name, infos[i].role);
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

  FillElementRect(out_rect, result.snapped_x, result.snapped_y,
                  result.snapped_w, result.snapped_h,
                  result.element.name, result.element.role);

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
  std::lock_guard<std::mutex> lock(mu_);
  if (!initialized_) {
    SetError(kPixelGrabErrorNotInitialized, "Context not initialized");
    return nullptr;
  }

  const HistoryEntry* entry = capture_history_.FindById(history_id);
  if (!entry) {
    SetError(kPixelGrabErrorHistoryEmpty, "History entry not found");
    return nullptr;
  }

  auto stored = capture_history_.GetImageById(history_id);
  if (stored) {
    ClearError();
    return stored.release();
  }

  // Fallback: recapture from screen if no stored image is available.
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
  std::lock_guard<std::mutex> lock(mu_);
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

  auto stored = capture_history_.GetImageById(entry.id);
  if (stored) {
    ClearError();
    return stored.release();
  }

  // Fallback: recapture from screen if no stored image is available.
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
