// Copyright 2024 PixelGrab Authors. All rights reserved.
// macOS clipboard reader (stub implementation).

#include "platform/macos/mac_clipboard.h"
#include "core/image.h"

namespace pixelgrab {
namespace internal {

PixelGrabClipboardFormat MacClipboardReader::GetAvailableFormat() const {
  // TODO: NSPasteboard check for image/text/HTML types.
  return kPixelGrabClipboardNone;
}

std::unique_ptr<Image> MacClipboardReader::ReadImage() {
  // TODO: NSPasteboard → NSImage → pixel data.
  return nullptr;
}

std::string MacClipboardReader::ReadText() {
  // TODO: NSPasteboard → NSString → UTF-8.
  return "";
}

std::unique_ptr<ClipboardReader> CreatePlatformClipboardReader() {
  return std::make_unique<MacClipboardReader>();
}

}  // namespace internal
}  // namespace pixelgrab
