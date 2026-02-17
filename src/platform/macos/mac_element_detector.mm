// Copyright 2024 PixelGrab Authors. All rights reserved.
// macOS Accessibility element detector (stub implementation).

#include "platform/macos/mac_element_detector.h"

namespace pixelgrab {
namespace internal {

bool MacElementDetector::Initialize() {
  // TODO: Check Accessibility permissions, create AXUIElement system-wide.
  return true;
}

bool MacElementDetector::DetectElement(int screen_x, int screen_y,
                                       ElementInfo* out_info) {
  // TODO: AXUIElementCopyElementAtPosition → AXPosition/AXSize/AXRole
  (void)screen_x;
  (void)screen_y;
  (void)out_info;
  return false;
}

int MacElementDetector::DetectElements(int screen_x, int screen_y,
                                       ElementInfo* out_infos,
                                       int max_count) {
  // TODO: Walk AXParent chain to collect nested elements.
  (void)screen_x;
  (void)screen_y;
  (void)out_infos;
  (void)max_count;
  return 0;
}

// Factory implementation.
std::unique_ptr<ElementDetector> CreatePlatformElementDetector() {
  auto detector = std::make_unique<MacElementDetector>();
  if (!detector->Initialize()) return nullptr;
  return detector;
}

}  // namespace internal
}  // namespace pixelgrab
