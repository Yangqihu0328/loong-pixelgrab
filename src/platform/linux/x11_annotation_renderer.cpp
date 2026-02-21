// Copyright 2026 The loong-pixelgrab Authors
// Linux annotation renderer — Cairo + Pango implementation.

#include "platform/linux/x11_annotation_renderer.h"

#if defined(__linux__)

#include <cmath>
#include <string>

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "annotation/annotation_renderer.h"
#include "core/image.h"

namespace pixelgrab {
namespace internal {

X11AnnotationRenderer::~X11AnnotationRenderer() { EndRender(); }

// -----------------------------------------------------------------------
// Begin / End
// -----------------------------------------------------------------------

bool X11AnnotationRenderer::BeginRender(Image* target) {
  if (!target) return false;
  EndRender();

  target_ = target;
  // Image is BGRA8 which matches CAIRO_FORMAT_ARGB32 on little-endian.
  surface_ = cairo_image_surface_create_for_data(
      target->mutable_data(), CAIRO_FORMAT_ARGB32,
      target->width(), target->height(), target->stride());
  if (cairo_surface_status(surface_) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(surface_);
    surface_ = nullptr;
    target_ = nullptr;
    return false;
  }
  cr_ = cairo_create(surface_);
  return true;
}

void X11AnnotationRenderer::EndRender() {
  if (cr_) {
    cairo_destroy(cr_);
    cr_ = nullptr;
  }
  if (surface_) {
    cairo_surface_flush(surface_);
    cairo_surface_destroy(surface_);
    surface_ = nullptr;
  }
  target_ = nullptr;
}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

namespace {

void ApplyStyle(cairo_t* cr, const ShapeStyle& style, bool stroke) {
  uint32_t c = stroke ? style.stroke_color : style.fill_color;
  double a = static_cast<double>((c >> 24) & 0xFF) / 255.0;
  double r = static_cast<double>((c >> 16) & 0xFF) / 255.0;
  double g = static_cast<double>((c >> 8) & 0xFF) / 255.0;
  double b = static_cast<double>(c & 0xFF) / 255.0;
  cairo_set_source_rgba(cr, r, g, b, a);
  if (stroke) cairo_set_line_width(cr, style.stroke_width);
}

}  // namespace

// -----------------------------------------------------------------------
// Primitives
// -----------------------------------------------------------------------

void X11AnnotationRenderer::DrawRect(int x, int y, int w, int h,
                                     const ShapeStyle& style) {
  if (!cr_) return;
  cairo_rectangle(cr_, x, y, w, h);
  if (style.filled && style.fill_color) {
    ApplyStyle(cr_, style, false);
    cairo_fill_preserve(cr_);
  }
  ApplyStyle(cr_, style, true);
  cairo_stroke(cr_);
}

void X11AnnotationRenderer::DrawEllipse(int cx, int cy, int rx, int ry,
                                        const ShapeStyle& style) {
  if (!cr_ || rx <= 0 || ry <= 0) return;
  cairo_save(cr_);
  cairo_translate(cr_, cx, cy);
  cairo_scale(cr_, static_cast<double>(rx), static_cast<double>(ry));
  cairo_arc(cr_, 0.0, 0.0, 1.0, 0.0, 2.0 * M_PI);
  cairo_restore(cr_);

  if (style.filled && style.fill_color) {
    ApplyStyle(cr_, style, false);
    cairo_fill_preserve(cr_);
  }
  ApplyStyle(cr_, style, true);
  cairo_stroke(cr_);
}

void X11AnnotationRenderer::DrawLine(int x1, int y1, int x2, int y2,
                                     const ShapeStyle& style) {
  if (!cr_) return;
  ApplyStyle(cr_, style, true);
  cairo_move_to(cr_, x1, y1);
  cairo_line_to(cr_, x2, y2);
  cairo_stroke(cr_);
}

void X11AnnotationRenderer::DrawArrow(int x1, int y1, int x2, int y2,
                                      float head_size,
                                      const ShapeStyle& style) {
  if (!cr_) return;

  // Shaft
  ApplyStyle(cr_, style, true);
  cairo_move_to(cr_, x1, y1);
  cairo_line_to(cr_, x2, y2);
  cairo_stroke(cr_);

  // Arrowhead triangle
  double dx = static_cast<double>(x2 - x1);
  double dy = static_cast<double>(y2 - y1);
  double len = std::sqrt(dx * dx + dy * dy);
  if (len < 1.0) return;

  double ux = dx / len;
  double uy = dy / len;
  double px = -uy;
  double py = ux;
  double hs = static_cast<double>(head_size);

  double bx = x2 - ux * hs;
  double by = y2 - uy * hs;
  double lx = bx + px * hs * 0.4;
  double ly = by + py * hs * 0.4;
  double rx = bx - px * hs * 0.4;
  double ry = by - py * hs * 0.4;

  ApplyStyle(cr_, style, true);
  cairo_move_to(cr_, x2, y2);
  cairo_line_to(cr_, lx, ly);
  cairo_line_to(cr_, rx, ry);
  cairo_close_path(cr_);
  cairo_fill(cr_);
}

void X11AnnotationRenderer::DrawPolyline(const Point* points, int count,
                                         const ShapeStyle& style) {
  if (!cr_ || !points || count < 2) return;

  ApplyStyle(cr_, style, true);
  cairo_set_line_join(cr_, CAIRO_LINE_JOIN_ROUND);
  cairo_set_line_cap(cr_, CAIRO_LINE_CAP_ROUND);

  cairo_move_to(cr_, points[0].x, points[0].y);
  for (int i = 1; i < count; ++i)
    cairo_line_to(cr_, points[i].x, points[i].y);
  cairo_stroke(cr_);
}

void X11AnnotationRenderer::DrawText(int x, int y, const char* text,
                                     const char* font_name, int font_size,
                                     uint32_t color) {
  if (!cr_ || !text) return;

  double a = static_cast<double>((color >> 24) & 0xFF) / 255.0;
  double r = static_cast<double>((color >> 16) & 0xFF) / 255.0;
  double g = static_cast<double>((color >> 8) & 0xFF) / 255.0;
  double b = static_cast<double>(color & 0xFF) / 255.0;
  cairo_set_source_rgba(cr_, r, g, b, a);

  PangoLayout* layout = pango_cairo_create_layout(cr_);
  pango_layout_set_text(layout, text, -1);

  std::string desc_str =
      std::string(font_name ? font_name : "Sans") + " " +
      std::to_string(font_size > 0 ? font_size : 14);
  PangoFontDescription* desc =
      pango_font_description_from_string(desc_str.c_str());
  pango_layout_set_font_description(layout, desc);
  pango_font_description_free(desc);

  cairo_move_to(cr_, x, y);
  pango_cairo_show_layout(cr_, layout);
  g_object_unref(layout);
}

// Factory.
std::unique_ptr<AnnotationRenderer> CreatePlatformAnnotationRenderer() {
  return std::make_unique<X11AnnotationRenderer>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // __linux__
