// Copyright 2024 PixelGrab Authors. All rights reserved.
// Capture history manager implementation.

#include "core/capture_history.h"

#include <chrono>

#include "core/image.h"

namespace pixelgrab {
namespace internal {

CaptureHistory::CaptureHistory() = default;

int CaptureHistory::Record(int x, int y, int width, int height,
                           std::unique_ptr<Image> image) {
  HistoryEntry entry;
  entry.id = next_id_++;
  entry.region_x = x;
  entry.region_y = y;
  entry.region_width = width;
  entry.region_height = height;

  auto now = std::chrono::system_clock::now();
  entry.timestamp =
      std::chrono::duration_cast<std::chrono::seconds>(
          now.time_since_epoch())
          .count();

  int id = entry.id;
  entries_.push_front(entry);

  if (image) {
    images_[id] = std::move(image);
  }

  PurgeExcess();
  return id;
}

int CaptureHistory::Count() const {
  return static_cast<int>(entries_.size());
}

bool CaptureHistory::GetEntry(int index, HistoryEntry* out) const {
  if (index < 0 || index >= static_cast<int>(entries_.size())) return false;
  if (!out) return false;
  *out = entries_[index];
  return true;
}

const HistoryEntry* CaptureHistory::FindById(int id) const {
  for (const auto& e : entries_) {
    if (e.id == id) return &e;
  }
  return nullptr;
}

const Image* CaptureHistory::GetImageById(int id) const {
  auto it = images_.find(id);
  return (it != images_.end()) ? it->second.get() : nullptr;
}

void CaptureHistory::Clear() {
  entries_.clear();
  images_.clear();
}

void CaptureHistory::SetMaxCount(int max_count) {
  if (max_count > 0) {
    max_count_ = max_count;
    PurgeExcess();
  }
}

void CaptureHistory::PurgeExcess() {
  while (static_cast<int>(entries_.size()) > max_count_) {
    int removed_id = entries_.back().id;
    entries_.pop_back();
    images_.erase(removed_id);
  }
}

}  // namespace internal
}  // namespace pixelgrab
