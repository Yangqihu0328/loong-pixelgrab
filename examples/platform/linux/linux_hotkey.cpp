// Copyright 2026 The PixelGrab Authors
// Linux implementation of IPlatformHotkey using X11 XGrabKey.

#include "core/platform_hotkey.h"

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <cstdio>
#include <vector>
#include <X11/Xlib.h>
#include <X11/keysym.h>

// Map platform-neutral VK_F* codes to X11 keysyms.
// VK_F1 = 0x70 ... VK_F12 = 0x7B (same values as Windows).
static KeySym VKToX11Keysym(int vk) {
  if (vk >= 0x70 && vk <= 0x7B)
    return XK_F1 + (vk - 0x70);
  return static_cast<KeySym>(vk);
}

class LinuxPlatformHotkey : public IPlatformHotkey {
 public:
  LinuxPlatformHotkey() {
    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) {
      std::fprintf(stderr, "  [Linux] Cannot open X display for hotkeys.\n");
    }
  }

  ~LinuxPlatformHotkey() override {
    UnregisterAll();
    if (dpy_) XCloseDisplay(dpy_);
  }

  bool Register(int hotkey_id, int key_code) override {
    if (!dpy_) return false;

    Window root = DefaultRootWindow(dpy_);
    KeySym ks = VKToX11Keysym(key_code);
    KeyCode kc = XKeysymToKeycode(dpy_, ks);
    if (kc == 0) {
      std::printf("  [Linux] Unknown keycode for VK 0x%X\n", key_code);
      return false;
    }

    // Grab with various modifier masks to ignore NumLock/CapsLock/ScrollLock.
    unsigned int modifiers[] = {0, Mod2Mask, LockMask, Mod2Mask | LockMask};
    for (unsigned int mod : modifiers) {
      XGrabKey(dpy_, kc, mod, root, True, GrabModeAsync, GrabModeAsync);
    }
    XFlush(dpy_);

    entries_.push_back({hotkey_id, kc});
    return true;
  }

  void Unregister(int hotkey_id) override {
    if (!dpy_) return;
    Window root = DefaultRootWindow(dpy_);
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
      if (it->id == hotkey_id) {
        unsigned int modifiers[] = {0, Mod2Mask, LockMask, Mod2Mask | LockMask};
        for (unsigned int mod : modifiers) {
          XUngrabKey(dpy_, it->keycode, mod, root);
        }
        XFlush(dpy_);
        entries_.erase(it);
        return;
      }
    }
  }

  void UnregisterAll() override {
    if (!dpy_) return;
    Window root = DefaultRootWindow(dpy_);
    unsigned int modifiers[] = {0, Mod2Mask, LockMask, Mod2Mask | LockMask};
    for (const auto& e : entries_) {
      for (unsigned int mod : modifiers) {
        XUngrabKey(dpy_, e.keycode, mod, root);
      }
    }
    XFlush(dpy_);
    entries_.clear();
  }

 private:
  struct HotkeyEntry {
    int id;
    KeyCode keycode;
  };
  Display* dpy_ = nullptr;
  std::vector<HotkeyEntry> entries_;
};

std::unique_ptr<IPlatformHotkey> CreatePlatformHotkey() {
  return std::make_unique<LinuxPlatformHotkey>();
}

#endif  // __linux__
