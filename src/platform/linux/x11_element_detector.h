// Copyright 2024 PixelGrab Authors. All rights reserved.
// Linux AT-SPI2 element detector (stub).

#ifndef PIXELGRAB_PLATFORM_LINUX_X11_ELEMENT_DETECTOR_H_
#define PIXELGRAB_PLATFORM_LINUX_X11_ELEMENT_DETECTOR_H_

#include "detection/element_detector.h"

namespace pixelgrab {
namespace internal {

/// Linux implementation using AT-SPI2 D-Bus.
/// Currently a stub — returns false for all detection.
class X11ElementDetector : public ElementDetector {
 public:
  bool Initialize() override;
  bool DetectElement(int screen_x, int screen_y,
                     ElementInfo* out_info) override;
  int DetectElements(int screen_x, int screen_y,
                     ElementInfo* out_infos, int max_count) override;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PLATFORM_LINUX_X11_ELEMENT_DETECTOR_H_
