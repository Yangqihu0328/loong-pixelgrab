// Copyright 2026 The PixelGrab Authors
// Color picker overlay — shows magnifier + real-time color info at cursor.
// Linux implementation using GTK3 + Cairo.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_LINUX_LINUX_COLOR_PICKER_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_LINUX_LINUX_COLOR_PICKER_H_

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include "pixelgrab/pixelgrab.h"
#include <gtk/gtk.h>

class ColorPicker {
 public:
  void Show();
  void Dismiss();
  bool IsActive() const { return active_; }

 private:
  void UpdateAtCursor();

  static gboolean OnDraw(GtkWidget* widget, cairo_t* cr, gpointer data);
  static gboolean OnKeyPress(GtkWidget* widget, GdkEventKey* event,
                             gpointer data);
  static gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* event,
                                gpointer data);
  static gboolean OnTimer(gpointer data);

  bool        active_ = false;
  GtkWidget*  window_ = nullptr;
  GtkWidget*  drawing_area_ = nullptr;
  guint       timer_id_ = 0;

  PixelGrabColor    cur_color_ = {};
  PixelGrabColorHsv cur_hsv_ = {};
  char              hex_buf_[16] = {};
  int               cursor_x_ = 0;
  int               cursor_y_ = 0;
};

#endif  // __linux__
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_LINUX_LINUX_COLOR_PICKER_H_
