// Copyright 2024 PixelGrab Authors. All rights reserved.
// Windows clipboard reader.

#ifndef PIXELGRAB_PLATFORM_WINDOWS_WIN_CLIPBOARD_H_
#define PIXELGRAB_PLATFORM_WINDOWS_WIN_CLIPBOARD_H_

#include "pin/clipboard_reader.h"

namespace pixelgrab {
namespace internal {

/// Windows implementation using Win32 Clipboard API.
class WinClipboardReader : public ClipboardReader {
 public:
  PixelGrabClipboardFormat GetAvailableFormat() const override;
  std::unique_ptr<Image> ReadImage() override;
  std::string ReadText() override;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PLATFORM_WINDOWS_WIN_CLIPBOARD_H_
