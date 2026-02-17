// Copyright 2024 PixelGrab Authors. All rights reserved.
// Smart snapping engine implementation.

#include "detection/snap_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace pixelgrab {
namespace internal {

SnapEngine::SnapEngine(ElementDetector* detector) : detector_(detector) {}

void SnapEngine::SetSnapDistance(int distance) {
  if (distance > 0) {
    snap_distance_ = distance;
  }
}

bool SnapEngine::IsCacheValid(int x, int y) const {
  if (cache_.elements.empty()) return false;

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - cache_.timestamp);
  if (elapsed.count() > kCacheTtlMs) return false;

  int dx = std::abs(x - cache_.last_x);
  int dy = std::abs(y - cache_.last_y);
  if (dx > kCacheMoveThreshold || dy > kCacheMoveThreshold) return false;

  return true;
}

void SnapEngine::RefreshCache(int x, int y) {
  cache_.elements.clear();
  cache_.last_x = x;
  cache_.last_y = y;
  cache_.timestamp = std::chrono::steady_clock::now();

  if (!detector_) return;

  // Fetch up to 10 nested elements.
  static constexpr int kMaxElements = 10;
  ElementInfo infos[kMaxElements];
  int count = detector_->DetectElements(x, y, infos, kMaxElements);
  if (count > 0) {
    cache_.elements.assign(infos, infos + count);
  }
}

SnapResult SnapEngine::TrySnap(int cursor_x, int cursor_y) {
  SnapResult result;

  if (!detector_) return result;

  // Refresh cache if needed.
  if (!IsCacheValid(cursor_x, cursor_y)) {
    RefreshCache(cursor_x, cursor_y);
  }

  if (cache_.elements.empty()) return result;

  // Candidate structure for ranking edges.
  struct Candidate {
    int distance;
    int depth;
    const ElementInfo* element;
  };

  // Find the element whose bounding box best contains the cursor.
  // For snapping, we return the element that the cursor is inside of,
  // preferring the smallest (deepest) element.
  Candidate best;
  best.distance = snap_distance_ + 1;
  best.depth = -1;
  best.element = nullptr;

  for (const auto& elem : cache_.elements) {
    int left = elem.x;
    int top = elem.y;
    int right = elem.x + elem.width;
    int bottom = elem.y + elem.height;

    // Compute minimum distance from cursor to any edge of this element.
    int dist_left = std::abs(cursor_x - left);
    int dist_right = std::abs(cursor_x - right);
    int dist_top = std::abs(cursor_y - top);
    int dist_bottom = std::abs(cursor_y - bottom);

    int min_dist = (std::min)({dist_left, dist_right, dist_top, dist_bottom});

    // Also check if cursor is inside the element.
    bool inside = cursor_x >= left && cursor_x <= right &&
                  cursor_y >= top && cursor_y <= bottom;

    // If inside, distance effectively 0 for ranking purposes.
    if (inside) {
      min_dist = 0;
    }

    if (min_dist <= snap_distance_) {
      // Compare: prefer closer distance, then deeper element.
      if (min_dist < best.distance ||
          (min_dist == best.distance && elem.depth > best.depth)) {
        best.distance = min_dist;
        best.depth = elem.depth;
        best.element = &elem;
      }
    }
  }

  if (best.element) {
    result.snapped = true;
    result.snapped_x = best.element->x;
    result.snapped_y = best.element->y;
    result.snapped_w = best.element->width;
    result.snapped_h = best.element->height;
    result.element = *best.element;
  }

  return result;
}

}  // namespace internal
}  // namespace pixelgrab
