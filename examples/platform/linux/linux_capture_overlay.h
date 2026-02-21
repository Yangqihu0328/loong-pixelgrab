// Copyright 2026 The PixelGrab Authors
// Full-screen capture overlay — region selection + toolbar (GTK3 + Cairo).

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_LINUX_LINUX_CAPTURE_OVERLAY_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_LINUX_LINUX_CAPTURE_OVERLAY_H_

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <gtk/gtk.h>
#include "pixelgrab/pixelgrab.h"

class CaptureOverlay {
 public:
  void Start();
  void Dismiss();
  bool IsActive() const { return active_; }

 private:
  // GTK callbacks
  static gboolean OnDraw(GtkWidget* widget, cairo_t* cr, gpointer data);
  static gboolean OnKeyPress(GtkWidget* widget, GdkEventKey* ev,
                              gpointer data);
  static gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* ev,
                                 gpointer data);
  static gboolean OnButtonRelease(GtkWidget* widget, GdkEventButton* ev,
                                   gpointer data);
  static gboolean OnMotion(GtkWidget* widget, GdkEventMotion* ev,
                            gpointer data);

  void DrawOverlay(cairo_t* cr, int win_w, int win_h);
  void DrawToolbar(cairo_t* cr, int win_w, int win_h);

  void HandleToolbarClick(int mx, int my, int win_w, int win_h);
  void CopyToClipboard();
  void PinSelection();
  void SaveToFile();
  void DoAnnotation(int tool_index);

  // Toolbar button layout helpers
  struct BtnRect { int x, y, w, h; };
  BtnRect GetToolbarBtnRect(int index, int win_w, int win_h) const;
  int toolbar_btn_count() const;

  bool active_ = false;
  bool selecting_ = false;
  bool selected_ = false;

  int press_x_ = 0, press_y_ = 0;
  int sel_x_ = 0, sel_y_ = 0, sel_w_ = 0, sel_h_ = 0;
  int cur_x_ = 0, cur_y_ = 0;

  GtkWidget* window_ = nullptr;
  PixelGrabImage* screenshot_ = nullptr;
  cairo_surface_t* bg_surface_ = nullptr;
};

#endif  // __linux__
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_LINUX_LINUX_CAPTURE_OVERLAY_H_
