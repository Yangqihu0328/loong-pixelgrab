// Copyright 2026 The PixelGrab Authors
// Linux application implementation — GTK3 status icon / tray menu.

#include "platform/linux/linux_application.h"

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <cstdio>
#include <cstring>
#include <gtk/gtk.h>

// ========================================================================
// GTK callbacks
// ========================================================================

static void OnCapture(GtkMenuItem* /*item*/, gpointer /*user_data*/) {
  auto& app = LinuxApplication::instance();
  if (!app.GetColorPicker().IsActive())
    app.GetColorPicker().Show();
}

static void OnAbout(GtkMenuItem* /*item*/, gpointer /*user_data*/) {
  GtkWidget* dialog = gtk_message_dialog_new(
      nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
      "PixelGrab v%s\nCross-platform screenshot tool.",
      pixelgrab_version_string());
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

static void OnQuit(GtkMenuItem* /*item*/, gpointer /*user_data*/) {
  auto& app = LinuxApplication::instance();
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

  // Load language preference
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

  // Build popup menu
  GtkWidget* menu = gtk_menu_new();

  GtkWidget* capture_item = gtk_menu_item_new_with_label("Capture (F1)");
  g_signal_connect(capture_item, "activate", G_CALLBACK(OnCapture), nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), capture_item);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                        gtk_separator_menu_item_new());

  GtkWidget* about_item = gtk_menu_item_new_with_label("About");
  g_signal_connect(about_item, "activate", G_CALLBACK(OnAbout), nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), about_item);

  GtkWidget* quit_item = gtk_menu_item_new_with_label("Quit");
  g_signal_connect(quit_item, "activate", G_CALLBACK(OnQuit), nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);

  gtk_widget_show_all(menu);

  // Create status icon (GtkStatusIcon — deprecated but widely supported)
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GtkStatusIcon* icon = gtk_status_icon_new_from_icon_name("camera-photo");
  gtk_status_icon_set_tooltip_text(icon, "PixelGrab");
  gtk_status_icon_set_visible(icon, TRUE);
  g_signal_connect(icon, "popup-menu",
                   G_CALLBACK(OnStatusIconPopup), menu);
  G_GNUC_END_IGNORE_DEPRECATIONS

  std::printf("Ready. Use tray icon or hotkey to capture.\n");

  gtk_main();

  g_object_unref(icon);

  return 0;
}

void LinuxApplication::Shutdown() {
  color_picker_.Dismiss();

  if (hotkey_) hotkey_->UnregisterAll();

  if (ctx_) {
    pixelgrab_context_destroy(ctx_);
    ctx_ = nullptr;
  }
  std::printf("\nExiting...\n");
}

#endif  // __linux__
