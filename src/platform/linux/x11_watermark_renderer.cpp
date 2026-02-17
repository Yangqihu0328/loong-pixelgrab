// Copyright 2026 The loong-pixelgrab Authors
// Linux watermark renderer — Cairo + Pango implementation.

#include "watermark/watermark_renderer.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "core/logger.h"

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

namespace pixelgrab {
namespace internal {

namespace {

/// Resolve the watermark position to absolute (x, y) given image size and
/// estimated text bounds.  Returns top-down coordinates.
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

class X11WatermarkRenderer : public WatermarkRenderer {
 public:
  X11WatermarkRenderer() = default;
  ~X11WatermarkRenderer() override = default;

  bool ApplyTextWatermark(Image* image,
                          const PixelGrabTextWatermarkConfig& config) override {
    if (!image || !config.text) return false;

    int w = image->width();
    int h = image->height();
    int stride = image->stride();
    uint8_t* pixels = image->mutable_data();

    // Create a Cairo image surface backed by the existing pixel buffer.
    // Our Image is BGRA8 which matches CAIRO_FORMAT_ARGB32 on little-endian.
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        pixels, CAIRO_FORMAT_ARGB32, w, h, stride);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
      PIXELGRAB_LOG_ERROR("Cairo surface creation failed");
      cairo_surface_destroy(surface);
      return false;
    }

    cairo_t* cr = cairo_create(surface);

    // Font setup via Pango.
    int font_size = (config.font_size > 0) ? config.font_size : 16;
    const char* font_name = config.font_name ? config.font_name : "Sans";

    PangoLayout* layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, config.text, -1);

    // Build Pango font description.
    std::string font_desc_str =
        std::string(font_name) + " " + std::to_string(font_size);
    PangoFontDescription* font_desc =
        pango_font_description_from_string(font_desc_str.c_str());
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    // Measure text bounds.
    int text_w = 0, text_h = 0;
    pango_layout_get_pixel_size(layout, &text_w, &text_h);

    // Color (ARGB).
    uint32_t argb = config.color;
    if (argb == 0) argb = 0x80FFFFFF;  // default: semi-transparent white
    double a = static_cast<double>((argb >> 24) & 0xFF) / 255.0;
    double r = static_cast<double>((argb >> 16) & 0xFF) / 255.0;
    double g = static_cast<double>((argb >> 8) & 0xFF) / 255.0;
    double b = static_cast<double>(argb & 0xFF) / 255.0;
    cairo_set_source_rgba(cr, r, g, b, a);

    // Resolve position.
    int px = 0, py = 0;
    ResolvePosition(config, w, h, text_w, text_h, &px, &py);

    // Draw text.
    cairo_move_to(cr, static_cast<double>(px), static_cast<double>(py));
    pango_cairo_show_layout(cr, layout);

    // Cleanup.
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return true;
  }

  bool ApplyTiledTextWatermark(
      Image* image, const PixelGrabTextWatermarkConfig& config,
      float angle_deg, int spacing_x, int spacing_y) override {
    (void)image; (void)config;
    (void)angle_deg; (void)spacing_x; (void)spacing_y;
    PIXELGRAB_LOG_WARN("ApplyTiledTextWatermark not implemented on Linux");
    return false;
  }

  bool ApplyImageWatermark(Image* target, const Image& watermark,
                           int x, int y, float opacity) override {
    if (!target) return false;
    if (opacity <= 0.0f) return true;

    int tw = target->width();
    int th = target->height();
    int ts = target->stride();
    uint8_t* tpx = target->mutable_data();

    int ww = watermark.width();
    int wh = watermark.height();
    int ws = watermark.stride();
    const uint8_t* wpx = watermark.data();

    float alpha_scale = std::min(opacity, 1.0f);

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
            std::min(255.0f, sp[3] * sa + dp[3] * da));          // A
      }
    }

    return true;
  }
};

std::unique_ptr<WatermarkRenderer> CreatePlatformWatermarkRenderer() {
  return std::make_unique<X11WatermarkRenderer>();
}

}  // namespace internal
}  // namespace pixelgrab
