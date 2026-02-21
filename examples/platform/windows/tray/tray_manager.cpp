// Copyright 2026 The PixelGrab Authors
// System tray WndProc and menu.

#include "platform/windows/tray/tray_manager.h"

#ifdef _WIN32

#include "platform/windows/win_application.h"

void TrayManager::ShowMenu() {
  auto& app = Application::instance();
  HMENU menu = CreatePopupMenu();

  wchar_t capLabel[64], pinLabel[64];
  wsprintfW(capLabel, T(kStr_TrayCapture), VKToFKeyName(app.Settings().VkCapture()));
  wsprintfW(pinLabel, T(kStr_TrayPin), VKToFKeyName(app.Settings().VkPin()));

  AppendMenuW(menu, MF_STRING, kTrayCapture, capLabel);
  AppendMenuW(menu, MF_STRING, kTrayPin,     pinLabel);
  AppendMenuW(menu, MF_STRING, kTrayPasteClip, T(kStr_TrayPasteClip));
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

  int hist_count = pixelgrab_history_count(app.Ctx());
  if (hist_count > 0) {
    HMENU histMenu = CreatePopupMenu();
    int show_count = IntMin(hist_count, 10);
    for (int i = 0; i < show_count; ++i) {
      PixelGrabHistoryEntry entry = {};
      if (pixelgrab_history_get_entry(app.Ctx(), i, &entry) == kPixelGrabOk) {
        wchar_t label[64];
        wsprintfW(label, L"#%d  %dx%d  (%d,%d)", entry.id,
                  entry.region_width, entry.region_height,
                  entry.region_x, entry.region_y);
        AppendMenuW(histMenu, MF_STRING,
                    static_cast<UINT>(kTrayHistoryBase + entry.id), label);
      }
    }
    AppendMenuW(histMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(histMenu, MF_STRING, kTrayClearHistory, T(kStr_TrayClearHistory));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(histMenu),
                T(kStr_TrayHistory));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  }

  AppendMenuW(menu, MF_STRING, kTraySettings, T(kStr_TraySettings));
  UINT autoFlag = app.Settings().IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED;
  AppendMenuW(menu, MF_STRING | autoFlag, kTrayAutoStart, T(kStr_TrayAutoStart));

  HMENU langMenu = CreatePopupMenu();
  UINT zhFlag = (GetLanguage() == kLangZhCN) ? MF_CHECKED : MF_UNCHECKED;
  UINT enFlag = (GetLanguage() == kLangEnUS) ? MF_CHECKED : MF_UNCHECKED;
  AppendMenuW(langMenu, MF_STRING | zhFlag, kTrayLangZhCN, T(kStr_LangChinese));
  AppendMenuW(langMenu, MF_STRING | enFlag, kTrayLangEnUS, T(kStr_LangEnglish));
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(langMenu), T(kStr_TrayLanguage));

  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kTrayAbout, T(kStr_TrayAbout));
  AppendMenuW(menu, MF_STRING, kTrayExit, T(kStr_TrayExit));

  SetForegroundWindow(tray_hwnd_);
  POINT pt;
  GetCursorPos(&pt);
  TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, tray_hwnd_, nullptr);
  PostMessage(tray_hwnd_, WM_NULL, 0, 0);
  DestroyMenu(menu);
}

LRESULT CALLBACK TrayManager::WndProc(HWND hwnd, UINT msg,
                                        WPARAM wp, LPARAM lp) {
  auto& app = Application::instance();
  auto& self = app.Tray();

  if (msg == kTrayMsg) {
    switch (LOWORD(lp)) {
      case WM_RBUTTONUP:
        self.ShowMenu();
        return 0;
      case WM_LBUTTONDBLCLK:
        return 0;
    }
    return 0;
  }

  if (msg == WM_COMMAND) {
    switch (LOWORD(wp)) {
      case kTrayCapture:
        if (!app.Annotation().IsAnnotating() && !app.Recording().IsStandaloneRecording())
          app.GetF1Toolbar().ShowMenu();
        break;
      case kTrayPin:
        if (!app.Annotation().IsAnnotating()) app.Pins().PinCapture();
        break;
      case kTraySettings:
        app.Settings().Show();
        break;
      case kTrayAutoStart:
        app.Settings().SetAutoStart(!app.Settings().IsAutoStartEnabled());
        break;
      case kTrayAbout:
        app.About().Show();
        break;
      case kTrayExit:
        app.Quit();
        break;
      case kTrayLangZhCN:
        SetLanguage(kLangZhCN);
        app.Settings().SaveLanguageSetting();
        break;
      case kTrayLangEnUS:
        SetLanguage(kLangEnUS);
        app.Settings().SaveLanguageSetting();
        break;
      case kTrayPasteClip:
        app.Pins().PinFromClipboard();
        break;
      case kTrayClearHistory:
        pixelgrab_history_clear(app.Ctx());
        std::printf("  History cleared.\n");
        break;
      default:
        if (LOWORD(wp) >= kTrayHistoryBase &&
            LOWORD(wp) < kTrayHistoryBase + 1000) {
          int history_id = LOWORD(wp) - kTrayHistoryBase;
          app.Pins().PinFromHistory(history_id);
        }
        break;
    }
    return 0;
  }

  return DefWindowProcW(hwnd, msg, wp, lp);
}

#endif
