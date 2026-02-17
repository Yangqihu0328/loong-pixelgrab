// Copyright 2026 The PixelGrab Authors
// About dialog + update checking UI.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_TRAY_ABOUT_DIALOG_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_TRAY_ABOUT_DIALOG_H_

#ifdef _WIN32

#include "platform/windows/win_app_defs.h"
#include "core/update_checker.h"

class AboutDialog {
 public:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  void GetExeDirectory(wchar_t* buf, int bufLen);
  Gdiplus::Image* LoadImageSafe(const wchar_t* path);
  void Show();
  bool IsAppBusy();
  void ShowPendingUpdate();
  void TriggerUpdateCheck(bool manual);

  bool PendingUpdate() const { return pending_update_; }
  void SetPendingUpdate(bool v) { pending_update_ = v; }
  bool PendingUpdateManual() const { return pending_update_manual_; }
  void SetPendingUpdateManual(bool v) { pending_update_manual_ = v; }
  UpdateInfo& PendingUpdateInfo() { return pending_update_info_; }
  const UpdateInfo& PendingUpdateInfo() const { return pending_update_info_; }

 private:
  bool       about_done_ = false;
  bool       about_check_update_ = false;
  Gdiplus::Image* qr_wechat_ = nullptr;
  Gdiplus::Image* qr_wechat_pay_ = nullptr;
  Gdiplus::Image* qr_alipay_pay_ = nullptr;
  bool       pending_update_ = false;
  bool       pending_update_manual_ = false;
  UpdateInfo pending_update_info_ = {};
};

#endif  // _WIN32
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_TRAY_ABOUT_DIALOG_H_
