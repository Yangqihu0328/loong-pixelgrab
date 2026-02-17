// Copyright 2026 The PixelGrab Authors
//
// Windows-specific constants, types, and helpers for the PixelGrab demo.
// Includes platform-neutral core/app_defs.h, then adds Win32 specifics.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_APP_DEFS_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_APP_DEFS_H_

// Pull in all platform-neutral definitions first.
#include "core/app_defs.h"
#include "core/i18n.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objidl.h>
#include <gdiplus.h>

// Application icon resource ID (must match resources/app.rc)
#define IDI_APPICON 101

// ===================================================================
// Win32 color constants
// ===================================================================

static constexpr COLORREF kHighlightColor = RGB(0, 120, 215);
static constexpr COLORREF kConfirmColor   = RGB(0, 200, 80);
static constexpr COLORREF kHandleFill     = RGB(255, 255, 255);
static constexpr COLORREF kHandleBorder   = RGB(0, 0, 0);

// ===================================================================
// Win32 system tray constants
// ===================================================================

static constexpr UINT kTrayMsg = WM_APP + 10;
static constexpr UINT kTrayId  = 1;

// Standalone recording control bar
static constexpr UINT_PTR kStandaloneRecTimerId = 2;
static constexpr int kRecCtrlStopBtn  = 7001;
static constexpr int kRecCtrlPauseBtn = 7002;

// Countdown timer
static constexpr UINT_PTR kCountdownTimerId = 3;

// ===================================================================
// Win32 custom messages
// ===================================================================

enum CustomMsg {
  kMsgLeftDown       = WM_APP + 2,
  kMsgLeftUp         = WM_APP + 3,
  kMsgRightClick     = WM_APP + 4,
  kMsgDoubleClick    = WM_APP + 5,
  kMsgKeyReturn      = WM_APP + 6,
  kMsgKeyEscape      = WM_APP + 7,
  kMsgCopyColor      = WM_APP + 8,
  kMsgToggleColor    = WM_APP + 9,
};

// ===================================================================
// Win32 window class names
// ===================================================================

static const wchar_t kOverlayClass[]     = L"PGOverlay";
static const wchar_t kCanvasClass[]      = L"PGCanvas";
static const wchar_t kToolbarClass[]     = L"PGToolbar";
static const wchar_t kTextDlgClass[]     = L"PGTextDlg";
static const wchar_t kTrayClass[]        = L"PGTrayHost";
static const wchar_t kSettingsClass[]    = L"PGSettings";
static const wchar_t kRecCtrlClass[]     = L"PGRecCtrl";
static const wchar_t kF1ToolbarClass[]   = L"PGF1Toolbar";
static const wchar_t kPinBorderClass[]   = L"PGPinBorder";
static const wchar_t kRecDimClass[]      = L"PGRecDim";
static const wchar_t kRecSettingsClass[] = L"PGRecSettings";
static const wchar_t kCountdownClass[]   = L"PGCountdown";
static const wchar_t kRecPreviewClass[]  = L"PGRecPreview";
static const wchar_t kAboutClass[]       = L"PGAbout";
static const wchar_t kColorPanelClass[]  = L"PGColorPanel";

// ===================================================================
// Win32 registry keys
// ===================================================================

static const wchar_t kRunKey[]      = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t kRunValue[]    = L"PixelGrab";
static const wchar_t kSettingsKey[] = L"Software\\PixelGrab";

// ===================================================================
// Win32 F-key display names
// ===================================================================

static const wchar_t* kFKeyNames[] = {
  L"F1", L"F2", L"F3", L"F4", L"F5", L"F6",
  L"F7", L"F8", L"F9", L"F10", L"F11", L"F12"
};

// ===================================================================
// Win32 types
// ===================================================================

struct PinEntry {
  PixelGrabPinWindow* pin    = nullptr;
  HWND                border = nullptr;
  HWND                hwnd   = nullptr;
};

// ===================================================================
// Win32 inline helpers
// ===================================================================

inline int LParamX(LPARAM lp) {
  return static_cast<int>(static_cast<short>(LOWORD(lp)));
}
inline int LParamY(LPARAM lp) {
  return static_cast<int>(static_cast<short>(HIWORD(lp)));
}

inline COLORREF ArgbToColorref(uint32_t argb) {
  return RGB((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF);
}

inline int VKToFKeyIndex(UINT vk) {
  if (vk >= VK_F1 && vk <= VK_F12) return static_cast<int>(vk - VK_F1);
  return 0;
}
inline UINT FKeyIndexToVK(int idx) {
  if (idx >= 0 && idx < kFKeyCount) return static_cast<UINT>(VK_F1 + idx);
  return VK_F1;
}
inline const wchar_t* VKToFKeyName(UINT vk) {
  int i = VKToFKeyIndex(vk);
  return kFKeyNames[i];
}

#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_APP_DEFS_H_
