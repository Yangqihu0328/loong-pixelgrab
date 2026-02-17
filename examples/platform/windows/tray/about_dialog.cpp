// Copyright 2026 The PixelGrab Authors
// About dialog + update checking UI.

#include "platform/windows/tray/about_dialog.h"

#ifdef _WIN32

#include "platform/windows/win_application.h"

void AboutDialog::GetExeDirectory(wchar_t* buf, int bufLen) {
  GetModuleFileNameW(nullptr, buf, bufLen);
  wchar_t* p = wcsrchr(buf, L'\\');
  if (p) *(p + 1) = L'\0';
}

Gdiplus::Image* AboutDialog::LoadImageSafe(const wchar_t* path) {
  Gdiplus::Image* img = Gdiplus::Image::FromFile(path);
  if (img && img->GetLastStatus() != Gdiplus::Ok) {
    delete img;
    return nullptr;
  }
  return img;
}

LRESULT CALLBACK AboutDialog::WndProc(HWND hwnd, UINT msg,
                                        WPARAM wp, LPARAM lp) {
  auto& self = Application::instance().About();
  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      Gdiplus::Graphics gfx(hdc);
      gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
      gfx.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

      Gdiplus::FontFamily family(L"Microsoft YaHei");
      if (!family.IsAvailable()) {
        Gdiplus::FontFamily fallback(L"SimSun");
      }
      Gdiplus::Font titleFont(&family, 15, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
      Gdiplus::Font normalFont(&family, 12, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
      Gdiplus::Font labelFont(&family, 12, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

      Gdiplus::SolidBrush blackBrush(Gdiplus::Color(255, 0, 0, 0));
      Gdiplus::SolidBrush grayBrush(Gdiplus::Color(255, 100, 100, 100));

      Gdiplus::StringFormat centerFmt;
      centerFmt.SetAlignment(Gdiplus::StringAlignmentCenter);

      const char* ver = pixelgrab_version_string();
      wchar_t wver[32] = {};
      for (int i = 0; i < 31 && ver[i]; ++i)
        wver[i] = static_cast<wchar_t>(ver[i]);
      wchar_t title[64];
      wsprintfW(title, L"PixelGrab v%s", wver);

      RECT cr;
      GetClientRect(hwnd, &cr);
      Gdiplus::REAL cw = static_cast<Gdiplus::REAL>(cr.right);

      Gdiplus::RectF titleRect(0, 15, cw, 22);
      gfx.DrawString(title, -1, &titleFont, titleRect, &centerFmt, &blackBrush);

      Gdiplus::RectF descRect(0, 40, cw, 18);
      gfx.DrawString(T(kStr_AboutDesc), -1, &normalFont, descRect, &centerFmt, &grayBrush);

      Gdiplus::RectF copyRect(0, 60, cw, 18);
      gfx.DrawString(L"Copyright 2026 The PixelGrab Authors",
                     -1, &normalFont, copyRect, &centerFmt, &grayBrush);

      const int qrSize = 220;
      const int labelY = 88;
      const int qrY    = 108;
      const int qrGap = 20;
      int startX = (cr.right - (3 * qrSize + 2 * qrGap)) / 2;
      int positions[3] = { startX, startX + qrSize + qrGap,
                           startX + 2 * (qrSize + qrGap) };

      const wchar_t* labels[] = {
        T(kStr_AboutQRWechat), T(kStr_AboutQRWechatPay), T(kStr_AboutQRAlipayPay)
      };
      Gdiplus::Image* images[] = {
        self.qr_wechat_, self.qr_wechat_pay_, self.qr_alipay_pay_
      };

      Gdiplus::Pen borderPen(Gdiplus::Color(255, 210, 210, 210), 1);
      for (int i = 0; i < 3; ++i) {
        Gdiplus::RectF lblR(static_cast<Gdiplus::REAL>(positions[i]),
                            static_cast<Gdiplus::REAL>(labelY),
                            static_cast<Gdiplus::REAL>(qrSize), 16.0f);
        gfx.DrawString(labels[i], -1, &labelFont, lblR, &centerFmt, &blackBrush);

        if (images[i]) {
          int iw = static_cast<int>(images[i]->GetWidth());
          int ih = static_cast<int>(images[i]->GetHeight());
          if (iw > 0 && ih > 0) {
            float scale = (std::min)(static_cast<float>(qrSize) / iw,
                                     static_cast<float>(qrSize) / ih);
            int dw = static_cast<int>(iw * scale);
            int dh = static_cast<int>(ih * scale);
            int dx = positions[i] + (qrSize - dw) / 2;
            int dy = qrY + (qrSize - dh) / 2;
            gfx.DrawImage(images[i], dx, dy, dw, dh);
          }
        } else {
          gfx.DrawRectangle(&borderPen, positions[i], qrY, qrSize, qrSize);
          Gdiplus::RectF phR(static_cast<Gdiplus::REAL>(positions[i]),
                             static_cast<Gdiplus::REAL>(qrY + 45),
                             static_cast<Gdiplus::REAL>(qrSize), 30.0f);
          gfx.DrawString(T(kStr_AboutImageNotFound), -1, &labelFont,
              phR, &centerFmt, &grayBrush);
        }
      }

      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_COMMAND:
      if (LOWORD(wp) == kAboutCheckUpdate) {
        self.about_check_update_ = true;
        self.about_done_ = true;
        return 0;
      }
      if (LOWORD(wp) == kAboutClose) {
        self.about_done_ = true;
        return 0;
      }
      break;
    case WM_CLOSE:
      self.about_done_ = true;
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

void AboutDialog::Show() {
  about_done_         = false;
  about_check_update_ = false;

  Gdiplus::GdiplusStartupInput gdipInput;
  ULONG_PTR gdipToken = 0;
  Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr);

  wchar_t exeDir[MAX_PATH] = {};
  GetExeDirectory(exeDir, MAX_PATH);
  wchar_t path[MAX_PATH];

  wsprintfW(path, L"%sqrcode\\IMG_3994.JPG", exeDir);
  qr_wechat_ = LoadImageSafe(path);
  wsprintfW(path, L"%sqrcode\\IMG_3995.JPG", exeDir);
  qr_wechat_pay_ = LoadImageSafe(path);
  wsprintfW(path, L"%sqrcode\\img_3996.jpg", exeDir);
  qr_alipay_pay_ = LoadImageSafe(path);

  int dlg_w = 750, dlg_h = 450;
  int sx = GetSystemMetrics(SM_CXSCREEN);
  int sy = GetSystemMetrics(SM_CYSCREEN);

  HWND dlg = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
      kAboutClass, T(kStr_TitleAbout),
      WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
      (sx - dlg_w) / 2, (sy - dlg_h) / 2, dlg_w, dlg_h,
      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

  HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

  HWND update_btn = CreateWindowExW(0, L"BUTTON", T(kStr_BtnCheckUpdate),
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      270, 370, 90, 28, dlg,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kAboutCheckUpdate)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessageW(update_btn, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

  HWND close_btn = CreateWindowExW(0, L"BUTTON", T(kStr_BtnClose),
      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
      390, 370, 90, 28, dlg,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kAboutClose)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessageW(close_btn, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

  SetForegroundWindow(dlg);

  MSG tmsg;
  while (!about_done_ && GetMessageW(&tmsg, nullptr, 0, 0)) {
    TranslateMessage(&tmsg);
    DispatchMessageW(&tmsg);
  }

  if (IsWindow(dlg)) DestroyWindow(dlg);

  delete qr_wechat_;     qr_wechat_     = nullptr;
  delete qr_wechat_pay_; qr_wechat_pay_ = nullptr;
  delete qr_alipay_pay_; qr_alipay_pay_ = nullptr;

  Gdiplus::GdiplusShutdown(gdipToken);

  if (about_check_update_) {
    TriggerUpdateCheck(true);
  }
}

bool AboutDialog::IsAppBusy() {
  auto& app = Application::instance();
  return app.Selection().IsSelecting() || app.GetF1Toolbar().Toolbar() != nullptr
      || app.Annotation().IsAnnotating()
      || app.Recording().IsStandaloneRecording()
      || app.Recording().RecSettingsWnd() != nullptr
      || app.Recording().RecPreviewWnd() != nullptr;
}

void AboutDialog::ShowPendingUpdate() {
  if (!pending_update_) return;
  if (IsAppBusy()) return;

  pending_update_ = false;
  const UpdateInfo info = pending_update_info_;
  bool manual = pending_update_manual_;

  if (info.available) {
    wchar_t msg[1024];
    wchar_t wver[32] = {};
    for (int i = 0; i < 31 && info.latest_version[i]; ++i)
      wver[i] = static_cast<wchar_t>(info.latest_version[i]);
    wsprintfW(msg, T(kStr_MsgNewVersion), wver);
    int r = MessageBoxW(nullptr, msg, T(kStr_TitleUpdate),
            MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST);
    if (r == IDYES && info.download_url[0]) {
      OpenUrlInBrowser(info.download_url);
    }
  } else if (manual) {
    MessageBoxW(nullptr, T(kStr_MsgUpToDate), T(kStr_TitleUpdate),
        MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
  }
}

void AboutDialog::TriggerUpdateCheck(bool manual) {
  StartUpdateCheckAsync(
      PIXELGRAB_GITHUB_REPO,
      pixelgrab_version_string(),
      [this, manual](const UpdateInfo& info) {
        if (IsAppBusy()) {
          if (info.available || manual) {
            pending_update_        = true;
            pending_update_manual_ = manual;
            pending_update_info_   = info;
          }
          return;
        }

        if (info.available) {
          wchar_t msg[1024];
          wchar_t wver[32] = {};
          for (int i = 0; i < 31 && info.latest_version[i]; ++i)
            wver[i] = static_cast<wchar_t>(info.latest_version[i]);
          wsprintfW(msg, T(kStr_MsgNewVersion), wver);
          int r = MessageBoxW(nullptr, msg, T(kStr_TitleUpdate),
                  MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST);
          if (r == IDYES && info.download_url[0]) {
            OpenUrlInBrowser(info.download_url);
          }
        } else if (manual) {
          MessageBoxW(nullptr, T(kStr_MsgUpToDate), T(kStr_TitleUpdate),
              MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
        }
      });
}

#endif
