// Copyright 2026 The loong-pixelgrab Authors

#include "platform/macos/mac_annotation_renderer.h"

#ifdef __APPLE__

#include "annotation/annotation_renderer.h"
#include "core/image.h"

namespace pixelgrab {
namespace internal {

bool MacAnnotationRenderer::BeginRender(Image* /*target*/) {
  // TODO(macos): Create CGBitmapContext from image data.
  return true;
}

void MacAnnotationRenderer::EndRender() {
  // TODO(macos): Release CGContext.
}

void MacAnnotationRenderer::DrawRect(int /*x*/, int /*y*/, int /*w*/,
                                     int /*h*/, const ShapeStyle& /*style*/) {
  // TODO(macos): CGContextAddRect + CGContextStrokePath / CGContextFillPath
}

void MacAnnotationRenderer::DrawEllipse(int /*cx*/, int /*cy*/, int /*rx*/,
                                        int /*ry*/,
                                        const ShapeStyle& /*style*/) {
  // TODO(macos): CGContextAddEllipseInRect
}

void MacAnnotationRenderer::DrawLine(int /*x1*/, int /*y1*/, int /*x2*/,
                                     int /*y2*/,
                                     const ShapeStyle& /*style*/) {
  // TODO(macos): CGContextMoveToPoint + CGContextAddLineToPoint
}

void MacAnnotationRenderer::DrawArrow(int /*x1*/, int /*y1*/, int /*x2*/,
                                      int /*y2*/, float /*head_size*/,
                                      const ShapeStyle& /*style*/) {
  // TODO(macos): Line + CGContextAddLines for arrowhead
}

void MacAnnotationRenderer::DrawPolyline(const Point* /*points*/,
                                         int /*count*/,
                                         const ShapeStyle& /*style*/) {
  // TODO(macos): CGContextAddLines
}

void MacAnnotationRenderer::DrawText(int /*x*/, int /*y*/,
                                     const char* /*text*/,
                                     const char* /*font_name*/,
                                     int /*font_size*/, uint32_t /*color*/) {
  // TODO(macos): CTLineDraw or NSString drawAtPoint
}

// Factory.
std::unique_ptr<AnnotationRenderer> CreatePlatformAnnotationRenderer() {
  return std::make_unique<MacAnnotationRenderer>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // __APPLE__
