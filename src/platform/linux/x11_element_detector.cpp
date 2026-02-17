// Copyright 2024 PixelGrab Authors. All rights reserved.
// Linux AT-SPI2 element detector (stub implementation).

#include "platform/linux/x11_element_detector.h"

namespace pixelgrab {
namespace internal {

bool X11ElementDetector::Initialize() {
  // TODO: Connect to AT-SPI2 via D-Bus.
  return true;
}

bool X11ElementDetector::DetectElement(int screen_x, int screen_y,
                                       ElementInfo* out_info) {
  // TODO: atspi_component_get_accessible_at_point
  (void)screen_x;
  (void)screen_y;
  (void)out_info;
  return false;
}

int X11ElementDetector::DetectElements(int screen_x, int screen_y,
                                       ElementInfo* out_infos,
                                       int max_count) {
  // TODO: Walk AT-SPI tree upward to collect ancestors.
  (void)screen_x;
  (void)screen_y;
  (void)out_infos;
  (void)max_count;
  return 0;
}

// Factory implementation.
std::unique_ptr<ElementDetector> CreatePlatformElementDetector() {
  auto detector = std::make_unique<X11ElementDetector>();
  if (!detector->Initialize()) return nullptr;
  return detector;
}

}  // namespace internal
}  // namespace pixelgrab
