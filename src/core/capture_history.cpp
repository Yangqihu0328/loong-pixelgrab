// Copyright 2024 PixelGrab Authors. All rights reserved.
// Capture history manager implementation.

#include "core/capture_history.h"

#include <chrono>

namespace pixelgrab {
namespace internal {

CaptureHistory::CaptureHistory() = default;

int CaptureHistory::Record(int x, int y, int width, int height) {
  HistoryEntry entry;
  entry.id = next_id_++;
  entry.region_x = x;
  entry.region_y = y;
  entry.region_width = width;
  entry.region_height = height;

  // Unix timestamp (seconds since epoch).
  auto now = std::chrono::system_clock::now();
  entry.timestamp =
      std::chrono::duration_cast<std::chrono::seconds>(
          now.time_since_epoch())
          .count();

  entries_.push_front(entry);  // Most recent first.

  // Enforce max capacity.
  while (static_cast<int>(entries_.size()) > max_count_) {
    entries_.pop_back();
  }

  return entry.id;
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

void CaptureHistory::Clear() {
  entries_.clear();
}

void CaptureHistory::SetMaxCount(int max_count) {
  if (max_count > 0) {
    max_count_ = max_count;
    while (static_cast<int>(entries_.size()) > max_count_) {
      entries_.pop_back();
    }
  }
}

}  // namespace internal
}  // namespace pixelgrab
