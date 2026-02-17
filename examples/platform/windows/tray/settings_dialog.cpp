// Copyright 2026 The PixelGrab Authors
// Settings dialog + registry persistence.

#include "platform/windows/tray/settings_dialog.h"

#ifdef _WIN32

#include "core/platform_settings.h"
#include "platform/windows/win_application.h"

bool SettingsDialog::IsAutoStartEnabled() {
  auto settings = CreatePlatformSettings();
  return settings->IsAutoStartEnabled();
}

void SettingsDialog::SetAutoStart(bool enable) {
  auto settings = CreatePlatformSettings();
  settings->SetAutoStart(enable);
}

void SettingsDialog::LoadHotkeySettings() {
  auto& app = Application::instance();
  auto settings = CreatePlatformSettings();

  int val = 0;
  if (settings->GetInt("CaptureKey", &val)) {
    if (val >= VK_F1 && val <= VK_F12) vk_capture_ = static_cast<UINT>(val);
  }
  if (settings->GetInt("PinKey", &val)) {
    if (val >= VK_F1 && val <= VK_F12) vk_pin_ = static_cast<UINT>(val);
  }

  auto& rec = app.Recording();
  if (settings->GetInt("RecWatermarkEnabled", &val)) {
    rec.SetRecUserWmEnabled(val != 0);
  }
  char text_buf[256] = {};
  if (settings->GetString("RecWatermarkText", text_buf, 256)) {
    strncpy_s(rec.RecUserWmTextBuf(), 256, text_buf, 255);
  } else {
    rec.RecUserWmTextBuf()[0] = '\0';
  }
  if (settings->GetInt("RecWatermarkFontSize", &val)) {
    if (val == 10 || val == 14 || val == 18 || val == 24)
      rec.SetRecUserWmFontSize(val);
  }
  if (settings->GetInt("RecWatermarkOpacity", &val)) {
    if (val >= 0 && val <= 100) rec.SetRecUserWmOpacity(val);
  }
}

void SettingsDialog::SaveHotkeySettings() {
  auto settings = CreatePlatformSettings();
  settings->SetInt("CaptureKey", static_cast<int>(vk_capture_));
  settings->SetInt("PinKey", static_cast<int>(vk_pin_));
}

void SettingsDialog::SaveRecWatermarkSettings() {
  auto& rec = Application::instance().Recording();
  auto settings = CreatePlatformSettings();
  settings->SetInt("RecWatermarkEnabled", rec.RecUserWmEnabled() ? 1 : 0);
  settings->SetString("RecWatermarkText", rec.RecUserWmText());
  settings->SetInt("RecWatermarkFontSize", rec.RecUserWmFontSize());
  settings->SetInt("RecWatermarkOpacity", rec.RecUserWmOpacity());
}

void SettingsDialog::ReregisterHotkeys() {
  auto& app = Application::instance();
  app.Hotkey().Unregister(kHotkeyF1);
  app.Hotkey().Unregister(kHotkeyF3);
  app.Hotkey().Register(kHotkeyF1, vk_capture_);
  app.Hotkey().Register(kHotkeyF3, vk_pin_);
}

void SettingsDialog::LoadLanguageSetting() {
  auto settings = CreatePlatformSettings();
  int val = 0;
  if (settings->GetInt("Language", &val)) {
    if (val >= 0 && val < static_cast<int>(kLangCount))
      SetLanguage(static_cast<Language>(val));
    else
      SetLanguage(DetectSystemLanguage());
  } else {
    SetLanguage(DetectSystemLanguage());
  }
}

void SettingsDialog::SaveLanguageSetting() {
  auto settings = CreatePlatformSettings();
  settings->SetInt("Language", static_cast<int>(GetLanguage()));
}

LRESULT CALLBACK SettingsDialog::WndProc(HWND hwnd, UINT msg,
                                          WPARAM wp, LPARAM lp) {
  auto& self = Application::instance().Settings();
  switch (msg) {
    case WM_COMMAND:
      if (LOWORD(wp) == kSettingsOK) {
        int ci = static_cast<int>(SendMessageW(self.settings_capture_combo_, CB_GETCURSEL, 0, 0));
        int pi = static_cast<int>(SendMessageW(self.settings_pin_combo_, CB_GETCURSEL, 0, 0));
        if (ci == pi) {
          MessageBoxW(hwnd, T(kStr_MsgHotkeyConflict), T(kStr_MsgHint),
              MB_OK | MB_ICONWARNING);
          return 0;
        }
        self.settings_ok_   = true;
        self.settings_done_ = true;
        return 0;
      }
      if (LOWORD(wp) == kSettingsCancel) {
        self.settings_done_ = true;
        return 0;
      }
      break;
    case WM_CLOSE:
      self.settings_done_ = true;
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

void SettingsDialog::Show() {
  settings_done_ = false;
  settings_ok_   = false;

  int dlg_w = 290, dlg_h = 155;
  int sx = GetSystemMetrics(SM_CXSCREEN);
  int sy = GetSystemMetrics(SM_CYSCREEN);

  HWND dlg = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
      kSettingsClass, T(kStr_TitleHotkeySettings),
      WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
      (sx - dlg_w) / 2, (sy - dlg_h) / 2, dlg_w, dlg_h,
      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

  HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

  HWND lbl1 = CreateWindowExW(0, L"STATIC", T(kStr_LabelCaptureHotkey),
      WS_CHILD | WS_VISIBLE | SS_RIGHT,
      10, 18, 80, 20, dlg, nullptr, GetModuleHandleW(nullptr), nullptr);
  SendMessageW(lbl1, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

  settings_capture_combo_ = CreateWindowExW(0, L"COMBOBOX", nullptr,
      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
      100, 15, 100, 200, dlg,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSettingsCaptureCombo)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessageW(settings_capture_combo_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

  HWND lbl2 = CreateWindowExW(0, L"STATIC", T(kStr_LabelPinHotkey),
      WS_CHILD | WS_VISIBLE | SS_RIGHT,
      10, 53, 80, 20, dlg, nullptr, GetModuleHandleW(nullptr), nullptr);
  SendMessageW(lbl2, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

  settings_pin_combo_ = CreateWindowExW(0, L"COMBOBOX", nullptr,
      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
      100, 50, 100, 200, dlg,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSettingsPinCombo)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessageW(settings_pin_combo_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

  for (int i = 0; i < kFKeyCount; ++i) {
    SendMessageW(settings_capture_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kFKeyNames[i]));
    SendMessageW(settings_pin_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kFKeyNames[i]));
  }
  SendMessageW(settings_capture_combo_, CB_SETCURSEL,
               static_cast<WPARAM>(VKToFKeyIndex(vk_capture_)), 0);
  SendMessageW(settings_pin_combo_, CB_SETCURSEL,
               static_cast<WPARAM>(VKToFKeyIndex(vk_pin_)), 0);

  HWND ok_btn = CreateWindowExW(0, L"BUTTON", T(kStr_BtnOK),
      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
      60, 90, 70, 28, dlg,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSettingsOK)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessageW(ok_btn, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

  HWND cancel_btn = CreateWindowExW(0, L"BUTTON", T(kStr_ToolCancel),
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      150, 90, 70, 28, dlg,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSettingsCancel)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessageW(cancel_btn, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

  SetForegroundWindow(dlg);

  MSG tmsg;
  while (!settings_done_ && GetMessageW(&tmsg, nullptr, 0, 0)) {
    TranslateMessage(&tmsg);
    DispatchMessageW(&tmsg);
  }

  if (settings_ok_) {
    int ci = static_cast<int>(SendMessageW(settings_capture_combo_, CB_GETCURSEL, 0, 0));
    int pi = static_cast<int>(SendMessageW(settings_pin_combo_, CB_GETCURSEL, 0, 0));
    vk_capture_ = FKeyIndexToVK(ci);
    vk_pin_     = FKeyIndexToVK(pi);
    SaveHotkeySettings();
    ReregisterHotkeys();
    std::printf("  Hotkeys updated: Capture=%ls, Pin=%ls\n",
                VKToFKeyName(vk_capture_), VKToFKeyName(vk_pin_));
  }

  if (IsWindow(dlg)) DestroyWindow(dlg);
  settings_capture_combo_ = nullptr;
  settings_pin_combo_     = nullptr;
}

#endif
