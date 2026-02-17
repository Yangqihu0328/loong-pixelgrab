// Copyright 2024 PixelGrab Authors. All rights reserved.
// Multi-pin-window manager.

#ifndef PIXELGRAB_PIN_PIN_WINDOW_MANAGER_H_
#define PIXELGRAB_PIN_PIN_WINDOW_MANAGER_H_

#include <map>
#include <memory>

#include "pin/clipboard_reader.h"
#include "pin/pin_window_backend.h"

namespace pixelgrab {
namespace internal {

/// Manages multiple floating pin windows.
class PinWindowManager {
 public:
  PinWindowManager();
  ~PinWindowManager();

  // -- Create pin windows (return pin_id > 0 on success, 0 on failure) --

  int PinImage(const Image* image, int x, int y);
  int PinText(const char* text, int x, int y);
  int PinClipboard(ClipboardReader* clipboard, int x, int y);

  // -- Per-window operations --

  bool SetOpacity(int pin_id, float opacity);
  float GetOpacity(int pin_id) const;
  bool SetPosition(int pin_id, int x, int y);
  bool SetSize(int pin_id, int width, int height);
  bool SetVisible(int pin_id, bool visible);
  void DestroyPin(int pin_id);

  // -- Content access --

  /// Get a copy of the image content for a pin.  Returns nullptr for text
  /// pins or if the pin_id is invalid.
  std::unique_ptr<Image> GetImage(int pin_id) const;

  /// Replace the image content of an existing image-type pin.
  bool SetImage(int pin_id, const Image* image);

  // -- Enumeration & information --

  /// Fill out_ids with active pin IDs.  Returns the number written.
  int Enumerate(int* out_ids, int max_count) const;

  /// Query position, size, opacity, visibility, content type for a pin.
  bool GetInfo(int pin_id, int* out_x, int* out_y, int* out_w, int* out_h,
               float* out_opacity, bool* out_visible,
               PinContentType* out_type) const;

  // -- Multi-pin operations --

  /// Show or hide every pin window at once.
  void SetVisibleAll(bool visible);

  /// Duplicate pin_id as a new window offset by (dx, dy).
  /// Returns the new pin_id, or 0 on failure.
  int Duplicate(int pin_id, int dx, int dy);

  // -- Global operations --

  void DestroyAll();
  int Count() const;

  /// Process events for all windows. Auto-cleans closed windows.
  /// Returns the number of remaining active windows.
  int ProcessEvents();

  // -- Per-ID backend access (for C API wrappers) --

  PinWindowBackend* GetBackend(int pin_id);
  const PinWindowBackend* GetBackend(int pin_id) const;

 private:
  struct PinEntry {
    std::unique_ptr<PinWindowBackend> backend;
    PinContentType content_type;
  };

  std::map<int, PinEntry> windows_;
  int next_id_ = 1;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PIN_PIN_WINDOW_MANAGER_H_
