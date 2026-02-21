// Copyright 2026 The loong-pixelgrab Authors
//
// Simple performance benchmarks for capture operations.
// Compile: cmake --build build --config Release --target pixelgrab_bench
// Run:     build/bin/Release/pixelgrab_bench

#include <chrono>
#include <cstdio>
#include <vector>

#include "pixelgrab/pixelgrab.h"

struct BenchResult {
  const char* name;
  int iterations;
  double avg_ms;
  double min_ms;
  double max_ms;
};

template <typename Fn>
BenchResult RunBench(const char* name, int iterations, Fn&& fn) {
  std::vector<double> times;
  times.reserve(iterations);

  for (int i = 0; i < iterations; ++i) {
    auto t0 = std::chrono::high_resolution_clock::now();
    fn();
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
    times.push_back(ms);
  }

  double total = 0, mn = times[0], mx = times[0];
  for (double t : times) {
    total += t;
    if (t < mn) mn = t;
    if (t > mx) mx = t;
  }
  return {name, iterations, total / iterations, mn, mx};
}

void PrintResult(const BenchResult& r) {
  std::printf("  %-40s  %4d iters  avg=%.3f ms  min=%.3f ms  max=%.3f ms\n",
              r.name, r.iterations, r.avg_ms, r.min_ms, r.max_ms);
}

int main() {
  std::printf("PixelGrab Performance Benchmarks\n");
  std::printf("================================\n\n");

  PixelGrabContext* ctx = pixelgrab_context_create();
  if (!ctx) {
    std::printf("ERROR: Failed to create context\n");
    return 1;
  }

  int screen_count = pixelgrab_get_screen_count(ctx);
  std::printf("Screens: %d\n\n", screen_count);

  // -- Screen capture --
  if (screen_count > 0) {
    PixelGrabScreenInfo info = {};
    pixelgrab_get_screen_info(ctx, 0, &info);
    std::printf("Primary screen: %dx%d\n\n", info.width, info.height);

    auto r1 = RunBench("capture_screen(0)", 50, [&]() {
      PixelGrabImage* img = pixelgrab_capture_screen(ctx, 0);
      pixelgrab_image_destroy(img);
    });
    PrintResult(r1);

    auto r2 = RunBench("capture_region(100x100)", 100, [&]() {
      PixelGrabImage* img = pixelgrab_capture_region(ctx, 0, 0, 100, 100);
      pixelgrab_image_destroy(img);
    });
    PrintResult(r2);

    auto r3 = RunBench("capture_region(500x500)", 50, [&]() {
      PixelGrabImage* img = pixelgrab_capture_region(ctx, 0, 0, 500, 500);
      pixelgrab_image_destroy(img);
    });
    PrintResult(r3);

    auto r4 = RunBench("capture_region(1920x1080)", 20, [&]() {
      PixelGrabImage* img = pixelgrab_capture_region(ctx, 0, 0, 1920, 1080);
      pixelgrab_image_destroy(img);
    });
    PrintResult(r4);
  }

  // -- Color picker --
  std::printf("\n");
  auto r5 = RunBench("pick_color", 200, [&]() {
    PixelGrabColor c = {};
    pixelgrab_pick_color(ctx, 100, 100, &c);
  });
  PrintResult(r5);

  // -- Magnifier --
  auto r6 = RunBench("get_magnifier(r=5, m=4)", 100, [&]() {
    PixelGrabImage* img = pixelgrab_get_magnifier(ctx, 100, 100, 5, 4);
    pixelgrab_image_destroy(img);
  });
  PrintResult(r6);

  // -- Color conversion --
  auto r7 = RunBench("color_rgb_to_hsv (1000x)", 100, [&]() {
    PixelGrabColor c = {128, 64, 200, 255};
    PixelGrabColorHsv hsv = {};
    for (int i = 0; i < 1000; ++i) {
      pixelgrab_color_rgb_to_hsv(&c, &hsv);
    }
  });
  PrintResult(r7);

  // -- Window enumeration --
  std::printf("\n");
  auto r8 = RunBench("enumerate_windows", 50, [&]() {
    PixelGrabWindowInfo windows[128];
    pixelgrab_enumerate_windows(ctx, windows, 128);
  });
  PrintResult(r8);

  // -- Context create/destroy --
  auto r9 = RunBench("context_create+destroy", 20, [&]() {
    PixelGrabContext* c = pixelgrab_context_create();
    pixelgrab_context_destroy(c);
  });
  PrintResult(r9);

  std::printf("\nDone.\n");
  pixelgrab_context_destroy(ctx);
  return 0;
}
