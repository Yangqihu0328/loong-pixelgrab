// Copyright 2026 The PixelGrab Authors
// Linux application implementation — GTK3 tray + hotkey dispatch + capture.

#include "platform/linux/linux_application.h"

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <cstdio>
#include <cstring>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include "core/app_defs.h"

// ========================================================================
// X11 hotkey event polling
// ========================================================================

static Display* s_hotkey_dpy = nullptr;

static void InitHotkeyPoll() {
  s_hotkey_dpy = XOpenDisplay(nullptr);
  if (!s_hotkey_dpy) {
    std::fprintf(stderr, "  [Hotkey] Cannot open X display for polling.\n");
    return;
  }

  // Grab F1 and F3 on root window.
  Window root = DefaultRootWindow(s_hotkey_dpy);
  unsigned int mods[] = {0, Mod2Mask, LockMask, Mod2Mask | LockMask};
  KeyCode f1 = XKeysymToKeycode(s_hotkey_dpy, XK_F1);
  KeyCode f3 = XKeysymToKeycode(s_hotkey_dpy, XK_F3);

  for (unsigned int m : mods) {
    if (f1) XGrabKey(s_hotkey_dpy, f1, m, root, True,
                     GrabModeAsync, GrabModeAsync);
    if (f3) XGrabKey(s_hotkey_dpy, f3, m, root, True,
                     GrabModeAsync, GrabModeAsync);
  }
  XFlush(s_hotkey_dpy);
}

static void ShutdownHotkeyPoll() {
  if (!s_hotkey_dpy) return;
  Window root = DefaultRootWindow(s_hotkey_dpy);
  XUngrabKey(s_hotkey_dpy, AnyKey, AnyModifier, root);
  XCloseDisplay(s_hotkey_dpy);
  s_hotkey_dpy = nullptr;
}

gboolean LinuxApplication::OnHotkeyPoll(gpointer /*data*/) {
  if (!s_hotkey_dpy) return TRUE;

  while (XPending(s_hotkey_dpy)) {
    XEvent ev;
    XNextEvent(s_hotkey_dpy, &ev);
    if (ev.type != KeyPress) continue;

    KeySym ks = XkbKeycodeToKeysym(s_hotkey_dpy, ev.xkey.keycode, 0, 0);

    auto& app = LinuxApplication::instance();

    if (ks == XK_F1) {
      if (!app.capture_overlay_.IsActive())
        app.capture_overlay_.Start();
    } else if (ks == XK_F3) {
      // Pin last clipboard image.
      PixelGrabImage* clip = pixelgrab_clipboard_get_image(app.ctx_);
      if (clip) {
        int w = pixelgrab_image_get_width(clip);
        int h = pixelgrab_image_get_height(clip);
        pixelgrab_pin_image(app.ctx_, clip, 100, 100);
        pixelgrab_image_destroy(clip);
        std::printf("[F3] Pinned clipboard image (%dx%d).\n", w, h);
      } else {
        std::printf("[F3] No image in clipboard.\n");
      }
    }
  }
  return TRUE;
}

// ========================================================================
// GTK tray menu callbacks
// ========================================================================

static void OnMenuCapture(GtkMenuItem* /*item*/, gpointer /*data*/) {
  auto& app = LinuxApplication::instance();
  if (!app.GetCaptureOverlay().IsActive())
    app.GetCaptureOverlay().Start();
}

static void OnMenuColorPick(GtkMenuItem* /*item*/, gpointer /*data*/) {
  auto& app = LinuxApplication::instance();
  if (!app.GetColorPicker().IsActive())
    app.GetColorPicker().Show();
}

static void OnMenuPinClipboard(GtkMenuItem* /*item*/, gpointer /*data*/) {
  auto& app = LinuxApplication::instance();
  PixelGrabImage* clip = pixelgrab_clipboard_get_image(app.Ctx());
  if (clip) {
    pixelgrab_pin_image(app.Ctx(), clip, 100, 100);
    pixelgrab_image_destroy(clip);
  }
}

static void OnMenuAbout(GtkMenuItem* /*item*/, gpointer /*data*/) {
  GtkWidget* dialog = gtk_message_dialog_new(
      nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
      "PixelGrab v%s\n\n"
      "Cross-platform screenshot & annotation tool.\n"
      "Hotkeys: F1 = Capture, F3 = Pin clipboard",
      pixelgrab_version_string());
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

static void OnMenuQuit(GtkMenuItem* /*item*/, gpointer /*data*/) {
  auto& app = LinuxApplication::instance();
  app.GetCaptureOverlay().Dismiss();
  app.GetColorPicker().Dismiss();
  gtk_main_quit();
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void OnStatusIconPopup(GtkStatusIcon* /*icon*/, guint button,
                               guint activate_time, gpointer menu) {
  gtk_menu_popup(GTK_MENU(menu), nullptr, nullptr,
                 gtk_status_icon_position_menu, nullptr,
                 button, activate_time);
}

G_GNUC_END_IGNORE_DEPRECATIONS

// ========================================================================
// LinuxApplication singleton
// ========================================================================

LinuxApplication& LinuxApplication::instance() {
  static LinuxApplication app;
  return app;
}

bool LinuxApplication::Init() {
  ctx_ = pixelgrab_context_create();
  if (!ctx_) {
    std::fprintf(stderr, "FATAL: pixelgrab_context_create failed.\n");
    return false;
  }

  settings_ = CreatePlatformSettings();
  hotkey_   = CreatePlatformHotkey();
  http_     = CreatePlatformHttp();

  int lang_val = 0;
  if (settings_->GetInt("Language", &lang_val)) {
    if (lang_val >= 0 && lang_val < static_cast<int>(kLangCount))
      SetLanguage(static_cast<Language>(lang_val));
    else
      SetLanguage(DetectSystemLanguage());
  } else {
    SetLanguage(DetectSystemLanguage());
  }

  return true;
}

int LinuxApplication::Run() {
  int argc = 0;
  char** argv = nullptr;
  gtk_init(&argc, &argv);

  std::printf("PixelGrab v%s -- Linux (GTK3)\n", pixelgrab_version_string());
  std::printf("  F1 = Screenshot capture\n");
  std::printf("  F3 = Pin clipboard image\n");

  // Initialize X11 hotkey polling.
  InitHotkeyPoll();
  g_timeout_add(50, OnHotkeyPoll, nullptr);

  // Build popup menu.
  GtkWidget* menu = gtk_menu_new();

  GtkWidget* capture_item =
      gtk_menu_item_new_with_label("Screenshot  (F1)");
  g_signal_connect(capture_item, "activate",
                   G_CALLBACK(OnMenuCapture), nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), capture_item);

  GtkWidget* color_item =
      gtk_menu_item_new_with_label("Color Picker");
  g_signal_connect(color_item, "activate",
                   G_CALLBACK(OnMenuColorPick), nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), color_item);

  GtkWidget* pin_item =
      gtk_menu_item_new_with_label("Pin Clipboard  (F3)");
  g_signal_connect(pin_item, "activate",
                   G_CALLBACK(OnMenuPinClipboard), nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), pin_item);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                        gtk_separator_menu_item_new());

  GtkWidget* about_item = gtk_menu_item_new_with_label("About");
  g_signal_connect(about_item, "activate",
                   G_CALLBACK(OnMenuAbout), nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), about_item);

  GtkWidget* quit_item = gtk_menu_item_new_with_label("Quit");
  g_signal_connect(quit_item, "activate",
                   G_CALLBACK(OnMenuQuit), nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);

  gtk_widget_show_all(menu);

  // Create status icon.
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GtkStatusIcon* icon = gtk_status_icon_new_from_icon_name("camera-photo");
  gtk_status_icon_set_tooltip_text(icon, "PixelGrab");
  gtk_status_icon_set_visible(icon, TRUE);
  g_signal_connect(icon, "popup-menu",
                   G_CALLBACK(OnStatusIconPopup), menu);
  G_GNUC_END_IGNORE_DEPRECATIONS

  std::printf("Ready. Right-click tray icon for menu.\n");
  gtk_main();

  g_object_unref(icon);
  ShutdownHotkeyPoll();
  return 0;
}

void LinuxApplication::Shutdown() {
  capture_overlay_.Dismiss();
  color_picker_.Dismiss();

  if (hotkey_) hotkey_->UnregisterAll();

  if (ctx_) {
    pixelgrab_context_destroy(ctx_);
    ctx_ = nullptr;
  }
  std::printf("\nExiting...\n");
}

#endif  // __linux__
