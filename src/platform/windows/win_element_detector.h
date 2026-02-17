// Copyright 2024 PixelGrab Authors. All rights reserved.
// Windows UI Automation element detector.

#ifndef PIXELGRAB_PLATFORM_WINDOWS_WIN_ELEMENT_DETECTOR_H_
#define PIXELGRAB_PLATFORM_WINDOWS_WIN_ELEMENT_DETECTOR_H_

#include <UIAutomation.h>

#include "detection/element_detector.h"

namespace pixelgrab {
namespace internal {

/// Windows implementation using IUIAutomation (COM).
class WinElementDetector : public ElementDetector {
 public:
  WinElementDetector();
  ~WinElementDetector() override;

  bool Initialize() override;

  bool DetectElement(int screen_x, int screen_y,
                     ElementInfo* out_info) override;

  int DetectElements(int screen_x, int screen_y,
                     ElementInfo* out_infos,
                     int max_count) override;

 private:
  /// Fill an ElementInfo from an IUIAutomationElement.
  bool FillElementInfo(IUIAutomationElement* elem, ElementInfo* out,
                       int depth) const;

  IUIAutomation* automation_ = nullptr;
  bool com_initialized_ = false;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PLATFORM_WINDOWS_WIN_ELEMENT_DETECTOR_H_
