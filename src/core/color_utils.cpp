// Copyright 2026 The loong-pixelgrab Authors

#include "core/color_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace pixelgrab {
namespace internal {

void RgbToHsv(const PixelGrabColor& rgb, PixelGrabColorHsv* out_hsv) {
  float r = rgb.r / 255.0f;
  float g = rgb.g / 255.0f;
  float b = rgb.b / 255.0f;

  float max_val = (std::max)({r, g, b});
  float min_val = (std::min)({r, g, b});
  float delta = max_val - min_val;

  // Value
  out_hsv->v = max_val;

  // Saturation
  if (max_val < 1e-6f) {
    out_hsv->s = 0.0f;
  } else {
    out_hsv->s = delta / max_val;
  }

  // Hue
  if (delta < 1e-6f) {
    out_hsv->h = 0.0f;
  } else if (max_val == r) {
    out_hsv->h = 60.0f * std::fmod((g - b) / delta, 6.0f);
  } else if (max_val == g) {
    out_hsv->h = 60.0f * ((b - r) / delta + 2.0f);
  } else {
    out_hsv->h = 60.0f * ((r - g) / delta + 4.0f);
  }

  if (out_hsv->h < 0.0f) {
    out_hsv->h += 360.0f;
  }
}

void HsvToRgb(const PixelGrabColorHsv& hsv, PixelGrabColor* out_rgb) {
  float h = hsv.h;
  float s = hsv.s;
  float v = hsv.v;

  // Clamp inputs
  if (h < 0.0f) h = 0.0f;
  if (h >= 360.0f) h = 0.0f;
  s = (std::max)(0.0f, (std::min)(1.0f, s));
  v = (std::max)(0.0f, (std::min)(1.0f, v));

  float c = v * s;
  float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
  float m = v - c;

  float r1, g1, b1;
  if (h < 60.0f) {
    r1 = c; g1 = x; b1 = 0.0f;
  } else if (h < 120.0f) {
    r1 = x; g1 = c; b1 = 0.0f;
  } else if (h < 180.0f) {
    r1 = 0.0f; g1 = c; b1 = x;
  } else if (h < 240.0f) {
    r1 = 0.0f; g1 = x; b1 = c;
  } else if (h < 300.0f) {
    r1 = x; g1 = 0.0f; b1 = c;
  } else {
    r1 = c; g1 = 0.0f; b1 = x;
  }

  out_rgb->r = static_cast<uint8_t>(std::round((r1 + m) * 255.0f));
  out_rgb->g = static_cast<uint8_t>(std::round((g1 + m) * 255.0f));
  out_rgb->b = static_cast<uint8_t>(std::round((b1 + m) * 255.0f));
  out_rgb->a = 255;
}

void ColorToHex(const PixelGrabColor& color, char* buf, int buf_size,
                bool include_alpha) {
  if (!buf || buf_size < 2) return;

  if (include_alpha) {
    if (buf_size >= 10) {
      std::snprintf(buf, buf_size, "#%02X%02X%02X%02X", color.r, color.g,
                    color.b, color.a);
    }
  } else {
    if (buf_size >= 8) {
      std::snprintf(buf, buf_size, "#%02X%02X%02X", color.r, color.g,
                    color.b);
    }
  }
}

namespace {

// Parse a single hex character to its value (0-15). Returns -1 on failure.
int HexCharToVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

}  // namespace

bool ColorFromHex(const char* hex, PixelGrabColor* out_color) {
  if (!hex || !out_color) return false;

  // Skip '#' prefix if present.
  if (hex[0] == '#') ++hex;

  size_t len = std::strlen(hex);

  if (len == 3) {
    // #RGB → expand to #RRGGBB
    int r = HexCharToVal(hex[0]);
    int g = HexCharToVal(hex[1]);
    int b = HexCharToVal(hex[2]);
    if (r < 0 || g < 0 || b < 0) return false;
    out_color->r = static_cast<uint8_t>(r * 17);  // 0xF → 0xFF
    out_color->g = static_cast<uint8_t>(g * 17);
    out_color->b = static_cast<uint8_t>(b * 17);
    out_color->a = 255;
    return true;
  }

  if (len == 6) {
    // #RRGGBB
    for (int i = 0; i < 6; ++i) {
      if (HexCharToVal(hex[i]) < 0) return false;
    }
    out_color->r =
        static_cast<uint8_t>(HexCharToVal(hex[0]) * 16 + HexCharToVal(hex[1]));
    out_color->g =
        static_cast<uint8_t>(HexCharToVal(hex[2]) * 16 + HexCharToVal(hex[3]));
    out_color->b =
        static_cast<uint8_t>(HexCharToVal(hex[4]) * 16 + HexCharToVal(hex[5]));
    out_color->a = 255;
    return true;
  }

  if (len == 8) {
    // #RRGGBBAA
    for (int i = 0; i < 8; ++i) {
      if (HexCharToVal(hex[i]) < 0) return false;
    }
    out_color->r =
        static_cast<uint8_t>(HexCharToVal(hex[0]) * 16 + HexCharToVal(hex[1]));
    out_color->g =
        static_cast<uint8_t>(HexCharToVal(hex[2]) * 16 + HexCharToVal(hex[3]));
    out_color->b =
        static_cast<uint8_t>(HexCharToVal(hex[4]) * 16 + HexCharToVal(hex[5]));
    out_color->a =
        static_cast<uint8_t>(HexCharToVal(hex[6]) * 16 + HexCharToVal(hex[7]));
    return true;
  }

  return false;
}

}  // namespace internal
}  // namespace pixelgrab
