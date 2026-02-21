// Copyright 2024 PixelGrab Authors. All rights reserved.
// Capture history manager — stores region metadata for recall.

#ifndef PIXELGRAB_CORE_CAPTURE_HISTORY_H_
#define PIXELGRAB_CORE_CAPTURE_HISTORY_H_

#include <cstdint>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

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
///
/// Images are stored in a compressed format (simple RLE on BGRA rows) to
/// reduce memory footprint.  A configurable memory budget caps total storage;
/// when the budget is exceeded the oldest images are evicted while their
/// metadata entries are kept (enabling re-capture from screen).
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

  /// Get the stored image for a given entry ID.
  /// Decompresses on demand; returns nullptr if not found or evicted.
  std::unique_ptr<Image> GetImageById(int id) const;

  /// Clear all history entries.
  void Clear();

  /// Set the maximum number of stored entries (default 50).
  void SetMaxCount(int max_count);

  /// Set the maximum total memory budget for stored images (bytes).
  /// Default: 128 MB.  When exceeded, oldest images are evicted.
  void SetMaxMemoryBytes(size_t bytes);

 private:
  struct CompressedImage {
    int width;
    int height;
    int stride;
    PixelGrabPixelFormat format;
    std::vector<uint8_t> data;  // Compressed payload.
    size_t raw_size;            // Original uncompressed size.
  };

  static std::unique_ptr<CompressedImage> Compress(const Image& img);
  static std::unique_ptr<Image> Decompress(const CompressedImage& ci);

  void PurgeExcess();
  void EnforceMemoryBudget();

  std::deque<HistoryEntry> entries_;
  std::unordered_map<int, std::unique_ptr<CompressedImage>> images_;
  size_t total_compressed_bytes_ = 0;
  size_t max_memory_bytes_ = 128ULL * 1024 * 1024;  // 128 MB
  int max_count_ = 50;
  int next_id_ = 1;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_CORE_CAPTURE_HISTORY_H_
