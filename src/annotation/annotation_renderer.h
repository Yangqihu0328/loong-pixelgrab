// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_ANNOTATION_ANNOTATION_RENDERER_H_
#define PIXELGRAB_ANNOTATION_ANNOTATION_RENDERER_H_

#include <cstdint>
#include <memory>

#include "annotation/shape.h"

namespace pixelgrab {
namespace internal {

class Image;

/// Abstract interface for platform-specific annotation rendering.
///
/// Each platform implements this using its native 2D graphics API:
///   Windows: GDI+  |  macOS: CoreGraphics  |  Linux: Cairo
///
/// Pixel effects (mosaic, blur) are implemented platform-independently
/// in AnnotationSession since they operate on raw pixel data.
class AnnotationRenderer {
 public:
  virtual ~AnnotationRenderer() = default;

  // Non-copyable.
  AnnotationRenderer(const AnnotationRenderer&) = delete;
  AnnotationRenderer& operator=(const AnnotationRenderer&) = delete;

  /// Begin rendering to the target image.
  /// Creates a platform graphics context backed by the image pixel data.
  /// @return true on success.
  virtual bool BeginRender(Image* target) = 0;

  /// Finish rendering and flush all drawing operations to the image.
  virtual void EndRender() = 0;

  // --- Primitive drawing operations ---

  virtual void DrawRect(int x, int y, int w, int h,
                        const ShapeStyle& style) = 0;

  virtual void DrawEllipse(int cx, int cy, int rx, int ry,
                           const ShapeStyle& style) = 0;

  virtual void DrawLine(int x1, int y1, int x2, int y2,
                        const ShapeStyle& style) = 0;

  virtual void DrawArrow(int x1, int y1, int x2, int y2, float head_size,
                         const ShapeStyle& style) = 0;

  virtual void DrawPolyline(const Point* points, int count,
                            const ShapeStyle& style) = 0;

  virtual void DrawText(int x, int y, const char* text,
                        const char* font_name, int font_size,
                        uint32_t color) = 0;

 protected:
  AnnotationRenderer() = default;
};

/// Factory function — returns the platform-native renderer.
/// Defined per-platform in platform/<os>/xxx_annotation_renderer.cpp.
std::unique_ptr<AnnotationRenderer> CreatePlatformAnnotationRenderer();

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_ANNOTATION_ANNOTATION_RENDERER_H_
