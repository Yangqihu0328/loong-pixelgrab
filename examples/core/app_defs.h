// Copyright 2026 The PixelGrab Authors
//
// Platform-neutral constants, enums, types, and inline helpers for the
// PixelGrab interactive demo application.
//
// This header contains ONLY cross-platform definitions.  Platform-specific
// types (HWND, COLORREF, wchar_t literals …) live in the platform layer,
// e.g.  platform/windows/win_app_defs.h.

#ifndef PIXELGRAB_EXAMPLES_CORE_APP_DEFS_H_
#define PIXELGRAB_EXAMPLES_CORE_APP_DEFS_H_

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "pixelgrab/pixelgrab.h"

// ===================================================================
// Layout constants (platform-neutral)
// ===================================================================

static constexpr int kHighlightBorder = 3;
static constexpr int kHandleSize      = 8;

// Toolbar layout
static constexpr int kToolbarH   = 36;
static constexpr int kBtnW       = 56;
static constexpr int kBtnH       = 28;
static constexpr int kBtnGap     = 2;
static constexpr int kBtnMarginY = (kToolbarH - kBtnH) / 2;
static constexpr int kSepGap     = 10;

// Annotation defaults
static constexpr uint32_t kStrokeColor  = 0xFFFF0000;
static constexpr float    kStrokeWidth  = 3.0f;
static constexpr int      kMosaicBlock  = 8;
static constexpr int      kBlurRadius   = 5;
static constexpr uint32_t kTextColor    = 0xFFFFFF00;
static constexpr int      kTextFontSize = 18;
static constexpr float    kArrowHead    = 12.0f;

// Canvas border hit-test margin
static constexpr int kEdgeThreshold = 8;

// Annotation width presets
static constexpr float kWidthThin   = 1.5f;
static constexpr float kWidthMedium = 3.0f;
static constexpr float kWidthThick  = 5.0f;

// Annotation font size presets
static constexpr int kFontSmall  = 14;
static constexpr int kFontMedium = 18;
static constexpr int kFontLarge  = 28;

// Property button width (narrower than tool buttons)
static constexpr int kPropBtnW = 36;

// Annotation color palette (ARGB)
static constexpr uint32_t kColorPalette[] = {
  0xFFFF0000,  // Red
  0xFF00CC00,  // Green
  0xFF0000FF,  // Blue
  0xFFFFFF00,  // Yellow
  0xFFFF8000,  // Orange
  0xFF8000FF,  // Purple
  0xFF00FFFF,  // Cyan
  0xFFFF00FF,  // Magenta
  0xFFFFFFFF,  // White
  0xFF000000,  // Black
  0xFF808080,  // Gray
  0xFF800000,  // Maroon
};
static constexpr int kPaletteCount = 12;
static constexpr int kPaletteCols  = 4;
static constexpr int kPaletteRows  = 3;
static constexpr int kSwatchSize   = 22;
static constexpr int kSwatchGap    = 2;
static constexpr int kPanelPad     = 4;

// F1 toolbar layout
static constexpr int kF1BarH     = 40;
static constexpr int kF1BtnW     = 72;
static constexpr int kF1BtnH     = 30;
static constexpr int kF1BtnGap   = 8;
static constexpr int kF1BtnCount = 3;
static constexpr int kF1BarW     = kF1BtnCount * kF1BtnW
                                 + (kF1BtnCount - 1) * kF1BtnGap + 16;

// Countdown overlay
static constexpr int kCountdownSize = 180;

// ===================================================================
// Enums (platform-neutral IDs — no Win32 types)
// ===================================================================

enum HotkeyId { kHotkeyF1 = 1, kHotkeyF3 = 3 };

enum F1MenuId {
  kF1Capture    = 6001,
  kF1Record     = 6002,
  kF1OCR        = 6003,
  kF1ColorPick  = 6004,
};

enum SelectPurpose {
  kSelectForCapture = 0,
  kSelectForRecord,
};

enum AnnotTool {
  kToolRect, kToolEllipse, kToolArrow, kToolLine,
  kToolText, kToolMosaic, kToolBlur,
};

enum ToolbarBtnId {
  kBtnRect = 2001, kBtnEllipse, kBtnArrow, kBtnLine,
  kBtnText, kBtnMosaic, kBtnBlur,
  kBtnUndo, kBtnRedo,
  kBtnPin, kBtnCopy, kBtnCancel,
  kBtnRecord, kBtnStopRec,
  kBtnColor,
  kBtnWidthThin, kBtnWidthMed, kBtnWidthThick,
  kBtnFontSmall, kBtnFontMed, kBtnFontLarge,
};

enum TrayMenuId {
  kTrayCapture      = 3001,
  kTrayPin          = 3002,
  kTraySettings     = 3003,
  kTrayAutoStart    = 3004,
  kTrayAbout        = 3006,
  kTrayExit         = 3007,
  kTrayPasteClip    = 3008,
  kTrayClearHistory = 3009,
  kTrayLangZhCN     = 3010,
  kTrayLangEnUS     = 3011,
  kTrayHistoryBase  = 3100,
};

enum SettingsCtrlId {
  kSettingsCaptureCombo = 4001,
  kSettingsPinCombo     = 4002,
  kSettingsOK           = 4003,
  kSettingsCancel       = 4004,
};

enum RecSettingsCtrlId {
  kRecAudioSpk       = 8003,
  kRecAudioMic       = 8004,
  kRecStart          = 8005,
  kRecCancel         = 8006,
  kRecWatermarkCheck    = 8007,
  kRecWatermarkEdit     = 8008,
  kRecWatermarkFontSize = 8009,
  kRecWatermarkOpacity  = 8010,
  kRecAudioDeviceCombo  = 8011,
};

enum RecPreviewCtrlId {
  kRecPrevPlay = 8101,
  kRecPrevCopy = 8102,
  kRecPrevDone = 8103,
};

enum AboutCtrlId {
  kAboutCheckUpdate = 9001,
  kAboutClose       = 9002,
};

// ===================================================================
// Platform-neutral key codes
// ===================================================================

static constexpr int kKeyF1  = 0x70;
static constexpr int kKeyF2  = 0x71;
static constexpr int kKeyF3  = 0x72;
static constexpr int kKeyF4  = 0x73;
static constexpr int kKeyF5  = 0x74;
static constexpr int kKeyF6  = 0x75;
static constexpr int kKeyF7  = 0x76;
static constexpr int kKeyF8  = 0x77;
static constexpr int kKeyF9  = 0x78;
static constexpr int kKeyF10 = 0x79;
static constexpr int kKeyF11 = 0x7A;
static constexpr int kKeyF12 = 0x7B;
static constexpr int kFKeyCount = 12;

// ===================================================================
// Platform-neutral inline helpers
// ===================================================================

inline int IntAbs(int v) { return v < 0 ? -v : v; }
inline int IntMin(int a, int b) { return a < b ? a : b; }
inline int IntMax(int a, int b) { return a > b ? a : b; }

// GitHub repository for update checking (set via CMake or fallback)
#ifndef PIXELGRAB_GITHUB_REPO
#define PIXELGRAB_GITHUB_REPO "Yangqihu0328/loong-pixelgrab"
#endif

#endif  // PIXELGRAB_EXAMPLES_CORE_APP_DEFS_H_
