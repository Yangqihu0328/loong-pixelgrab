// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_CORE_COLOR_UTILS_H_
#define PIXELGRAB_CORE_COLOR_UTILS_H_

#include "pixelgrab/pixelgrab.h"

namespace pixelgrab {
namespace internal {

/// Convert RGB color to HSV color space.
/// H: [0, 360), S: [0, 1], V: [0, 1]
void RgbToHsv(const PixelGrabColor& rgb, PixelGrabColorHsv* out_hsv);

/// Convert HSV color to RGB color space.
/// Alpha is set to 255.
void HsvToRgb(const PixelGrabColorHsv& hsv, PixelGrabColor* out_rgb);

/// Convert RGB color to hex string.
/// If include_alpha is true, format is "#RRGGBBAA", otherwise "#RRGGBB".
/// buf must be at least 10 bytes.
void ColorToHex(const PixelGrabColor& color, char* buf, int buf_size,
                bool include_alpha);

/// Parse a hex color string to RGB.
/// Supports "#RGB", "#RRGGBB", "#RRGGBBAA" (with or without '#' prefix).
/// Returns true on success.
bool ColorFromHex(const char* hex, PixelGrabColor* out_color);

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_CORE_COLOR_UTILS_H_
