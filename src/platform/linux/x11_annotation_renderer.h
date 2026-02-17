// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_PLATFORM_LINUX_X11_ANNOTATION_RENDERER_H_
#define PIXELGRAB_PLATFORM_LINUX_X11_ANNOTATION_RENDERER_H_

#include "annotation/annotation_renderer.h"

namespace pixelgrab {
namespace internal {

/// Linux annotation renderer stub (Cairo).
class X11AnnotationRenderer : public AnnotationRenderer {
 public:
  X11AnnotationRenderer() = default;
  ~X11AnnotationRenderer() override = default;

  bool BeginRender(Image* target) override;
  void EndRender() override;

  void DrawRect(int x, int y, int w, int h, const ShapeStyle& style) override;
  void DrawEllipse(int cx, int cy, int rx, int ry,
                   const ShapeStyle& style) override;
  void DrawLine(int x1, int y1, int x2, int y2,
                const ShapeStyle& style) override;
  void DrawArrow(int x1, int y1, int x2, int y2, float head_size,
                 const ShapeStyle& style) override;
  void DrawPolyline(const Point* points, int count,
                    const ShapeStyle& style) override;
  void DrawText(int x, int y, const char* text, const char* font_name,
                int font_size, uint32_t color) override;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PLATFORM_LINUX_X11_ANNOTATION_RENDERER_H_
