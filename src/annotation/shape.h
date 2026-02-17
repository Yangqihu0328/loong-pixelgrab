// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_ANNOTATION_SHAPE_H_
#define PIXELGRAB_ANNOTATION_SHAPE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pixelgrab {
namespace internal {

// Forward declarations.
class AnnotationRenderer;
class Image;

/// Shape types for the annotation engine.
enum class ShapeType {
  kRect,
  kEllipse,
  kLine,
  kArrow,
  kPencil,
  kText,
  kMosaic,
  kBlur,
};

/// Point in 2D space.
struct Point {
  int x;
  int y;
};

/// Shape drawing style (mirrors public PixelGrabShapeStyle).
struct ShapeStyle {
  uint32_t stroke_color;  // ARGB
  uint32_t fill_color;    // ARGB (0 = no fill)
  float stroke_width;
  bool filled;
};

/// Abstract base class for all annotation shapes.
class Shape {
 public:
  virtual ~Shape() = default;

  /// Get the shape type.
  virtual ShapeType type() const = 0;

  /// Render this shape using the given renderer.
  virtual void Render(AnnotationRenderer* renderer) const = 0;

  /// Create a deep copy of this shape.
  virtual std::unique_ptr<Shape> Clone() const = 0;

  /// Get shape ID (assigned by AnnotationSession).
  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

 protected:
  Shape() = default;
  explicit Shape(const ShapeStyle& style) : style_(style) {}
  ShapeStyle style_ = {};

 private:
  int id_ = -1;
};

// ---------------------------------------------------------------------------
// Concrete shape types
// ---------------------------------------------------------------------------

class RectShape : public Shape {
 public:
  RectShape(int x, int y, int w, int h, const ShapeStyle& style)
      : Shape(style), x_(x), y_(y), w_(w), h_(h) {}

  ShapeType type() const override { return ShapeType::kRect; }
  void Render(AnnotationRenderer* renderer) const override;
  std::unique_ptr<Shape> Clone() const override {
    auto c = std::make_unique<RectShape>(x_, y_, w_, h_, style_);
    c->set_id(id());
    return c;
  }

  int x_, y_, w_, h_;
};

class EllipseShape : public Shape {
 public:
  EllipseShape(int cx, int cy, int rx, int ry, const ShapeStyle& style)
      : Shape(style), cx_(cx), cy_(cy), rx_(rx), ry_(ry) {}

  ShapeType type() const override { return ShapeType::kEllipse; }
  void Render(AnnotationRenderer* renderer) const override;
  std::unique_ptr<Shape> Clone() const override {
    auto c = std::make_unique<EllipseShape>(cx_, cy_, rx_, ry_, style_);
    c->set_id(id());
    return c;
  }

  int cx_, cy_, rx_, ry_;
};

class LineShape : public Shape {
 public:
  LineShape(int x1, int y1, int x2, int y2, const ShapeStyle& style)
      : Shape(style), x1_(x1), y1_(y1), x2_(x2), y2_(y2) {}

  ShapeType type() const override { return ShapeType::kLine; }
  void Render(AnnotationRenderer* renderer) const override;
  std::unique_ptr<Shape> Clone() const override {
    auto c = std::make_unique<LineShape>(x1_, y1_, x2_, y2_, style_);
    c->set_id(id());
    return c;
  }

  int x1_, y1_, x2_, y2_;
};

class ArrowShape : public Shape {
 public:
  ArrowShape(int x1, int y1, int x2, int y2, float head_size,
             const ShapeStyle& style)
      : Shape(style),
        x1_(x1), y1_(y1), x2_(x2), y2_(y2), head_size_(head_size) {}

  ShapeType type() const override { return ShapeType::kArrow; }
  void Render(AnnotationRenderer* renderer) const override;
  std::unique_ptr<Shape> Clone() const override {
    auto c = std::make_unique<ArrowShape>(x1_, y1_, x2_, y2_, head_size_,
                                          style_);
    c->set_id(id());
    return c;
  }

  int x1_, y1_, x2_, y2_;
  float head_size_;
};

class PencilShape : public Shape {
 public:
  PencilShape(std::vector<Point> points, const ShapeStyle& style)
      : Shape(style), points_(std::move(points)) {}

  ShapeType type() const override { return ShapeType::kPencil; }
  void Render(AnnotationRenderer* renderer) const override;
  std::unique_ptr<Shape> Clone() const override {
    auto c = std::make_unique<PencilShape>(points_, style_);
    c->set_id(id());
    return c;
  }

  std::vector<Point> points_;
};

class TextShape : public Shape {
 public:
  TextShape(int x, int y, const std::string& text,
            const std::string& font_name, int font_size, uint32_t color)
      : x_(x), y_(y), text_(text), font_name_(font_name),
        font_size_(font_size), color_(color) {}

  ShapeType type() const override { return ShapeType::kText; }
  void Render(AnnotationRenderer* renderer) const override;
  std::unique_ptr<Shape> Clone() const override {
    auto c = std::make_unique<TextShape>(x_, y_, text_, font_name_, font_size_,
                                         color_);
    c->set_id(id());
    return c;
  }

  int x_, y_;
  std::string text_;
  std::string font_name_;
  int font_size_;
  uint32_t color_;
};

class MosaicEffect : public Shape {
 public:
  MosaicEffect(int x, int y, int w, int h, int block_size)
      : x_(x), y_(y), w_(w), h_(h), block_size_(block_size) {}

  ShapeType type() const override { return ShapeType::kMosaic; }
  void Render(AnnotationRenderer* renderer) const override;
  std::unique_ptr<Shape> Clone() const override {
    auto c = std::make_unique<MosaicEffect>(x_, y_, w_, h_, block_size_);
    c->set_id(id());
    return c;
  }

  int x_, y_, w_, h_;
  int block_size_;
};

class BlurEffect : public Shape {
 public:
  BlurEffect(int x, int y, int w, int h, int radius)
      : x_(x), y_(y), w_(w), h_(h), radius_(radius) {}

  ShapeType type() const override { return ShapeType::kBlur; }
  void Render(AnnotationRenderer* renderer) const override;
  std::unique_ptr<Shape> Clone() const override {
    auto c = std::make_unique<BlurEffect>(x_, y_, w_, h_, radius_);
    c->set_id(id());
    return c;
  }

  int x_, y_, w_, h_;
  int radius_;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_ANNOTATION_SHAPE_H_
