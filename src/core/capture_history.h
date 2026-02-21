// Copyright 2024 PixelGrab Authors. All rights reserved.
// Capture history manager — stores region metadata for recall.

#ifndef PIXELGRAB_CORE_CAPTURE_HISTORY_H_
#define PIXELGRAB_CORE_CAPTURE_HISTORY_H_

#include <cstdint>
#include <deque>
#include <memory>
#include <unordered_map>

#include "pixelgrab/pixelgrab.h"

namespace pixelgrab {
namespace internal {

class Image;

/// A single capture history entry (region metadata only, no image data).
struct HistoryEntry {
  int id;              // Unique monotonic ID
  int region_x;
  int region_y;
  int region_width;
  int region_height;
  int64_t timestamp;   // Unix epoch seconds
};

/// Manages a bounded list of past capture regions for recall / re-capture.
class CaptureHistory {
 public:
  CaptureHistory();

  /// Record a new capture region with an optional image snapshot.
  /// Returns the assigned entry ID.
  int Record(int x, int y, int width, int height,
             std::unique_ptr<Image> image = nullptr);

  /// Total number of entries.
  int Count() const;

  /// Get entry by reverse-chronological index (0 = most recent).
  bool GetEntry(int index, HistoryEntry* out) const;

  /// Find entry by its unique ID. Returns nullptr if not found.
  const HistoryEntry* FindById(int id) const;

  /// Get the stored image for a given entry ID. Returns nullptr if not found.
  const Image* GetImageById(int id) const;

  /// Clear all history entries.
  void Clear();

  /// Set the maximum number of stored entries (default 50).
  void SetMaxCount(int max_count);

 private:
  void PurgeExcess();

  std::deque<HistoryEntry> entries_;
  std::unordered_map<int, std::unique_ptr<Image>> images_;
  int max_count_ = 50;
  int next_id_ = 1;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_CORE_CAPTURE_HISTORY_H_
