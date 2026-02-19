// Copyright 2026 The PixelGrab Authors
// Screen recording: standalone recording, recording border, settings, countdown, preview.

#include "platform/windows/recording/recording_manager.h"

#ifdef _WIN32

#include "platform/windows/win_application.h"

void RecordingManager::StopRecording() {
  if (!recording_ || !recorder_) return;

  pixelgrab_recorder_stop(recorder_);
  int64_t ms = pixelgrab_recorder_get_duration_ms(recorder_);
  std::printf("  [Record] Stopped: %lld ms recorded.\n",
              static_cast<long long>(ms));

  pixelgrab_recorder_destroy(recorder_);
  recorder_  = nullptr;
  recording_ = false;
}

LRESULT CALLBACK RecordingManager::RecCtrlWndProc(HWND hwnd, UINT msg,
                                                    WPARAM wp, LPARAM lp) {
  auto& self = Application::instance().Recording();
  if (msg == WM_TIMER && wp == kStandaloneRecTimerId) {
    if (self.standalone_recording_ && self.rec_ctrl_label_) {
      ULONGLONG elapsed = GetTickCount64() - self.standalone_rec_start_;
      int secs = static_cast<int>(elapsed / 1000);
      int mins = secs / 60;
      secs %= 60;
      PixelGrabRecordState state = self.standalone_recorder_
          ? pixelgrab_recorder_get_state(self.standalone_recorder_)
          : kPixelGrabRecordIdle;
      const wchar_t* icon = (state == kPixelGrabRecordPaused) ? L"\x23F8" : L"\x25CF";
      wchar_t buf[32];
      wsprintfW(buf, L"%s %02d:%02d", icon, mins, secs);
      SetWindowTextW(self.rec_ctrl_label_, buf);
    }
    // Re-assert topmost z-order so the control bar stays visible even when
    // the user interacts with other windows or the taskbar.
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (self.rec_border_) {
      SetWindowPos(self.rec_border_, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    return 0;
  }

  if (msg == WM_CTLCOLORSTATIC) {
    HWND ctrl = reinterpret_cast<HWND>(lp);
    if (ctrl == self.rec_ctrl_label_) {
      HDC hdc = reinterpret_cast<HDC>(wp);
      SetTextColor(hdc, RGB(220, 30, 30));
      SetBkMode(hdc, TRANSPARENT);
      return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
    }
  }

  if (msg == WM_COMMAND) {
    if (LOWORD(wp) == kRecCtrlStopBtn) {
      self.StopStandalone();
      return 0;
    }
    if (LOWORD(wp) == kRecCtrlPauseBtn && self.standalone_recorder_) {
      PixelGrabRecordState state =
          pixelgrab_recorder_get_state(self.standalone_recorder_);
      if (state == kPixelGrabRecordRecording) {
        pixelgrab_recorder_pause(self.standalone_recorder_);
        SetWindowTextW(self.rec_ctrl_pause_btn_, L"\x25B6");
        std::printf("  [Record] Paused.\n");
      } else if (state == kPixelGrabRecordPaused) {
        pixelgrab_recorder_resume(self.standalone_recorder_);
        SetWindowTextW(self.rec_ctrl_pause_btn_, L"\x23F8");
        std::printf("  [Record] Resumed.\n");
      }
      return 0;
    }
  }

  if (msg == WM_CLOSE) {
    self.StopStandalone();
    return 0;
  }

  return DefWindowProcW(hwnd, msg, wp, lp);
}

void RecordingManager::StartStandalone(RECT rc) {
  auto& app = Application::instance();
  if (standalone_recording_) return;

  if (!pixelgrab_recorder_is_supported(app.Ctx())) {
    std::printf("  [Record] Recording not supported on this system.\n");
    MessageBoxW(nullptr, L"Screen recording is not supported.",
        L"PixelGrab", MB_OK | MB_ICONERROR | MB_TOPMOST);
    return;
  }

  standalone_rec_rect_ = rc;
  int rw = static_cast<int>(rc.right - rc.left);
  int rh = static_cast<int>(rc.bottom - rc.top);

  SYSTEMTIME st;
  GetLocalTime(&st);
  std::snprintf(rec_output_path_, sizeof(rec_output_path_),
                "PixelGrab_Rec_%04d%02d%02d_%02d%02d%02d.mp4",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond);

  PixelGrabTextWatermarkConfig wm = {};
  char wm_brand_utf8[256];
  WideCharToMultiByte(CP_UTF8, 0, T(kStr_WatermarkBranding), -1,
                      wm_brand_utf8, sizeof(wm_brand_utf8), nullptr, nullptr);
  wm.text      = wm_brand_utf8;
  wm.font_name = "Microsoft YaHei";
  wm.font_size = 16;
  wm.color     = 0xCCFFFFFF;
  wm.position  = kPixelGrabWatermarkBottomRight;
  wm.margin    = 10;

  PixelGrabAudioSource audio = kPixelGrabAudioNone;
  if (rec_audio_speaker_ && rec_audio_mic_)
    audio = kPixelGrabAudioBoth;
  else if (rec_audio_speaker_)
    audio = kPixelGrabAudioSystem;
  else if (rec_audio_mic_)
    audio = kPixelGrabAudioMicrophone;

  PixelGrabTextWatermarkConfig user_wm = {};
  if (rec_user_wm_enabled_ && rec_user_wm_text_[0]) {
    user_wm.text      = rec_user_wm_text_;
    user_wm.font_name = "Microsoft YaHei";
    user_wm.font_size = rec_user_wm_font_size_;
    uint32_t alpha    = static_cast<uint32_t>(rec_user_wm_opacity_ * 255 / 100);
    user_wm.color     = (alpha << 24) | 0x00FFFFFF;
    user_wm.margin    = 10;
  }

  PixelGrabRecordConfig cfg = {};
  cfg.output_path    = rec_output_path_;
  cfg.region_x       = static_cast<int>(rc.left);
  cfg.region_y       = static_cast<int>(rc.top);
  cfg.region_width   = rw;
  cfg.region_height  = rh;
  cfg.fps            = 15;
  cfg.bitrate        = 2000000;
  cfg.watermark      = &wm;
  cfg.user_watermark = (rec_user_wm_enabled_ && rec_user_wm_text_[0])
                           ? &user_wm : nullptr;
  cfg.auto_capture   = 1;
  cfg.audio_source   = audio;
  cfg.audio_device_id = rec_audio_device_id_[0] ? rec_audio_device_id_ : nullptr;

  standalone_recorder_ = pixelgrab_recorder_create(app.Ctx(), &cfg);
  if (!standalone_recorder_) {
    std::printf("  [Record] Failed to create recorder: %s\n",
                pixelgrab_get_last_error_message(app.Ctx()));
    MessageBoxW(nullptr, T(kStr_MsgCreateRecorderFailed),
        L"PixelGrab", MB_OK | MB_ICONERROR | MB_TOPMOST);
    return;
  }

  PixelGrabError err = pixelgrab_recorder_start(standalone_recorder_);
  if (err != kPixelGrabOk) {
    std::printf("  [Record] Failed to start: %s\n",
                pixelgrab_get_last_error_message(app.Ctx()));
    pixelgrab_recorder_destroy(standalone_recorder_);
    standalone_recorder_ = nullptr;
    MessageBoxW(nullptr, T(kStr_MsgStartRecordFailed),
        L"PixelGrab", MB_OK | MB_ICONERROR | MB_TOPMOST);
    return;
  }

  standalone_recording_ = true;
  standalone_rec_start_ = GetTickCount64();

  ShowRecBorder(rc);

  int bar_w = 240, bar_h = 36;
  int bar_x = static_cast<int>(rc.left) + (rw - bar_w) / 2;
  int bar_y = static_cast<int>(rc.bottom) + 4;

  int scr_w = GetSystemMetrics(SM_CXSCREEN);
  int scr_h = GetSystemMetrics(SM_CYSCREEN);
  if (bar_x < 0) bar_x = 0;
  if (bar_x + bar_w > scr_w) bar_x = scr_w - bar_w;
  if (bar_y + bar_h > scr_h) bar_y = static_cast<int>(rc.top) - bar_h - 4;

  rec_ctrl_wnd_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      kRecCtrlClass, nullptr, WS_POPUP | WS_VISIBLE,
      bar_x, bar_y, bar_w, bar_h,
      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

  HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  int margin = 4;

  rec_ctrl_label_ = CreateWindowExW(
      0, L"STATIC", L"\x25CF 00:00",
      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
      margin, margin, 90, bar_h - margin * 2,
      rec_ctrl_wnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
  SendMessage(rec_ctrl_label_, WM_SETFONT,
              reinterpret_cast<WPARAM>(font), TRUE);

  rec_ctrl_pause_btn_ = CreateWindowExW(
      0, L"BUTTON", L"\x23F8",
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      margin + 94, margin, 40, bar_h - margin * 2,
      rec_ctrl_wnd_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRecCtrlPauseBtn)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessage(rec_ctrl_pause_btn_, WM_SETFONT,
              reinterpret_cast<WPARAM>(font), TRUE);

  rec_ctrl_stop_btn_ = CreateWindowExW(
      0, L"BUTTON", T(kStr_BtnStop),
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      margin + 140, margin, 70, bar_h - margin * 2,
      rec_ctrl_wnd_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRecCtrlStopBtn)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessage(rec_ctrl_stop_btn_, WM_SETFONT,
              reinterpret_cast<WPARAM>(font), TRUE);

  SetTimer(rec_ctrl_wnd_, kStandaloneRecTimerId, 1000, nullptr);

  std::printf("  [Record] Standalone started: %dx%d @15fps -> %s\n",
              rw, rh, rec_output_path_);
}

void RecordingManager::StopStandalone() {
  if (!standalone_recording_ || !standalone_recorder_) return;

  HideRecBorder();

  if (rec_ctrl_wnd_) {
    KillTimer(rec_ctrl_wnd_, kStandaloneRecTimerId);
    DestroyWindow(rec_ctrl_wnd_);
    rec_ctrl_wnd_       = nullptr;
    rec_ctrl_label_     = nullptr;
    rec_ctrl_stop_btn_  = nullptr;
    rec_ctrl_pause_btn_ = nullptr;
  }

  pixelgrab_recorder_stop(standalone_recorder_);
  rec_final_duration_ms_ = pixelgrab_recorder_get_duration_ms(standalone_recorder_);
  std::printf("  [Record] Standalone stopped: %lld ms recorded.\n",
              static_cast<long long>(rec_final_duration_ms_));

  pixelgrab_recorder_destroy(standalone_recorder_);
  standalone_recorder_  = nullptr;
  standalone_recording_ = false;

  ShowPreview();
}

void RecordingManager::ShowRecBorder(RECT rc) {
  HideRecBorder();

  int b = kHighlightBorder;
  int bx = static_cast<int>(rc.left) - b;
  int by = static_cast<int>(rc.top) - b;
  int bw = static_cast<int>(rc.right - rc.left) + 2 * b;
  int bh = static_cast<int>(rc.bottom - rc.top) + 2 * b;

  rec_border_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
      kPinBorderClass, nullptr, WS_POPUP,
      bx, by, bw, bh,
      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (!rec_border_) return;

  SetLayeredWindowAttributes(rec_border_, 0, 180, LWA_ALPHA);

  HRGN outer = CreateRectRgn(0, 0, bw, bh);
  HRGN inner = CreateRectRgn(b, b, bw - b, bh - b);
  CombineRgn(outer, outer, inner, RGN_DIFF);
  SetWindowRgn(rec_border_, outer, TRUE);
  DeleteObject(inner);

  ShowWindow(rec_border_, SW_SHOWNOACTIVATE);
}

void RecordingManager::HideRecBorder() {
  if (rec_border_) {
    DestroyWindow(rec_border_);
    rec_border_ = nullptr;
  }
}

LRESULT CALLBACK RecordingManager::RecSettingsWndProc(HWND hwnd, UINT msg,
                                                       WPARAM wp, LPARAM lp) {
  auto& self = Application::instance().Recording();
  if (msg == WM_COMMAND) {
    WORD id = LOWORD(wp);
    if (id == kRecWatermarkCheck) {
      BOOL checked = (IsDlgButtonChecked(hwnd, kRecWatermarkCheck) == BST_CHECKED);
      EnableWindow(GetDlgItem(hwnd, kRecWatermarkEdit), checked);
      EnableWindow(GetDlgItem(hwnd, kRecWatermarkFontSize), checked);
      EnableWindow(GetDlgItem(hwnd, kRecWatermarkOpacity), checked);
      return 0;
    }
    if (id == kRecStart) {
      self.rec_audio_speaker_ = (IsDlgButtonChecked(hwnd, kRecAudioSpk) == BST_CHECKED);
      self.rec_audio_mic_ = (IsDlgButtonChecked(hwnd, kRecAudioMic) == BST_CHECKED);
      self.rec_user_wm_enabled_ = (IsDlgButtonChecked(hwnd, kRecWatermarkCheck) == BST_CHECKED);
      if (self.rec_user_wm_enabled_) {
        HWND edit = GetDlgItem(hwnd, kRecWatermarkEdit);
        wchar_t wbuf[128] = {};
        GetWindowTextW(edit, wbuf, 128);
        WideCharToMultiByte(CP_UTF8, 0, wbuf, -1,
                            self.rec_user_wm_text_,
                            static_cast<int>(sizeof(self.rec_user_wm_text_)),
                            nullptr, nullptr);
      } else {
        self.rec_user_wm_text_[0] = '\0';
      }
      {
        HWND combo = GetDlgItem(hwnd, kRecWatermarkFontSize);
        int sel = static_cast<int>(SendMessage(combo, CB_GETCURSEL, 0, 0));
        constexpr int kSizes[] = {10, 14, 18, 24};
        if (sel >= 0 && sel < 4) self.rec_user_wm_font_size_ = kSizes[sel];
      }
      {
        HWND combo = GetDlgItem(hwnd, kRecWatermarkOpacity);
        int sel = static_cast<int>(SendMessage(combo, CB_GETCURSEL, 0, 0));
        constexpr int kOpacities[] = {25, 50, 75, 100};
        if (sel >= 0 && sel < 4) self.rec_user_wm_opacity_ = kOpacities[sel];
      }
      // Audio device selection
      {
        HWND devCombo = GetDlgItem(hwnd, kRecAudioDeviceCombo);
        int devSel = static_cast<int>(SendMessage(devCombo, CB_GETCURSEL, 0, 0));
        if (devSel > 0) {
          auto& app2 = Application::instance();
          PixelGrabAudioDeviceInfo devices[16] = {};
          int n = pixelgrab_audio_enumerate_devices(app2.Ctx(), devices, 16);
          if (devSel - 1 < n) {
            strncpy_s(self.rec_audio_device_id_, devices[devSel - 1].id, 255);
          } else {
            self.rec_audio_device_id_[0] = '\0';
          }
        } else {
          self.rec_audio_device_id_[0] = '\0';
        }
      }
      self.rec_settings_ok_   = true;
      self.rec_settings_done_ = true;
      return 0;
    }
    if (id == kRecCancel) {
      self.rec_settings_done_ = true;
      return 0;
    }
  }
  if (msg == WM_CLOSE) {
    self.rec_settings_done_ = true;
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

void RecordingManager::ShowSettings(RECT rc) {
  auto& app = Application::instance();
  rec_settings_done_ = false;
  rec_settings_ok_   = false;
  rec_pending_rect_  = rc;

  ShowRecBorder(rc);

  int dlg_w = 380, dlg_h = 410;
  int cx = (static_cast<int>(rc.left) + static_cast<int>(rc.right)) / 2;
  int cy = (static_cast<int>(rc.top) + static_cast<int>(rc.bottom)) / 2;
  int dlg_x = cx - dlg_w / 2;
  int dlg_y = cy - dlg_h / 2;

  int scr_w = GetSystemMetrics(SM_CXSCREEN);
  int scr_h = GetSystemMetrics(SM_CYSCREEN);
  if (dlg_x < 0) dlg_x = 0;
  if (dlg_y < 0) dlg_y = 0;
  if (dlg_x + dlg_w > scr_w) dlg_x = scr_w - dlg_w;
  if (dlg_y + dlg_h > scr_h) dlg_y = scr_h - dlg_h;

  rec_settings_wnd_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
      kRecSettingsClass, T(kStr_TitleRecSettings),
      WS_POPUP | WS_CAPTION | WS_VISIBLE,
      dlg_x, dlg_y, dlg_w, dlg_h,
      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

  NONCLIENTMETRICSW ncm = {};
  ncm.cbSize = sizeof(ncm);
  SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
  HFONT font = CreateFontIndirectW(&ncm.lfMessageFont);
  auto mkctrl = [&](const wchar_t* cls, const wchar_t* txt, DWORD style,
                     int x, int y, int w, int h, int id) -> HWND {
    HWND c = CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | style,
        x, y, w, h, rec_settings_wnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessage(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return c;
  };

  int m = 16;
  int cw = dlg_w - m * 2 - 10;
  int row = 10;
  int indent = m + 16;

  // ── Audio group ──
  int audio_y = row;
  mkctrl(L"BUTTON", T(kStr_LabelAudio), BS_GROUPBOX, m, audio_y, cw, 130, 0);
  row += 22;
  mkctrl(L"BUTTON", T(kStr_LabelSpeakerSystem), BS_AUTOCHECKBOX, indent, row, cw - 32, 20, kRecAudioSpk);
  row += 26;
  mkctrl(L"BUTTON", T(kStr_LabelMicrophone), BS_AUTOCHECKBOX, indent, row, cw - 32, 20, kRecAudioMic);
  row += 28;

  mkctrl(L"STATIC", L"Device:", SS_LEFT | SS_CENTERIMAGE, indent, row, 48, 22, 0);
  HWND audioDevCombo = CreateWindowExW(0, L"COMBOBOX", nullptr,
      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
      indent + 52, row - 2, cw - (indent - m) - 52 - 16, 200, rec_settings_wnd_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRecAudioDeviceCombo)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessage(audioDevCombo, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  SendMessageW(audioDevCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"(Default)"));
  if (pixelgrab_audio_is_supported(app.Ctx())) {
    PixelGrabAudioDeviceInfo devices[16] = {};
    int n = pixelgrab_audio_enumerate_devices(app.Ctx(), devices, 16);
    for (int i = 0; i < n; ++i) {
      wchar_t wname[256] = {};
      MultiByteToWideChar(CP_UTF8, 0, devices[i].name, -1, wname, 256);
      SendMessageW(audioDevCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wname));
    }
  }
  SendMessage(audioDevCombo, CB_SETCURSEL, 0, 0);

  row = audio_y + 130 + 10;

  // ── Watermark group ──
  int wm_y = row;
  mkctrl(L"BUTTON", T(kStr_LabelWatermark), BS_GROUPBOX, m, wm_y, cw, 150, 0);
  row += 22;
  HWND wm_check = mkctrl(L"BUTTON", T(kStr_LabelEnable), BS_AUTOCHECKBOX, indent, row, cw - 32, 20, kRecWatermarkCheck);
  row += 24;
  HWND wm_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_DISABLED,
      indent, row, cw - 32, 24,
      rec_settings_wnd_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRecWatermarkEdit)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessage(wm_edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  SendMessageW(wm_edit, EM_SETCUEBANNER, FALSE,
      reinterpret_cast<LPARAM>(T(kStr_PlaceholderWatermark)));
  row += 30;

  mkctrl(L"STATIC", T(kStr_LabelFontSize), SS_LEFT | SS_CENTERIMAGE, indent, row, 36, 22, 0);
  HWND wm_font_combo = CreateWindowExW(0, L"COMBOBOX", nullptr,
      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_DISABLED,
      indent + 38, row, 56, 120, rec_settings_wnd_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRecWatermarkFontSize)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessage(wm_font_combo, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  SendMessageW(wm_font_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"10"));
  SendMessageW(wm_font_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"14"));
  SendMessageW(wm_font_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"18"));
  SendMessageW(wm_font_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"24"));

  int gap2 = indent + 38 + 56 + 18;
  mkctrl(L"STATIC", T(kStr_LabelOpacity), SS_LEFT | SS_CENTERIMAGE, gap2, row, 50, 22, 0);
  HWND wm_opacity_combo = CreateWindowExW(0, L"COMBOBOX", nullptr,
      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_DISABLED,
      gap2 + 52, row, 64, 120, rec_settings_wnd_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRecWatermarkOpacity)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessage(wm_opacity_combo, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  SendMessageW(wm_opacity_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"25%"));
  SendMessageW(wm_opacity_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"50%"));
  SendMessageW(wm_opacity_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"75%"));
  SendMessageW(wm_opacity_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"100%"));
  row += 28;

  mkctrl(L"STATIC", T(kStr_HintWatermarkDesc), SS_LEFT, indent, row, cw - 32, 28, 0);

  row = wm_y + 150 + 10;

  {
    constexpr int kSizes[] = {10, 14, 18, 24};
    int font_sel = 1;
    for (int i = 0; i < 4; ++i)
      if (kSizes[i] == rec_user_wm_font_size_) { font_sel = i; break; }
    SendMessage(wm_font_combo, CB_SETCURSEL, font_sel, 0);

    constexpr int kOpacities[] = {25, 50, 75, 100};
    int opacity_sel = 2;
    for (int i = 0; i < 4; ++i)
      if (kOpacities[i] == rec_user_wm_opacity_) { opacity_sel = i; break; }
    SendMessage(wm_opacity_combo, CB_SETCURSEL, opacity_sel, 0);
  }

  if (rec_user_wm_enabled_) {
    CheckDlgButton(rec_settings_wnd_, kRecWatermarkCheck, BST_CHECKED);
    EnableWindow(wm_edit, TRUE);
    EnableWindow(wm_font_combo, TRUE);
    EnableWindow(wm_opacity_combo, TRUE);
    if (rec_user_wm_text_[0]) {
      wchar_t wtxt[256] = {};
      MultiByteToWideChar(CP_UTF8, 0, rec_user_wm_text_, -1, wtxt, 256);
      SetWindowTextW(wm_edit, wtxt);
    }
  }
  (void)wm_check;

  int btn_w = 110, btn_h = 30;
  int gap = 16;
  int total = btn_w * 2 + gap;
  int bx = (dlg_w - total) / 2 - 5;
  mkctrl(L"BUTTON", T(kStr_BtnStartRecord), BS_DEFPUSHBUTTON, bx, row, btn_w, btn_h, kRecStart);
  mkctrl(L"BUTTON", T(kStr_ToolCancel), BS_PUSHBUTTON, bx + btn_w + gap, row, btn_w, btn_h, kRecCancel);

  SetForegroundWindow(rec_settings_wnd_);

  MSG tmsg;
  while (!rec_settings_done_ && GetMessage(&tmsg, nullptr, 0, 0)) {
    if (tmsg.message == WM_KEYDOWN && tmsg.wParam == VK_ESCAPE) break;
    TranslateMessage(&tmsg);
    DispatchMessage(&tmsg);
  }

  if (IsWindow(rec_settings_wnd_)) DestroyWindow(rec_settings_wnd_);
  rec_settings_wnd_ = nullptr;

  if (rec_settings_ok_) {
    app.Settings().SaveRecWatermarkSettings();
    ShowCountdown(rec_pending_rect_);
  } else {
    HideRecBorder();
    app.About().ShowPendingUpdate();
  }
}

LRESULT CALLBACK RecordingManager::CountdownWndProc(HWND hwnd, UINT msg,
                                                      WPARAM wp, LPARAM lp) {
  auto& self = Application::instance().Recording();
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT cr;
    GetClientRect(hwnd, &cr);
    HBRUSH bg = CreateSolidBrush(RGB(40, 40, 40));
    SelectObject(hdc, bg);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, cr.left, cr.top, cr.right, cr.bottom);
    DeleteObject(bg);
    wchar_t num[4];
    wsprintfW(num, L"%d", self.countdown_value_);
    HFONT big = CreateFontW(90, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT old = static_cast<HFONT>(SelectObject(hdc, big));
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, num, -1, &cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);
    DeleteObject(big);
    EndPaint(hwnd, &ps);
    return 0;
  }
  if (msg == WM_TIMER && wp == kCountdownTimerId) {
    self.countdown_value_--;
    if (self.countdown_value_ <= 0) {
      KillTimer(hwnd, kCountdownTimerId);
      RECT rec_rc = self.countdown_rec_rect_;
      DestroyWindow(hwnd);
      self.countdown_wnd_ = nullptr;
      self.StartStandalone(rec_rc);
    } else {
      InvalidateRect(hwnd, nullptr, TRUE);
      // Re-assert topmost so the countdown stays visible during user interaction.
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    return 0;
  }
  if (msg == WM_ERASEBKGND) return 1;
  return DefWindowProcW(hwnd, msg, wp, lp);
}

void RecordingManager::ShowCountdown(RECT rc) {
  countdown_value_    = 3;
  countdown_rec_rect_ = rc;

  int cx = (static_cast<int>(rc.left) + static_cast<int>(rc.right)) / 2;
  int cy = (static_cast<int>(rc.top) + static_cast<int>(rc.bottom)) / 2;
  int wx = cx - kCountdownSize / 2;
  int wy = cy - kCountdownSize / 2;

  countdown_wnd_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
      kCountdownClass, nullptr, WS_POPUP | WS_VISIBLE,
      wx, wy, kCountdownSize, kCountdownSize,
      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

  HRGN rgn = CreateEllipticRgn(0, 0, kCountdownSize, kCountdownSize);
  SetWindowRgn(countdown_wnd_, rgn, TRUE);
  SetLayeredWindowAttributes(countdown_wnd_, 0, 192, LWA_ALPHA);
  SetForegroundWindow(countdown_wnd_);
  SetTimer(countdown_wnd_, kCountdownTimerId, 1000, nullptr);
  std::printf("  [Record] Countdown started...\n");
}

LRESULT CALLBACK RecordingManager::RecPreviewWndProc(HWND hwnd, UINT msg,
                                                       WPARAM wp, LPARAM lp) {
  auto& self = Application::instance().Recording();
  if (msg == WM_COMMAND) {
    WORD id = LOWORD(wp);
    if (id == kRecPrevPlay) {
      wchar_t wpath[MAX_PATH] = {};
      MultiByteToWideChar(CP_UTF8, 0, self.rec_output_path_, -1, wpath, MAX_PATH);
      ShellExecuteW(nullptr, L"open", wpath, nullptr, nullptr, SW_SHOWNORMAL);
      return 0;
    }
    if (id == kRecPrevCopy) {
      self.CopyFileToClipboard(self.rec_output_path_);
      MessageBoxW(hwnd, T(kStr_MsgCopiedClipboard),
          L"PixelGrab", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
      return 0;
    }
    if (id == kRecPrevDone) {
      self.DismissPreview();
      return 0;
    }
  }
  if (msg == WM_CLOSE) {
    self.DismissPreview();
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

void RecordingManager::ShowPreview() {
  if (rec_output_path_[0] == '\0') return;

  int dlg_w = 420, dlg_h = 200;
  int scr_w = GetSystemMetrics(SM_CXSCREEN);
  int scr_h = GetSystemMetrics(SM_CYSCREEN);
  int dlg_x = (scr_w - dlg_w) / 2;
  int dlg_y = (scr_h - dlg_h) / 2;

  rec_preview_wnd_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
      kRecPreviewClass, T(kStr_TitleRecComplete),
      WS_POPUP | WS_CAPTION | WS_VISIBLE,
      dlg_x, dlg_y, dlg_w, dlg_h,
      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

  HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  auto mk = [&](const wchar_t* cls, const wchar_t* txt, DWORD style,
                 int x, int y, int w, int h, int id) -> HWND {
    HWND c = CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | style,
        x, y, w, h, rec_preview_wnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessage(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return c;
  };

  int m = 16, row = 12;

  wchar_t dur_buf[128];
  int secs = static_cast<int>(rec_final_duration_ms_ / 1000);
  int mins = secs / 60;
  secs %= 60;
  wsprintfW(dur_buf, T(kStr_FmtDuration), mins, secs);
  mk(L"STATIC", dur_buf, SS_LEFT, m, row, dlg_w - m * 2, 22, 0);
  row += 28;

  wchar_t wpath[MAX_PATH] = {};
  MultiByteToWideChar(CP_UTF8, 0, rec_output_path_, -1, wpath, MAX_PATH);
  wchar_t path_buf[MAX_PATH + 32];
  wsprintfW(path_buf, T(kStr_FmtFile), wpath);
  mk(L"STATIC", path_buf, SS_LEFT | SS_PATHELLIPSIS, m, row, dlg_w - m * 2 - 10, 22, 0);
  row += 28;

  wchar_t info_buf[128];
  wsprintfW(info_buf, T(kStr_FmtFormatAudio),
            (rec_audio_speaker_ && rec_audio_mic_) ? T(kStr_AudioSpeakerMic)
                : rec_audio_speaker_ ? T(kStr_AudioSpeaker)
                    : rec_audio_mic_ ? T(kStr_AudioMic)
                        : T(kStr_AudioNone));
  mk(L"STATIC", info_buf, SS_LEFT, m, row, dlg_w - m * 2, 22, 0);
  row += 36;

  int btn_w = 110, btn_h = 32, gap = 14;
  int total = btn_w * 3 + gap * 2;
  int bx = (dlg_w - total) / 2 - 5;
  mk(L"BUTTON", T(kStr_BtnPlayPreview), BS_PUSHBUTTON, bx, row, btn_w, btn_h, kRecPrevPlay);
  mk(L"BUTTON", T(kStr_BtnCopyClipboard), BS_PUSHBUTTON, bx + btn_w + gap, row, btn_w, btn_h, kRecPrevCopy);
  mk(L"BUTTON", T(kStr_BtnDone), BS_DEFPUSHBUTTON, bx + (btn_w + gap) * 2, row, btn_w, btn_h, kRecPrevDone);

  SetForegroundWindow(rec_preview_wnd_);
}

void RecordingManager::DismissPreview() {
  if (rec_preview_wnd_) {
    DestroyWindow(rec_preview_wnd_);
    rec_preview_wnd_ = nullptr;
  }
  Application::instance().About().ShowPendingUpdate();
}

bool RecordingManager::ApplyUserWatermark(PixelGrabImage* image) {
  auto& app = Application::instance();
  if (!rec_user_wm_enabled_ || !rec_user_wm_text_[0]) return false;
  if (!pixelgrab_watermark_is_supported(app.Ctx())) return false;

  PixelGrabTextWatermarkConfig wm = {};
  wm.text      = rec_user_wm_text_;
  wm.font_name = "Microsoft YaHei";
  wm.font_size = rec_user_wm_font_size_;
  uint32_t alpha = static_cast<uint32_t>(rec_user_wm_opacity_ * 255 / 100);
  wm.color     = (alpha << 24) | 0x00FFFFFF;
  wm.position  = kPixelGrabWatermarkBottomRight;
  wm.margin    = 10;

  PixelGrabError err = pixelgrab_watermark_apply_text(app.Ctx(), image, &wm);
  if (err == kPixelGrabOk) {
    std::printf("  [Watermark] Applied text watermark.\n");
    return true;
  }
  return false;
}

void RecordingManager::CopyFileToClipboard(const char* filepath) {
  wchar_t wpath[MAX_PATH] = {};
  MultiByteToWideChar(CP_UTF8, 0, filepath, -1, wpath, MAX_PATH);
  wchar_t full[MAX_PATH] = {};
  GetFullPathNameW(wpath, MAX_PATH, full, nullptr);
  size_t path_bytes = (wcslen(full) + 2) * sizeof(wchar_t);
  size_t total = sizeof(DROPFILES) + path_bytes;
  HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total);
  if (!hg) return;
  auto* df = static_cast<DROPFILES*>(GlobalLock(hg));
  df->pFiles = sizeof(DROPFILES);
  df->fWide  = TRUE;
  wchar_t* dest = reinterpret_cast<wchar_t*>(
      reinterpret_cast<char*>(df) + sizeof(DROPFILES));
  wcscpy_s(dest, wcslen(full) + 1, full);
  GlobalUnlock(hg);
  if (OpenClipboard(nullptr)) {
    EmptyClipboard();
    SetClipboardData(CF_HDROP, hg);
    CloseClipboard();
  } else {
    GlobalFree(hg);
  }
}

#endif
