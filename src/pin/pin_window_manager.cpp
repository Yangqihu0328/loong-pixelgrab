// Copyright 2024 PixelGrab Authors. All rights reserved.
// Multi-pin-window manager implementation.

#include "pin/pin_window_manager.h"

#include <vector>

#include "core/image.h"

namespace pixelgrab {
namespace internal {

PinWindowManager::PinWindowManager() = default;

PinWindowManager::~PinWindowManager() {
  DestroyAll();
}

int PinWindowManager::PinImage(const Image* image, int x, int y) {
  if (!image) return 0;

  auto backend = CreatePlatformPinWindowBackend();
  if (!backend) return 0;

  PinWindowConfig config;
  config.x = x;
  config.y = y;
  config.opacity = 1.0f;
  config.topmost = true;

  if (!backend->Create(config)) return 0;
  if (!backend->SetImageContent(image)) {
    backend->Destroy();
    return 0;
  }

  int id = next_id_++;
  PinEntry entry;
  entry.backend = std::move(backend);
  entry.content_type = PinContentType::kImage;
  windows_.emplace(id, std::move(entry));
  return id;
}

static constexpr int kDefaultTextPinWidth = 300;
static constexpr int kDefaultTextPinHeight = 200;

int PinWindowManager::PinText(const char* text, int x, int y) {
  if (!text) return 0;

  auto backend = CreatePlatformPinWindowBackend();
  if (!backend) return 0;

  PinWindowConfig config;
  config.x = x;
  config.y = y;
  config.width = kDefaultTextPinWidth;
  config.height = kDefaultTextPinHeight;
  config.opacity = 1.0f;
  config.topmost = true;

  if (!backend->Create(config)) return 0;
  if (!backend->SetTextContent(text)) {
    backend->Destroy();
    return 0;
  }

  int id = next_id_++;
  PinEntry entry;
  entry.backend = std::move(backend);
  entry.content_type = PinContentType::kText;
  windows_.emplace(id, std::move(entry));
  return id;
}

int PinWindowManager::PinClipboard(ClipboardReader* clipboard, int x, int y) {
  if (!clipboard) return 0;

  auto format = clipboard->GetAvailableFormat();
  if (format == kPixelGrabClipboardNone) return 0;

  if (format == kPixelGrabClipboardImage) {
    auto image = clipboard->ReadImage();
    if (!image) return 0;
    return PinImage(image.get(), x, y);
  }

  if (format == kPixelGrabClipboardText ||
      format == kPixelGrabClipboardHtml) {
    std::string text = clipboard->ReadText();
    if (text.empty()) return 0;
    return PinText(text.c_str(), x, y);
  }

  return 0;
}

std::unique_ptr<Image> PinWindowManager::GetImage(int pin_id) const {
  auto it = windows_.find(pin_id);
  if (it == windows_.end()) return nullptr;
  return it->second.backend->GetImageContent();
}

bool PinWindowManager::SetImage(int pin_id, const Image* image) {
  if (!image) return false;
  auto it = windows_.find(pin_id);
  if (it == windows_.end()) return false;
  if (it->second.content_type != PinContentType::kImage) return false;
  return it->second.backend->SetImageContent(image);
}

int PinWindowManager::Enumerate(int* out_ids, int max_count) const {
  if (!out_ids || max_count <= 0) return 0;
  int count = 0;
  for (const auto& kv : windows_) {
    if (count >= max_count) break;
    out_ids[count++] = kv.first;
  }
  return count;
}

bool PinWindowManager::GetInfo(int pin_id, int* out_x, int* out_y,
                               int* out_w, int* out_h, float* out_opacity,
                               bool* out_visible,
                               PinContentType* out_type) const {
  auto it = windows_.find(pin_id);
  if (it == windows_.end()) return false;

  const auto& backend = it->second.backend;

  if (out_x && out_y) backend->GetPosition(out_x, out_y);
  if (out_w && out_h) backend->GetSize(out_w, out_h);
  if (out_opacity) *out_opacity = backend->GetOpacity();
  if (out_visible) *out_visible = backend->IsVisible();
  if (out_type) *out_type = it->second.content_type;

  return true;
}

void PinWindowManager::SetVisibleAll(bool visible) {
  for (auto& kv : windows_) {
    kv.second.backend->SetVisible(visible);
  }
}

int PinWindowManager::Duplicate(int pin_id, int dx, int dy) {
  auto it = windows_.find(pin_id);
  if (it == windows_.end()) return 0;

  const auto& src = it->second;

  // Get source position.
  int src_x = 0, src_y = 0;
  src.backend->GetPosition(&src_x, &src_y);

  if (src.content_type == PinContentType::kImage) {
    auto image = src.backend->GetImageContent();
    if (!image) return 0;
    return PinImage(image.get(), src_x + dx, src_y + dy);
  }

  // Text pins: we don't have a GetTextContent() yet, so duplicate is
  // only supported for image pins at this time.
  return 0;
}

bool PinWindowManager::SetOpacity(int pin_id, float opacity) {
  auto it = windows_.find(pin_id);
  if (it == windows_.end()) return false;
  it->second.backend->SetOpacity(opacity);
  return true;
}

float PinWindowManager::GetOpacity(int pin_id) const {
  auto it = windows_.find(pin_id);
  if (it == windows_.end()) return 1.0f;
  return it->second.backend->GetOpacity();
}

bool PinWindowManager::SetPosition(int pin_id, int x, int y) {
  auto it = windows_.find(pin_id);
  if (it == windows_.end()) return false;
  it->second.backend->SetPosition(x, y);
  return true;
}

bool PinWindowManager::SetSize(int pin_id, int width, int height) {
  auto it = windows_.find(pin_id);
  if (it == windows_.end()) return false;
  it->second.backend->SetSize(width, height);
  return true;
}

bool PinWindowManager::SetVisible(int pin_id, bool visible) {
  auto it = windows_.find(pin_id);
  if (it == windows_.end()) return false;
  it->second.backend->SetVisible(visible);
  return true;
}

void PinWindowManager::DestroyPin(int pin_id) {
  auto it = windows_.find(pin_id);
  if (it != windows_.end()) {
    it->second.backend->Destroy();
    windows_.erase(it);
  }
}

void PinWindowManager::DestroyAll() {
  for (auto& kv : windows_) {
    kv.second.backend->Destroy();
  }
  windows_.clear();
}

int PinWindowManager::Count() const {
  return static_cast<int>(windows_.size());
}

int PinWindowManager::ProcessEvents() {
  // Collect IDs of closed windows to remove after iteration.
  std::vector<int> to_remove;

  for (auto& kv : windows_) {
    if (!kv.second.backend->ProcessEvents()) {
      // Window was closed by user.
      to_remove.push_back(kv.first);
    }
  }

  for (int id : to_remove) {
    windows_.erase(id);
  }

  return static_cast<int>(windows_.size());
}

PinWindowBackend* PinWindowManager::GetBackend(int pin_id) {
  auto it = windows_.find(pin_id);
  if (it == windows_.end()) return nullptr;
  return it->second.backend.get();
}

const PinWindowBackend* PinWindowManager::GetBackend(int pin_id) const {
  auto it = windows_.find(pin_id);
  if (it == windows_.end()) return nullptr;
  return it->second.backend.get();
}

}  // namespace internal
}  // namespace pixelgrab
