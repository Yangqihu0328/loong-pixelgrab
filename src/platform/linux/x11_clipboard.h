// Copyright 2026 The loong-pixelgrab Authors
// Linux clipboard reader using X11 Selection protocol.

#ifndef PIXELGRAB_PLATFORM_LINUX_X11_CLIPBOARD_H_
#define PIXELGRAB_PLATFORM_LINUX_X11_CLIPBOARD_H_

#include "pin/clipboard_reader.h"

namespace pixelgrab {
namespace internal {

class X11ClipboardReader : public ClipboardReader {
 public:
  PixelGrabClipboardFormat GetAvailableFormat() const override;
  std::unique_ptr<Image> ReadImage() override;
  std::string ReadText() override;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PLATFORM_LINUX_X11_CLIPBOARD_H_
