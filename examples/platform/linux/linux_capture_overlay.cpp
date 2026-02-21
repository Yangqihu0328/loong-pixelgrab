// Copyright 2026 The PixelGrab Authors
// Full-screen capture overlay — region selection + toolbar (GTK3 + Cairo).

#include "platform/linux/linux_capture_overlay.h"

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include "platform/linux/linux_application.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

static constexpr int kTBBtnW = 64;
static constexpr int kTBBtnH = 28;
static constexpr int kTBGap  = 4;
static constexpr int kTBPad  = 8;
static constexpr int kTBBarH = kTBBtnH + kTBPad * 2;

static const char* kBtnLabels[] = {
    "Copy", "Pin", "Save", "Cancel",
};
static constexpr int kBtnCount = 4;

// -----------------------------------------------------------------------
// Start / Dismiss
// -----------------------------------------------------------------------

void CaptureOverlay::Start() {
  if (active_) return;

  auto& app = LinuxApplication::instance();

  // Capture full screen first.
  screenshot_ = pixelgrab_capture_screen(app.Ctx(), 0);
  if (!screenshot_) {
    std::fprintf(stderr, "[Capture] pixelgrab_capture_screen failed.\n");
    return;
  }

  int sw = pixelgrab_image_get_width(screenshot_);
  int sh = pixelgrab_image_get_height(screenshot_);
  int stride = pixelgrab_image_get_stride(screenshot_);
  const uint8_t* pixels = pixelgrab_image_get_data(screenshot_);

  // Create Cairo surface from screenshot (BGRA → ARGB32 on LE — same thing).
  bg_surface_ = cairo_image_surface_create_for_data(
      const_cast<uint8_t*>(pixels), CAIRO_FORMAT_ARGB32, sw, sh, stride);

  active_ = true;
  selecting_ = false;
  selected_ = false;

  // Create fullscreen window.
  window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(window_), FALSE);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window_), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(window_), TRUE);
  gtk_window_fullscreen(GTK_WINDOW(window_));
  gtk_widget_set_app_paintable(window_, TRUE);

  gtk_widget_add_events(window_,
                        GDK_KEY_PRESS_MASK | GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

  g_signal_connect(window_, "draw", G_CALLBACK(OnDraw), this);
  g_signal_connect(window_, "key-press-event", G_CALLBACK(OnKeyPress), this);
  g_signal_connect(window_, "button-press-event",
                   G_CALLBACK(OnButtonPress), this);
  g_signal_connect(window_, "button-release-event",
                   G_CALLBACK(OnButtonRelease), this);
  g_signal_connect(window_, "motion-notify-event",
                   G_CALLBACK(OnMotion), this);

  gtk_widget_show_all(window_);

  // Set crosshair cursor.
  GdkWindow* gdk_win = gtk_widget_get_window(window_);
  if (gdk_win) {
    GdkCursor* cross = gdk_cursor_new_from_name(
        gdk_window_get_display(gdk_win), "crosshair");
    gdk_window_set_cursor(gdk_win, cross);
    g_object_unref(cross);
  }

  std::printf("[Capture] Overlay active. Select region or press Esc.\n");
}

void CaptureOverlay::Dismiss() {
  if (!active_) return;
  active_ = false;
  selecting_ = false;
  selected_ = false;

  if (window_) {
    gtk_widget_destroy(window_);
    window_ = nullptr;
  }
  if (bg_surface_) {
    cairo_surface_destroy(bg_surface_);
    bg_surface_ = nullptr;
  }
  if (screenshot_) {
    pixelgrab_image_destroy(screenshot_);
    screenshot_ = nullptr;
  }
}

// -----------------------------------------------------------------------
// Drawing
// -----------------------------------------------------------------------

gboolean CaptureOverlay::OnDraw(GtkWidget* /*widget*/, cairo_t* cr,
                                 gpointer data) {
  auto* self = static_cast<CaptureOverlay*>(data);
  int w = 0, h = 0;
  if (self->bg_surface_) {
    w = cairo_image_surface_get_width(self->bg_surface_);
    h = cairo_image_surface_get_height(self->bg_surface_);
  }
  self->DrawOverlay(cr, w, h);
  return FALSE;
}

void CaptureOverlay::DrawOverlay(cairo_t* cr, int win_w, int win_h) {
  if (!bg_surface_) return;

  // Draw full screenshot as background.
  cairo_set_source_surface(cr, bg_surface_, 0, 0);
  cairo_paint(cr);

  // Dim overlay (semi-transparent black).
  cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
  cairo_rectangle(cr, 0, 0, win_w, win_h);

  // If we have a selection, cut it out of the dim.
  int sx = sel_x_, sy = sel_y_, sw = sel_w_, sh = sel_h_;
  if (selecting_ && !selected_) {
    sx = std::min(press_x_, cur_x_);
    sy = std::min(press_y_, cur_y_);
    sw = std::abs(cur_x_ - press_x_);
    sh = std::abs(cur_y_ - press_y_);
  }

  if ((selecting_ || selected_) && sw > 0 && sh > 0) {
    // Subtract the selection rect from the dim area.
    cairo_rectangle(cr, sx, sy, sw, sh);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_fill(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);

    // Selection border (white dashed).
    cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.9);
    cairo_set_line_width(cr, 2.0);
    double dashes[] = {6.0, 3.0};
    cairo_set_dash(cr, dashes, 2, 0);
    cairo_rectangle(cr, sx + 0.5, sy + 0.5, sw - 1, sh - 1);
    cairo_stroke(cr);
    cairo_set_dash(cr, nullptr, 0, 0);

    // Size label.
    char label[64];
    std::snprintf(label, sizeof(label), "%d × %d", sw, sh);
    cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.9);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13);
    int lx = sx;
    int ly = sy - 8;
    if (ly < 16) ly = sy + sh + 18;
    cairo_move_to(cr, lx, ly);
    cairo_show_text(cr, label);
  } else {
    cairo_fill(cr);
  }

  // Draw toolbar if selection is finalized.
  if (selected_ && sel_w_ > 0 && sel_h_ > 0)
    DrawToolbar(cr, win_w, win_h);
}

int CaptureOverlay::toolbar_btn_count() const { return kBtnCount; }

CaptureOverlay::BtnRect CaptureOverlay::GetToolbarBtnRect(
    int index, int /*win_w*/, int /*win_h*/) const {
  int total_w = kBtnCount * kTBBtnW + (kBtnCount - 1) * kTBGap + kTBPad * 2;
  int bar_x = sel_x_ + sel_w_ - total_w;
  if (bar_x < 0) bar_x = sel_x_;
  int bar_y = sel_y_ + sel_h_ + 6;

  return {bar_x + kTBPad + index * (kTBBtnW + kTBGap),
          bar_y + kTBPad, kTBBtnW, kTBBtnH};
}

void CaptureOverlay::DrawToolbar(cairo_t* cr, int win_w, int win_h) {
  int total_w = kBtnCount * kTBBtnW + (kBtnCount - 1) * kTBGap + kTBPad * 2;
  int bar_x = sel_x_ + sel_w_ - total_w;
  if (bar_x < 0) bar_x = sel_x_;
  int bar_y = sel_y_ + sel_h_ + 6;

  // Toolbar background.
  cairo_set_source_rgba(cr, 0.15, 0.15, 0.15, 0.92);
  double r = 6.0;
  double x0 = bar_x, y0 = bar_y;
  double x1 = bar_x + total_w, y1 = bar_y + kTBBarH;
  cairo_new_sub_path(cr);
  cairo_arc(cr, x1 - r, y0 + r, r, -M_PI / 2, 0);
  cairo_arc(cr, x1 - r, y1 - r, r, 0, M_PI / 2);
  cairo_arc(cr, x0 + r, y1 - r, r, M_PI / 2, M_PI);
  cairo_arc(cr, x0 + r, y0 + r, r, M_PI, 3 * M_PI / 2);
  cairo_close_path(cr);
  cairo_fill(cr);

  // Buttons.
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 12);

  for (int i = 0; i < kBtnCount; ++i) {
    BtnRect br = GetToolbarBtnRect(i, win_w, win_h);

    // Button background.
    bool is_cancel = (i == kBtnCount - 1);
    if (is_cancel)
      cairo_set_source_rgba(cr, 0.6, 0.15, 0.15, 0.9);
    else
      cairo_set_source_rgba(cr, 0.25, 0.55, 0.85, 0.9);

    double bx = br.x, by = br.y, bw = br.w, bh = br.h;
    double br2 = 4.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, bx + bw - br2, by + br2, br2, -M_PI / 2, 0);
    cairo_arc(cr, bx + bw - br2, by + bh - br2, br2, 0, M_PI / 2);
    cairo_arc(cr, bx + br2, by + bh - br2, br2, M_PI / 2, M_PI);
    cairo_arc(cr, bx + br2, by + br2, br2, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Button label.
    cairo_text_extents_t ext;
    cairo_text_extents(cr, kBtnLabels[i], &ext);
    double tx = bx + (bw - ext.width) / 2 - ext.x_bearing;
    double ty = by + (bh - ext.height) / 2 - ext.y_bearing;
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, tx, ty);
    cairo_show_text(cr, kBtnLabels[i]);
  }
}

// -----------------------------------------------------------------------
// Input events
// -----------------------------------------------------------------------

gboolean CaptureOverlay::OnKeyPress(GtkWidget* /*widget*/, GdkEventKey* ev,
                                     gpointer data) {
  auto* self = static_cast<CaptureOverlay*>(data);
  if (ev->keyval == GDK_KEY_Escape) {
    self->Dismiss();
    return TRUE;
  }
  return FALSE;
}

gboolean CaptureOverlay::OnButtonPress(GtkWidget* /*widget*/,
                                        GdkEventButton* ev, gpointer data) {
  auto* self = static_cast<CaptureOverlay*>(data);
  if (ev->button == 3) {
    self->Dismiss();
    return TRUE;
  }
  if (ev->button != 1) return FALSE;

  // Check toolbar click first.
  if (self->selected_) {
    int w = 0, h = 0;
    if (self->bg_surface_) {
      w = cairo_image_surface_get_width(self->bg_surface_);
      h = cairo_image_surface_get_height(self->bg_surface_);
    }
    self->HandleToolbarClick(static_cast<int>(ev->x),
                              static_cast<int>(ev->y), w, h);
    return TRUE;
  }

  // Start selection.
  self->press_x_ = static_cast<int>(ev->x);
  self->press_y_ = static_cast<int>(ev->y);
  self->cur_x_ = self->press_x_;
  self->cur_y_ = self->press_y_;
  self->selecting_ = true;
  self->selected_ = false;
  gtk_widget_queue_draw(self->window_);
  return TRUE;
}

gboolean CaptureOverlay::OnButtonRelease(GtkWidget* /*widget*/,
                                          GdkEventButton* ev,
                                          gpointer data) {
  auto* self = static_cast<CaptureOverlay*>(data);
  if (ev->button != 1 || !self->selecting_) return FALSE;

  self->cur_x_ = static_cast<int>(ev->x);
  self->cur_y_ = static_cast<int>(ev->y);

  self->sel_x_ = std::min(self->press_x_, self->cur_x_);
  self->sel_y_ = std::min(self->press_y_, self->cur_y_);
  self->sel_w_ = std::abs(self->cur_x_ - self->press_x_);
  self->sel_h_ = std::abs(self->cur_y_ - self->press_y_);

  self->selecting_ = false;

  if (self->sel_w_ > 5 && self->sel_h_ > 5) {
    self->selected_ = true;
    // Restore default cursor.
    GdkWindow* gdk_win = gtk_widget_get_window(self->window_);
    if (gdk_win) gdk_window_set_cursor(gdk_win, nullptr);
    std::printf("[Capture] Selected region: %d,%d %dx%d\n",
                self->sel_x_, self->sel_y_, self->sel_w_, self->sel_h_);
  } else {
    self->sel_w_ = 0;
    self->sel_h_ = 0;
  }

  gtk_widget_queue_draw(self->window_);
  return TRUE;
}

gboolean CaptureOverlay::OnMotion(GtkWidget* /*widget*/, GdkEventMotion* ev,
                                   gpointer data) {
  auto* self = static_cast<CaptureOverlay*>(data);
  if (!self->selecting_) return FALSE;

  self->cur_x_ = static_cast<int>(ev->x);
  self->cur_y_ = static_cast<int>(ev->y);
  gtk_widget_queue_draw(self->window_);
  return TRUE;
}

// -----------------------------------------------------------------------
// Toolbar actions
// -----------------------------------------------------------------------

void CaptureOverlay::HandleToolbarClick(int mx, int my,
                                         int win_w, int win_h) {
  for (int i = 0; i < kBtnCount; ++i) {
    BtnRect br = GetToolbarBtnRect(i, win_w, win_h);
    if (mx >= br.x && mx < br.x + br.w &&
        my >= br.y && my < br.y + br.h) {
      switch (i) {
        case 0: CopyToClipboard(); break;
        case 1: PinSelection();    break;
        case 2: SaveToFile();      break;
        case 3: Dismiss();         break;
      }
      return;
    }
  }

  // Click outside toolbar restarts selection.
  selected_ = false;
  selecting_ = false;
  sel_w_ = 0;
  sel_h_ = 0;

  GdkWindow* gdk_win = gtk_widget_get_window(window_);
  if (gdk_win) {
    GdkCursor* cross = gdk_cursor_new_from_name(
        gdk_window_get_display(gdk_win), "crosshair");
    gdk_window_set_cursor(gdk_win, cross);
    g_object_unref(cross);
  }

  // Start new selection at this click position.
  press_x_ = mx;
  press_y_ = my;
  cur_x_ = mx;
  cur_y_ = my;
  selecting_ = true;
  gtk_widget_queue_draw(window_);
}

void CaptureOverlay::CopyToClipboard() {
  if (sel_w_ <= 0 || sel_h_ <= 0) return;

  auto& app = LinuxApplication::instance();
  PixelGrabImage* region = pixelgrab_capture_region(
      app.Ctx(), sel_x_, sel_y_, sel_w_, sel_h_);
  if (!region) {
    // Fallback: crop from the already-captured screenshot.
    region = screenshot_;
  }

  // Use GTK clipboard to copy the image.
  int w = pixelgrab_image_get_width(region);
  int h = pixelgrab_image_get_height(region);
  const uint8_t* data = pixelgrab_image_get_data(region);
  int stride = pixelgrab_image_get_stride(region);

  // Convert BGRA to RGBA for GdkPixbuf.
  GdkPixbuf* pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);
  int pb_stride = gdk_pixbuf_get_rowstride(pb);
  guchar* pb_pixels = gdk_pixbuf_get_pixels(pb);

  for (int row = 0; row < h; ++row) {
    const uint8_t* src = data + row * stride;
    guchar* dst = pb_pixels + row * pb_stride;
    for (int col = 0; col < w; ++col) {
      dst[col * 4 + 0] = src[col * 4 + 2];  // R
      dst[col * 4 + 1] = src[col * 4 + 1];  // G
      dst[col * 4 + 2] = src[col * 4 + 0];  // B
      dst[col * 4 + 3] = src[col * 4 + 3];  // A
    }
  }

  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_image(clipboard, pb);
  gtk_clipboard_store(clipboard);
  g_object_unref(pb);

  if (region != screenshot_)
    pixelgrab_image_destroy(region);

  std::printf("[Capture] Copied to clipboard (%dx%d).\n", w, h);
  Dismiss();
}

void CaptureOverlay::PinSelection() {
  if (sel_w_ <= 0 || sel_h_ <= 0) return;

  auto& app = LinuxApplication::instance();
  PixelGrabImage* region = pixelgrab_capture_region(
      app.Ctx(), sel_x_, sel_y_, sel_w_, sel_h_);
  if (!region) return;

  pixelgrab_pin_image(app.Ctx(), region, sel_x_, sel_y_);
  pixelgrab_image_destroy(region);

  std::printf("[Capture] Pinned to screen.\n");
  Dismiss();
}

void CaptureOverlay::SaveToFile() {
  if (sel_w_ <= 0 || sel_h_ <= 0) return;

  auto& app = LinuxApplication::instance();
  PixelGrabImage* region = pixelgrab_capture_region(
      app.Ctx(), sel_x_, sel_y_, sel_w_, sel_h_);
  if (!region) return;

  // Use GTK file chooser.
  Dismiss();

  GtkWidget* dialog = gtk_file_chooser_dialog_new(
      "Save Screenshot", nullptr, GTK_FILE_CHOOSER_ACTION_SAVE,
      "_Cancel", GTK_RESPONSE_CANCEL,
      "_Save", GTK_RESPONSE_ACCEPT, nullptr);
  gtk_file_chooser_set_do_overwrite_confirmation(
      GTK_FILE_CHOOSER(dialog), TRUE);
  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
                                     "screenshot.png");

  GtkFileFilter* filter = gtk_file_filter_new();
  gtk_file_filter_set_name(filter, "PNG Images");
  gtk_file_filter_add_pattern(filter, "*.png");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    PixelGrabError err = pixelgrab_image_export(
        region, filename, kPixelGrabImageFormatPng, 0);
    if (err == kPixelGrabOk)
      std::printf("[Capture] Saved to: %s\n", filename);
    else
      std::fprintf(stderr, "[Capture] Save failed: %d\n", err);
    g_free(filename);
  }

  gtk_widget_destroy(dialog);
  pixelgrab_image_destroy(region);
}

#endif  // __linux__
