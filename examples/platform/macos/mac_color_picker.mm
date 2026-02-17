// Copyright 2026 The PixelGrab Authors
// Color picker overlay — magnifier + HEX/RGB/HSV display.
// macOS implementation using Cocoa + Core Graphics.

#include "platform/macos/mac_color_picker.h"

#ifdef __APPLE__

#include "platform/macos/mac_application.h"
#import <Cocoa/Cocoa.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

static constexpr int kPickerW = 200;
static constexpr int kPickerH = 160;
static constexpr int kMagRadius = 7;
static constexpr int kMagZoom = 8;
static constexpr int kMagSize = kMagRadius * 2 * kMagZoom;

// ========================================================================
// PGPickerView — custom NSView for drawing magnifier + color info
// ========================================================================

@interface PGPickerView : NSView
@property (nonatomic, assign) ColorPicker* picker;
@end

@implementation PGPickerView

- (BOOL)acceptsFirstResponder { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
  (void)dirtyRect;
  auto* self_picker = self.picker;
  if (!self_picker || !self_picker->IsActive()) return;

  auto& app = MacApplication::instance();
  NSGraphicsContext* nsCtx = [NSGraphicsContext currentContext];
  CGContextRef cg = [nsCtx CGContext];

  // Access color state via friend-like pointers stored in the picker
  // We read from the picker's public API and stored state
  PixelGrabColor color = {};
  PixelGrabColorHsv hsv = {};
  char hex_buf[16] = {};
  int cx = 0, cy = 0;

  // Get current cursor position
  NSPoint mouse = [NSEvent mouseLocation];
  cx = static_cast<int>(mouse.x);
  // macOS screen coordinates: origin at bottom-left, convert to top-left
  NSScreen* screen = [NSScreen mainScreen];
  cy = static_cast<int>(screen.frame.size.height - mouse.y);

  pixelgrab_pick_color(app.Ctx(), cx, cy, &color);
  pixelgrab_color_rgb_to_hsv(&color, &hsv);
  pixelgrab_color_to_hex(&color, hex_buf, sizeof(hex_buf), 0);

  // Background (dark gray)
  CGContextSetRGBFillColor(cg, 40.0/255, 40.0/255, 40.0/255, 1.0);
  CGContextFillRect(cg, NSMakeRect(0, 0, kPickerW, kPickerH));

  // Flip coordinate system (macOS is bottom-up, we want top-down)
  CGContextSaveGState(cg);
  CGContextTranslateCTM(cg, 0, kPickerH);
  CGContextScaleCTM(cg, 1.0, -1.0);

  // Color swatch (top-left, 40x40)
  CGContextSetRGBFillColor(cg, color.r/255.0, color.g/255.0,
                           color.b/255.0, 1.0);
  CGContextFillRect(cg, NSMakeRect(8, 8, 40, 40));

  // Swatch border (white)
  CGContextSetRGBStrokeColor(cg, 1.0, 1.0, 1.0, 1.0);
  CGContextSetLineWidth(cg, 1.0);
  CGContextStrokeRect(cg, NSMakeRect(8.5, 8.5, 39, 39));

  // Text setup
  CGContextSetRGBFillColor(cg, 240.0/255, 240.0/255, 240.0/255, 1.0);
  CGContextSelectFont(cg, "Menlo", 11, kCGEncodingMacRoman);
  CGContextSetTextDrawingMode(cg, kCGTextFill);

  // For CGContextShowTextAtPoint, Y is baseline in the current (flipped) coords
  // We need to un-flip text since CGContextShowTextAtPoint uses its own coord
  CGContextSaveGState(cg);
  CGContextTranslateCTM(cg, 0, kPickerH);
  CGContextScaleCTM(cg, 1.0, -1.0);
  // Now we're back to bottom-up macOS coords

  char buf[128];

  // HEX (y = kPickerH - 22 in bottom-up = 22 from top)
  std::snprintf(buf, sizeof(buf), "HEX: %s", hex_buf);
  CGContextShowTextAtPoint(cg, 56, kPickerH - 22, buf, std::strlen(buf));

  // RGB
  std::snprintf(buf, sizeof(buf), "RGB: %d, %d, %d",
                color.r, color.g, color.b);
  CGContextShowTextAtPoint(cg, 56, kPickerH - 38, buf, std::strlen(buf));

  // HSV
  std::snprintf(buf, sizeof(buf), "HSV: %.0f, %.0f%%, %.0f%%",
                hsv.h, hsv.s * 100, hsv.v * 100);
  CGContextShowTextAtPoint(cg, 56, kPickerH - 54, buf, std::strlen(buf));

  // Coordinates (gray)
  CGContextSetRGBFillColor(cg, 160.0/255, 160.0/255, 160.0/255, 1.0);
  std::snprintf(buf, sizeof(buf), "(%d, %d)", cx, cy);
  CGContextShowTextAtPoint(cg, 8, 6, buf, std::strlen(buf));

  CGContextRestoreGState(cg);
  // Now back in flipped (top-down) coords

  // Magnifier image
  PixelGrabImage* mag = pixelgrab_get_magnifier(
      app.Ctx(), cx, cy, kMagRadius, kMagZoom);
  if (mag) {
    int mw = pixelgrab_image_get_width(mag);
    int mh = pixelgrab_image_get_height(mag);
    const uint8_t* pixels = pixelgrab_image_get_data(mag);
    int stride = pixelgrab_image_get_stride(mag);

    int draw_size = std::min(kMagSize, std::min(mw, mh));
    int draw_max = std::min(draw_size, kPickerW - 16);
    int mag_y = 58;
    int mag_h = std::min(draw_max, kPickerH - mag_y - 4);

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef bmpCtx = CGBitmapContextCreate(
        const_cast<uint8_t*>(pixels), mw, mh, 8, stride,
        cs, kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst);
    if (bmpCtx) {
      CGImageRef img = CGBitmapContextCreateImage(bmpCtx);
      if (img) {
        CGContextSetInterpolationQuality(cg, kCGInterpolationNone);
        CGContextDrawImage(cg, NSMakeRect(8, mag_y, draw_max, mag_h), img);
        CGImageRelease(img);
      }
      CGContextRelease(bmpCtx);
    }
    CGColorSpaceRelease(cs);

    // Crosshair in center of magnifier
    int crossX = 8 + draw_max / 2;
    int crossY = mag_y + mag_h / 2;
    CGContextSetRGBStrokeColor(cg, 1.0, 1.0, 1.0, 1.0);
    CGContextSetLineWidth(cg, 1.0);
    CGContextMoveToPoint(cg, crossX - 4, crossY + 0.5);
    CGContextAddLineToPoint(cg, crossX + 5, crossY + 0.5);
    CGContextMoveToPoint(cg, crossX + 0.5, crossY - 4);
    CGContextAddLineToPoint(cg, crossX + 0.5, crossY + 5);
    CGContextStrokePath(cg);

    pixelgrab_image_destroy(mag);
  }

  CGContextRestoreGState(cg);
}

- (void)keyDown:(NSEvent*)event {
  if (event.keyCode == 53) {  // ESC
    auto* self_picker = self.picker;
    if (self_picker) self_picker->Dismiss();
    return;
  }
  [super keyDown:event];
}

- (void)mouseDown:(NSEvent*)event {
  (void)event;
  auto* self_picker = self.picker;
  if (!self_picker) return;

  // Pick color at current cursor
  auto& app = MacApplication::instance();
  NSPoint mouse = [NSEvent mouseLocation];
  int cx = static_cast<int>(mouse.x);
  NSScreen* screen = [NSScreen mainScreen];
  int cy = static_cast<int>(screen.frame.size.height - mouse.y);

  PixelGrabColor color = {};
  char hex_buf[16] = {};
  pixelgrab_pick_color(app.Ctx(), cx, cy, &color);
  pixelgrab_color_to_hex(&color, hex_buf, sizeof(hex_buf), 0);

  // Copy to clipboard
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [pb clearContents];
  [pb setString:[NSString stringWithUTF8String:hex_buf]
        forType:NSPasteboardTypeString];

  std::printf("  Color copied: %s  RGB(%d,%d,%d)\n",
              hex_buf, color.r, color.g, color.b);
  self_picker->Dismiss();
}

@end

// ========================================================================
// ColorPicker implementation
// ========================================================================

void ColorPicker::Show() {
  if (active_) return;
  active_ = true;

  NSRect frame = NSMakeRect(0, 0, kPickerW, kPickerH);
  window_ = [[NSWindow alloc]
      initWithContentRect:frame
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];
  [window_ setLevel:NSFloatingWindowLevel];
  [window_ setOpaque:YES];
  [window_ setHasShadow:YES];
  [window_ setReleasedWhenClosed:NO];
  [window_ setAcceptsMouseMovedEvents:YES];

  PGPickerView* view = [[PGPickerView alloc] initWithFrame:frame];
  view.picker = this;
  [window_ setContentView:view];
  content_view_ = view;

  [window_ makeKeyAndOrderFront:nil];
  [window_ makeFirstResponder:view];

  // Timer for 30ms refresh
  timer_ = [NSTimer scheduledTimerWithTimeInterval:0.03
      repeats:YES
      block:^(NSTimer* t) {
        (void)t;
        if (!this->active_) return;

        // Update window position near cursor
        NSPoint mouse = [NSEvent mouseLocation];
        NSScreen* screen = [NSScreen mainScreen];
        CGFloat scr_w = screen.frame.size.width;
        CGFloat scr_h = screen.frame.size.height;

        CGFloat wx = mouse.x + 20;
        CGFloat wy = mouse.y - kPickerH - 20;
        if (wx + kPickerW > scr_w) wx = mouse.x - kPickerW - 10;
        if (wy < 0) wy = mouse.y + 20;

        [this->window_ setFrameOrigin:NSMakePoint(wx, wy)];
        [this->content_view_ setNeedsDisplay:YES];
      }];

  std::printf("  [ColorPicker] Active. Left-click to copy, Esc to cancel.\n");
}

void ColorPicker::Dismiss() {
  if (!active_) return;
  active_ = false;

  if (timer_) {
    [timer_ invalidate];
    timer_ = nil;
  }
  if (window_) {
    [window_ orderOut:nil];
    [window_ close];
    window_ = nil;
    content_view_ = nil;
  }
}

#endif  // __APPLE__
