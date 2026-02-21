// Copyright 2026 The loong-pixelgrab Authors
// Linux clipboard reader — X11 Selection protocol.

#include "platform/linux/x11_clipboard.h"

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <cstring>
#include <vector>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cairo/cairo.h>

#include "core/image.h"
#include "core/logger.h"

namespace pixelgrab {
namespace internal {

namespace {

constexpr int kSelectionTimeout = 500;  // ms

struct XDisplayGuard {
  Display* dpy = nullptr;
  Window win = None;

  XDisplayGuard() {
    dpy = XOpenDisplay(nullptr);
    if (dpy) {
      win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
                                0, 0, 1, 1, 0, 0, 0);
    }
  }
  ~XDisplayGuard() {
    if (dpy) {
      if (win) XDestroyWindow(dpy, win);
      XCloseDisplay(dpy);
    }
  }
  explicit operator bool() const { return dpy && win; }
};

// Request a selection target and wait for SelectionNotify.
// Returns the property data, or empty vector on failure.
std::vector<uint8_t> RequestSelection(Display* dpy, Window win,
                                      Atom selection, Atom target) {
  Atom prop = XInternAtom(dpy, "PIXELGRAB_SEL", False);
  XConvertSelection(dpy, selection, target, prop, win, CurrentTime);
  XFlush(dpy);

  XEvent ev;
  for (int i = 0; i < kSelectionTimeout; ++i) {
    if (XCheckTypedWindowEvent(dpy, win, SelectionNotify, &ev)) {
      if (ev.xselection.property == None) return {};

      Atom type;
      int format;
      unsigned long items, after;
      unsigned char* data = nullptr;
      XGetWindowProperty(dpy, win, prop, 0, ~0L, True, AnyPropertyType,
                         &type, &format, &items, &after, &data);
      if (!data) return {};

      size_t byte_len = items * (static_cast<size_t>(format) / 8);
      std::vector<uint8_t> result(data, data + byte_len);
      XFree(data);
      return result;
    }
    // Sleep 1ms between polls.
    struct timespec ts = {0, 1000000};
    nanosleep(&ts, nullptr);
  }
  return {};
}

// Check if the selection owner advertises a given target.
bool SelectionSupportsTarget(Display* dpy, Window win,
                             Atom selection, Atom target) {
  Atom targets_atom = XInternAtom(dpy, "TARGETS", False);
  auto data = RequestSelection(dpy, win, selection, targets_atom);
  if (data.empty()) return false;

  size_t count = data.size() / sizeof(Atom);
  auto* atoms = reinterpret_cast<Atom*>(data.data());
  for (size_t i = 0; i < count; ++i) {
    if (atoms[i] == target) return true;
  }
  return false;
}

}  // namespace

PixelGrabClipboardFormat X11ClipboardReader::GetAvailableFormat() const {
  XDisplayGuard x;
  if (!x) return kPixelGrabClipboardNone;

  Atom clipboard = XInternAtom(x.dpy, "CLIPBOARD", False);
  if (XGetSelectionOwner(x.dpy, clipboard) == None)
    return kPixelGrabClipboardNone;

  Atom png = XInternAtom(x.dpy, "image/png", False);
  if (SelectionSupportsTarget(x.dpy, x.win, clipboard, png))
    return kPixelGrabClipboardImage;

  Atom utf8 = XInternAtom(x.dpy, "UTF8_STRING", False);
  Atom xa_string = XA_STRING;
  if (SelectionSupportsTarget(x.dpy, x.win, clipboard, utf8) ||
      SelectionSupportsTarget(x.dpy, x.win, clipboard, xa_string))
    return kPixelGrabClipboardText;

  return kPixelGrabClipboardNone;
}

std::unique_ptr<Image> X11ClipboardReader::ReadImage() {
  XDisplayGuard x;
  if (!x) return nullptr;

  Atom clipboard = XInternAtom(x.dpy, "CLIPBOARD", False);
  Atom png = XInternAtom(x.dpy, "image/png", False);

  auto data = RequestSelection(x.dpy, x.win, clipboard, png);
  if (data.empty()) return nullptr;

  // Decode PNG data using Cairo.
  // cairo_image_surface_create_from_png_stream would be ideal but requires
  // a read-func.  For simplicity we write to a temp and read back.
  // A direct memory reader:
  struct PngReadCtx {
    const uint8_t* data;
    size_t len;
    size_t pos;
  };
  PngReadCtx ctx{data.data(), data.size(), 0};

  auto read_fn = [](void* closure, unsigned char* buf,
                     unsigned int length) -> cairo_status_t {
    auto* c = static_cast<PngReadCtx*>(closure);
    if (c->pos + length > c->len) return CAIRO_STATUS_READ_ERROR;
    std::memcpy(buf, c->data + c->pos, length);
    c->pos += length;
    return CAIRO_STATUS_SUCCESS;
  };

  cairo_surface_t* surf =
      cairo_image_surface_create_from_png_stream(read_fn, &ctx);
  if (!surf || cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
    if (surf) cairo_surface_destroy(surf);
    return nullptr;
  }

  int w = cairo_image_surface_get_width(surf);
  int h = cairo_image_surface_get_height(surf);
  int stride = w * 4;
  std::vector<uint8_t> pixels(static_cast<size_t>(stride) * h);

  // Copy from Cairo ARGB32 surface (same as BGRA8 on LE).
  cairo_surface_flush(surf);
  const uint8_t* src = cairo_image_surface_get_data(surf);
  int src_stride = cairo_image_surface_get_stride(surf);
  for (int row = 0; row < h; ++row) {
    std::memcpy(pixels.data() + row * stride, src + row * src_stride,
                static_cast<size_t>(w) * 4);
  }

  cairo_surface_destroy(surf);
  return Image::CreateFromData(w, h, stride, kPixelGrabFormatBgra8,
                               std::move(pixels));
}

std::string X11ClipboardReader::ReadText() {
  XDisplayGuard x;
  if (!x) return {};

  Atom clipboard = XInternAtom(x.dpy, "CLIPBOARD", False);
  Atom utf8 = XInternAtom(x.dpy, "UTF8_STRING", False);

  auto data = RequestSelection(x.dpy, x.win, clipboard, utf8);
  if (data.empty()) {
    data = RequestSelection(x.dpy, x.win, clipboard, XA_STRING);
  }
  if (data.empty()) return {};
  return std::string(data.begin(), data.end());
}

std::unique_ptr<ClipboardReader> CreatePlatformClipboardReader() {
  return std::make_unique<X11ClipboardReader>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // __linux__
