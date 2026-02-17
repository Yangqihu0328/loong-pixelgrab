// Copyright 2026 The PixelGrab Authors
// Settings dialog + registry persistence (hotkeys, language, auto-start).

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_TRAY_SETTINGS_DIALOG_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_TRAY_SETTINGS_DIALOG_H_

#ifdef _WIN32

#include "platform/windows/win_app_defs.h"

class SettingsDialog {
 public:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  bool IsAutoStartEnabled();
  void SetAutoStart(bool enable);
  void LoadHotkeySettings();
  void SaveHotkeySettings();
  void SaveRecWatermarkSettings();
  void ReregisterHotkeys();
  void LoadLanguageSetting();
  void SaveLanguageSetting();
  void Show();

  UINT VkCapture() const { return vk_capture_; }
  void SetVkCapture(UINT v) { vk_capture_ = v; }
  UINT VkPin() const { return vk_pin_; }
  void SetVkPin(UINT v) { vk_pin_ = v; }

 private:
  UINT vk_capture_ = VK_F1;
  UINT vk_pin_ = VK_F3;
  bool settings_done_ = false;
  bool settings_ok_ = false;
  HWND settings_capture_combo_ = nullptr;
  HWND settings_pin_combo_ = nullptr;
};

#endif  // _WIN32
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_TRAY_SETTINGS_DIALOG_H_
