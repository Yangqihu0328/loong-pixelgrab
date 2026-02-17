// Copyright 2026 The PixelGrab Authors
// Color picker overlay — magnifier + HEX/RGB/HSV display.
// Linux implementation using GTK3 + Cairo.

#include "platform/linux/linux_color_picker.h"

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include "platform/linux/linux_application.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

static constexpr int kPickerW = 200;
static constexpr int kPickerH = 160;
static constexpr int kMagRadius = 7;
static constexpr int kMagZoom = 8;
static constexpr int kMagSize = kMagRadius * 2 * kMagZoom;

// ---------------------------------------------------------------------------
// Show / Dismiss
// ---------------------------------------------------------------------------

void ColorPicker::Show() {
  if (active_) return;
  active_ = true;

  window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(window_), FALSE);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window_), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(window_), TRUE);
  gtk_window_set_keep_above(GTK_WINDOW(window_), TRUE);
  gtk_window_set_accept_focus(GTK_WINDOW(window_), TRUE);
  gtk_window_set_default_size(GTK_WINDOW(window_), kPickerW, kPickerH);
  gtk_window_set_resizable(GTK_WINDOW(window_), FALSE);
  gtk_window_set_type_hint(GTK_WINDOW(window_),
                           GDK_WINDOW_TYPE_HINT_UTILITY);
  gtk_widget_set_app_paintable(window_, TRUE);

  drawing_area_ = gtk_drawing_area_new();
  gtk_widget_set_size_request(drawing_area_, kPickerW, kPickerH);
  gtk_container_add(GTK_CONTAINER(window_), drawing_area_);

  gtk_widget_add_events(window_,
                        GDK_KEY_PRESS_MASK | GDK_BUTTON_PRESS_MASK);
  g_signal_connect(drawing_area_, "draw", G_CALLBACK(OnDraw), this);
  g_signal_connect(window_, "key-press-event",
                   G_CALLBACK(OnKeyPress), this);
  g_signal_connect(window_, "button-press-event",
                   G_CALLBACK(OnButtonPress), this);

  gtk_widget_show_all(window_);

  timer_id_ = g_timeout_add(30, OnTimer, this);
  UpdateAtCursor();

  std::printf("  [ColorPicker] Active. Left-click to copy, Esc to cancel.\n");
}

void ColorPicker::Dismiss() {
  if (!active_) return;
  active_ = false;

  if (timer_id_) {
    g_source_remove(timer_id_);
    timer_id_ = 0;
  }
  if (window_) {
    gtk_widget_destroy(window_);
    window_ = nullptr;
    drawing_area_ = nullptr;
  }
}

// ---------------------------------------------------------------------------
// Update / Timer
// ---------------------------------------------------------------------------

void ColorPicker::UpdateAtCursor() {
  auto& app = LinuxApplication::instance();

  GdkDisplay* display = gdk_display_get_default();
  GdkSeat* seat = gdk_display_get_default_seat(display);
  GdkDevice* pointer = gdk_seat_get_pointer(seat);
  int x = 0, y = 0;
  gdk_device_get_position(pointer, nullptr, &x, &y);
  cursor_x_ = x;
  cursor_y_ = y;

  pixelgrab_pick_color(app.Ctx(), cursor_x_, cursor_y_, &cur_color_);
  pixelgrab_color_rgb_to_hsv(&cur_color_, &cur_hsv_);
  pixelgrab_color_to_hex(&cur_color_, hex_buf_, sizeof(hex_buf_), 0);

  // Position the picker window near the cursor
  GdkScreen* screen = gdk_screen_get_default();
  int scr_w = gdk_screen_get_width(screen);
  int scr_h = gdk_screen_get_height(screen);

  int wx = cursor_x_ + 20;
  int wy = cursor_y_ + 20;
  if (wx + kPickerW > scr_w) wx = cursor_x_ - kPickerW - 10;
  if (wy + kPickerH > scr_h) wy = cursor_y_ - kPickerH - 10;

  if (window_)
    gtk_window_move(GTK_WINDOW(window_), wx, wy);
}

gboolean ColorPicker::OnTimer(gpointer data) {
  auto* self = static_cast<ColorPicker*>(data);
  if (!self->active_) return FALSE;

  self->UpdateAtCursor();
  if (self->drawing_area_)
    gtk_widget_queue_draw(self->drawing_area_);
  return TRUE;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

gboolean ColorPicker::OnDraw(GtkWidget* /*widget*/, cairo_t* cr,
                             gpointer data) {
  auto* self = static_cast<ColorPicker*>(data);
  auto& app = LinuxApplication::instance();

  // Background
  cairo_set_source_rgb(cr, 40.0 / 255, 40.0 / 255, 40.0 / 255);
  cairo_rectangle(cr, 0, 0, kPickerW, kPickerH);
  cairo_fill(cr);

  // Color swatch (top-left, 40x40)
  cairo_set_source_rgb(cr, self->cur_color_.r / 255.0,
                       self->cur_color_.g / 255.0,
                       self->cur_color_.b / 255.0);
  cairo_rectangle(cr, 8, 8, 40, 40);
  cairo_fill(cr);

  // Swatch border (white)
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  cairo_set_line_width(cr, 1.0);
  cairo_rectangle(cr, 8.5, 8.5, 39, 39);
  cairo_stroke(cr);

  // Text — Consolas-like monospace font
  cairo_select_font_face(cr, "monospace",
                         CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 12);

  char buf[128];

  // HEX
  cairo_set_source_rgb(cr, 240.0 / 255, 240.0 / 255, 240.0 / 255);
  std::snprintf(buf, sizeof(buf), "HEX: %s", self->hex_buf_);
  cairo_move_to(cr, 56, 22);
  cairo_show_text(cr, buf);

  // RGB
  std::snprintf(buf, sizeof(buf), "RGB: %d, %d, %d",
                self->cur_color_.r, self->cur_color_.g, self->cur_color_.b);
  cairo_move_to(cr, 56, 38);
  cairo_show_text(cr, buf);

  // HSV
  std::snprintf(buf, sizeof(buf), "HSV: %.0f, %.0f%%, %.0f%%",
                self->cur_hsv_.h, self->cur_hsv_.s * 100,
                self->cur_hsv_.v * 100);
  cairo_move_to(cr, 56, 54);
  cairo_show_text(cr, buf);

  // Magnifier image
  PixelGrabImage* mag = pixelgrab_get_magnifier(
      app.Ctx(), self->cursor_x_, self->cursor_y_, kMagRadius, kMagZoom);
  if (mag) {
    int mw = pixelgrab_image_get_width(mag);
    int mh = pixelgrab_image_get_height(mag);
    const uint8_t* pixels = pixelgrab_image_get_data(mag);
    int stride = pixelgrab_image_get_stride(mag);

    int draw_size = std::min(kMagSize, std::min(mw, mh));
    int draw_max = std::min(draw_size, kPickerW - 16);
    int mag_y = 58;
    int mag_h = std::min(draw_max, kPickerH - mag_y - 4);

    // Cairo ARGB32 = BGRA in memory on little-endian = pixelgrab BGRA8
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        const_cast<uint8_t*>(pixels),
        CAIRO_FORMAT_ARGB32, mw, mh, stride);

    cairo_save(cr);
    // Scale the image to fit the drawing area
    double sx = static_cast<double>(draw_max) / mw;
    double sy = static_cast<double>(mag_h) / mh;
    cairo_translate(cr, 8, mag_y);
    cairo_scale(cr, sx, sy);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_restore(cr);

    cairo_surface_destroy(surface);

    // Crosshair in center of magnifier
    int cx = 8 + draw_max / 2;
    int cy = mag_y + mag_h / 2;
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, cx - 4, cy + 0.5);
    cairo_line_to(cr, cx + 5, cy + 0.5);
    cairo_move_to(cr, cx + 0.5, cy - 4);
    cairo_line_to(cr, cx + 0.5, cy + 5);
    cairo_stroke(cr);

    pixelgrab_image_destroy(mag);
  }

  // Coordinates
  std::snprintf(buf, sizeof(buf), "(%d, %d)", self->cursor_x_,
                self->cursor_y_);
  cairo_set_source_rgb(cr, 160.0 / 255, 160.0 / 255, 160.0 / 255);
  cairo_move_to(cr, 8, kPickerH - 6);
  cairo_show_text(cr, buf);

  return FALSE;
}

// ---------------------------------------------------------------------------
// Keyboard / Mouse
// ---------------------------------------------------------------------------

gboolean ColorPicker::OnKeyPress(GtkWidget* /*widget*/, GdkEventKey* event,
                                 gpointer data) {
  auto* self = static_cast<ColorPicker*>(data);
  if (event->keyval == GDK_KEY_Escape) {
    self->Dismiss();
    return TRUE;
  }
  return FALSE;
}

gboolean ColorPicker::OnButtonPress(GtkWidget* /*widget*/,
                                    GdkEventButton* event, gpointer data) {
  auto* self = static_cast<ColorPicker*>(data);
  if (event->button == 1) {
    // Copy hex color to clipboard
    GtkClipboard* clipboard =
        gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, self->hex_buf_, -1);

    std::printf("  Color copied: %s  RGB(%d,%d,%d)\n",
                self->hex_buf_, self->cur_color_.r,
                self->cur_color_.g, self->cur_color_.b);
    self->Dismiss();
    return TRUE;
  }
  return FALSE;
}

#endif  // __linux__
