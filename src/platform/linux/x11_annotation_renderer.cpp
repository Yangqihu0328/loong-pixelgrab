// Copyright 2026 The loong-pixelgrab Authors

#include "platform/linux/x11_annotation_renderer.h"

#if defined(__linux__)

#include "annotation/annotation_renderer.h"
#include "core/image.h"

namespace pixelgrab {
namespace internal {

bool X11AnnotationRenderer::BeginRender(Image* /*target*/) {
  // TODO(linux): Create cairo_surface_t from image data.
  return true;
}

void X11AnnotationRenderer::EndRender() {
  // TODO(linux): Destroy cairo surface/context.
}

void X11AnnotationRenderer::DrawRect(int /*x*/, int /*y*/, int /*w*/,
                                     int /*h*/, const ShapeStyle& /*style*/) {
  // TODO(linux): cairo_rectangle + cairo_stroke / cairo_fill
}

void X11AnnotationRenderer::DrawEllipse(int /*cx*/, int /*cy*/, int /*rx*/,
                                        int /*ry*/,
                                        const ShapeStyle& /*style*/) {
  // TODO(linux): cairo_arc (scaled for ellipse)
}

void X11AnnotationRenderer::DrawLine(int /*x1*/, int /*y1*/, int /*x2*/,
                                     int /*y2*/,
                                     const ShapeStyle& /*style*/) {
  // TODO(linux): cairo_move_to + cairo_line_to + cairo_stroke
}

void X11AnnotationRenderer::DrawArrow(int /*x1*/, int /*y1*/, int /*x2*/,
                                      int /*y2*/, float /*head_size*/,
                                      const ShapeStyle& /*style*/) {
  // TODO(linux): Line + triangle arrowhead
}

void X11AnnotationRenderer::DrawPolyline(const Point* /*points*/,
                                         int /*count*/,
                                         const ShapeStyle& /*style*/) {
  // TODO(linux): cairo_move_to + cairo_line_to sequence
}

void X11AnnotationRenderer::DrawText(int /*x*/, int /*y*/,
                                     const char* /*text*/,
                                     const char* /*font_name*/,
                                     int /*font_size*/, uint32_t /*color*/) {
  // TODO(linux): pango_layout or cairo_show_text
}

// Factory.
std::unique_ptr<AnnotationRenderer> CreatePlatformAnnotationRenderer() {
  return std::make_unique<X11AnnotationRenderer>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // __linux__
