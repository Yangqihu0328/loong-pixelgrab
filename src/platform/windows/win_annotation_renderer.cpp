// Copyright 2026 The loong-pixelgrab Authors

#include "platform/windows/win_annotation_renderer.h"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>   // IStream — required by GDI+ headers.

// GDI+ headers.
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// Undo Windows macro pollution that conflicts with our method names.
#ifdef DrawText
#undef DrawText
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "core/image.h"

namespace pixelgrab {
namespace internal {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

// Convert ARGB (0xAARRGGBB) to Gdiplus::Color.
Gdiplus::Color ToGdipColor(uint32_t argb) {
  uint8_t a = (argb >> 24) & 0xFF;
  uint8_t r = (argb >> 16) & 0xFF;
  uint8_t g = (argb >> 8) & 0xFF;
  uint8_t b = argb & 0xFF;
  return Gdiplus::Color(a, r, g, b);
}

// Convert UTF-8 to wide string.
std::wstring Utf8ToWide(const char* utf8) {
  if (!utf8 || !utf8[0]) return L"";
  int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
  if (len <= 0) return L"";
  std::wstring result(len - 1, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &result[0], len);
  return result;
}

}  // namespace

// ---------------------------------------------------------------------------
// WinAnnotationRenderer
// ---------------------------------------------------------------------------

WinAnnotationRenderer::WinAnnotationRenderer() {
  Gdiplus::GdiplusStartupInput input;
  ULONG_PTR token = 0;
  if (Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok) {
    gdiplus_token_ = static_cast<unsigned long>(token);
    gdiplus_initialized_ = true;
  }
}

WinAnnotationRenderer::~WinAnnotationRenderer() {
  EndRender();
  if (gdiplus_initialized_) {
    Gdiplus::GdiplusShutdown(static_cast<ULONG_PTR>(gdiplus_token_));
  }
}

bool WinAnnotationRenderer::BeginRender(Image* target) {
  if (!target || !gdiplus_initialized_) return false;

  target_ = target;

  int w = target->width();
  int h = target->height();
  int img_stride = target->stride();

  // Create a STANDALONE GDI+ Bitmap (GDI+ manages its own pixel buffer).
  // We explicitly copy pixels in/out via LockBits instead of wrapping
  // the image's scan0 pointer, because GDI+ Bitmap-from-scan0 does not
  // reliably sync pixel data back to the external buffer, which causes
  // shapes drawn after a pixel effect (mosaic/blur) to appear underneath.
  auto* bmp = new Gdiplus::Bitmap(w, h, PixelFormat32bppARGB);

  // Copy current image pixels → GDI+ Bitmap.
  Gdiplus::BitmapData bd;
  Gdiplus::Rect rect(0, 0, w, h);
  if (bmp->LockBits(&rect, Gdiplus::ImageLockModeWrite,
                     PixelFormat32bppARGB, &bd) == Gdiplus::Ok) {
    const uint8_t* src = target->data();
    uint8_t* dst = static_cast<uint8_t*>(bd.Scan0);
    for (int y = 0; y < h; ++y)
      std::memcpy(dst + y * bd.Stride, src + y * img_stride,
                  static_cast<size_t>(w) * 4);
    bmp->UnlockBits(&bd);
  }

  bitmap_ = bmp;

  auto* gfx = Gdiplus::Graphics::FromImage(bmp);
  if (!gfx) {
    delete bmp;
    bitmap_ = nullptr;
    return false;
  }
  gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  gfx->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
  graphics_ = gfx;
  return true;
}

void WinAnnotationRenderer::EndRender() {
  if (graphics_) {
    // Flush all pending GDI+ drawing operations.
    static_cast<Gdiplus::Graphics*>(graphics_)->Flush(
        Gdiplus::FlushIntentionSync);
    delete static_cast<Gdiplus::Graphics*>(graphics_);
    graphics_ = nullptr;
  }
  if (bitmap_ && target_) {
    // Copy rendered pixels back from GDI+ Bitmap → target image buffer.
    auto* bmp = static_cast<Gdiplus::Bitmap*>(bitmap_);
    int w = target_->width();
    int h = target_->height();
    int img_stride = target_->stride();

    Gdiplus::BitmapData bd;
    Gdiplus::Rect rect(0, 0, w, h);
    if (bmp->LockBits(&rect, Gdiplus::ImageLockModeRead,
                       PixelFormat32bppARGB, &bd) == Gdiplus::Ok) {
      const uint8_t* src = static_cast<const uint8_t*>(bd.Scan0);
      uint8_t* dst = target_->mutable_data();
      for (int y = 0; y < h; ++y)
        std::memcpy(dst + y * img_stride, src + y * bd.Stride,
                    static_cast<size_t>(w) * 4);
      bmp->UnlockBits(&bd);
    }
  }
  if (bitmap_) {
    delete static_cast<Gdiplus::Bitmap*>(bitmap_);
    bitmap_ = nullptr;
  }
  target_ = nullptr;
}

void WinAnnotationRenderer::DrawRect(int x, int y, int w, int h,
                                     const ShapeStyle& style) {
  auto* gfx = static_cast<Gdiplus::Graphics*>(graphics_);
  if (!gfx) return;

  if (style.filled && style.fill_color != 0) {
    Gdiplus::SolidBrush brush(ToGdipColor(style.fill_color));
    gfx->FillRectangle(&brush, x, y, w, h);
  }
  if (style.stroke_width > 0) {
    Gdiplus::Pen pen(ToGdipColor(style.stroke_color), style.stroke_width);
    gfx->DrawRectangle(&pen, x, y, w, h);
  }
}

void WinAnnotationRenderer::DrawEllipse(int cx, int cy, int rx, int ry,
                                        const ShapeStyle& style) {
  auto* gfx = static_cast<Gdiplus::Graphics*>(graphics_);
  if (!gfx) return;

  int left = cx - rx;
  int top = cy - ry;
  int width = rx * 2;
  int height = ry * 2;

  if (style.filled && style.fill_color != 0) {
    Gdiplus::SolidBrush brush(ToGdipColor(style.fill_color));
    gfx->FillEllipse(&brush, left, top, width, height);
  }
  if (style.stroke_width > 0) {
    Gdiplus::Pen pen(ToGdipColor(style.stroke_color), style.stroke_width);
    gfx->DrawEllipse(&pen, left, top, width, height);
  }
}

void WinAnnotationRenderer::DrawLine(int x1, int y1, int x2, int y2,
                                     const ShapeStyle& style) {
  auto* gfx = static_cast<Gdiplus::Graphics*>(graphics_);
  if (!gfx) return;

  Gdiplus::Pen pen(ToGdipColor(style.stroke_color), style.stroke_width);
  gfx->DrawLine(&pen, x1, y1, x2, y2);
}

void WinAnnotationRenderer::DrawArrow(int x1, int y1, int x2, int y2,
                                      float head_size,
                                      const ShapeStyle& style) {
  auto* gfx = static_cast<Gdiplus::Graphics*>(graphics_);
  if (!gfx) return;

  // Draw the line.
  Gdiplus::Pen pen(ToGdipColor(style.stroke_color), style.stroke_width);
  gfx->DrawLine(&pen, x1, y1, x2, y2);

  // Draw the arrowhead as a filled triangle.
  float dx = static_cast<float>(x2 - x1);
  float dy = static_cast<float>(y2 - y1);
  float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1.0f) return;

  dx /= len;
  dy /= len;

  // Perpendicular.
  float px = -dy;
  float py = dx;

  float hs = head_size > 0.0f ? head_size : 10.0f;

  Gdiplus::PointF arrow_points[3];
  arrow_points[0] = Gdiplus::PointF(static_cast<float>(x2),
                                    static_cast<float>(y2));
  arrow_points[1] = Gdiplus::PointF(x2 - dx * hs + px * hs * 0.4f,
                                    y2 - dy * hs + py * hs * 0.4f);
  arrow_points[2] = Gdiplus::PointF(x2 - dx * hs - px * hs * 0.4f,
                                    y2 - dy * hs - py * hs * 0.4f);

  Gdiplus::SolidBrush brush(ToGdipColor(style.stroke_color));
  gfx->FillPolygon(&brush, arrow_points, 3);
}

void WinAnnotationRenderer::DrawPolyline(const Point* points, int count,
                                         const ShapeStyle& style) {
  auto* gfx = static_cast<Gdiplus::Graphics*>(graphics_);
  if (!gfx || !points || count < 2) return;

  std::vector<Gdiplus::Point> gdip_points(count);
  for (int i = 0; i < count; ++i) {
    gdip_points[i] = Gdiplus::Point(points[i].x, points[i].y);
  }

  Gdiplus::Pen pen(ToGdipColor(style.stroke_color), style.stroke_width);
  pen.SetLineJoin(Gdiplus::LineJoinRound);
  pen.SetStartCap(Gdiplus::LineCapRound);
  pen.SetEndCap(Gdiplus::LineCapRound);
  gfx->DrawLines(&pen, gdip_points.data(), count);
}

void WinAnnotationRenderer::DrawText(int x, int y, const char* text,
                                     const char* font_name, int font_size,
                                     uint32_t color) {
  auto* gfx = static_cast<Gdiplus::Graphics*>(graphics_);
  if (!gfx || !text) return;

  std::wstring wtext = Utf8ToWide(text);
  std::wstring wfont = font_name ? Utf8ToWide(font_name) : L"Arial";
  if (wfont.empty()) wfont = L"Arial";

  Gdiplus::FontFamily family(wfont.c_str());
  Gdiplus::Font font(&family, static_cast<Gdiplus::REAL>(font_size),
                     Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
  Gdiplus::SolidBrush brush(ToGdipColor(color));
  Gdiplus::PointF origin(static_cast<Gdiplus::REAL>(x),
                         static_cast<Gdiplus::REAL>(y));
  gfx->DrawString(wtext.c_str(), -1, &font, origin, &brush);
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<AnnotationRenderer> CreatePlatformAnnotationRenderer() {
  return std::make_unique<WinAnnotationRenderer>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // _WIN32
