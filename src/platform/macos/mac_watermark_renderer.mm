// Copyright 2026 The loong-pixelgrab Authors
// macOS watermark renderer — CoreGraphics + CoreText implementation.

#include "watermark/watermark_renderer.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "core/logger.h"

#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#import <Foundation/Foundation.h>

namespace pixelgrab {
namespace internal {

namespace {

/// Resolve the watermark position to absolute (x, y) given image size and
/// estimated text bounds.  Returns CoreGraphics coordinates (origin at
/// bottom-left).
void ResolvePosition(const PixelGrabTextWatermarkConfig& config,
                     int img_w, int img_h, int text_w, int text_h,
                     int* out_x, int* out_y) {
  int margin = (config.margin > 0) ? config.margin : 10;
  int top_y = 0;  // top-down Y for human-readable logic

  switch (config.position) {
    case kPixelGrabWatermarkTopLeft:
      *out_x = margin;
      top_y = margin;
      break;
    case kPixelGrabWatermarkTopRight:
      *out_x = img_w - text_w - margin;
      top_y = margin;
      break;
    case kPixelGrabWatermarkBottomLeft:
      *out_x = margin;
      top_y = img_h - text_h - margin;
      break;
    case kPixelGrabWatermarkBottomRight:
      *out_x = img_w - text_w - margin;
      top_y = img_h - text_h - margin;
      break;
    case kPixelGrabWatermarkCenter:
      *out_x = (img_w - text_w) / 2;
      top_y = (img_h - text_h) / 2;
      break;
    case kPixelGrabWatermarkCustom:
    default:
      *out_x = config.x;
      top_y = config.y;
      break;
  }

  // CoreGraphics Y-axis is bottom-up, so flip.
  *out_y = img_h - top_y - text_h;
}

}  // namespace

class MacWatermarkRenderer : public WatermarkRenderer {
 public:
  MacWatermarkRenderer() = default;
  ~MacWatermarkRenderer() override = default;

  bool ApplyTextWatermark(Image* image,
                          const PixelGrabTextWatermarkConfig& config) override {
    if (!image || !config.text) return false;

    int w = image->width();
    int h = image->height();
    int stride = image->stride();
    uint8_t* pixels = image->mutable_data();

    // Create a CGBitmapContext backed by the existing pixel buffer.
    // Our Image is BGRA8; on macOS we use kCGBitmapByteOrder32Little |
    // kCGImageAlphaPremultipliedFirst which maps to little-endian BGRA.
    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    if (!color_space) return false;

    CGContextRef cg_ctx = CGBitmapContextCreate(
        pixels, static_cast<size_t>(w), static_cast<size_t>(h),
        8, static_cast<size_t>(stride), color_space,
        kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(color_space);
    if (!cg_ctx) return false;

    // Font.
    int font_size = (config.font_size > 0) ? config.font_size : 16;
    const char* font_name = config.font_name ? config.font_name : "Helvetica";

    NSString* ns_font_name =
        [NSString stringWithUTF8String:font_name];
    CTFontRef ct_font =
        CTFontCreateWithName((__bridge CFStringRef)ns_font_name,
                             static_cast<CGFloat>(font_size), nullptr);
    if (!ct_font) {
      // Fallback to system font.
      ct_font = CTFontCreateUIFontForLanguage(
          kCTFontUIFontSystem, static_cast<CGFloat>(font_size), nullptr);
    }

    // Color (ARGB).
    uint32_t argb = config.color;
    if (argb == 0) argb = 0x80FFFFFF;  // default: semi-transparent white
    CGFloat a = static_cast<CGFloat>((argb >> 24) & 0xFF) / 255.0;
    CGFloat r = static_cast<CGFloat>((argb >> 16) & 0xFF) / 255.0;
    CGFloat g = static_cast<CGFloat>((argb >> 8) & 0xFF) / 255.0;
    CGFloat b = static_cast<CGFloat>(argb & 0xFF) / 255.0;

    CGColorRef cg_color = CGColorCreateGenericRGB(r, g, b, a);

    // Convert text to CFString.
    NSString* ns_text = [NSString stringWithUTF8String:config.text];
    CFStringRef cf_text = (__bridge CFStringRef)ns_text;

    // Create attributed string.
    CFStringRef keys[] = {kCTFontAttributeName, kCTForegroundColorAttributeName};
    CFTypeRef values[] = {ct_font, cg_color};
    CFDictionaryRef attrs = CFDictionaryCreate(
        kCFAllocatorDefault, (const void**)keys, (const void**)values, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFAttributedStringRef attr_str =
        CFAttributedStringCreate(kCFAllocatorDefault, cf_text, attrs);
    CFRelease(attrs);

    // Create CTLine for measurement and drawing.
    CTLineRef line = CTLineCreateWithAttributedString(attr_str);
    CFRelease(attr_str);

    // Measure text bounds.
    CGFloat ascent = 0, descent = 0, leading = 0;
    double line_width = CTLineGetTypographicBounds(line, &ascent, &descent,
                                                   &leading);
    int text_w = static_cast<int>(line_width + 0.5);
    int text_h = static_cast<int>(ascent + descent + leading + 0.5);

    // Resolve position.
    int px = 0, py = 0;
    ResolvePosition(config, w, h, text_w, text_h, &px, &py);

    // Draw text.
    CGContextSetTextPosition(cg_ctx, static_cast<CGFloat>(px),
                             static_cast<CGFloat>(py) + descent);
    CTLineDraw(line, cg_ctx);

    // Cleanup.
    CFRelease(line);
    CGColorRelease(cg_color);
    CFRelease(ct_font);
    CGContextRelease(cg_ctx);

    return true;
  }

  bool ApplyTiledTextWatermark(
      Image* image, const PixelGrabTextWatermarkConfig& config,
      float angle_deg, int spacing_x, int spacing_y) override {
    (void)image; (void)config;
    (void)angle_deg; (void)spacing_x; (void)spacing_y;
    PIXELGRAB_LOG_WARN("ApplyTiledTextWatermark not implemented on macOS");
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
  return std::make_unique<MacWatermarkRenderer>();
}

}  // namespace internal
}  // namespace pixelgrab
