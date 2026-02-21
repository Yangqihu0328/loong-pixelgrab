// Copyright 2026 The loong-pixelgrab Authors
//
// C++ RAII wrapper for the pixelgrab C API.
// Header-only — just include this file.  Requires C++17 or later.
//
// Usage:
//   #include "pixelgrab/pixelgrab.hpp"
//   pixelgrab::Context ctx;
//   auto img = ctx.CaptureScreen(0);
//   printf("Size: %dx%d\n", img.width(), img.height());

#ifndef PIXELGRAB_PIXELGRAB_HPP_
#define PIXELGRAB_PIXELGRAB_HPP_

#include "pixelgrab/pixelgrab.h"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace pixelgrab {

// ---------------------------------------------------------------------------
// Exception
// ---------------------------------------------------------------------------

class Error : public std::runtime_error {
 public:
  Error(PixelGrabError code, const char* msg)
      : std::runtime_error(msg ? msg : "pixelgrab error"), code_(code) {}
  PixelGrabError code() const noexcept { return code_; }

 private:
  PixelGrabError code_;
};

// ---------------------------------------------------------------------------
// Image  (move-only RAII wrapper)
// ---------------------------------------------------------------------------

class Image {
 public:
  Image() noexcept = default;
  explicit Image(PixelGrabImage* raw) noexcept : raw_(raw) {}
  ~Image() { pixelgrab_image_destroy(raw_); }

  Image(Image&& o) noexcept : raw_(o.raw_) { o.raw_ = nullptr; }
  Image& operator=(Image&& o) noexcept {
    if (this != &o) {
      pixelgrab_image_destroy(raw_);
      raw_ = o.raw_;
      o.raw_ = nullptr;
    }
    return *this;
  }
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  explicit operator bool() const noexcept { return raw_ != nullptr; }
  PixelGrabImage* get() const noexcept { return raw_; }
  PixelGrabImage* release() noexcept {
    auto* p = raw_;
    raw_ = nullptr;
    return p;
  }

  int width() const noexcept { return pixelgrab_image_get_width(raw_); }
  int height() const noexcept { return pixelgrab_image_get_height(raw_); }
  int stride() const noexcept { return pixelgrab_image_get_stride(raw_); }
  PixelGrabPixelFormat format() const noexcept {
    return pixelgrab_image_get_format(raw_);
  }
  const uint8_t* data() const noexcept {
    return pixelgrab_image_get_data(raw_);
  }
  size_t data_size() const noexcept {
    return pixelgrab_image_get_data_size(raw_);
  }

 private:
  PixelGrabImage* raw_ = nullptr;
};

// ---------------------------------------------------------------------------
// ImageView  (non-owning, for annotation get_result)
// ---------------------------------------------------------------------------

class ImageView {
 public:
  ImageView() noexcept = default;
  explicit ImageView(const PixelGrabImage* raw) noexcept : raw_(raw) {}

  explicit operator bool() const noexcept { return raw_ != nullptr; }
  const PixelGrabImage* get() const noexcept { return raw_; }

  int width() const noexcept { return pixelgrab_image_get_width(raw_); }
  int height() const noexcept { return pixelgrab_image_get_height(raw_); }
  int stride() const noexcept { return pixelgrab_image_get_stride(raw_); }
  const uint8_t* data() const noexcept {
    return pixelgrab_image_get_data(raw_);
  }
  size_t data_size() const noexcept {
    return pixelgrab_image_get_data_size(raw_);
  }

 private:
  const PixelGrabImage* raw_ = nullptr;
};

// ---------------------------------------------------------------------------
// Context  (move-only RAII wrapper)
// ---------------------------------------------------------------------------

class Context {
 public:
  Context() : raw_(pixelgrab_context_create()) {
    if (!raw_) throw Error(kPixelGrabErrorNotInitialized, "Context creation failed");
  }
  ~Context() { pixelgrab_context_destroy(raw_); }

  Context(Context&& o) noexcept : raw_(o.raw_) { o.raw_ = nullptr; }
  Context& operator=(Context&& o) noexcept {
    if (this != &o) {
      pixelgrab_context_destroy(raw_);
      raw_ = o.raw_;
      o.raw_ = nullptr;
    }
    return *this;
  }
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  PixelGrabContext* get() const noexcept { return raw_; }

  PixelGrabError last_error() const {
    return pixelgrab_get_last_error(raw_);
  }
  const char* last_error_message() const {
    return pixelgrab_get_last_error_message(raw_);
  }

  // -- Screen info --

  int screen_count() { return pixelgrab_get_screen_count(raw_); }

  PixelGrabScreenInfo screen_info(int index) {
    PixelGrabScreenInfo info = {};
    check(pixelgrab_get_screen_info(raw_, index, &info));
    return info;
  }

  // -- Capture --

  Image CaptureScreen(int screen_index) {
    auto* img = pixelgrab_capture_screen(raw_, screen_index);
    if (!img) throw_last("CaptureScreen failed");
    return Image(img);
  }

  Image CaptureRegion(int x, int y, int w, int h) {
    auto* img = pixelgrab_capture_region(raw_, x, y, w, h);
    if (!img) throw_last("CaptureRegion failed");
    return Image(img);
  }

  Image CaptureWindow(PixelGrabWindowId wid) {
    auto* img = pixelgrab_capture_window(raw_, wid);
    if (!img) throw_last("CaptureWindow failed");
    return Image(img);
  }

  Image CaptureScreenExcludePins(int screen_index) {
    auto* img = pixelgrab_capture_screen_exclude_pins(raw_, screen_index);
    if (!img) throw_last("CaptureScreenExcludePins failed");
    return Image(img);
  }

  Image CaptureRegionExcludePins(int x, int y, int w, int h) {
    auto* img = pixelgrab_capture_region_exclude_pins(raw_, x, y, w, h);
    if (!img) throw_last("CaptureRegionExcludePins failed");
    return Image(img);
  }

  // -- Window enumeration --

  std::vector<PixelGrabWindowInfo> EnumerateWindows(int max_count = 128) {
    std::vector<PixelGrabWindowInfo> buf(max_count);
    int n = pixelgrab_enumerate_windows(raw_, buf.data(), max_count);
    if (n < 0) n = 0;
    buf.resize(n);
    return buf;
  }

  // -- DPI --

  void EnableDpiAwareness() {
    check(pixelgrab_enable_dpi_awareness(raw_));
  }

  PixelGrabDpiInfo dpi_info(int screen_index) {
    PixelGrabDpiInfo info = {};
    check(pixelgrab_get_dpi_info(raw_, screen_index, &info));
    return info;
  }

  // -- Color picker --

  PixelGrabColor PickColor(int x, int y) {
    PixelGrabColor c = {};
    check(pixelgrab_pick_color(raw_, x, y, &c));
    return c;
  }

  Image GetMagnifier(int x, int y, int radius, int magnification) {
    auto* img = pixelgrab_get_magnifier(raw_, x, y, radius, magnification);
    if (!img) throw_last("GetMagnifier failed");
    return Image(img);
  }

  // -- Element detection --

  PixelGrabElementRect DetectElement(int x, int y) {
    PixelGrabElementRect r = {};
    check(pixelgrab_detect_element(raw_, x, y, &r));
    return r;
  }

  PixelGrabElementRect SnapToElement(int x, int y, int snap_dist) {
    PixelGrabElementRect r = {};
    check(pixelgrab_snap_to_element(raw_, x, y, snap_dist, &r));
    return r;
  }

  // -- Capture history --

  int history_count() { return pixelgrab_history_count(raw_); }

  PixelGrabHistoryEntry history_entry(int index) {
    PixelGrabHistoryEntry e = {};
    check(pixelgrab_history_get_entry(raw_, index, &e));
    return e;
  }

  Image HistoryRecapture(int history_id) {
    auto* img = pixelgrab_history_recapture(raw_, history_id);
    if (!img) throw_last("HistoryRecapture failed");
    return Image(img);
  }

  Image RecaptureLast() {
    auto* img = pixelgrab_recapture_last(raw_);
    if (!img) throw_last("RecaptureLast failed");
    return Image(img);
  }

  void HistoryClear() { pixelgrab_history_clear(raw_); }
  void HistorySetMaxCount(int n) { pixelgrab_history_set_max_count(raw_, n); }

  // -- Clipboard --

  PixelGrabClipboardFormat clipboard_format() {
    return pixelgrab_clipboard_get_format(raw_);
  }

  Image ClipboardGetImage() {
    return Image(pixelgrab_clipboard_get_image(raw_));
  }

  std::string ClipboardGetText() {
    char* s = pixelgrab_clipboard_get_text(raw_);
    if (!s) return {};
    std::string result(s);
    pixelgrab_free_string(s);
    return result;
  }

  // -- OCR --

  bool ocr_supported() { return pixelgrab_ocr_is_supported(raw_) != 0; }

  std::string OcrRecognize(const Image& img, const char* language = nullptr) {
    char* text = nullptr;
    check(pixelgrab_ocr_recognize(raw_, img.get(), language, &text));
    if (!text) return {};
    std::string result(text);
    pixelgrab_free_string(text);
    return result;
  }

  // -- Translation --

  void TranslateSetConfig(const char* provider, const char* app_id,
                          const char* secret_key) {
    check(pixelgrab_translate_set_config(raw_, provider, app_id, secret_key));
  }

  bool translate_supported() {
    return pixelgrab_translate_is_supported(raw_) != 0;
  }

  std::string Translate(const char* text, const char* from, const char* to) {
    char* result = nullptr;
    check(pixelgrab_translate_text(raw_, text, from, to, &result));
    if (!result) return {};
    std::string s(result);
    pixelgrab_free_string(result);
    return s;
  }

  // -- Pin windows --

  int pin_count() { return pixelgrab_pin_count(raw_); }
  int PinProcessEvents() { return pixelgrab_pin_process_events(raw_); }
  void PinDestroyAll() { pixelgrab_pin_destroy_all(raw_); }

  void PinSetVisibleAll(bool visible) {
    check(pixelgrab_pin_set_visible_all(raw_, visible ? 1 : 0));
  }

  // -- Watermark --

  bool watermark_supported() {
    return pixelgrab_watermark_is_supported(raw_) != 0;
  }

  void WatermarkApplyText(Image& img,
                          const PixelGrabTextWatermarkConfig& config) {
    check(pixelgrab_watermark_apply_text(raw_, img.get(), &config));
  }

  void WatermarkApplyImage(Image& target, const Image& watermark, int x,
                           int y, float opacity) {
    check(pixelgrab_watermark_apply_image(raw_, target.get(), watermark.get(),
                                          x, y, opacity));
  }

  // -- Audio --

  bool audio_supported() {
    return pixelgrab_audio_is_supported(raw_) != 0;
  }

 private:
  void check(PixelGrabError err) {
    if (err != kPixelGrabOk)
      throw Error(err, pixelgrab_get_last_error_message(raw_));
  }
  [[noreturn]] void throw_last(const char* fallback) {
    auto err = pixelgrab_get_last_error(raw_);
    const char* msg = pixelgrab_get_last_error_message(raw_);
    throw Error(err != kPixelGrabOk ? err : kPixelGrabErrorUnknown,
                (msg && msg[0]) ? msg : fallback);
  }

  PixelGrabContext* raw_ = nullptr;
};

// ---------------------------------------------------------------------------
// Annotation  (move-only RAII wrapper)
// ---------------------------------------------------------------------------

class Annotation {
 public:
  Annotation(Context& ctx, const Image& base)
      : raw_(pixelgrab_annotation_create(ctx.get(), base.get())),
        ctx_(&ctx) {
    if (!raw_)
      throw Error(ctx.last_error(), ctx.last_error_message());
  }
  ~Annotation() { pixelgrab_annotation_destroy(raw_); }

  Annotation(Annotation&& o) noexcept : raw_(o.raw_), ctx_(o.ctx_) {
    o.raw_ = nullptr;
  }
  Annotation& operator=(Annotation&& o) noexcept {
    if (this != &o) {
      pixelgrab_annotation_destroy(raw_);
      raw_ = o.raw_;
      ctx_ = o.ctx_;
      o.raw_ = nullptr;
    }
    return *this;
  }
  Annotation(const Annotation&) = delete;
  Annotation& operator=(const Annotation&) = delete;

  PixelGrabAnnotation* get() const noexcept { return raw_; }

  int AddRect(int x, int y, int w, int h, const PixelGrabShapeStyle& s) {
    return pixelgrab_annotation_add_rect(raw_, x, y, w, h, &s);
  }
  int AddEllipse(int cx, int cy, int rx, int ry,
                 const PixelGrabShapeStyle& s) {
    return pixelgrab_annotation_add_ellipse(raw_, cx, cy, rx, ry, &s);
  }
  int AddLine(int x1, int y1, int x2, int y2,
              const PixelGrabShapeStyle& s) {
    return pixelgrab_annotation_add_line(raw_, x1, y1, x2, y2, &s);
  }
  int AddArrow(int x1, int y1, int x2, int y2, float head,
               const PixelGrabShapeStyle& s) {
    return pixelgrab_annotation_add_arrow(raw_, x1, y1, x2, y2, head, &s);
  }
  int AddPencil(const int* pts, int count, const PixelGrabShapeStyle& s) {
    return pixelgrab_annotation_add_pencil(raw_, pts, count, &s);
  }
  int AddText(int x, int y, const char* text, const char* font, int size,
              uint32_t color) {
    return pixelgrab_annotation_add_text(raw_, x, y, text, font, size, color);
  }
  int AddMosaic(int x, int y, int w, int h, int block_size) {
    return pixelgrab_annotation_add_mosaic(raw_, x, y, w, h, block_size);
  }
  int AddBlur(int x, int y, int w, int h, int radius) {
    return pixelgrab_annotation_add_blur(raw_, x, y, w, h, radius);
  }

  void RemoveShape(int id) {
    auto err = pixelgrab_annotation_remove_shape(raw_, id);
    if (err != kPixelGrabOk)
      throw Error(err, ctx_->last_error_message());
  }

  void Undo() {
    auto err = pixelgrab_annotation_undo(raw_);
    if (err != kPixelGrabOk)
      throw Error(err, ctx_->last_error_message());
  }
  void Redo() {
    auto err = pixelgrab_annotation_redo(raw_);
    if (err != kPixelGrabOk)
      throw Error(err, ctx_->last_error_message());
  }
  bool can_undo() const { return pixelgrab_annotation_can_undo(raw_) != 0; }
  bool can_redo() const { return pixelgrab_annotation_can_redo(raw_) != 0; }

  ImageView GetResult() {
    return ImageView(pixelgrab_annotation_get_result(raw_));
  }

  Image Export() {
    auto* img = pixelgrab_annotation_export(raw_);
    if (!img) throw Error(ctx_->last_error(), ctx_->last_error_message());
    return Image(img);
  }

 private:
  PixelGrabAnnotation* raw_ = nullptr;
  Context* ctx_ = nullptr;
};

// ---------------------------------------------------------------------------
// PinWindow  (move-only RAII wrapper)
// ---------------------------------------------------------------------------

class PinWindow {
 public:
  static PinWindow FromImage(Context& ctx, const Image& img, int x, int y) {
    auto* p = pixelgrab_pin_image(ctx.get(), img.get(), x, y);
    if (!p) throw Error(ctx.last_error(), ctx.last_error_message());
    return PinWindow(p);
  }

  static PinWindow FromText(Context& ctx, const char* text, int x, int y) {
    auto* p = pixelgrab_pin_text(ctx.get(), text, x, y);
    if (!p) throw Error(ctx.last_error(), ctx.last_error_message());
    return PinWindow(p);
  }

  static PinWindow FromClipboard(Context& ctx, int x, int y) {
    auto* p = pixelgrab_pin_clipboard(ctx.get(), x, y);
    if (!p) throw Error(ctx.last_error(), ctx.last_error_message());
    return PinWindow(p);
  }

  ~PinWindow() { pixelgrab_pin_destroy(raw_); }

  PinWindow(PinWindow&& o) noexcept : raw_(o.raw_) { o.raw_ = nullptr; }
  PinWindow& operator=(PinWindow&& o) noexcept {
    if (this != &o) {
      pixelgrab_pin_destroy(raw_);
      raw_ = o.raw_;
      o.raw_ = nullptr;
    }
    return *this;
  }
  PinWindow(const PinWindow&) = delete;
  PinWindow& operator=(const PinWindow&) = delete;

  PixelGrabPinWindow* get() const noexcept { return raw_; }

  void set_opacity(float v) { pixelgrab_pin_set_opacity(raw_, v); }
  float opacity() const { return pixelgrab_pin_get_opacity(raw_); }
  void set_position(int x, int y) { pixelgrab_pin_set_position(raw_, x, y); }
  void set_size(int w, int h) { pixelgrab_pin_set_size(raw_, w, h); }
  void set_visible(bool v) { pixelgrab_pin_set_visible(raw_, v ? 1 : 0); }

  PixelGrabPinInfo info() {
    PixelGrabPinInfo i = {};
    pixelgrab_pin_get_info(raw_, &i);
    return i;
  }

  Image get_image() { return Image(pixelgrab_pin_get_image(raw_)); }

 private:
  explicit PinWindow(PixelGrabPinWindow* raw) noexcept : raw_(raw) {}
  PixelGrabPinWindow* raw_ = nullptr;
};

// ---------------------------------------------------------------------------
// Free functions (color utilities)
// ---------------------------------------------------------------------------

inline PixelGrabColorHsv to_hsv(const PixelGrabColor& rgb) {
  PixelGrabColorHsv hsv = {};
  pixelgrab_color_rgb_to_hsv(&rgb, &hsv);
  return hsv;
}

inline PixelGrabColor to_rgb(const PixelGrabColorHsv& hsv) {
  PixelGrabColor rgb = {};
  pixelgrab_color_hsv_to_rgb(&hsv, &rgb);
  return rgb;
}

inline std::string to_hex(const PixelGrabColor& c, bool alpha = false) {
  char buf[16] = {};
  pixelgrab_color_to_hex(&c, buf, sizeof(buf), alpha ? 1 : 0);
  return buf;
}

inline PixelGrabColor from_hex(const char* hex) {
  PixelGrabColor c = {};
  auto err = pixelgrab_color_from_hex(hex, &c);
  if (err != kPixelGrabOk) throw Error(err, "Invalid hex color");
  return c;
}

inline const char* version_string() { return pixelgrab_version_string(); }

}  // namespace pixelgrab

#endif  // PIXELGRAB_PIXELGRAB_HPP_
