// Copyright 2024 PixelGrab Authors. All rights reserved.
// Clipboard reading abstract interface.

#ifndef PIXELGRAB_PIN_CLIPBOARD_READER_H_
#define PIXELGRAB_PIN_CLIPBOARD_READER_H_

#include <memory>
#include <string>

#include "pixelgrab/pixelgrab.h"

namespace pixelgrab {
namespace internal {

class Image;

/// Abstract interface for platform-specific clipboard reading.
class ClipboardReader {
 public:
  virtual ~ClipboardReader() = default;

  /// Return the format of the current clipboard content.
  virtual PixelGrabClipboardFormat GetAvailableFormat() const = 0;

  /// Read an image from the clipboard. Returns nullptr if not an image.
  virtual std::unique_ptr<Image> ReadImage() = 0;

  /// Read text from the clipboard. Returns empty string if not text.
  virtual std::string ReadText() = 0;

 protected:
  ClipboardReader() = default;
};

/// Factory: creates the platform-specific clipboard reader.
std::unique_ptr<ClipboardReader> CreatePlatformClipboardReader();

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_PIN_CLIPBOARD_READER_H_
