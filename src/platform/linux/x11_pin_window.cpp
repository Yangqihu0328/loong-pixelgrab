// Copyright 2026 The loong-pixelgrab Authors
// Linux X11 floating pin window backend.

#include "platform/linux/x11_pin_window.h"

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <cstring>
#include <vector>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cairo/cairo.h>

#include "core/image.h"
#include "core/logger.h"

namespace pixelgrab {
namespace internal {

X11PinWindowBackend::X11PinWindowBackend() = default;

X11PinWindowBackend::~X11PinWindowBackend() { Destroy(); }

bool X11PinWindowBackend::Create(const PinWindowConfig& config) {
  Destroy();

  display_ = XOpenDisplay(nullptr);
  if (!display_) {
    PIXELGRAB_LOG_ERROR("PinWindow: failed to open X11 display");
    return false;
  }

  int scr = DefaultScreen(display_);
  Window root = RootWindow(display_, scr);

  x_ = config.x;
  y_ = config.y;
  width_ = config.width > 0 ? config.width : 200;
  height_ = config.height > 0 ? config.height : 200;
  opacity_ = config.opacity;

  XSetWindowAttributes attrs{};
  attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                     StructureNotifyMask;
  attrs.override_redirect = False;

  window_ = XCreateWindow(display_, root, x_, y_,
                           static_cast<unsigned>(width_),
                           static_cast<unsigned>(height_),
                           0, CopyFromParent, InputOutput,
                           CopyFromParent, CWEventMask, &attrs);
  if (!window_) {
    PIXELGRAB_LOG_ERROR("PinWindow: XCreateWindow failed");
    XCloseDisplay(display_);
    display_ = nullptr;
    return false;
  }

  gc_ = static_cast<void*>(XCreateGC(display_, window_, 0, nullptr));

  // EWMH: always-on-top
  if (config.topmost) {
    Atom state = XInternAtom(display_, "_NET_WM_STATE", False);
    Atom above = XInternAtom(display_, "_NET_WM_STATE_ABOVE", False);
    XChangeProperty(display_, window_, state, XA_ATOM, 32,
                    PropModeReplace,
                    reinterpret_cast<unsigned char*>(&above), 1);
  }

  // Window type: utility
  Atom wm_type = XInternAtom(display_, "_NET_WM_WINDOW_TYPE", False);
  Atom type_util =
      XInternAtom(display_, "_NET_WM_WINDOW_TYPE_UTILITY", False);
  XChangeProperty(display_, window_, wm_type, XA_ATOM, 32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&type_util), 1);

  // Window title
  XStoreName(display_, window_, "PixelGrab Pin");

  // WM_DELETE_WINDOW protocol
  Atom wm_del = XInternAtom(display_, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display_, window_, &wm_del, 1);

  SetOpacity(opacity_);

  XMapWindow(display_, window_);
  XFlush(display_);

  visible_ = true;
  valid_ = true;
  return true;
}

void X11PinWindowBackend::Destroy() {
  if (!display_) return;

  if (gc_) {
    XFreeGC(display_, reinterpret_cast<GC>(gc_));
    gc_ = 0;
  }
  if (window_) {
    XDestroyWindow(display_, window_);
    window_ = 0;
  }
  XCloseDisplay(display_);
  display_ = nullptr;
  valid_ = false;
  visible_ = false;
  content_.reset();
}

bool X11PinWindowBackend::IsValid() const { return valid_; }

bool X11PinWindowBackend::SetImageContent(const Image* image) {
  if (!valid_ || !image) return false;
  content_ = image->Clone();
  width_ = image->width();
  height_ = image->height();
  XResizeWindow(display_, window_,
                static_cast<unsigned>(width_),
                static_cast<unsigned>(height_));
  Repaint();
  return true;
}

bool X11PinWindowBackend::SetTextContent(const char* text) {
  if (!valid_ || !text) return false;

  // Render text to a small image using Cairo.
  int w = 300, h = 80;
  auto img = Image::Create(w, h, kPixelGrabFormatBgra8);
  if (!img) return false;

  // Fill with white background
  std::memset(img->mutable_data(), 0xFF, img->data_size());

  // Use Cairo for text rendering
  cairo_surface_t* surf = cairo_image_surface_create_for_data(
      img->mutable_data(), CAIRO_FORMAT_ARGB32, w, h, img->stride());
  cairo_t* cr = cairo_create(surf);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 16);
  cairo_move_to(cr, 8, 28);
  cairo_show_text(cr, text);
  cairo_destroy(cr);
  cairo_surface_destroy(surf);

  return SetImageContent(img.get());
}

std::unique_ptr<Image> X11PinWindowBackend::GetImageContent() const {
  return content_ ? content_->Clone() : nullptr;
}

void X11PinWindowBackend::SetPosition(int x, int y) {
  if (!valid_) return;
  x_ = x;
  y_ = y;
  XMoveWindow(display_, window_, x, y);
  XFlush(display_);
}

void X11PinWindowBackend::SetSize(int w, int h) {
  if (!valid_ || w <= 0 || h <= 0) return;
  width_ = w;
  height_ = h;
  XResizeWindow(display_, window_,
                static_cast<unsigned>(w), static_cast<unsigned>(h));
  XFlush(display_);
}

void X11PinWindowBackend::SetOpacity(float o) {
  opacity_ = o;
  if (!valid_) return;

  Atom opacity_atom =
      XInternAtom(display_, "_NET_WM_WINDOW_OPACITY", False);
  auto val = static_cast<uint32_t>(o * 0xFFFFFFFF);
  XChangeProperty(display_, window_, opacity_atom, XA_CARDINAL, 32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&val), 1);
  XFlush(display_);
}

float X11PinWindowBackend::GetOpacity() const { return opacity_; }

void X11PinWindowBackend::SetVisible(bool v) {
  if (!valid_) return;
  if (v && !visible_) {
    XMapWindow(display_, window_);
    visible_ = true;
  } else if (!v && visible_) {
    XUnmapWindow(display_, window_);
    visible_ = false;
  }
  XFlush(display_);
}

bool X11PinWindowBackend::IsVisible() const { return visible_; }

void X11PinWindowBackend::GetPosition(int* out_x, int* out_y) const {
  if (out_x) *out_x = x_;
  if (out_y) *out_y = y_;
}

void X11PinWindowBackend::GetSize(int* out_width, int* out_height) const {
  if (out_width) *out_width = width_;
  if (out_height) *out_height = height_;
}

void* X11PinWindowBackend::GetNativeHandle() const {
  return reinterpret_cast<void*>(window_);
}

bool X11PinWindowBackend::ProcessEvents() {
  if (!valid_) return false;

  Atom wm_protocols = XInternAtom(display_, "WM_PROTOCOLS", False);
  Atom wm_del = XInternAtom(display_, "WM_DELETE_WINDOW", False);

  while (XPending(display_)) {
    XEvent ev;
    XNextEvent(display_, &ev);

    switch (ev.type) {
      case Expose:
        if (ev.xexpose.count == 0) Repaint();
        break;
      case ConfigureNotify:
        x_ = ev.xconfigure.x;
        y_ = ev.xconfigure.y;
        width_ = ev.xconfigure.width;
        height_ = ev.xconfigure.height;
        break;
      case ClientMessage:
        if (static_cast<Atom>(ev.xclient.message_type) == wm_protocols &&
            static_cast<Atom>(ev.xclient.data.l[0]) == wm_del) {
          Destroy();
          return false;
        }
        break;
      default:
        break;
    }
  }
  return valid_;
}

void X11PinWindowBackend::Repaint() {
  if (!valid_ || !content_) return;

  int w = content_->width();
  int h = content_->height();
  int scr = DefaultScreen(display_);
  Visual* vis = DefaultVisual(display_, scr);
  int depth = DefaultDepth(display_, scr);

  // XImage from our BGRA pixel data.
  XImage* ximg = XCreateImage(display_, vis, depth, ZPixmap, 0,
                               nullptr, w, h, 32, 0);
  if (!ximg) return;

  // Allocate a copy of the pixel data (XDestroyImage will free it).
  ximg->data = static_cast<char*>(std::malloc(ximg->bytes_per_line * h));
  if (!ximg->data) {
    ximg->data = nullptr;
    XDestroyImage(ximg);
    return;
  }

  const uint8_t* src = content_->data();
  int src_stride = content_->stride();
  for (int row = 0; row < h; ++row) {
    std::memcpy(ximg->data + row * ximg->bytes_per_line,
                src + row * src_stride,
                static_cast<size_t>(w) * 4);
  }

  XPutImage(display_, window_, reinterpret_cast<GC>(gc_),
            ximg, 0, 0, 0, 0, w, h);
  XFlush(display_);
  XDestroyImage(ximg);
}

std::unique_ptr<PinWindowBackend> CreatePlatformPinWindowBackend() {
  return std::make_unique<X11PinWindowBackend>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // __linux__
