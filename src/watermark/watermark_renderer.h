// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_WATERMARK_WATERMARK_RENDERER_H_
#define PIXELGRAB_WATERMARK_WATERMARK_RENDERER_H_

#include <memory>

#include "core/image.h"
#include "pixelgrab/pixelgrab.h"

namespace pixelgrab {
namespace internal {

/// Abstract interface for platform-specific watermark rendering.
///
/// Each platform implements this using its native 2D graphics API:
///   Windows: GDI+  |  macOS: CoreGraphics  |  Linux: Cairo
class WatermarkRenderer {
 public:
  virtual ~WatermarkRenderer() = default;

  // Non-copyable.
  WatermarkRenderer(const WatermarkRenderer&) = delete;
  WatermarkRenderer& operator=(const WatermarkRenderer&) = delete;

  /// Apply a text watermark to the target image.
  /// The image pixel data is modified in-place.
  ///
  /// @param image   Target image (mutable).
  /// @param config  Text watermark configuration.
  /// @return true on success.
  virtual bool ApplyTextWatermark(
      Image* image, const PixelGrabTextWatermarkConfig& config) = 0;

  /// Apply a tiled (diagonal) text watermark across the entire image.
  /// Text is rotated and repeated in a grid pattern (rain-like effect).
  ///
  /// @param image       Target image (mutable).
  /// @param config      Text watermark configuration (position field is ignored).
  /// @param angle_deg   Rotation angle in degrees (e.g. -30 for rain effect).
  /// @param spacing_x   Horizontal spacing between tiles in pixels.
  /// @param spacing_y   Vertical spacing between tiles in pixels.
  /// @return true on success.
  virtual bool ApplyTiledTextWatermark(
      Image* image, const PixelGrabTextWatermarkConfig& config,
      float angle_deg, int spacing_x, int spacing_y) = 0;

  /// Apply an image watermark (overlay) onto the target image.
  /// Alpha blending is performed based on the opacity parameter and
  /// the watermark image's own alpha channel.
  ///
  /// @param target     Destination image (mutable).
  /// @param watermark  Source watermark image.
  /// @param x          Watermark X position on the target.
  /// @param y          Watermark Y position on the target.
  /// @param opacity    Overall opacity (0.0 = invisible, 1.0 = full).
  /// @return true on success.
  virtual bool ApplyImageWatermark(Image* target, const Image& watermark,
                                   int x, int y, float opacity) = 0;

 protected:
  WatermarkRenderer() = default;
};

/// Factory function — returns the platform-native watermark renderer.
/// Defined per-platform in platform/<os>/xxx_watermark_renderer.cpp.
std::unique_ptr<WatermarkRenderer> CreatePlatformWatermarkRenderer();

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_WATERMARK_WATERMARK_RENDERER_H_
