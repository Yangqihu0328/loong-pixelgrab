// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_CORE_PIXELGRAB_CONTEXT_H_
#define PIXELGRAB_CORE_PIXELGRAB_CONTEXT_H_

#include <memory>
#include <string>
#include <vector>

#include "core/audio_backend.h"
#include "core/capture_backend.h"
#include "core/capture_history.h"
#include "core/color_utils.h"
#include "core/image.h"
#include "detection/element_detector.h"
#include "detection/snap_engine.h"
#include "pin/clipboard_reader.h"
#include "pin/pin_window_manager.h"
#include "pixelgrab/pixelgrab.h"
#include "watermark/watermark_renderer.h"

namespace pixelgrab {
namespace internal {

/// Internal implementation of the opaque PixelGrabContext handle.
///
/// Owns the platform backend and provides the bridge between the public C API
/// and the internal C++ implementation.
class PixelGrabContextImpl {
 public:
  PixelGrabContextImpl();
  ~PixelGrabContextImpl();

  // Non-copyable.
  PixelGrabContextImpl(const PixelGrabContextImpl&) = delete;
  PixelGrabContextImpl& operator=(const PixelGrabContextImpl&) = delete;

  /// Initialize the context and its backend.
  bool Initialize();

  /// Check if the context has been successfully initialized.
  bool is_initialized() const { return initialized_; }

  // -- Error state --

  PixelGrabError last_error() const { return last_error_; }
  const char* last_error_message() const { return last_error_message_.c_str(); }

  void SetError(PixelGrabError code, const std::string& message);
  void ClearError();

  // -- Screen information --

  int GetScreenCount();
  PixelGrabError GetScreenInfo(int screen_index, PixelGrabScreenInfo* out_info);

  // -- Capture operations --

  /// Capture a screen. Returns a new Image, or nullptr on failure.
  /// The returned Image is owned by the caller (via C API destroy).
  Image* CaptureScreen(int screen_index);
  Image* CaptureRegion(int x, int y, int width, int height);
  Image* CaptureWindow(uint64_t window_handle);

  // -- Window enumeration --

  int EnumerateWindows(PixelGrabWindowInfo* out_windows, int max_count);

  // -- DPI support --

  PixelGrabError EnableDpiAwareness();
  PixelGrabError GetDpiInfo(int screen_index, PixelGrabDpiInfo* out_info);
  PixelGrabError LogicalToPhysical(int screen_index, int logical_x,
                                   int logical_y, int* out_physical_x,
                                   int* out_physical_y);
  PixelGrabError PhysicalToLogical(int screen_index, int physical_x,
                                   int physical_y, int* out_logical_x,
                                   int* out_logical_y);

  // -- Color picker --

  PixelGrabError PickColor(int x, int y, PixelGrabColor* out_color);

  // -- Magnifier --

  Image* GetMagnifier(int x, int y, int radius, int magnification);

  // -- Element detection / snapping --

  PixelGrabError DetectElement(int x, int y, PixelGrabElementRect* out_rect);
  int DetectElements(int x, int y, PixelGrabElementRect* out_rects,
                     int max_count);
  PixelGrabError SnapToElement(int x, int y, int snap_distance,
                               PixelGrabElementRect* out_rect);

  // -- Capture history --

  int HistoryCount() const;
  PixelGrabError HistoryGetEntry(int index, PixelGrabHistoryEntry* out_entry);
  Image* HistoryRecapture(int history_id);
  Image* RecaptureLast();
  void HistoryClear();
  void HistorySetMaxCount(int max_count);

  // -- Pin windows --

  PinWindowManager& pin_manager() { return pin_manager_; }
  const PinWindowManager& pin_manager() const { return pin_manager_; }

  // -- Clipboard --

  ClipboardReader* clipboard_reader();

  // -- Watermark --

  /// Get the watermark renderer (lazy-initialized).
  WatermarkRenderer* watermark_renderer();

  // -- Audio --

  /// Get the audio backend (lazy-initialized).
  AudioBackend* audio_backend();

  // -- Recorder support --

  /// Get the capture backend (used by Recorder for frame capture).
  CaptureBackend* capture_backend() { return backend_.get(); }

 private:
  /// Ensure screens cache is populated.
  void RefreshScreens();

  std::unique_ptr<CaptureBackend> backend_;
  std::vector<PixelGrabScreenInfo> cached_screens_;
  bool initialized_ = false;

  // Element detection.
  std::unique_ptr<ElementDetector> element_detector_;
  std::unique_ptr<SnapEngine> snap_engine_;

  // Capture history.
  CaptureHistory capture_history_;

  // Pin window management.
  PinWindowManager pin_manager_;

  // Clipboard reader (lazy-initialized).
  std::unique_ptr<ClipboardReader> clipboard_reader_;

  // Watermark renderer (lazy-initialized).
  std::unique_ptr<WatermarkRenderer> watermark_renderer_;

  // Audio backend (lazy-initialized).
  std::unique_ptr<AudioBackend> audio_backend_;

  // Error state (per-context, so thread-safe across contexts).
  PixelGrabError last_error_ = kPixelGrabOk;
  std::string last_error_message_ = "No error";
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_CORE_PIXELGRAB_CONTEXT_H_
