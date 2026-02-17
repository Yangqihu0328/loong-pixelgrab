// Copyright 2026 The loong-pixelgrab Authors

#include "watermark/watermark_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/logger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>   // IStream — required by GDI+ headers.

// GDI+ headers (must come after windows.h + objidl.h)
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

namespace pixelgrab {
namespace internal {

namespace {

/// Resolve the watermark position to absolute (x, y) given image size and
/// estimated text bounds.
void ResolvePosition(const PixelGrabTextWatermarkConfig& config,
                     int img_w, int img_h, int text_w, int text_h,
                     int* out_x, int* out_y) {
  int margin = (config.margin > 0) ? config.margin : 10;
  switch (config.position) {
    case kPixelGrabWatermarkTopLeft:
      *out_x = margin;
      *out_y = margin;
      break;
    case kPixelGrabWatermarkTopRight:
      *out_x = img_w - text_w - margin;
      *out_y = margin;
      break;
    case kPixelGrabWatermarkBottomLeft:
      *out_x = margin;
      *out_y = img_h - text_h - margin;
      break;
    case kPixelGrabWatermarkBottomRight:
      *out_x = img_w - text_w - margin;
      *out_y = img_h - text_h - margin;
      break;
    case kPixelGrabWatermarkCenter:
      *out_x = (img_w - text_w) / 2;
      *out_y = (img_h - text_h) / 2;
      break;
    case kPixelGrabWatermarkCustom:
    default:
      *out_x = config.x;
      *out_y = config.y;
      break;
  }
}

}  // namespace

class WinWatermarkRenderer : public WatermarkRenderer {
 public:
  WinWatermarkRenderer() {
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok) {
      gdiplus_token_ = token;
      gdiplus_initialized_ = true;
    } else {
      PIXELGRAB_LOG_ERROR("GDI+ startup failed in WatermarkRenderer");
    }
  }

  ~WinWatermarkRenderer() override {
    if (gdiplus_initialized_) {
      Gdiplus::GdiplusShutdown(gdiplus_token_);
    }
  }

  bool ApplyTextWatermark(Image* image,
                          const PixelGrabTextWatermarkConfig& config) override {
    if (!image || !config.text) return false;
    if (!gdiplus_initialized_) return false;

    int w = image->width();
    int h = image->height();
    int stride = image->stride();
    uint8_t* pixels = image->mutable_data();

    // Create GDI+ Bitmap backed by the existing pixel buffer.
    Gdiplus::Bitmap bmp(w, h, stride, PixelFormat32bppARGB, pixels);
    Gdiplus::Graphics graphics(&bmp);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

    // Font.
    int font_size = (config.font_size > 0) ? config.font_size : 16;
    const char* font_name = config.font_name ? config.font_name : "Arial";

    // Convert font name to wide string.
    int wlen = MultiByteToWideChar(CP_UTF8, 0, font_name, -1, nullptr, 0);
    std::wstring wfont(static_cast<size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, font_name, -1, wfont.data(), wlen);

    Gdiplus::Font font(wfont.c_str(), static_cast<Gdiplus::REAL>(font_size),
                       Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

    // Color (ARGB).
    uint32_t argb = config.color;
    if (argb == 0) argb = 0x80FFFFFF;  // default: semi-transparent white
    uint8_t a = static_cast<uint8_t>((argb >> 24) & 0xFF);
    uint8_t r = static_cast<uint8_t>((argb >> 16) & 0xFF);
    uint8_t g = static_cast<uint8_t>((argb >> 8) & 0xFF);
    uint8_t b = static_cast<uint8_t>(argb & 0xFF);
    Gdiplus::SolidBrush brush(Gdiplus::Color(a, r, g, b));

    // Convert text to wide string.
    int tlen = MultiByteToWideChar(CP_UTF8, 0, config.text, -1, nullptr, 0);
    std::wstring wtext(static_cast<size_t>(tlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, config.text, -1, wtext.data(), tlen);

    // Measure text bounds.
    Gdiplus::RectF text_rect;
    graphics.MeasureString(wtext.c_str(), -1, &font, Gdiplus::PointF(0, 0),
                           &text_rect);
    int text_w = static_cast<int>(text_rect.Width + 0.5f);
    int text_h = static_cast<int>(text_rect.Height + 0.5f);

    // Resolve position.
    int px = 0, py = 0;
    ResolvePosition(config, w, h, text_w, text_h, &px, &py);

    // Apply rotation around text center if rotation != 0.
    bool rotated = (config.rotation != 0.0f);
    if (rotated) {
      Gdiplus::REAL cx = static_cast<Gdiplus::REAL>(px) + text_rect.Width * 0.5f;
      Gdiplus::REAL cy = static_cast<Gdiplus::REAL>(py) + text_rect.Height * 0.5f;
      graphics.TranslateTransform(cx, cy);
      graphics.RotateTransform(config.rotation);
      graphics.TranslateTransform(-cx, -cy);
    }

    // Build text outline path for stroke rendering (black fill + white edge).
    Gdiplus::GraphicsPath text_path;
    {
      Gdiplus::FontFamily family;
      font.GetFamily(&family);
      text_path.AddString(wtext.c_str(), -1, &family,
                          font.GetStyle(),
                          font.GetSize(),
                          Gdiplus::PointF(static_cast<Gdiplus::REAL>(px),
                                          static_cast<Gdiplus::REAL>(py)),
                          nullptr);
    }

    // 1) White outline (stroke).
    {
      constexpr Gdiplus::REAL kOutlineWidth = 3.0f;
      Gdiplus::Pen outline_pen(Gdiplus::Color(a, 255, 255, 255), kOutlineWidth);
      outline_pen.SetLineJoin(Gdiplus::LineJoinRound);
      graphics.DrawPath(&outline_pen, &text_path);
    }

    // 2) Black fill.
    {
      Gdiplus::SolidBrush fill_brush(Gdiplus::Color(a, 0, 0, 0));
      Gdiplus::Status status = graphics.FillPath(&fill_brush, &text_path);
      if (status != Gdiplus::Ok) {
        PIXELGRAB_LOG_ERROR("GDI+ FillPath failed with status {}",
                           static_cast<int>(status));
        return false;
      }
    }

    // Restore transform.
    if (rotated) {
      graphics.ResetTransform();
    }

    return true;
  }

  bool ApplyTiledTextWatermark(
      Image* image, const PixelGrabTextWatermarkConfig& config,
      float angle_deg, int spacing_x, int spacing_y) override {
    if (!image || !config.text) return false;
    if (!gdiplus_initialized_) return false;

    int w = image->width();
    int h = image->height();
    int stride = image->stride();
    uint8_t* pixels = image->mutable_data();

    Gdiplus::Bitmap bmp(w, h, stride, PixelFormat32bppARGB, pixels);
    Gdiplus::Graphics graphics(&bmp);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

    // Font.
    int font_size = (config.font_size > 0) ? config.font_size : 16;
    const char* font_name = config.font_name ? config.font_name : "Arial";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, font_name, -1, nullptr, 0);
    std::wstring wfont(static_cast<size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, font_name, -1, wfont.data(), wlen);
    Gdiplus::Font font(wfont.c_str(), static_cast<Gdiplus::REAL>(font_size),
                       Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

    // Color — extract alpha only; use black fill + white outline.
    uint32_t argb = config.color;
    if (argb == 0) argb = 0x80FFFFFF;
    uint8_t a = static_cast<uint8_t>((argb >> 24) & 0xFF);

    // Text to wide string.
    int tlen = MultiByteToWideChar(CP_UTF8, 0, config.text, -1, nullptr, 0);
    std::wstring wtext(static_cast<size_t>(tlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, config.text, -1, wtext.data(), tlen);

    // Measure text.
    Gdiplus::RectF text_rect;
    graphics.MeasureString(wtext.c_str(), -1, &font,
                           Gdiplus::PointF(0, 0), &text_rect);
    int text_w = static_cast<int>(text_rect.Width + 0.5f);
    int text_h = static_cast<int>(text_rect.Height + 0.5f);

    // Font family for path-based outlined text.
    Gdiplus::FontFamily family;
    font.GetFamily(&family);

    // Tile spacing (ensure minimum).
    int sx = (spacing_x > 0) ? spacing_x : text_w + 80;
    int sy = (spacing_y > 0) ? spacing_y : text_h + 60;

    // We need to cover the entire image even after rotation.
    // Expand the tiling area by the diagonal of the image.
    int diag = static_cast<int>(std::sqrt(
        static_cast<double>(w) * w + static_cast<double>(h) * h));
    int start_x = -(diag - w) / 2;
    int start_y = -(diag - h) / 2;
    int end_x = w + (diag - w) / 2;
    int end_y = h + (diag - h) / 2;

    // Apply rotation around center of image.
    graphics.TranslateTransform(
        static_cast<Gdiplus::REAL>(w) / 2,
        static_cast<Gdiplus::REAL>(h) / 2);
    graphics.RotateTransform(angle_deg);
    graphics.TranslateTransform(
        -static_cast<Gdiplus::REAL>(w) / 2,
        -static_cast<Gdiplus::REAL>(h) / 2);

    // Tile across the expanded area.
    for (int ty = start_y; ty < end_y; ty += sy) {
      for (int tx = start_x; tx < end_x; tx += sx) {
        Gdiplus::GraphicsPath path;
        path.AddString(wtext.c_str(), -1, &family,
                       font.GetStyle(), font.GetSize(),
                       Gdiplus::PointF(static_cast<Gdiplus::REAL>(tx),
                                       static_cast<Gdiplus::REAL>(ty)),
                       nullptr);
        // White outline.
        Gdiplus::Pen pen(Gdiplus::Color(a, 255, 255, 255), 2.5f);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        graphics.DrawPath(&pen, &path);
        // Black fill.
        Gdiplus::SolidBrush brush(Gdiplus::Color(a, 0, 0, 0));
        graphics.FillPath(&brush, &path);
      }
    }

    graphics.ResetTransform();
    return true;
  }

  bool ApplyImageWatermark(Image* target, const Image& watermark,
                           int x, int y, float opacity) override {
    if (!target) return false;
    if (opacity <= 0.0f) return true;  // nothing to do

    int tw = target->width();
    int th = target->height();
    int ts = target->stride();
    uint8_t* tpx = target->mutable_data();

    int ww = watermark.width();
    int wh = watermark.height();
    int ws = watermark.stride();
    const uint8_t* wpx = watermark.data();

    float alpha_scale = (std::min)(opacity, 1.0f);

    // Simple alpha-blend loop (BGRA format).
    for (int row = 0; row < wh; ++row) {
      int dy = y + row;
      if (dy < 0 || dy >= th) continue;
      for (int col = 0; col < ww; ++col) {
        int dx = x + col;
        if (dx < 0 || dx >= tw) continue;

        const uint8_t* sp = wpx + row * ws + col * 4;
        uint8_t* dp = tpx + dy * ts + dx * 4;

        float sa = (sp[3] / 255.0f) * alpha_scale;
        float da = 1.0f - sa;

        dp[0] = static_cast<uint8_t>(sp[0] * sa + dp[0] * da);  // B
        dp[1] = static_cast<uint8_t>(sp[1] * sa + dp[1] * da);  // G
        dp[2] = static_cast<uint8_t>(sp[2] * sa + dp[2] * da);  // R
        dp[3] = static_cast<uint8_t>(
            (std::min)(255.0f, sp[3] * sa + dp[3] * da));  // A
      }
    }

    return true;
  }

 private:
  ULONG_PTR gdiplus_token_ = 0;
  bool gdiplus_initialized_ = false;
};

std::unique_ptr<WatermarkRenderer> CreatePlatformWatermarkRenderer() {
  return std::make_unique<WinWatermarkRenderer>();
}

}  // namespace internal
}  // namespace pixelgrab
