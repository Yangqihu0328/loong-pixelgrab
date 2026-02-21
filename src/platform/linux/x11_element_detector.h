// Copyright 2026 The loong-pixelgrab Authors
// Linux element detector using X11 window tree traversal.

#ifndef PIXELGRAB_PLATFORM_LINUX_X11_ELEMENT_DETECTOR_H_
#define PIXELGRAB_PLATFORM_LINUX_X11_ELEMENT_DETECTOR_H_

#include "detection/element_detector.h"

namespace pixelgrab {
namespace internal {

/// Linux implementation using X11 XQueryTree-based window detection.
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
