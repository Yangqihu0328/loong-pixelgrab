// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_PLATFORM_LINUX_X11_ANNOTATION_RENDERER_H_
#define PIXELGRAB_PLATFORM_LINUX_X11_ANNOTATION_RENDERER_H_

#include "annotation/annotation_renderer.h"

typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;

namespace pixelgrab {
namespace internal {

/// Linux annotation renderer using Cairo + Pango.
class X11AnnotationRenderer : public AnnotationRenderer {
 public:
  X11AnnotationRenderer() = default;
  ~X11AnnotationRenderer() override;

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

 private:
  Image* target_ = nullptr;
  cairo_surface_t* surface_ = nullptr;
  cairo_t* cr_ = nullptr;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PLATFORM_LINUX_X11_ANNOTATION_RENDERER_H_
