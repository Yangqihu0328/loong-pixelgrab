// Copyright 2026 The loong-pixelgrab Authors
//
// This file implements all public C API functions declared in pixelgrab.h.
// It bridges the extern "C" interface to the internal C++ implementation.

#include "pixelgrab/pixelgrab.h"

#include <chrono>
#include <new>
#include <thread>
#include <utility>

#include <cstdlib>
#include <cstring>

#include "annotation/annotation_renderer.h"
#include "annotation/annotation_session.h"
#include "annotation/shape.h"
#include "core/audio_backend.h"
#include "core/callback_sink.h"
#include "core/color_utils.h"
#include "core/image.h"
#include "core/logger.h"
#include "core/pixelgrab_context.h"
#include "core/recorder_backend.h"
#include "pin/pin_window_manager.h"
#include "watermark/watermark_renderer.h"

using pixelgrab::internal::AnnotationRenderer;
using pixelgrab::internal::AnnotationSession;
using pixelgrab::internal::Image;
using pixelgrab::internal::PinWindowManager;
using pixelgrab::internal::PixelGrabContextImpl;
using pixelgrab::internal::RecorderBackend;
using pixelgrab::internal::RecordConfig;
using pixelgrab::internal::RecordState;
using pixelgrab::internal::ShapeStyle;
using pixelgrab::internal::WatermarkRenderer;

// ---------------------------------------------------------------------------
// The opaque PixelGrabContext struct wraps the C++ implementation.
// ---------------------------------------------------------------------------
struct PixelGrabContext {
  PixelGrabContextImpl impl;
};

// ---------------------------------------------------------------------------
// The opaque PixelGrabImage struct wraps the C++ Image object.
// ---------------------------------------------------------------------------
struct PixelGrabImage {
  std::unique_ptr<Image> impl;

  explicit PixelGrabImage(Image* raw) : impl(raw) {}
};

// Wrap a raw Image* into a heap-allocated PixelGrabImage*.
static PixelGrabImage* WrapImage(Image* raw) {
  if (!raw) return nullptr;
  return new (std::nothrow) PixelGrabImage(raw);
}

// ---------------------------------------------------------------------------
// The opaque PixelGrabAnnotation struct wraps AnnotationSession.
// ---------------------------------------------------------------------------
struct PixelGrabAnnotation {
  PixelGrabContext* ctx;  // Parent context for error reporting (non-owning).
  std::unique_ptr<AnnotationSession> session;
  PixelGrabImage result_view{nullptr};  // Non-owning wrapper for get_result.

  ~PixelGrabAnnotation() {
    result_view.impl.release();
  }
};

// ---------------------------------------------------------------------------
// The opaque PixelGrabPinWindow struct wraps a pin_id + context pointer.
// ---------------------------------------------------------------------------
struct PixelGrabPinWindow {
  PixelGrabContext* ctx;  // Parent context (non-owning).
  int pin_id;             // Manager-assigned ID.
};

// Convert public PixelGrabShapeStyle to internal ShapeStyle.
static ShapeStyle ToInternal(const PixelGrabShapeStyle* s) {
  ShapeStyle out = {};
  if (s) {
    out.stroke_color = s->stroke_color;
    out.fill_color = s->fill_color;
    out.stroke_width = s->stroke_width;
    out.filled = s->filled != 0;
  }
  return out;
}

// ---------------------------------------------------------------------------
// Context management
// ---------------------------------------------------------------------------

PixelGrabContext* pixelgrab_context_create(void) {
  auto* ctx = new (std::nothrow) PixelGrabContext();
  if (!ctx) return nullptr;

  if (!ctx->impl.Initialize()) {
    delete ctx;
    return nullptr;
  }
  return ctx;
}

void pixelgrab_context_destroy(PixelGrabContext* ctx) {
  delete ctx;
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

PixelGrabError pixelgrab_get_last_error(const PixelGrabContext* ctx) {
  if (!ctx) return kPixelGrabErrorInvalidParam;
  return ctx->impl.last_error();
}

const char* pixelgrab_get_last_error_message(const PixelGrabContext* ctx) {
  if (!ctx) return "Invalid context (NULL)";
  return ctx->impl.last_error_message();
}

// ---------------------------------------------------------------------------
// Screen / monitor information
// ---------------------------------------------------------------------------

int pixelgrab_get_screen_count(PixelGrabContext* ctx) {
  if (!ctx) return -1;
  return ctx->impl.GetScreenCount();
}

PixelGrabError pixelgrab_get_screen_info(PixelGrabContext* ctx,
                                         int screen_index,
                                         PixelGrabScreenInfo* out_info) {
  if (!ctx) return kPixelGrabErrorInvalidParam;
  return ctx->impl.GetScreenInfo(screen_index, out_info);
}

// ---------------------------------------------------------------------------
// Capture operations
// ---------------------------------------------------------------------------

PixelGrabImage* pixelgrab_capture_screen(PixelGrabContext* ctx,
                                         int screen_index) {
  if (!ctx) return nullptr;
  return WrapImage(ctx->impl.CaptureScreen(screen_index));
}

PixelGrabImage* pixelgrab_capture_region(PixelGrabContext* ctx, int x, int y,
                                         int width, int height) {
  if (!ctx) return nullptr;
  return WrapImage(ctx->impl.CaptureRegion(x, y, width, height));
}

PixelGrabImage* pixelgrab_capture_window(PixelGrabContext* ctx,
                                         PixelGrabWindowId window_id) {
  if (!ctx) return nullptr;
  return WrapImage(ctx->impl.CaptureWindow(window_id));
}

// ---------------------------------------------------------------------------
// Window enumeration
// ---------------------------------------------------------------------------

int pixelgrab_enumerate_windows(PixelGrabContext* ctx,
                                PixelGrabWindowInfo* out_windows,
                                int max_count) {
  if (!ctx) return -1;
  return ctx->impl.EnumerateWindows(out_windows, max_count);
}

// ---------------------------------------------------------------------------
// Image accessors
// ---------------------------------------------------------------------------

int pixelgrab_image_get_width(const PixelGrabImage* image) {
  if (!image || !image->impl) return 0;
  return image->impl->width();
}

int pixelgrab_image_get_height(const PixelGrabImage* image) {
  if (!image || !image->impl) return 0;
  return image->impl->height();
}

int pixelgrab_image_get_stride(const PixelGrabImage* image) {
  if (!image || !image->impl) return 0;
  return image->impl->stride();
}

PixelGrabPixelFormat pixelgrab_image_get_format(const PixelGrabImage* image) {
  if (!image || !image->impl) return kPixelGrabFormatBgra8;
  return image->impl->format();
}

const uint8_t* pixelgrab_image_get_data(const PixelGrabImage* image) {
  if (!image || !image->impl) return nullptr;
  return image->impl->data();
}

size_t pixelgrab_image_get_data_size(const PixelGrabImage* image) {
  if (!image || !image->impl) return 0;
  return image->impl->data_size();
}

void pixelgrab_image_destroy(PixelGrabImage* image) {
  delete image;
}

// ---------------------------------------------------------------------------
// DPI awareness
// ---------------------------------------------------------------------------

PixelGrabError pixelgrab_enable_dpi_awareness(PixelGrabContext* ctx) {
  if (!ctx) return kPixelGrabErrorInvalidParam;
  return ctx->impl.EnableDpiAwareness();
}

PixelGrabError pixelgrab_get_dpi_info(PixelGrabContext* ctx, int screen_index,
                                      PixelGrabDpiInfo* out_info) {
  if (!ctx) return kPixelGrabErrorInvalidParam;
  return ctx->impl.GetDpiInfo(screen_index, out_info);
}

PixelGrabError pixelgrab_logical_to_physical(PixelGrabContext* ctx,
                                             int screen_index, int logical_x,
                                             int logical_y,
                                             int* out_physical_x,
                                             int* out_physical_y) {
  if (!ctx) return kPixelGrabErrorInvalidParam;
  return ctx->impl.LogicalToPhysical(screen_index, logical_x, logical_y,
                                     out_physical_x, out_physical_y);
}

PixelGrabError pixelgrab_physical_to_logical(PixelGrabContext* ctx,
                                             int screen_index, int physical_x,
                                             int physical_y,
                                             int* out_logical_x,
                                             int* out_logical_y) {
  if (!ctx) return kPixelGrabErrorInvalidParam;
  return ctx->impl.PhysicalToLogical(screen_index, physical_x, physical_y,
                                     out_logical_x, out_logical_y);
}

// ---------------------------------------------------------------------------
// Color picker
// ---------------------------------------------------------------------------

PixelGrabError pixelgrab_pick_color(PixelGrabContext* ctx, int x, int y,
                                    PixelGrabColor* out_color) {
  if (!ctx) return kPixelGrabErrorInvalidParam;
  return ctx->impl.PickColor(x, y, out_color);
}

void pixelgrab_color_rgb_to_hsv(const PixelGrabColor* rgb,
                                PixelGrabColorHsv* out_hsv) {
  if (!rgb || !out_hsv) return;
  pixelgrab::internal::RgbToHsv(*rgb, out_hsv);
}

void pixelgrab_color_hsv_to_rgb(const PixelGrabColorHsv* hsv,
                                PixelGrabColor* out_rgb) {
  if (!hsv || !out_rgb) return;
  pixelgrab::internal::HsvToRgb(*hsv, out_rgb);
}

void pixelgrab_color_to_hex(const PixelGrabColor* color, char* buf,
                            int buf_size, int include_alpha) {
  if (!color || !buf || buf_size <= 0) return;
  pixelgrab::internal::ColorToHex(*color, buf, buf_size, include_alpha != 0);
}

PixelGrabError pixelgrab_color_from_hex(const char* hex,
                                        PixelGrabColor* out_color) {
  if (!hex || !out_color) return kPixelGrabErrorInvalidParam;
  if (pixelgrab::internal::ColorFromHex(hex, out_color)) {
    return kPixelGrabOk;
  }
  return kPixelGrabErrorInvalidParam;
}

PixelGrabImage* pixelgrab_get_magnifier(PixelGrabContext* ctx, int x, int y,
                                        int radius, int magnification) {
  if (!ctx) return nullptr;
  return WrapImage(ctx->impl.GetMagnifier(x, y, radius, magnification));
}

// ---------------------------------------------------------------------------
// Annotation engine
// ---------------------------------------------------------------------------

PixelGrabAnnotation* pixelgrab_annotation_create(PixelGrabContext* ctx,
                                                 const PixelGrabImage* base_image) {
  if (!ctx) return nullptr;
  if (!base_image || !base_image->impl) {
    ctx->impl.SetError(kPixelGrabErrorInvalidParam,
                       "base_image is NULL or empty");
    return nullptr;
  }

  const Image* src = base_image->impl.get();
  std::vector<uint8_t> data(src->data(), src->data() + src->data_size());
  auto base_copy = Image::CreateFromData(src->width(), src->height(),
                                         src->stride(), src->format(),
                                         std::move(data));
  if (!base_copy) {
    ctx->impl.SetError(kPixelGrabErrorOutOfMemory,
                       "Failed to copy base image for annotation");
    return nullptr;
  }

  auto renderer = pixelgrab::internal::CreatePlatformAnnotationRenderer();

  auto* ann = new (std::nothrow) PixelGrabAnnotation();
  if (!ann) {
    ctx->impl.SetError(kPixelGrabErrorOutOfMemory,
                       "Failed to allocate annotation session");
    return nullptr;
  }

  ann->ctx = ctx;
  ann->session = std::make_unique<AnnotationSession>(std::move(base_copy),
                                                     std::move(renderer));
  ctx->impl.ClearError();
  return ann;
}

void pixelgrab_annotation_destroy(PixelGrabAnnotation* ann) {
  delete ann;
}

int pixelgrab_annotation_add_rect(PixelGrabAnnotation* ann, int x, int y,
                                  int width, int height,
                                  const PixelGrabShapeStyle* style) {
  if (!ann || !ann->session) return -1;
  if (width <= 0 || height <= 0) {
    if (ann->ctx)
      ann->ctx->impl.SetError(kPixelGrabErrorInvalidParam,
                               "Rectangle width and height must be positive");
    return -1;
  }
  auto shape = std::make_unique<pixelgrab::internal::RectShape>(
      x, y, width, height, ToInternal(style));
  return ann->session->AddShape(std::move(shape));
}

int pixelgrab_annotation_add_ellipse(PixelGrabAnnotation* ann, int cx, int cy,
                                     int rx, int ry,
                                     const PixelGrabShapeStyle* style) {
  if (!ann || !ann->session) return -1;
  if (rx <= 0 || ry <= 0) {
    if (ann->ctx)
      ann->ctx->impl.SetError(kPixelGrabErrorInvalidParam,
                               "Ellipse radii must be positive");
    return -1;
  }
  auto shape = std::make_unique<pixelgrab::internal::EllipseShape>(
      cx, cy, rx, ry, ToInternal(style));
  return ann->session->AddShape(std::move(shape));
}

int pixelgrab_annotation_add_line(PixelGrabAnnotation* ann, int x1, int y1,
                                  int x2, int y2,
                                  const PixelGrabShapeStyle* style) {
  if (!ann || !ann->session) return -1;
  auto shape = std::make_unique<pixelgrab::internal::LineShape>(
      x1, y1, x2, y2, ToInternal(style));
  return ann->session->AddShape(std::move(shape));
}

int pixelgrab_annotation_add_arrow(PixelGrabAnnotation* ann, int x1, int y1,
                                   int x2, int y2, float head_size,
                                   const PixelGrabShapeStyle* style) {
  if (!ann || !ann->session) return -1;
  auto shape = std::make_unique<pixelgrab::internal::ArrowShape>(
      x1, y1, x2, y2, head_size, ToInternal(style));
  return ann->session->AddShape(std::move(shape));
}

int pixelgrab_annotation_add_pencil(PixelGrabAnnotation* ann,
                                    const int* points, int point_count,
                                    const PixelGrabShapeStyle* style) {
  if (!ann || !ann->session) return -1;
  if (!points || point_count < 2) {
    if (ann->ctx)
      ann->ctx->impl.SetError(kPixelGrabErrorInvalidParam,
                               "Pencil requires non-NULL points with count>=2");
    return -1;
  }
  static constexpr int kMaxPencilPoints = 100000;
  if (point_count > kMaxPencilPoints) {
    if (ann->ctx)
      ann->ctx->impl.SetError(kPixelGrabErrorInvalidParam,
                               "Pencil point_count exceeds maximum (100000)");
    return -1;
  }
  std::vector<pixelgrab::internal::Point> pts(point_count);
  for (int i = 0; i < point_count; ++i) {
    pts[i].x = points[i * 2];
    pts[i].y = points[i * 2 + 1];
  }
  auto shape = std::make_unique<pixelgrab::internal::PencilShape>(
      std::move(pts), ToInternal(style));
  return ann->session->AddShape(std::move(shape));
}

int pixelgrab_annotation_add_text(PixelGrabAnnotation* ann, int x, int y,
                                  const char* text, const char* font_name,
                                  int font_size, uint32_t color) {
  if (!ann || !ann->session) return -1;
  if (!text) {
    if (ann->ctx)
      ann->ctx->impl.SetError(kPixelGrabErrorInvalidParam,
                               "Annotation text must not be NULL");
    return -1;
  }
  if (font_size <= 0) font_size = 16;
  auto shape = std::make_unique<pixelgrab::internal::TextShape>(
      x, y, text, font_name ? font_name : "Arial", font_size, color);
  return ann->session->AddShape(std::move(shape));
}

int pixelgrab_annotation_add_mosaic(PixelGrabAnnotation* ann, int x, int y,
                                    int width, int height, int block_size) {
  if (!ann || !ann->session) return -1;
  if (width <= 0 || height <= 0 || block_size <= 0) {
    if (ann->ctx)
      ann->ctx->impl.SetError(
          kPixelGrabErrorInvalidParam,
          "Mosaic width, height, and block_size must be positive");
    return -1;
  }
  auto shape = std::make_unique<pixelgrab::internal::MosaicEffect>(
      x, y, width, height, block_size);
  return ann->session->AddShape(std::move(shape));
}

int pixelgrab_annotation_add_blur(PixelGrabAnnotation* ann, int x, int y,
                                  int width, int height, int radius) {
  if (!ann || !ann->session) return -1;
  if (width <= 0 || height <= 0 || radius <= 0) {
    if (ann->ctx)
      ann->ctx->impl.SetError(
          kPixelGrabErrorInvalidParam,
          "Blur width, height, and radius must be positive");
    return -1;
  }
  auto shape = std::make_unique<pixelgrab::internal::BlurEffect>(
      x, y, width, height, radius);
  return ann->session->AddShape(std::move(shape));
}

PixelGrabError pixelgrab_annotation_remove_shape(PixelGrabAnnotation* ann,
                                                 int shape_id) {
  if (!ann || !ann->session) return kPixelGrabErrorInvalidParam;
  if (ann->session->RemoveShape(shape_id) < 0) {
    if (ann->ctx)
      ann->ctx->impl.SetError(kPixelGrabErrorInvalidParam,
                               "Invalid shape_id for removal");
    return kPixelGrabErrorInvalidParam;
  }
  return kPixelGrabOk;
}

PixelGrabError pixelgrab_annotation_undo(PixelGrabAnnotation* ann) {
  if (!ann || !ann->session) return kPixelGrabErrorInvalidParam;
  if (!ann->session->Undo()) {
    if (ann->ctx)
      ann->ctx->impl.SetError(kPixelGrabErrorAnnotationFailed,
                               "Nothing to undo");
    return kPixelGrabErrorAnnotationFailed;
  }
  return kPixelGrabOk;
}

PixelGrabError pixelgrab_annotation_redo(PixelGrabAnnotation* ann) {
  if (!ann || !ann->session) return kPixelGrabErrorInvalidParam;
  if (!ann->session->Redo()) {
    if (ann->ctx)
      ann->ctx->impl.SetError(kPixelGrabErrorAnnotationFailed,
                               "Nothing to redo");
    return kPixelGrabErrorAnnotationFailed;
  }
  return kPixelGrabOk;
}

int pixelgrab_annotation_can_undo(const PixelGrabAnnotation* ann) {
  if (!ann || !ann->session) return 0;
  return ann->session->CanUndo() ? 1 : 0;
}

int pixelgrab_annotation_can_redo(const PixelGrabAnnotation* ann) {
  if (!ann || !ann->session) return 0;
  return ann->session->CanRedo() ? 1 : 0;
}

const PixelGrabImage* pixelgrab_annotation_get_result(
    PixelGrabAnnotation* ann) {
  if (!ann || !ann->session) return nullptr;
  const Image* result = ann->session->GetResult();
  if (!result) return nullptr;

  // Return a non-owning view. The Image is owned by the AnnotationSession.
  // result_view is released (not deleted) in ~PixelGrabAnnotation.
  ann->result_view.impl.release();  // Release previous (non-owning).
  ann->result_view.impl.reset(const_cast<Image*>(result));
  return &ann->result_view;
}

PixelGrabImage* pixelgrab_annotation_export(PixelGrabAnnotation* ann) {
  if (!ann || !ann->session) return nullptr;
  auto exported = ann->session->Export();
  if (!exported) return nullptr;
  return WrapImage(exported.release());
}

// ---------------------------------------------------------------------------
// UI Element Detection & Smart Snapping
// ---------------------------------------------------------------------------

PixelGrabError pixelgrab_detect_element(PixelGrabContext* ctx, int x, int y,
                                        PixelGrabElementRect* out_rect) {
  if (!ctx) return kPixelGrabErrorNotInitialized;
  return ctx->impl.DetectElement(x, y, out_rect);
}

int pixelgrab_detect_elements(PixelGrabContext* ctx, int x, int y,
                              PixelGrabElementRect* out_rects,
                              int max_count) {
  if (!ctx) return -1;
  return ctx->impl.DetectElements(x, y, out_rects, max_count);
}

PixelGrabError pixelgrab_snap_to_element(PixelGrabContext* ctx, int x, int y,
                                         int snap_distance,
                                         PixelGrabElementRect* out_rect) {
  if (!ctx) return kPixelGrabErrorNotInitialized;
  return ctx->impl.SnapToElement(x, y, snap_distance, out_rect);
}

// ---------------------------------------------------------------------------
// Capture History & Region Recall
// ---------------------------------------------------------------------------

int pixelgrab_history_count(PixelGrabContext* ctx) {
  if (!ctx) return 0;
  return ctx->impl.HistoryCount();
}

PixelGrabError pixelgrab_history_get_entry(PixelGrabContext* ctx, int index,
                                           PixelGrabHistoryEntry* out_entry) {
  if (!ctx) return kPixelGrabErrorNotInitialized;
  return ctx->impl.HistoryGetEntry(index, out_entry);
}

PixelGrabImage* pixelgrab_history_recapture(PixelGrabContext* ctx,
                                            int history_id) {
  if (!ctx) return nullptr;
  auto* img = ctx->impl.HistoryRecapture(history_id);
  if (!img) return nullptr;
  return WrapImage(img);
}

PixelGrabImage* pixelgrab_recapture_last(PixelGrabContext* ctx) {
  if (!ctx) return nullptr;
  auto* img = ctx->impl.RecaptureLast();
  if (!img) return nullptr;
  return WrapImage(img);
}

void pixelgrab_history_clear(PixelGrabContext* ctx) {
  if (!ctx) return;
  ctx->impl.HistoryClear();
}

void pixelgrab_history_set_max_count(PixelGrabContext* ctx, int max_count) {
  if (!ctx) return;
  if (max_count <= 0) return;
  ctx->impl.HistorySetMaxCount(max_count);
}

// ---------------------------------------------------------------------------
// Pin Windows (Floating Overlay)
// ---------------------------------------------------------------------------

PixelGrabPinWindow* pixelgrab_pin_image(PixelGrabContext* ctx,
                                        const PixelGrabImage* image,
                                        int x, int y) {
  if (!ctx) return nullptr;
  if (!image || !image->impl) {
    ctx->impl.SetError(kPixelGrabErrorInvalidParam,
                       "Pin image is NULL or empty");
    return nullptr;
  }
  int id = ctx->impl.pin_manager().PinImage(image->impl.get(), x, y);
  if (id <= 0) {
    ctx->impl.SetError(kPixelGrabErrorWindowCreateFailed,
                       "Failed to create image pin window");
    return nullptr;
  }
  auto* pin = new (std::nothrow) PixelGrabPinWindow();
  if (!pin) {
    ctx->impl.pin_manager().DestroyPin(id);
    ctx->impl.SetError(kPixelGrabErrorOutOfMemory,
                       "Failed to allocate pin window handle");
    return nullptr;
  }
  pin->ctx = ctx;
  pin->pin_id = id;
  ctx->impl.ClearError();
  return pin;
}

PixelGrabPinWindow* pixelgrab_pin_text(PixelGrabContext* ctx,
                                       const char* text, int x, int y) {
  if (!ctx) return nullptr;
  if (!text) {
    ctx->impl.SetError(kPixelGrabErrorInvalidParam, "Pin text is NULL");
    return nullptr;
  }
  int id = ctx->impl.pin_manager().PinText(text, x, y);
  if (id <= 0) {
    ctx->impl.SetError(kPixelGrabErrorWindowCreateFailed,
                       "Failed to create text pin window");
    return nullptr;
  }
  auto* pin = new (std::nothrow) PixelGrabPinWindow();
  if (!pin) {
    ctx->impl.pin_manager().DestroyPin(id);
    ctx->impl.SetError(kPixelGrabErrorOutOfMemory,
                       "Failed to allocate pin window handle");
    return nullptr;
  }
  pin->ctx = ctx;
  pin->pin_id = id;
  ctx->impl.ClearError();
  return pin;
}

PixelGrabPinWindow* pixelgrab_pin_clipboard(PixelGrabContext* ctx,
                                            int x, int y) {
  if (!ctx) return nullptr;
  auto* reader = ctx->impl.clipboard_reader();
  if (!reader) {
    ctx->impl.SetError(kPixelGrabErrorNotSupported,
                       "Clipboard reader not available");
    return nullptr;
  }
  int id = ctx->impl.pin_manager().PinClipboard(reader, x, y);
  if (id <= 0) {
    ctx->impl.SetError(kPixelGrabErrorClipboardEmpty,
                       "No pinnable content on clipboard");
    return nullptr;
  }
  auto* pin = new (std::nothrow) PixelGrabPinWindow();
  if (!pin) {
    ctx->impl.pin_manager().DestroyPin(id);
    ctx->impl.SetError(kPixelGrabErrorOutOfMemory,
                       "Failed to allocate pin window handle");
    return nullptr;
  }
  pin->ctx = ctx;
  pin->pin_id = id;
  ctx->impl.ClearError();
  return pin;
}

void pixelgrab_pin_destroy(PixelGrabPinWindow* pin) {
  if (!pin) return;
  if (pin->ctx) {
    pin->ctx->impl.pin_manager().DestroyPin(pin->pin_id);
  }
  delete pin;
}

PixelGrabError pixelgrab_pin_set_opacity(PixelGrabPinWindow* pin,
                                         float opacity) {
  if (!pin || !pin->ctx) return kPixelGrabErrorInvalidParam;
  if (!pin->ctx->impl.pin_manager().SetOpacity(pin->pin_id, opacity)) {
    return kPixelGrabErrorWindowCreateFailed;
  }
  return kPixelGrabOk;
}

float pixelgrab_pin_get_opacity(const PixelGrabPinWindow* pin) {
  if (!pin || !pin->ctx) return 1.0f;
  return pin->ctx->impl.pin_manager().GetOpacity(pin->pin_id);
}

PixelGrabError pixelgrab_pin_set_position(PixelGrabPinWindow* pin,
                                          int x, int y) {
  if (!pin || !pin->ctx) return kPixelGrabErrorInvalidParam;
  if (!pin->ctx->impl.pin_manager().SetPosition(pin->pin_id, x, y)) {
    return kPixelGrabErrorWindowCreateFailed;
  }
  return kPixelGrabOk;
}

PixelGrabError pixelgrab_pin_set_size(PixelGrabPinWindow* pin,
                                      int width, int height) {
  if (!pin || !pin->ctx) return kPixelGrabErrorInvalidParam;
  if (!pin->ctx->impl.pin_manager().SetSize(pin->pin_id, width, height)) {
    return kPixelGrabErrorWindowCreateFailed;
  }
  return kPixelGrabOk;
}

PixelGrabError pixelgrab_pin_set_visible(PixelGrabPinWindow* pin,
                                         int visible) {
  if (!pin || !pin->ctx) return kPixelGrabErrorInvalidParam;
  if (!pin->ctx->impl.pin_manager().SetVisible(pin->pin_id, visible != 0)) {
    return kPixelGrabErrorWindowCreateFailed;
  }
  return kPixelGrabOk;
}

int pixelgrab_pin_process_events(PixelGrabContext* ctx) {
  if (!ctx) return 0;
  return ctx->impl.pin_manager().ProcessEvents();
}

int pixelgrab_pin_count(PixelGrabContext* ctx) {
  if (!ctx) return 0;
  return ctx->impl.pin_manager().Count();
}

void pixelgrab_pin_destroy_all(PixelGrabContext* ctx) {
  if (!ctx) return;
  ctx->impl.pin_manager().DestroyAll();
}

// ---------------------------------------------------------------------------
// Pin Window — Enumeration, Content Access & Multi-Pin Operations
// ---------------------------------------------------------------------------

int pixelgrab_pin_enumerate(PixelGrabContext* ctx, int* out_ids,
                            int max_count) {
  if (!ctx || !out_ids || max_count <= 0) return -1;
  return ctx->impl.pin_manager().Enumerate(out_ids, max_count);
}

PixelGrabError pixelgrab_pin_get_info(PixelGrabPinWindow* pin,
                                      PixelGrabPinInfo* out_info) {
  if (!pin || !pin->ctx || !out_info) return kPixelGrabErrorInvalidParam;

  using pixelgrab::internal::PinContentType;

  int x = 0, y = 0, w = 0, h = 0;
  float opacity = 1.0f;
  bool visible = false;
  PinContentType ctype = PinContentType::kImage;

  if (!pin->ctx->impl.pin_manager().GetInfo(pin->pin_id, &x, &y, &w, &h,
                                            &opacity, &visible, &ctype)) {
    return kPixelGrabErrorInvalidParam;
  }

  out_info->id = pin->pin_id;
  out_info->x = x;
  out_info->y = y;
  out_info->width = w;
  out_info->height = h;
  out_info->opacity = opacity;
  out_info->is_visible = visible ? 1 : 0;
  out_info->content_type = (ctype == PinContentType::kImage) ? 0 : 1;
  return kPixelGrabOk;
}

PixelGrabImage* pixelgrab_pin_get_image(PixelGrabPinWindow* pin) {
  if (!pin || !pin->ctx) return nullptr;
  auto image = pin->ctx->impl.pin_manager().GetImage(pin->pin_id);
  if (!image) return nullptr;
  return WrapImage(image.release());
}

PixelGrabError pixelgrab_pin_set_image(PixelGrabPinWindow* pin,
                                       const PixelGrabImage* image) {
  if (!pin || !pin->ctx || !image || !image->impl)
    return kPixelGrabErrorInvalidParam;
  if (!pin->ctx->impl.pin_manager().SetImage(pin->pin_id, image->impl.get())) {
    return kPixelGrabErrorInvalidParam;
  }
  return kPixelGrabOk;
}

PixelGrabError pixelgrab_pin_set_visible_all(PixelGrabContext* ctx,
                                             int visible) {
  if (!ctx) return kPixelGrabErrorInvalidParam;
  ctx->impl.pin_manager().SetVisibleAll(visible != 0);
  return kPixelGrabOk;
}

PixelGrabPinWindow* pixelgrab_pin_duplicate(PixelGrabPinWindow* pin,
                                            int offset_x, int offset_y) {
  if (!pin || !pin->ctx) return nullptr;
  int new_id =
      pin->ctx->impl.pin_manager().Duplicate(pin->pin_id, offset_x, offset_y);
  if (new_id <= 0) {
    pin->ctx->impl.SetError(kPixelGrabErrorWindowCreateFailed,
                             "Failed to duplicate pin window");
    return nullptr;
  }
  auto* dup = new (std::nothrow) PixelGrabPinWindow();
  if (!dup) {
    pin->ctx->impl.pin_manager().DestroyPin(new_id);
    pin->ctx->impl.SetError(kPixelGrabErrorOutOfMemory,
                             "Failed to allocate duplicated pin handle");
    return nullptr;
  }
  dup->ctx = pin->ctx;
  dup->pin_id = new_id;
  return dup;
}

void* pixelgrab_pin_get_native_handle(PixelGrabPinWindow* pin) {
  if (!pin || !pin->ctx) return nullptr;
  auto* backend = pin->ctx->impl.pin_manager().GetBackend(pin->pin_id);
  if (!backend) return nullptr;
  return backend->GetNativeHandle();
}

static constexpr int kCompositorSettleMs = 1;

PixelGrabImage* pixelgrab_capture_screen_exclude_pins(PixelGrabContext* ctx,
                                                      int screen_index) {
  if (!ctx) return nullptr;
  auto& pm = ctx->impl.pin_manager();
  pm.SetVisibleAll(false);
  std::this_thread::sleep_for(std::chrono::milliseconds(kCompositorSettleMs));
  Image* raw = ctx->impl.CaptureScreen(screen_index);
  pm.SetVisibleAll(true);
  if (!raw) return nullptr;
  return WrapImage(raw);
}

PixelGrabImage* pixelgrab_capture_region_exclude_pins(PixelGrabContext* ctx,
                                                      int x, int y, int width,
                                                      int height) {
  if (!ctx) return nullptr;
  auto& pm = ctx->impl.pin_manager();
  pm.SetVisibleAll(false);
  std::this_thread::sleep_for(std::chrono::milliseconds(kCompositorSettleMs));
  Image* raw = ctx->impl.CaptureRegion(x, y, width, height);
  pm.SetVisibleAll(true);
  if (!raw) return nullptr;
  return WrapImage(raw);
}

// ---------------------------------------------------------------------------
// Clipboard Reading
// ---------------------------------------------------------------------------

PixelGrabClipboardFormat pixelgrab_clipboard_get_format(
    PixelGrabContext* ctx) {
  if (!ctx) return kPixelGrabClipboardNone;
  auto* reader = ctx->impl.clipboard_reader();
  if (!reader) return kPixelGrabClipboardNone;
  return reader->GetAvailableFormat();
}

PixelGrabImage* pixelgrab_clipboard_get_image(PixelGrabContext* ctx) {
  if (!ctx) return nullptr;
  auto* reader = ctx->impl.clipboard_reader();
  if (!reader) return nullptr;
  auto img = reader->ReadImage();
  if (!img) return nullptr;
  return WrapImage(img.release());
}

char* pixelgrab_clipboard_get_text(PixelGrabContext* ctx) {
  if (!ctx) return nullptr;
  auto* reader = ctx->impl.clipboard_reader();
  if (!reader) return nullptr;
  std::string text = reader->ReadText();
  if (text.empty()) return nullptr;
  // Allocate a new C string for the caller.
  char* result = static_cast<char*>(std::malloc(text.size() + 1));
  if (!result) return nullptr;
  std::memcpy(result, text.c_str(), text.size() + 1);
  return result;
}

void pixelgrab_free_string(char* str) {
  std::free(str);
}

// ---------------------------------------------------------------------------
// The opaque PixelGrabRecorder struct wraps the recording backend.
// ---------------------------------------------------------------------------
struct PixelGrabRecorder {
  PixelGrabContext* ctx;
  std::unique_ptr<RecorderBackend> backend;
  std::unique_ptr<WatermarkRenderer> watermark;
  PixelGrabTextWatermarkConfig watermark_config;
  PixelGrabTextWatermarkConfig user_watermark_config;
  std::string watermark_text_storage;       // Owns the system watermark text.
  std::string user_watermark_text_storage;  // Owns the user watermark text.
  bool has_watermark;
  bool has_user_watermark;
  bool auto_capture;
  RecordConfig config;
};

// ---------------------------------------------------------------------------
// Screen Recording
// ---------------------------------------------------------------------------

int pixelgrab_recorder_is_supported(PixelGrabContext* ctx) {
  if (!ctx) return 0;
  // Test by attempting to create a platform recorder.
  auto backend = pixelgrab::internal::CreatePlatformRecorder();
  return backend ? 1 : 0;
}

PixelGrabRecorder* pixelgrab_recorder_create(
    PixelGrabContext* ctx, const PixelGrabRecordConfig* config) {
  if (!ctx || !config || !config->output_path) return nullptr;

  auto backend = pixelgrab::internal::CreatePlatformRecorder();
  if (!backend) {
    ctx->impl.SetError(kPixelGrabErrorEncoderNotAvailable,
                       "Platform recorder not available");
    return nullptr;
  }

  auto* rec = new (std::nothrow) PixelGrabRecorder();
  if (!rec) return nullptr;

  rec->ctx = ctx;
  rec->backend = std::move(backend);
  rec->has_watermark = (config->watermark != nullptr);
  rec->has_user_watermark = (config->user_watermark != nullptr &&
                             config->user_watermark->text != nullptr &&
                             config->user_watermark->text[0] != '\0');
  rec->auto_capture = (config->auto_capture != 0);

  // Copy watermark config if provided.
  bool need_renderer = rec->has_watermark || rec->has_user_watermark;
  if (rec->has_watermark) {
    rec->watermark_config = *config->watermark;
    // Own the text string (the caller's pointer may not survive).
    if (config->watermark->text) {
      rec->watermark_text_storage = config->watermark->text;
      rec->watermark_config.text = rec->watermark_text_storage.c_str();
    }
  }
  if (rec->has_user_watermark) {
    rec->user_watermark_config = *config->user_watermark;
    // Own the text string (the caller's pointer may not survive).
    rec->user_watermark_text_storage = config->user_watermark->text;
    rec->user_watermark_config.text = rec->user_watermark_text_storage.c_str();
  }
  if (need_renderer) {
    rec->watermark = pixelgrab::internal::CreatePlatformWatermarkRenderer();
  }

  static constexpr int kDefaultFps = 30;
  static constexpr int kDefaultBitrate = 4000000;  // 4 Mbps

  rec->config.output_path = config->output_path;
  rec->config.region_x = config->region_x;
  rec->config.region_y = config->region_y;
  rec->config.region_width = config->region_width;
  rec->config.region_height = config->region_height;
  rec->config.fps = (config->fps > 0) ? config->fps : kDefaultFps;
  rec->config.bitrate = (config->bitrate > 0) ? config->bitrate : kDefaultBitrate;
  rec->config.auto_capture = rec->auto_capture;

  // Set up auto-capture dependencies.
  if (rec->auto_capture) {
    rec->config.capture_backend = ctx->impl.capture_backend();
    if (need_renderer && rec->watermark) {
      rec->config.watermark_renderer = rec->watermark.get();
    }
    if (rec->has_watermark) {
      rec->config.watermark_config = rec->watermark_config;
      rec->config.has_watermark = true;
    }
    if (rec->has_user_watermark) {
      rec->config.user_watermark_config = rec->user_watermark_config;
      rec->config.has_user_watermark = true;
    }
  }

  // Audio configuration.
  rec->config.audio_source = config->audio_source;
  if (config->audio_device_id) {
    rec->config.audio_device_id = config->audio_device_id;
  }
  static constexpr int kDefaultAudioSampleRate = 44100;
  rec->config.audio_sample_rate = (config->audio_sample_rate > 0)
                                      ? config->audio_sample_rate
                                      : kDefaultAudioSampleRate;
  if (config->audio_source != kPixelGrabAudioNone) {
    rec->config.audio_backend = ctx->impl.audio_backend();
  }

  // GPU acceleration hint.
  rec->config.gpu_hint = config->gpu_hint;

  if (!rec->backend->Initialize(rec->config)) {
    ctx->impl.SetError(kPixelGrabErrorRecordFailed,
                       "Failed to initialize recorder backend");
    delete rec;
    return nullptr;
  }

  return rec;
}

void pixelgrab_recorder_destroy(PixelGrabRecorder* recorder) {
  if (!recorder) return;
  // Stop if still recording.
  if (recorder->backend) {
    auto state = recorder->backend->GetState();
    if (state == RecordState::kRecording ||
        state == RecordState::kPaused) {
      recorder->backend->Stop();
    }
  }
  delete recorder;
}

PixelGrabError pixelgrab_recorder_start(PixelGrabRecorder* recorder) {
  if (!recorder || !recorder->backend) return kPixelGrabErrorInvalidParam;
  if (!recorder->backend->Start()) return kPixelGrabErrorRecordFailed;
  // In auto mode, start the internal capture thread.
  if (recorder->auto_capture) {
    recorder->backend->StartCaptureLoop();
  }
  return kPixelGrabOk;
}

PixelGrabError pixelgrab_recorder_pause(PixelGrabRecorder* recorder) {
  if (!recorder || !recorder->backend) return kPixelGrabErrorInvalidParam;
  if (!recorder->backend->Pause()) return kPixelGrabErrorRecordFailed;
  return kPixelGrabOk;
}

PixelGrabError pixelgrab_recorder_resume(PixelGrabRecorder* recorder) {
  if (!recorder || !recorder->backend) return kPixelGrabErrorInvalidParam;
  if (!recorder->backend->Resume()) return kPixelGrabErrorRecordFailed;
  return kPixelGrabOk;
}

PixelGrabError pixelgrab_recorder_stop(PixelGrabRecorder* recorder) {
  if (!recorder || !recorder->backend) return kPixelGrabErrorInvalidParam;
  if (!recorder->backend->Stop()) return kPixelGrabErrorRecordFailed;
  return kPixelGrabOk;
}

PixelGrabRecordState pixelgrab_recorder_get_state(
    const PixelGrabRecorder* recorder) {
  if (!recorder || !recorder->backend) return kPixelGrabRecordIdle;
  return static_cast<PixelGrabRecordState>(recorder->backend->GetState());
}

int64_t pixelgrab_recorder_get_duration_ms(
    const PixelGrabRecorder* recorder) {
  if (!recorder || !recorder->backend) return 0;
  return recorder->backend->GetDurationMs();
}

PixelGrabError pixelgrab_recorder_write_frame(PixelGrabRecorder* recorder,
                                              const PixelGrabImage* frame) {
  if (!recorder || !recorder->backend) return kPixelGrabErrorInvalidParam;
  if (!frame || !frame->impl) return kPixelGrabErrorInvalidParam;

  // Reject write_frame in auto capture mode.
  if (recorder->auto_capture) {
    if (recorder->ctx) {
      recorder->ctx->impl.SetError(
          kPixelGrabErrorRecordFailed,
          "write_frame not available in auto capture mode");
    }
    return kPixelGrabErrorRecordFailed;
  }

  if (!recorder->backend->WriteFrame(*frame->impl)) {
    return kPixelGrabErrorRecordFailed;
  }
  return kPixelGrabOk;
}

// ---------------------------------------------------------------------------
// Watermark
// ---------------------------------------------------------------------------

int pixelgrab_watermark_is_supported(PixelGrabContext* ctx) {
  if (!ctx) return 0;
  auto* renderer = ctx->impl.watermark_renderer();
  return renderer ? 1 : 0;
}

PixelGrabError pixelgrab_watermark_apply_text(
    PixelGrabContext* ctx, PixelGrabImage* image,
    const PixelGrabTextWatermarkConfig* config) {
  if (!ctx || !image || !config || !config->text)
    return kPixelGrabErrorInvalidParam;
  if (!image->impl) return kPixelGrabErrorInvalidParam;

  auto* renderer = ctx->impl.watermark_renderer();
  if (!renderer) {
    ctx->impl.SetError(kPixelGrabErrorNotSupported,
                       "Watermark renderer not available");
    return kPixelGrabErrorNotSupported;
  }

  if (!renderer->ApplyTextWatermark(image->impl.get(), *config)) {
    ctx->impl.SetError(kPixelGrabErrorWatermarkFailed,
                       "Failed to apply text watermark");
    return kPixelGrabErrorWatermarkFailed;
  }

  return kPixelGrabOk;
}

PixelGrabError pixelgrab_watermark_apply_image(
    PixelGrabContext* ctx, PixelGrabImage* image,
    const PixelGrabImage* watermark, int x, int y, float opacity) {
  if (!ctx || !image || !watermark) return kPixelGrabErrorInvalidParam;
  if (!image->impl || !watermark->impl) return kPixelGrabErrorInvalidParam;

  auto* renderer = ctx->impl.watermark_renderer();
  if (!renderer) {
    ctx->impl.SetError(kPixelGrabErrorNotSupported,
                       "Watermark renderer not available");
    return kPixelGrabErrorNotSupported;
  }

  if (!renderer->ApplyImageWatermark(image->impl.get(), *watermark->impl,
                                     x, y, opacity)) {
    ctx->impl.SetError(kPixelGrabErrorWatermarkFailed,
                       "Failed to apply image watermark");
    return kPixelGrabErrorWatermarkFailed;
  }

  return kPixelGrabOk;
}

// ---------------------------------------------------------------------------
// Audio Device Query
// ---------------------------------------------------------------------------

using pixelgrab::internal::AudioBackend;

int pixelgrab_audio_is_supported(PixelGrabContext* ctx) {
  if (!ctx) return 0;
  auto* backend = ctx->impl.audio_backend();
  return (backend && backend->IsSupported()) ? 1 : 0;
}

int pixelgrab_audio_enumerate_devices(PixelGrabContext* ctx,
                                      PixelGrabAudioDeviceInfo* out_devices,
                                      int max_count) {
  if (!ctx || !out_devices || max_count <= 0) return -1;
  auto* backend = ctx->impl.audio_backend();
  if (!backend || !backend->IsSupported()) return 0;

  auto devices = backend->EnumerateDevices();
  int count = static_cast<int>(devices.size());
  if (count > max_count) count = max_count;

  for (int i = 0; i < count; ++i) {
    std::memset(&out_devices[i], 0, sizeof(PixelGrabAudioDeviceInfo));

    size_t id_len = (std::min)(devices[i].id.size(),
                               sizeof(out_devices[i].id) - 1);
    std::memcpy(out_devices[i].id, devices[i].id.c_str(), id_len);

    size_t name_len = (std::min)(devices[i].name.size(),
                                 sizeof(out_devices[i].name) - 1);
    std::memcpy(out_devices[i].name, devices[i].name.c_str(), name_len);

    out_devices[i].is_default = devices[i].is_default ? 1 : 0;
    out_devices[i].is_input = devices[i].is_input ? 1 : 0;
  }

  return count;
}

PixelGrabError pixelgrab_audio_get_default_device(
    PixelGrabContext* ctx, int is_input,
    PixelGrabAudioDeviceInfo* out_device) {
  if (!ctx || !out_device) return kPixelGrabErrorInvalidParam;
  auto* backend = ctx->impl.audio_backend();
  if (!backend || !backend->IsSupported()) {
    return kPixelGrabErrorNotSupported;
  }

  auto dev = backend->GetDefaultDevice(is_input != 0);
  std::memset(out_device, 0, sizeof(PixelGrabAudioDeviceInfo));

  size_t id_len =
      (std::min)(dev.id.size(), sizeof(out_device->id) - 1);
  std::memcpy(out_device->id, dev.id.c_str(), id_len);

  size_t name_len =
      (std::min)(dev.name.size(), sizeof(out_device->name) - 1);
  std::memcpy(out_device->name, dev.name.c_str(), name_len);

  out_device->is_default = dev.is_default ? 1 : 0;
  out_device->is_input = dev.is_input ? 1 : 0;

  return kPixelGrabOk;
}

// ---------------------------------------------------------------------------
// OCR
// ---------------------------------------------------------------------------

int pixelgrab_ocr_is_supported(PixelGrabContext* ctx) {
  if (!ctx) return 0;
  auto* backend = ctx->impl.ocr_backend();
  return (backend && backend->IsSupported()) ? 1 : 0;
}

PixelGrabError pixelgrab_ocr_recognize(PixelGrabContext* ctx,
                                       const PixelGrabImage* image,
                                       const char* language,
                                       char** out_text) {
  if (!ctx || !image || !out_text) return kPixelGrabErrorInvalidParam;
  *out_text = nullptr;

  auto* backend = ctx->impl.ocr_backend();
  if (!backend || !backend->IsSupported()) {
    ctx->impl.SetError(kPixelGrabErrorNotSupported, "OCR not supported");
    return kPixelGrabErrorNotSupported;
  }

  const uint8_t* data = image->impl->data();
  int w = image->impl->width();
  int h = image->impl->height();
  int stride = image->impl->stride();

  if (!data || w <= 0 || h <= 0) {
    ctx->impl.SetError(kPixelGrabErrorInvalidParam, "Invalid image for OCR");
    return kPixelGrabErrorInvalidParam;
  }

  std::string text = backend->RecognizeText(data, w, h, stride, language);
  if (text.empty()) {
    ctx->impl.SetError(kPixelGrabErrorOcrFailed, "OCR returned no text");
    return kPixelGrabErrorOcrFailed;
  }

  char* buf = static_cast<char*>(std::malloc(text.size() + 1));
  if (!buf) {
    ctx->impl.SetError(kPixelGrabErrorOutOfMemory,
                       "Failed to allocate OCR result");
    return kPixelGrabErrorOutOfMemory;
  }
  std::memcpy(buf, text.c_str(), text.size() + 1);
  *out_text = buf;

  ctx->impl.ClearError();
  return kPixelGrabOk;
}

// ---------------------------------------------------------------------------
// Translation
// ---------------------------------------------------------------------------

PixelGrabError pixelgrab_translate_set_config(PixelGrabContext* ctx,
                                              const char* provider,
                                              const char* app_id,
                                              const char* secret_key) {
  if (!ctx) return kPixelGrabErrorInvalidParam;
  if (!app_id || !secret_key) return kPixelGrabErrorInvalidParam;

  auto* backend = ctx->impl.translate_backend();
  if (!backend) {
    ctx->impl.SetError(kPixelGrabErrorNotSupported,
                       "Translation backend not available");
    return kPixelGrabErrorNotSupported;
  }

  pixelgrab::internal::TranslateConfig config;
  config.provider = provider ? provider : "baidu";
  config.app_id = app_id;
  config.secret_key = secret_key;
  backend->SetConfig(config);

  ctx->impl.ClearError();
  return kPixelGrabOk;
}

int pixelgrab_translate_is_supported(PixelGrabContext* ctx) {
  if (!ctx) return 0;
  auto* backend = ctx->impl.translate_backend();
  return (backend && backend->IsSupported()) ? 1 : 0;
}

PixelGrabError pixelgrab_translate_text(PixelGrabContext* ctx, const char* text,
                                        const char* source_lang,
                                        const char* target_lang,
                                        char** out_translated) {
  if (!ctx || !text || !target_lang || !out_translated)
    return kPixelGrabErrorInvalidParam;
  *out_translated = nullptr;

  auto* backend = ctx->impl.translate_backend();
  if (!backend || !backend->IsSupported()) {
    ctx->impl.SetError(kPixelGrabErrorNotSupported,
                       "Translation not configured");
    return kPixelGrabErrorNotSupported;
  }

  std::string result =
      backend->Translate(text, source_lang ? source_lang : "auto", target_lang);
  if (result.empty()) {
    const auto& detail = backend->last_error_detail();
    ctx->impl.SetError(kPixelGrabErrorTranslateFailed,
                       detail.empty() ? "Translation returned no result"
                                      : detail);
    return kPixelGrabErrorTranslateFailed;
  }

  char* buf = static_cast<char*>(std::malloc(result.size() + 1));
  if (!buf) {
    ctx->impl.SetError(kPixelGrabErrorOutOfMemory,
                       "Failed to allocate translation result");
    return kPixelGrabErrorOutOfMemory;
  }
  std::memcpy(buf, result.c_str(), result.size() + 1);
  *out_translated = buf;

  ctx->impl.ClearError();
  return kPixelGrabOk;
}

// ---------------------------------------------------------------------------
// Version information
// ---------------------------------------------------------------------------

const char* pixelgrab_version_string(void) {
  return PIXELGRAB_VERSION_STRING;
}

int pixelgrab_version_major(void) { return PIXELGRAB_VERSION_MAJOR; }
int pixelgrab_version_minor(void) { return PIXELGRAB_VERSION_MINOR; }
int pixelgrab_version_patch(void) { return PIXELGRAB_VERSION_PATCH; }

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

void pixelgrab_set_log_level(PixelGrabLogLevel level) {
  pixelgrab::internal::SetLogLevel(level);
}

void pixelgrab_set_log_callback(pixelgrab_log_callback_t callback,
                                void* userdata) {
  auto sink = pixelgrab::internal::GetCallbackSink();
  if (sink) {
    sink->SetCallback(callback, userdata);
  }
}

void pixelgrab_log(PixelGrabLogLevel level, const char* message) {
  if (!message) return;
  auto logger = pixelgrab::internal::GetLogger();
  if (logger) {
    logger->log(pixelgrab::internal::ToSpdlogLevel(level), "{}", message);
  }
}
