// Copyright 2026 The loong-pixelgrab Authors
// Tests for: pixelgrab_pick_color, pixelgrab_color_rgb_to_hsv,
//            pixelgrab_color_hsv_to_rgb, pixelgrab_color_to_hex,
//            pixelgrab_color_from_hex, pixelgrab_get_magnifier

#include <cmath>
#include <cstring>

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

// ---------------------------------------------------------------------------
// RGB ↔ HSV conversion
// ---------------------------------------------------------------------------

TEST(ColorTest, RgbToHsv_Red) {
  PixelGrabColor rgb = {255, 0, 0, 255};
  PixelGrabColorHsv hsv = {};
  pixelgrab_color_rgb_to_hsv(&rgb, &hsv);
  EXPECT_NEAR(hsv.h, 0.0f, 1.0f);
  EXPECT_NEAR(hsv.s, 1.0f, 0.01f);
  EXPECT_NEAR(hsv.v, 1.0f, 0.01f);
}

TEST(ColorTest, RgbToHsv_Green) {
  PixelGrabColor rgb = {0, 255, 0, 255};
  PixelGrabColorHsv hsv = {};
  pixelgrab_color_rgb_to_hsv(&rgb, &hsv);
  EXPECT_NEAR(hsv.h, 120.0f, 1.0f);
  EXPECT_NEAR(hsv.s, 1.0f, 0.01f);
  EXPECT_NEAR(hsv.v, 1.0f, 0.01f);
}

TEST(ColorTest, RgbToHsv_Blue) {
  PixelGrabColor rgb = {0, 0, 255, 255};
  PixelGrabColorHsv hsv = {};
  pixelgrab_color_rgb_to_hsv(&rgb, &hsv);
  EXPECT_NEAR(hsv.h, 240.0f, 1.0f);
  EXPECT_NEAR(hsv.s, 1.0f, 0.01f);
  EXPECT_NEAR(hsv.v, 1.0f, 0.01f);
}

TEST(ColorTest, RgbToHsv_White) {
  PixelGrabColor rgb = {255, 255, 255, 255};
  PixelGrabColorHsv hsv = {};
  pixelgrab_color_rgb_to_hsv(&rgb, &hsv);
  EXPECT_NEAR(hsv.s, 0.0f, 0.01f);
  EXPECT_NEAR(hsv.v, 1.0f, 0.01f);
}

TEST(ColorTest, RgbToHsv_Black) {
  PixelGrabColor rgb = {0, 0, 0, 255};
  PixelGrabColorHsv hsv = {};
  pixelgrab_color_rgb_to_hsv(&rgb, &hsv);
  EXPECT_NEAR(hsv.v, 0.0f, 0.01f);
}

TEST(ColorTest, HsvToRgb_Red) {
  PixelGrabColorHsv hsv = {0.0f, 1.0f, 1.0f};
  PixelGrabColor rgb = {};
  pixelgrab_color_hsv_to_rgb(&hsv, &rgb);
  EXPECT_EQ(rgb.r, 255);
  EXPECT_EQ(rgb.g, 0);
  EXPECT_EQ(rgb.b, 0);
}

TEST(ColorTest, HsvToRgb_Green) {
  PixelGrabColorHsv hsv = {120.0f, 1.0f, 1.0f};
  PixelGrabColor rgb = {};
  pixelgrab_color_hsv_to_rgb(&hsv, &rgb);
  EXPECT_EQ(rgb.r, 0);
  EXPECT_EQ(rgb.g, 255);
  EXPECT_EQ(rgb.b, 0);
}

TEST(ColorTest, RgbHsvRoundTrip) {
  PixelGrabColor original = {128, 64, 200, 255};
  PixelGrabColorHsv hsv = {};
  PixelGrabColor result = {};

  pixelgrab_color_rgb_to_hsv(&original, &hsv);
  pixelgrab_color_hsv_to_rgb(&hsv, &result);

  EXPECT_NEAR(result.r, original.r, 1);
  EXPECT_NEAR(result.g, original.g, 1);
  EXPECT_NEAR(result.b, original.b, 1);
}

// ---------------------------------------------------------------------------
// Hex conversion
// ---------------------------------------------------------------------------

TEST(ColorTest, ToHex_NoAlpha) {
  PixelGrabColor color = {255, 0, 0, 255};
  char buf[16] = {};
  pixelgrab_color_to_hex(&color, buf, sizeof(buf), 0);
  EXPECT_STREQ(buf, "#FF0000");
}

TEST(ColorTest, ToHex_WithAlpha) {
  PixelGrabColor color = {0, 255, 0, 128};
  char buf[16] = {};
  pixelgrab_color_to_hex(&color, buf, sizeof(buf), 1);
  EXPECT_STREQ(buf, "#00FF0080");
}

TEST(ColorTest, FromHex_RRGGBB) {
  PixelGrabColor color = {};
  PixelGrabError err = pixelgrab_color_from_hex("#FF8000", &color);
  EXPECT_EQ(err, kPixelGrabOk);
  EXPECT_EQ(color.r, 255);
  EXPECT_EQ(color.g, 128);
  EXPECT_EQ(color.b, 0);
  EXPECT_EQ(color.a, 255);
}

TEST(ColorTest, FromHex_RRGGBBAA) {
  PixelGrabColor color = {};
  PixelGrabError err = pixelgrab_color_from_hex("#FF000080", &color);
  EXPECT_EQ(err, kPixelGrabOk);
  EXPECT_EQ(color.r, 255);
  EXPECT_EQ(color.g, 0);
  EXPECT_EQ(color.b, 0);
  EXPECT_EQ(color.a, 128);
}

TEST(ColorTest, FromHex_ShortRGB) {
  PixelGrabColor color = {};
  PixelGrabError err = pixelgrab_color_from_hex("#F00", &color);
  EXPECT_EQ(err, kPixelGrabOk);
  EXPECT_EQ(color.r, 255);
  EXPECT_EQ(color.g, 0);
  EXPECT_EQ(color.b, 0);
}

TEST(ColorTest, FromHex_InvalidFormat) {
  PixelGrabColor color = {};
  EXPECT_EQ(pixelgrab_color_from_hex("invalid", &color),
            kPixelGrabErrorInvalidParam);
  EXPECT_EQ(pixelgrab_color_from_hex(nullptr, &color),
            kPixelGrabErrorInvalidParam);
  EXPECT_EQ(pixelgrab_color_from_hex("#FF0000", nullptr),
            kPixelGrabErrorInvalidParam);
}

// ---------------------------------------------------------------------------
// pick_color — NULL protection
// ---------------------------------------------------------------------------

TEST(ColorTest, PickColorNullCtx) {
  PixelGrabColor color = {};
  // NULL context should not crash; return value is error or nullptr-based.
  pixelgrab_pick_color(nullptr, 0, 0, &color);
}

// ---------------------------------------------------------------------------
// Magnifier — NULL protection + param validation
// ---------------------------------------------------------------------------

TEST(ColorTest, MagnifierNullCtx) {
  PixelGrabImage* img = pixelgrab_get_magnifier(nullptr, 0, 0, 5, 2);
  EXPECT_EQ(img, nullptr);
}
