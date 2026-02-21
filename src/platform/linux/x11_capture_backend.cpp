// Copyright 2026 The loong-pixelgrab Authors

#include "platform/linux/x11_capture_backend.h"

#if defined(__linux__)

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "core/image.h"
#include "core/logger.h"

namespace pixelgrab {
namespace internal {

X11CaptureBackend::X11CaptureBackend() = default;
X11CaptureBackend::~X11CaptureBackend() { Shutdown(); }

bool X11CaptureBackend::Initialize() {
  if (initialized_) return true;
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    PIXELGRAB_LOG_ERROR("Failed to open X11 display");
    return false;
  }
  display_ = dpy;
  initialized_ = true;
  return true;
}

void X11CaptureBackend::Shutdown() {
  if (display_) {
    XCloseDisplay(static_cast<Display*>(display_));
    display_ = nullptr;
  }
  initialized_ = false;
}

static std::unique_ptr<Image> XImageToImage(XImage* ximg) {
  if (!ximg) return nullptr;

  int w = ximg->width;
  int h = ximg->height;
  int stride = w * 4;
  std::vector<uint8_t> pixels(static_cast<size_t>(stride) * h);

  // Fast path: 32bpp little-endian with standard RGB masks (most common).
  // In-memory layout is already B G R pad — just copy and set alpha.
  if (ximg->bits_per_pixel == 32 && ximg->byte_order == LSBFirst &&
      ximg->red_mask == 0xFF0000 && ximg->green_mask == 0x00FF00 &&
      ximg->blue_mask == 0x0000FF) {
    for (int y = 0; y < h; ++y) {
      const uint8_t* src =
          reinterpret_cast<const uint8_t*>(ximg->data) +
          static_cast<ptrdiff_t>(y) * ximg->bytes_per_line;
      uint8_t* dst = pixels.data() + static_cast<ptrdiff_t>(y) * stride;
      std::memcpy(dst, src, static_cast<size_t>(w) * 4);
      for (int x = 0; x < w; ++x) dst[x * 4 + 3] = 0xFF;
    }
  } else {
    // Generic fallback via XGetPixel.
    for (int y = 0; y < h; ++y) {
      uint8_t* dst = pixels.data() + static_cast<ptrdiff_t>(y) * stride;
      for (int x = 0; x < w; ++x) {
        unsigned long px = XGetPixel(ximg, x, y);
        dst[x * 4 + 0] = static_cast<uint8_t>((px >> 0) & 0xFF);
        dst[x * 4 + 1] = static_cast<uint8_t>((px >> 8) & 0xFF);
        dst[x * 4 + 2] = static_cast<uint8_t>((px >> 16) & 0xFF);
        dst[x * 4 + 3] = 0xFF;
      }
    }
  }

  return Image::CreateFromData(w, h, stride, kPixelGrabFormatBgra8,
                               std::move(pixels));
}

// -----------------------------------------------------------------------

std::vector<PixelGrabScreenInfo> X11CaptureBackend::GetScreens() {
  std::vector<PixelGrabScreenInfo> screens;
  if (!initialized_) return screens;

  auto* dpy = static_cast<Display*>(display_);
  int scr = DefaultScreen(dpy);

  PixelGrabScreenInfo info{};
  info.index = 0;
  info.x = 0;
  info.y = 0;
  info.width = DisplayWidth(dpy, scr);
  info.height = DisplayHeight(dpy, scr);
  info.is_primary = 1;
  std::snprintf(info.name, sizeof(info.name), "Screen %d", scr);
  screens.push_back(info);
  return screens;
}

std::unique_ptr<Image> X11CaptureBackend::CaptureScreen(int screen_index) {
  (void)screen_index;
  if (!initialized_) return nullptr;

  auto* dpy = static_cast<Display*>(display_);
  int scr = DefaultScreen(dpy);
  Window root = RootWindow(dpy, scr);
  int w = DisplayWidth(dpy, scr);
  int h = DisplayHeight(dpy, scr);

  XImage* ximg = XGetImage(dpy, root, 0, 0, w, h, AllPlanes, ZPixmap);
  if (!ximg) {
    PIXELGRAB_LOG_ERROR("XGetImage failed for screen capture");
    return nullptr;
  }

  auto img = XImageToImage(ximg);
  XDestroyImage(ximg);
  return img;
}

std::unique_ptr<Image> X11CaptureBackend::CaptureRegion(int x, int y,
                                                        int width,
                                                        int height) {
  if (!initialized_) return nullptr;

  auto* dpy = static_cast<Display*>(display_);
  int scr = DefaultScreen(dpy);
  Window root = RootWindow(dpy, scr);
  int scr_w = DisplayWidth(dpy, scr);
  int scr_h = DisplayHeight(dpy, scr);

  if (x < 0) { width += x; x = 0; }
  if (y < 0) { height += y; y = 0; }
  if (x + width > scr_w) width = scr_w - x;
  if (y + height > scr_h) height = scr_h - y;
  if (width <= 0 || height <= 0) return nullptr;

  XImage* ximg = XGetImage(dpy, root, x, y, width, height,
                           AllPlanes, ZPixmap);
  if (!ximg) {
    PIXELGRAB_LOG_ERROR("XGetImage failed for region capture");
    return nullptr;
  }

  auto img = XImageToImage(ximg);
  XDestroyImage(ximg);
  return img;
}

std::unique_ptr<Image> X11CaptureBackend::CaptureWindow(
    uint64_t window_handle) {
  if (!initialized_) return nullptr;

  auto* dpy = static_cast<Display*>(display_);
  Window win = static_cast<Window>(window_handle);

  XWindowAttributes attrs;
  if (!XGetWindowAttributes(dpy, win, &attrs)) {
    PIXELGRAB_LOG_ERROR("XGetWindowAttributes failed");
    return nullptr;
  }

  int scr = DefaultScreen(dpy);
  Window root = RootWindow(dpy, scr);
  int abs_x = 0, abs_y = 0;
  Window child;
  XTranslateCoordinates(dpy, win, root, 0, 0, &abs_x, &abs_y, &child);

  XImage* ximg = XGetImage(dpy, root, abs_x, abs_y,
                           attrs.width, attrs.height, AllPlanes, ZPixmap);
  if (!ximg) {
    ximg = XGetImage(dpy, win, 0, 0, attrs.width, attrs.height,
                     AllPlanes, ZPixmap);
    if (!ximg) {
      PIXELGRAB_LOG_ERROR("XGetImage failed for window capture");
      return nullptr;
    }
  }

  auto img = XImageToImage(ximg);
  XDestroyImage(ximg);
  return img;
}

std::vector<PixelGrabWindowInfo> X11CaptureBackend::EnumerateWindows() {
  std::vector<PixelGrabWindowInfo> result;
  if (!initialized_) return result;

  auto* dpy = static_cast<Display*>(display_);
  int scr = DefaultScreen(dpy);
  Window root = RootWindow(dpy, scr);

  // Prefer EWMH _NET_CLIENT_LIST_STACKING (topmost last).
  Atom net_cl = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", True);
  if (net_cl == None)
    net_cl = XInternAtom(dpy, "_NET_CLIENT_LIST", True);

  Window* wins = nullptr;
  unsigned long n_wins = 0;
  bool ewmh = false;

  if (net_cl != None) {
    Atom type;
    int fmt;
    unsigned long items, after;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(dpy, root, net_cl, 0, ~0L, False, XA_WINDOW,
                           &type, &fmt, &items, &after, &data) == Success &&
        data) {
      wins = reinterpret_cast<Window*>(data);
      n_wins = items;
      ewmh = true;
    }
  }

  Window root_ret, parent_ret;
  Window* children = nullptr;
  unsigned int n_children = 0;
  if (!ewmh) {
    XQueryTree(dpy, root, &root_ret, &parent_ret, &children, &n_children);
    wins = children;
    n_wins = n_children;
  }

  Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", True);
  Atom utf8_str = XInternAtom(dpy, "UTF8_STRING", True);
  Atom net_wm_pid = XInternAtom(dpy, "_NET_WM_PID", True);

  for (unsigned long i = 0; i < n_wins; ++i) {
    Window w = wins[i];
    XWindowAttributes a;
    if (!XGetWindowAttributes(dpy, w, &a)) continue;
    if (a.map_state != IsViewable || a.width <= 1 || a.height <= 1) continue;

    int ax = 0, ay = 0;
    Window ch;
    XTranslateCoordinates(dpy, w, root, 0, 0, &ax, &ay, &ch);

    PixelGrabWindowInfo info{};
    info.id = static_cast<uint64_t>(w);
    info.x = ax;
    info.y = ay;
    info.width = a.width;
    info.height = a.height;
    info.is_visible = 1;

    // Window title: _NET_WM_NAME (UTF-8) > WM_NAME
    bool got = false;
    if (net_wm_name != None && utf8_str != None) {
      Atom type;
      int fmt;
      unsigned long items, after;
      unsigned char* data = nullptr;
      if (XGetWindowProperty(dpy, w, net_wm_name, 0, 256, False, utf8_str,
                             &type, &fmt, &items, &after, &data) == Success &&
          data) {
        std::strncpy(info.title, reinterpret_cast<char*>(data),
                     sizeof(info.title) - 1);
        XFree(data);
        got = true;
      }
    }
    if (!got) {
      XTextProperty tp;
      if (XGetWMName(dpy, w, &tp) && tp.value) {
        std::strncpy(info.title, reinterpret_cast<char*>(tp.value),
                     sizeof(info.title) - 1);
        XFree(tp.value);
      }
    }

    // Process name via _NET_WM_PID + /proc/PID/comm
    if (net_wm_pid != None) {
      Atom type;
      int fmt;
      unsigned long items, after;
      unsigned char* data = nullptr;
      if (XGetWindowProperty(dpy, w, net_wm_pid, 0, 1, False, XA_CARDINAL,
                             &type, &fmt, &items, &after, &data) == Success &&
          data) {
        auto pid = *reinterpret_cast<uint32_t*>(data);
        XFree(data);
        char path[64];
        std::snprintf(path, sizeof(path), "/proc/%u/comm", pid);
        FILE* f = std::fopen(path, "r");
        if (f) {
          if (std::fgets(info.process_name, sizeof(info.process_name), f)) {
            size_t len = std::strlen(info.process_name);
            if (len > 0 && info.process_name[len - 1] == '\n')
              info.process_name[len - 1] = '\0';
          }
          std::fclose(f);
        }
      }
    }

    result.push_back(info);
  }

  if (ewmh)
    XFree(wins);
  else if (children)
    XFree(children);

  return result;
}

bool X11CaptureBackend::EnableDpiAwareness() { return true; }

bool X11CaptureBackend::GetDpiInfo(int screen_index,
                                   PixelGrabDpiInfo* out_info) {
  if (!out_info) return false;

  out_info->screen_index = screen_index;
  out_info->scale_x = 1.0f;
  out_info->scale_y = 1.0f;
  out_info->dpi_x = 96;
  out_info->dpi_y = 96;

  if (!initialized_) return true;

  auto* dpy = static_cast<Display*>(display_);
  char* xdpi = XGetDefault(dpy, "Xft", "dpi");
  if (xdpi) {
    int dpi = std::atoi(xdpi);
    if (dpi > 0) {
      out_info->dpi_x = dpi;
      out_info->dpi_y = dpi;
      out_info->scale_x = static_cast<float>(dpi) / 96.0f;
      out_info->scale_y = static_cast<float>(dpi) / 96.0f;
      return true;
    }
  }

  const char* gdk_scale = std::getenv("GDK_SCALE");
  if (gdk_scale) {
    float scale = static_cast<float>(std::atof(gdk_scale));
    if (scale > 0.0f) {
      out_info->scale_x = scale;
      out_info->scale_y = scale;
      out_info->dpi_x = static_cast<int>(96.0f * scale);
      out_info->dpi_y = static_cast<int>(96.0f * scale);
    }
  }

  return true;
}

// Factory function.
std::unique_ptr<CaptureBackend> CreatePlatformBackend() {
  return std::make_unique<X11CaptureBackend>();
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // __linux__
