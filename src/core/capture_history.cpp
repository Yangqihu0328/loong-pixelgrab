// Copyright 2024 PixelGrab Authors. All rights reserved.
// Capture history manager implementation.

#include "core/capture_history.h"

#include <chrono>
#include <cstring>

#include "core/image.h"

namespace pixelgrab {
namespace internal {

CaptureHistory::CaptureHistory() = default;

// Simple byte-level RLE: pairs of (count, byte) for runs of identical bytes,
// with a literal escape for non-repeating data.  Tuned for BGRA image data
// where large uniform regions (alpha=255, solid backgrounds) are common.
//
// Format:  [tag, payload...]
//   tag 0x00..0x7F  => literal run of (tag+1) bytes follows
//   tag 0x80..0xFF  => repeat next byte (tag - 0x80 + 3) times  (3..130)
//
// Worst case expansion: ~1.006x for incompressible data.
std::unique_ptr<CaptureHistory::CompressedImage> CaptureHistory::Compress(
    const Image& img) {
  auto ci = std::make_unique<CompressedImage>();
  ci->width = img.width();
  ci->height = img.height();
  ci->stride = img.stride();
  ci->format = img.format();
  ci->raw_size = img.data_size();

  const uint8_t* src = img.data();
  size_t src_len = img.data_size();

  // Worst case: header bytes per 128-byte literal run.
  ci->data.reserve(src_len + src_len / 128 + 2);

  size_t i = 0;
  while (i < src_len) {
    // Check for a run of identical bytes (min 3 to break even).
    size_t run = 1;
    while (i + run < src_len && src[i + run] == src[i] && run < 130) {
      ++run;
    }

    if (run >= 3) {
      ci->data.push_back(static_cast<uint8_t>(0x80 + run - 3));
      ci->data.push_back(src[i]);
      i += run;
    } else {
      // Literal run: collect up to 128 non-repeating bytes.
      size_t lit_start = i;
      size_t lit_len = 0;
      while (lit_len < 128 && i < src_len) {
        size_t ahead = 1;
        while (i + ahead < src_len && src[i + ahead] == src[i] && ahead < 130)
          ++ahead;
        if (ahead >= 3) break;
        ++lit_len;
        ++i;
      }
      if (lit_len > 0) {
        ci->data.push_back(static_cast<uint8_t>(lit_len - 1));
        ci->data.insert(ci->data.end(), src + lit_start,
                         src + lit_start + lit_len);
      }
    }
  }

  ci->data.shrink_to_fit();
  return ci;
}

std::unique_ptr<Image> CaptureHistory::Decompress(const CompressedImage& ci) {
  auto img = Image::Create(ci.width, ci.height, ci.format);
  if (!img) return nullptr;

  uint8_t* dst = img->mutable_data();
  size_t dst_len = img->data_size();
  size_t di = 0;

  const uint8_t* src = ci.data.data();
  size_t si = 0;
  size_t src_len = ci.data.size();

  while (si < src_len && di < dst_len) {
    uint8_t tag = src[si++];
    if (tag <= 0x7F) {
      size_t count = static_cast<size_t>(tag) + 1;
      size_t copy = (std::min)(count, (std::min)(src_len - si, dst_len - di));
      std::memcpy(dst + di, src + si, copy);
      si += copy;
      di += copy;
    } else {
      size_t count = static_cast<size_t>(tag - 0x80) + 3;
      if (si >= src_len) break;
      uint8_t val = src[si++];
      size_t fill = (std::min)(count, dst_len - di);
      std::memset(dst + di, val, fill);
      di += fill;
    }
  }

  return img;
}

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
    auto compressed = Compress(*image);
    if (compressed) {
      total_compressed_bytes_ += compressed->data.size();
      images_[id] = std::move(compressed);
    }
  }

  PurgeExcess();
  EnforceMemoryBudget();
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

std::unique_ptr<Image> CaptureHistory::GetImageById(int id) const {
  auto it = images_.find(id);
  if (it == images_.end()) return nullptr;
  return Decompress(*it->second);
}

void CaptureHistory::Clear() {
  entries_.clear();
  images_.clear();
  total_compressed_bytes_ = 0;
}

void CaptureHistory::SetMaxCount(int max_count) {
  if (max_count > 0) {
    max_count_ = max_count;
    PurgeExcess();
  }
}

void CaptureHistory::SetMaxMemoryBytes(size_t bytes) {
  max_memory_bytes_ = bytes;
  EnforceMemoryBudget();
}

void CaptureHistory::PurgeExcess() {
  while (static_cast<int>(entries_.size()) > max_count_) {
    int removed_id = entries_.back().id;
    entries_.pop_back();
    auto it = images_.find(removed_id);
    if (it != images_.end()) {
      total_compressed_bytes_ -= it->second->data.size();
      images_.erase(it);
    }
  }
}

void CaptureHistory::EnforceMemoryBudget() {
  // Evict images (oldest first) until within budget.
  // Metadata entries are kept so re-capture from screen still works.
  for (auto rit = entries_.rbegin();
       rit != entries_.rend() && total_compressed_bytes_ > max_memory_bytes_;
       ++rit) {
    auto it = images_.find(rit->id);
    if (it != images_.end()) {
      total_compressed_bytes_ -= it->second->data.size();
      images_.erase(it);
    }
  }
}

}  // namespace internal
}  // namespace pixelgrab
