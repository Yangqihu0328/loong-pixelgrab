// Copyright 2024 PixelGrab Authors. All rights reserved.
// Linux X11 clipboard reader (stub implementation).

#include "platform/linux/x11_clipboard.h"
#include "core/image.h"

namespace pixelgrab {
namespace internal {

PixelGrabClipboardFormat X11ClipboardReader::GetAvailableFormat() const {
  // TODO: X11 Selections / XGetSelectionOwner
  return kPixelGrabClipboardNone;
}

std::unique_ptr<Image> X11ClipboardReader::ReadImage() {
  // TODO: XGetSelectionOwner + XConvertSelection
  return nullptr;
}

std::string X11ClipboardReader::ReadText() {
  // TODO: XGetSelectionOwner + XConvertSelection for UTF8_STRING
  return "";
}

std::unique_ptr<ClipboardReader> CreatePlatformClipboardReader() {
  return std::make_unique<X11ClipboardReader>();
}

}  // namespace internal
}  // namespace pixelgrab
