// Copyright 2026 The loong-pixelgrab Authors
// Linux element detector using X11 window tree traversal.
// Falls back to XQueryTree-based detection (no AT-SPI2 dependency).

#include "platform/linux/x11_element_detector.h"

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <algorithm>
#include <cstring>
#include <vector>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "core/logger.h"

namespace pixelgrab {
namespace internal {

namespace {

struct DetectorDisplay {
  Display* dpy = nullptr;
  DetectorDisplay() { dpy = XOpenDisplay(nullptr); }
  ~DetectorDisplay() {
    if (dpy) XCloseDisplay(dpy);
  }
  explicit operator bool() const { return dpy != nullptr; }
};

// Recursively find the deepest child window containing the point.
Window FindWindowAt(Display* dpy, Window parent, int x, int y) {
  Window root_ret, parent_ret;
  Window* children = nullptr;
  unsigned int n = 0;

  if (!XQueryTree(dpy, parent, &root_ret, &parent_ret, &children, &n))
    return parent;

  // Walk children back-to-front (topmost last in X11 stacking order).
  Window found = parent;
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    XWindowAttributes a;
    if (!XGetWindowAttributes(dpy, children[i], &a)) continue;
    if (a.map_state != IsViewable) continue;

    int abs_x = 0, abs_y = 0;
    Window child;
    XTranslateCoordinates(dpy, children[i], DefaultRootWindow(dpy),
                          0, 0, &abs_x, &abs_y, &child);

    if (x >= abs_x && x < abs_x + a.width &&
        y >= abs_y && y < abs_y + a.height) {
      found = FindWindowAt(dpy, children[i], x, y);
      break;
    }
  }

  if (children) XFree(children);
  return found;
}

void FillInfo(Display* dpy, Window win, ElementInfo* info) {
  *info = ElementInfo{};

  XWindowAttributes a;
  if (!XGetWindowAttributes(dpy, win, &a)) return;

  int abs_x = 0, abs_y = 0;
  Window child;
  XTranslateCoordinates(dpy, win, DefaultRootWindow(dpy),
                        0, 0, &abs_x, &abs_y, &child);

  info->x = abs_x;
  info->y = abs_y;
  info->width = a.width;
  info->height = a.height;

  // Window title: _NET_WM_NAME (UTF-8) > WM_NAME
  Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", True);
  Atom utf8 = XInternAtom(dpy, "UTF8_STRING", True);
  bool got = false;
  if (net_wm_name != None && utf8 != None) {
    Atom type;
    int fmt;
    unsigned long items, after;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(dpy, win, net_wm_name, 0, 256, False, utf8,
                           &type, &fmt, &items, &after, &data) == Success &&
        data) {
      info->name.assign(reinterpret_cast<char*>(data), items);
      XFree(data);
      got = true;
    }
  }
  if (!got) {
    XTextProperty tp;
    if (XGetWMName(dpy, win, &tp) && tp.value) {
      info->name.assign(reinterpret_cast<char*>(tp.value));
      XFree(tp.value);
    }
  }

  // WM_CLASS as the "role"
  XClassHint cls;
  if (XGetClassHint(dpy, win, &cls)) {
    if (cls.res_class) {
      info->role.assign(cls.res_class);
      XFree(cls.res_class);
    }
    if (cls.res_name) XFree(cls.res_name);
  }
}

}  // namespace

bool X11ElementDetector::Initialize() { return true; }

bool X11ElementDetector::DetectElement(int screen_x, int screen_y,
                                       ElementInfo* out_info) {
  if (!out_info) return false;
  DetectorDisplay x;
  if (!x) return false;

  Window root = DefaultRootWindow(x.dpy);
  Window found = FindWindowAt(x.dpy, root, screen_x, screen_y);
  if (found == root) return false;

  FillInfo(x.dpy, found, out_info);
  return true;
}

int X11ElementDetector::DetectElements(int screen_x, int screen_y,
                                       ElementInfo* out_infos,
                                       int max_count) {
  if (!out_infos || max_count <= 0) return 0;
  DetectorDisplay x;
  if (!x) return 0;

  Window root = DefaultRootWindow(x.dpy);
  Window found = FindWindowAt(x.dpy, root, screen_x, screen_y);
  if (found == root) return 0;

  // Collect ancestors from deepest to root.
  std::vector<Window> chain;
  Window cur = found;
  while (cur && cur != root) {
    chain.push_back(cur);
    Window parent_ret, root_ret;
    Window* ch = nullptr;
    unsigned int n = 0;
    if (!XQueryTree(x.dpy, cur, &root_ret, &parent_ret, &ch, &n)) break;
    if (ch) XFree(ch);
    cur = parent_ret;
  }

  int count = std::min(static_cast<int>(chain.size()), max_count);
  for (int i = 0; i < count; ++i)
    FillInfo(x.dpy, chain[i], &out_infos[i]);

  return count;
}

// Factory implementation.
std::unique_ptr<ElementDetector> CreatePlatformElementDetector() {
  auto detector = std::make_unique<X11ElementDetector>();
  if (!detector->Initialize()) return nullptr;
  return detector;
}

}  // namespace internal
}  // namespace pixelgrab

#endif  // __linux__
