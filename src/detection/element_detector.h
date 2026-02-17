// Copyright 2024 PixelGrab Authors. All rights reserved.
// Element detection abstract interface.

#ifndef PIXELGRAB_DETECTION_ELEMENT_DETECTOR_H_
#define PIXELGRAB_DETECTION_ELEMENT_DETECTOR_H_

#include <memory>
#include <string>

namespace pixelgrab {
namespace internal {

/// Information about a detected UI element.
struct ElementInfo {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  std::string name;   // Element label (e.g. "OK", "Close")
  std::string role;   // Element role (e.g. "button", "edit", "window")
  int depth = 0;      // Nesting depth (0 = top-level window)
};

/// Abstract interface for platform-specific UI element detection.
class ElementDetector {
 public:
  virtual ~ElementDetector() = default;

  /// Initialize the detector (may involve COM/D-Bus init).
  virtual bool Initialize() = 0;

  /// Detect the most precise UI element at screen coordinates (x, y).
  /// Coordinates are in physical pixels.
  virtual bool DetectElement(int screen_x, int screen_y,
                             ElementInfo* out_info) = 0;

  /// Detect all nested UI elements at (x, y), ordered from largest to smallest.
  /// Returns the number of elements written, or -1 on error.
  virtual int DetectElements(int screen_x, int screen_y,
                             ElementInfo* out_infos,
                             int max_count) = 0;

 protected:
  ElementDetector() = default;
};

/// Factory: creates the platform-specific detector.
std::unique_ptr<ElementDetector> CreatePlatformElementDetector();

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_DETECTION_ELEMENT_DETECTOR_H_
