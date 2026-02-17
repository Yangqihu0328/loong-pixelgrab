// Copyright 2024 PixelGrab Authors. All rights reserved.
// Smart snapping engine.

#ifndef PIXELGRAB_DETECTION_SNAP_ENGINE_H_
#define PIXELGRAB_DETECTION_SNAP_ENGINE_H_

#include <chrono>
#include <vector>

#include "detection/element_detector.h"

namespace pixelgrab {
namespace internal {

/// Result of a snap attempt.
struct SnapResult {
  bool snapped = false;
  int snapped_x = 0;
  int snapped_y = 0;
  int snapped_w = 0;
  int snapped_h = 0;
  ElementInfo element;
};

/// Snapping engine: snaps cursor/rectangle to nearby UI element boundaries.
class SnapEngine {
 public:
  /// Does NOT take ownership of detector.
  explicit SnapEngine(ElementDetector* detector);

  /// Set the snap distance threshold in pixels (default 8).
  void SetSnapDistance(int distance);

  /// Try to snap the cursor position to a nearby element boundary.
  SnapResult TrySnap(int cursor_x, int cursor_y);

 private:
  ElementDetector* detector_;  // Non-owning
  int snap_distance_ = 8;

  // Cache to reduce Accessibility API calls.
  struct Cache {
    int last_x = -1;
    int last_y = -1;
    std::vector<ElementInfo> elements;
    std::chrono::steady_clock::time_point timestamp;
  };
  Cache cache_;

  static constexpr int kCacheTtlMs = 100;
  static constexpr int kCacheMoveThreshold = 5;  // pixels

  bool IsCacheValid(int x, int y) const;
  void RefreshCache(int x, int y);
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_DETECTION_SNAP_ENGINE_H_
