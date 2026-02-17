// Copyright 2026 The loong-pixelgrab Authors

#include "annotation/annotation_session.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace pixelgrab {
namespace internal {

// ---------------------------------------------------------------------------
// Shape::Render implementations (delegate to renderer)
// ---------------------------------------------------------------------------

void RectShape::Render(AnnotationRenderer* r) const {
  r->DrawRect(x_, y_, w_, h_, style_);
}

void EllipseShape::Render(AnnotationRenderer* r) const {
  r->DrawEllipse(cx_, cy_, rx_, ry_, style_);
}

void LineShape::Render(AnnotationRenderer* r) const {
  r->DrawLine(x1_, y1_, x2_, y2_, style_);
}

void ArrowShape::Render(AnnotationRenderer* r) const {
  r->DrawArrow(x1_, y1_, x2_, y2_, head_size_, style_);
}

void PencilShape::Render(AnnotationRenderer* r) const {
  if (!points_.empty()) {
    r->DrawPolyline(points_.data(), static_cast<int>(points_.size()), style_);
  }
}

void TextShape::Render(AnnotationRenderer* r) const {
  r->DrawText(x_, y_, text_.c_str(), font_name_.c_str(), font_size_, color_);
}

void MosaicEffect::Render(AnnotationRenderer* /*r*/) const {
  // Handled specially in Redraw() — operates on output_image_ pixels.
}

void BlurEffect::Render(AnnotationRenderer* /*r*/) const {
  // Handled specially in Redraw() — operates on output_image_ pixels.
}

// ---------------------------------------------------------------------------
// AnnotationSession
// ---------------------------------------------------------------------------

AnnotationSession::AnnotationSession(
    std::unique_ptr<Image> base_image,
    std::unique_ptr<AnnotationRenderer> renderer)
    : base_image_(std::move(base_image)),
      renderer_(std::move(renderer)) {
  // Create output image as a copy of base.
  if (base_image_) {
    std::vector<uint8_t> copy(base_image_->data(),
                              base_image_->data() + base_image_->data_size());
    output_image_ = Image::CreateFromData(base_image_->width(),
                                          base_image_->height(),
                                          base_image_->stride(),
                                          base_image_->format(),
                                          std::move(copy));
  }
}

AnnotationSession::~AnnotationSession() = default;

int AnnotationSession::AddShape(std::unique_ptr<Shape> shape) {
  if (!shape) return -1;

  int id = next_id_++;
  shape->set_id(id);

  // Record undo command.
  AnnotationCommand cmd;
  cmd.type = AnnotationCommand::Type::kAdd;
  cmd.shape_id = id;
  cmd.shape_data = nullptr;  // For kAdd, undo = remove, no data needed to save.
  undo_stack_.push_back(std::move(cmd));

  // Clear redo stack on new action.
  redo_stack_.clear();

  shapes_.push_back(std::move(shape));
  dirty_ = true;
  return id;
}

int AnnotationSession::RemoveShape(int shape_id) {
  auto it = std::find_if(shapes_.begin(), shapes_.end(),
                         [shape_id](const auto& s) { return s->id() == shape_id; });
  if (it == shapes_.end()) return -1;

  // Save shape for undo.
  AnnotationCommand cmd;
  cmd.type = AnnotationCommand::Type::kRemove;
  cmd.shape_id = shape_id;
  cmd.shape_data = (*it)->Clone();
  undo_stack_.push_back(std::move(cmd));
  redo_stack_.clear();

  shapes_.erase(it);
  dirty_ = true;
  return 0;
}

bool AnnotationSession::Undo() {
  if (undo_stack_.empty()) return false;

  auto cmd = std::move(undo_stack_.back());
  undo_stack_.pop_back();

  if (cmd.type == AnnotationCommand::Type::kAdd) {
    // Undo add = remove the last shape with this ID.
    auto it = std::find_if(shapes_.begin(), shapes_.end(),
                           [&](const auto& s) { return s->id() == cmd.shape_id; });
    if (it != shapes_.end()) {
      // Save for redo.
      AnnotationCommand redo_cmd;
      redo_cmd.type = AnnotationCommand::Type::kAdd;
      redo_cmd.shape_id = cmd.shape_id;
      redo_cmd.shape_data = (*it)->Clone();
      redo_stack_.push_back(std::move(redo_cmd));

      shapes_.erase(it);
    }
  } else if (cmd.type == AnnotationCommand::Type::kRemove) {
    // Undo remove = re-add the saved shape.
    if (cmd.shape_data) {
      AnnotationCommand redo_cmd;
      redo_cmd.type = AnnotationCommand::Type::kRemove;
      redo_cmd.shape_id = cmd.shape_id;
      redo_cmd.shape_data = nullptr;
      redo_stack_.push_back(std::move(redo_cmd));

      shapes_.push_back(std::move(cmd.shape_data));
    }
  }

  dirty_ = true;
  return true;
}

bool AnnotationSession::Redo() {
  if (redo_stack_.empty()) return false;

  auto cmd = std::move(redo_stack_.back());
  redo_stack_.pop_back();

  if (cmd.type == AnnotationCommand::Type::kAdd) {
    // Redo add = re-add the saved shape.
    if (cmd.shape_data) {
      int id = cmd.shape_data->id();
      AnnotationCommand undo_cmd;
      undo_cmd.type = AnnotationCommand::Type::kAdd;
      undo_cmd.shape_id = id;
      undo_cmd.shape_data = nullptr;
      undo_stack_.push_back(std::move(undo_cmd));

      shapes_.push_back(std::move(cmd.shape_data));
    }
  } else if (cmd.type == AnnotationCommand::Type::kRemove) {
    // Redo remove = remove the shape again.
    auto it = std::find_if(shapes_.begin(), shapes_.end(),
                           [&](const auto& s) { return s->id() == cmd.shape_id; });
    if (it != shapes_.end()) {
      AnnotationCommand undo_cmd;
      undo_cmd.type = AnnotationCommand::Type::kRemove;
      undo_cmd.shape_id = cmd.shape_id;
      undo_cmd.shape_data = (*it)->Clone();
      undo_stack_.push_back(std::move(undo_cmd));

      shapes_.erase(it);
    }
  }

  dirty_ = true;
  return true;
}

const Image* AnnotationSession::GetResult() {
  if (dirty_) {
    Redraw();
  }
  return output_image_.get();
}

std::unique_ptr<Image> AnnotationSession::Export() {
  if (dirty_) {
    Redraw();
  }
  if (!output_image_) return nullptr;
  std::vector<uint8_t> copy(output_image_->data(),
                            output_image_->data() + output_image_->data_size());
  return Image::CreateFromData(output_image_->width(), output_image_->height(),
                               output_image_->stride(), output_image_->format(),
                               std::move(copy));
}

// ---------------------------------------------------------------------------
// Redraw: base image + all shapes
// ---------------------------------------------------------------------------

void AnnotationSession::Redraw() {
  if (!base_image_ || !output_image_) return;

  // 1. Copy base → output.
  std::memcpy(output_image_->mutable_data(), base_image_->data(),
              base_image_->data_size());

  // 2. Render shapes in order, interleaving GDI+ segments with pixel effects.
  //    This ensures that shapes drawn after a mosaic/blur appear ON TOP of it.
  bool gfx_active = false;

  for (const auto& shape : shapes_) {
    if (shape->type() == ShapeType::kMosaic ||
        shape->type() == ShapeType::kBlur) {
      // Flush current GDI+ segment before applying pixel effect.
      if (gfx_active) {
        renderer_->EndRender();
        gfx_active = false;
      }
      // Apply pixel effect directly on output pixels.
      if (shape->type() == ShapeType::kMosaic) {
        auto* m = static_cast<const MosaicEffect*>(shape.get());
        ApplyMosaic(output_image_.get(), m->x_, m->y_, m->w_, m->h_,
                    m->block_size_);
      } else {
        auto* b = static_cast<const BlurEffect*>(shape.get());
        ApplyBlur(output_image_.get(), b->x_, b->y_, b->w_, b->h_, b->radius_);
      }
    } else {
      // Start a new GDI+ segment if not currently active.
      if (!gfx_active && renderer_) {
        if (renderer_->BeginRender(output_image_.get())) {
          gfx_active = true;
        }
      }
      if (gfx_active) {
        shape->Render(renderer_.get());
      }
    }
  }

  if (gfx_active) {
    renderer_->EndRender();
  }

  dirty_ = false;
}

// ---------------------------------------------------------------------------
// Mosaic: block-average pixelation
// ---------------------------------------------------------------------------

void AnnotationSession::ApplyMosaic(Image* image, int x, int y, int w, int h,
                                     int block_size) {
  if (!image || block_size <= 1) return;

  int img_w = image->width();
  int img_h = image->height();
  int stride = image->stride();
  uint8_t* data = image->mutable_data();

  // Clamp region to image bounds.
  int x0 = (std::max)(0, x);
  int y0 = (std::max)(0, y);
  int x1 = (std::min)(img_w, x + w);
  int y1 = (std::min)(img_h, y + h);

  for (int by = y0; by < y1; by += block_size) {
    for (int bx = x0; bx < x1; bx += block_size) {
      int bx1 = (std::min)(bx + block_size, x1);
      int by1 = (std::min)(by + block_size, y1);
      int count = 0;
      uint32_t sum_b = 0, sum_g = 0, sum_r = 0, sum_a = 0;

      // Compute block average.
      for (int py = by; py < by1; ++py) {
        for (int px = bx; px < bx1; ++px) {
          const uint8_t* p = data + py * stride + px * 4;
          sum_b += p[0];
          sum_g += p[1];
          sum_r += p[2];
          sum_a += p[3];
          ++count;
        }
      }

      if (count == 0) continue;
      uint8_t avg_b = static_cast<uint8_t>(sum_b / count);
      uint8_t avg_g = static_cast<uint8_t>(sum_g / count);
      uint8_t avg_r = static_cast<uint8_t>(sum_r / count);
      uint8_t avg_a = static_cast<uint8_t>(sum_a / count);

      // Fill block with average color.
      for (int py = by; py < by1; ++py) {
        for (int px = bx; px < bx1; ++px) {
          uint8_t* p = data + py * stride + px * 4;
          p[0] = avg_b;
          p[1] = avg_g;
          p[2] = avg_r;
          p[3] = avg_a;
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Blur: 3-pass box blur approximating Gaussian
// ---------------------------------------------------------------------------

namespace {

// Horizontal box blur pass on a sub-region.
void BoxBlurH(uint8_t* data, int stride, int /*img_w*/, int /*img_h*/,
              int x0, int y0, int x1, int y1, int radius) {
  if (radius <= 0) return;
  int diam = radius * 2 + 1;

  for (int py = y0; py < y1; ++py) {
    // Running sums for B, G, R, A.
    int32_t sb = 0, sg = 0, sr = 0, sa = 0;
    int cnt = 0;

    // Initialize window for the first pixel in this row.
    for (int kx = x0 - radius; kx <= x0 + radius; ++kx) {
      int cx = (std::max)(x0, (std::min)(x1 - 1, kx));
      const uint8_t* p = data + py * stride + cx * 4;
      sb += p[0]; sg += p[1]; sr += p[2]; sa += p[3];
      ++cnt;
    }

    // We'll store results in a temp row to avoid read-after-write issues.
    // Using stack allocation for reasonable row widths.
    int row_len = x1 - x0;
    std::vector<uint8_t> tmp(row_len * 4);

    tmp[0] = static_cast<uint8_t>(sb / cnt);
    tmp[1] = static_cast<uint8_t>(sg / cnt);
    tmp[2] = static_cast<uint8_t>(sr / cnt);
    tmp[3] = static_cast<uint8_t>(sa / cnt);

    for (int px = x0 + 1; px < x1; ++px) {
      // Add right edge.
      int add_x = (std::min)(x1 - 1, px + radius);
      const uint8_t* pa = data + py * stride + add_x * 4;
      sb += pa[0]; sg += pa[1]; sr += pa[2]; sa += pa[3];

      // Remove left edge.
      int rem_x = (std::max)(x0, px - radius - 1);
      const uint8_t* pr = data + py * stride + rem_x * 4;
      sb -= pr[0]; sg -= pr[1]; sr -= pr[2]; sa -= pr[3];

      int idx = (px - x0) * 4;
      tmp[idx + 0] = static_cast<uint8_t>(sb / diam);
      tmp[idx + 1] = static_cast<uint8_t>(sg / diam);
      tmp[idx + 2] = static_cast<uint8_t>(sr / diam);
      tmp[idx + 3] = static_cast<uint8_t>(sa / diam);
    }

    // Write back.
    for (int px = x0; px < x1; ++px) {
      uint8_t* dst = data + py * stride + px * 4;
      int idx = (px - x0) * 4;
      dst[0] = tmp[idx + 0];
      dst[1] = tmp[idx + 1];
      dst[2] = tmp[idx + 2];
      dst[3] = tmp[idx + 3];
    }
  }
}

// Vertical box blur pass on a sub-region.
void BoxBlurV(uint8_t* data, int stride, int /*img_w*/, int /*img_h*/,
              int x0, int y0, int x1, int y1, int radius) {
  if (radius <= 0) return;
  int diam = radius * 2 + 1;

  for (int px = x0; px < x1; ++px) {
    int32_t sb = 0, sg = 0, sr = 0, sa = 0;
    int cnt = 0;

    for (int ky = y0 - radius; ky <= y0 + radius; ++ky) {
      int cy = (std::max)(y0, (std::min)(y1 - 1, ky));
      const uint8_t* p = data + cy * stride + px * 4;
      sb += p[0]; sg += p[1]; sr += p[2]; sa += p[3];
      ++cnt;
    }

    int col_len = y1 - y0;
    std::vector<uint8_t> tmp(col_len * 4);

    tmp[0] = static_cast<uint8_t>(sb / cnt);
    tmp[1] = static_cast<uint8_t>(sg / cnt);
    tmp[2] = static_cast<uint8_t>(sr / cnt);
    tmp[3] = static_cast<uint8_t>(sa / cnt);

    for (int py = y0 + 1; py < y1; ++py) {
      int add_y = (std::min)(y1 - 1, py + radius);
      const uint8_t* pa = data + add_y * stride + px * 4;
      sb += pa[0]; sg += pa[1]; sr += pa[2]; sa += pa[3];

      int rem_y = (std::max)(y0, py - radius - 1);
      const uint8_t* pr = data + rem_y * stride + px * 4;
      sb -= pr[0]; sg -= pr[1]; sr -= pr[2]; sa -= pr[3];

      int idx = (py - y0) * 4;
      tmp[idx + 0] = static_cast<uint8_t>(sb / diam);
      tmp[idx + 1] = static_cast<uint8_t>(sg / diam);
      tmp[idx + 2] = static_cast<uint8_t>(sr / diam);
      tmp[idx + 3] = static_cast<uint8_t>(sa / diam);
    }

    for (int py = y0; py < y1; ++py) {
      uint8_t* dst = data + py * stride + px * 4;
      int idx = (py - y0) * 4;
      dst[0] = tmp[idx + 0];
      dst[1] = tmp[idx + 1];
      dst[2] = tmp[idx + 2];
      dst[3] = tmp[idx + 3];
    }
  }
}

}  // namespace

void AnnotationSession::ApplyBlur(Image* image, int x, int y, int w, int h,
                                   int radius) {
  if (!image || radius <= 0) return;

  int img_w = image->width();
  int img_h = image->height();
  int stride = image->stride();
  uint8_t* data = image->mutable_data();

  int x0 = (std::max)(0, x);
  int y0 = (std::max)(0, y);
  int x1 = (std::min)(img_w, x + w);
  int y1 = (std::min)(img_h, y + h);

  if (x0 >= x1 || y0 >= y1) return;

  // 3-pass box blur approximates Gaussian blur.
  for (int pass = 0; pass < 3; ++pass) {
    BoxBlurH(data, stride, img_w, img_h, x0, y0, x1, y1, radius);
    BoxBlurV(data, stride, img_w, img_h, x0, y0, x1, y1, radius);
  }
}

}  // namespace internal
}  // namespace pixelgrab
