// Copyright 2026 The PixelGrab Authors
//
// Application singleton implementation — init, run, shutdown.

#include "platform/windows/win_application.h"

#ifdef _WIN32

#include <fstream>
#include <string>


Application& Application::instance() {
  static Application inst;
  return inst;
}

void Application::RegisterWindowClasses(HINSTANCE hInst) {
  WNDCLASSEXW wc;
  std::memset(&wc, 0, sizeof(wc));
  wc.cbSize    = sizeof(wc);
  wc.hInstance = hInst;

  wc.lpfnWndProc    = OverlayManager::WndProc;
  wc.hbrBackground  = nullptr;
  wc.lpszClassName  = kOverlayClass;
  wc.hCursor        = nullptr;
  RegisterClassExW(&wc);

  wc.style          = CS_DBLCLKS;
  wc.lpfnWndProc    = AnnotationManager::CanvasWndProc;
  wc.hbrBackground  = nullptr;
  wc.lpszClassName  = kCanvasClass;
  wc.hCursor        = LoadCursor(nullptr, IDC_CROSS);
  RegisterClassExW(&wc);
  wc.style          = 0;

  wc.lpfnWndProc    = AnnotationManager::ToolbarWndProc;
  wc.hbrBackground  = reinterpret_cast<HBRUSH>(
                          static_cast<INT_PTR>(COLOR_BTNFACE + 1));
  wc.lpszClassName  = kToolbarClass;
  wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);

  wc.lpfnWndProc    = AnnotationManager::TextDlgWndProc;
  wc.hbrBackground  = reinterpret_cast<HBRUSH>(
                          static_cast<INT_PTR>(COLOR_BTNFACE + 1));
  wc.lpszClassName  = kTextDlgClass;
  wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);

  wc.lpfnWndProc    = TrayManager::WndProc;
  wc.hbrBackground  = nullptr;
  wc.lpszClassName  = kTrayClass;
  wc.hCursor        = nullptr;
  RegisterClassExW(&wc);

  wc.lpfnWndProc    = SettingsDialog::WndProc;
  wc.hbrBackground  = reinterpret_cast<HBRUSH>(
                          static_cast<INT_PTR>(COLOR_BTNFACE + 1));
  wc.lpszClassName  = kSettingsClass;
  wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);

  wc.lpfnWndProc    = RecordingManager::RecCtrlWndProc;
  wc.hbrBackground  = reinterpret_cast<HBRUSH>(
                          static_cast<INT_PTR>(COLOR_BTNFACE + 1));
  wc.lpszClassName  = kRecCtrlClass;
  wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);

  wc.lpfnWndProc    = F1Toolbar::WndProc;
  wc.hbrBackground  = reinterpret_cast<HBRUSH>(
                          static_cast<INT_PTR>(COLOR_BTNFACE + 1));
  wc.lpszClassName  = kF1ToolbarClass;
  wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);

  wc.lpfnWndProc    = PinManager::PinBorderWndProc;
  wc.hbrBackground  = CreateSolidBrush(kConfirmColor);
  wc.lpszClassName  = kPinBorderClass;
  wc.hCursor        = nullptr;
  RegisterClassExW(&wc);

  wc.lpfnWndProc    = OverlayManager::DimWndProc;
  wc.hbrBackground  = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  wc.lpszClassName  = kRecDimClass;
  wc.hCursor        = nullptr;
  RegisterClassExW(&wc);

  wc.lpfnWndProc    = RecordingManager::RecSettingsWndProc;
  wc.hbrBackground  = reinterpret_cast<HBRUSH>(
                          static_cast<INT_PTR>(COLOR_BTNFACE + 1));
  wc.lpszClassName  = kRecSettingsClass;
  wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);

  wc.lpfnWndProc    = RecordingManager::CountdownWndProc;
  wc.hbrBackground  = nullptr;
  wc.lpszClassName  = kCountdownClass;
  wc.hCursor        = nullptr;
  RegisterClassExW(&wc);

  wc.lpfnWndProc    = RecordingManager::RecPreviewWndProc;
  wc.hbrBackground  = reinterpret_cast<HBRUSH>(
                          static_cast<INT_PTR>(COLOR_BTNFACE + 1));
  wc.lpszClassName  = kRecPreviewClass;
  wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);

  wc.lpfnWndProc    = AboutDialog::WndProc;
  wc.hbrBackground  = reinterpret_cast<HBRUSH>(
                          static_cast<INT_PTR>(COLOR_BTNFACE + 1));
  wc.lpszClassName  = kAboutClass;
  wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);

  wc.lpfnWndProc    = ColorPicker::WndProc;
  wc.hbrBackground  = nullptr;
  wc.lpszClassName  = L"PGColorPicker";
  wc.hCursor        = LoadCursor(nullptr, IDC_CROSS);
  RegisterClassExW(&wc);

  wc.lpfnWndProc    = AnnotationManager::ColorPanelWndProc;
  wc.hbrBackground  = nullptr;
  wc.lpszClassName  = kColorPanelClass;
  wc.hCursor        = LoadCursor(nullptr, IDC_HAND);
  RegisterClassExW(&wc);
}

void Application::unRegisterWindowClasses(HINSTANCE hInst) {
  UnregisterClassW(kOverlayClass,     hInst);
  UnregisterClassW(kCanvasClass,      hInst);
  UnregisterClassW(kToolbarClass,     hInst);
  UnregisterClassW(kTextDlgClass,     hInst);
  UnregisterClassW(kTrayClass,        hInst);
  UnregisterClassW(kSettingsClass,    hInst);
  UnregisterClassW(kRecCtrlClass,     hInst);
  UnregisterClassW(kF1ToolbarClass,   hInst);
  UnregisterClassW(kPinBorderClass,   hInst);
  UnregisterClassW(kRecDimClass,      hInst);
  UnregisterClassW(kRecSettingsClass, hInst);
  UnregisterClassW(kCountdownClass,   hInst);
  UnregisterClassW(kRecPreviewClass,  hInst);
  UnregisterClassW(kAboutClass,       hInst);
  UnregisterClassW(L"PGColorPicker",  hInst);
  UnregisterClassW(kColorPanelClass,  hInst);
}

bool Application::Init() {
  instance_mutex_ = CreateMutexW(nullptr, TRUE, L"PixelGrab_SingleInstance_Mutex");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    if (instance_mutex_) CloseHandle(instance_mutex_);
    instance_mutex_ = nullptr;
    MessageBoxW(nullptr, T(kStr_MsgAlreadyRunning),
        L"PixelGrab", MB_OK | MB_ICONINFORMATION);
    return false;
  }

  main_thread_ = GetCurrentThreadId();

  settings_.LoadLanguageSetting();

  std::printf("PixelGrab v%s -- Tray App\n", pixelgrab_version_string());

  ctx_ = pixelgrab_context_create();
  if (!ctx_) {
    std::fprintf(stderr, "FATAL: pixelgrab_context_create failed.\n");
    return false;
  }
  pixelgrab_enable_dpi_awareness(ctx_);
  pixelgrab_history_set_max_count(ctx_, 20);
  {
    wchar_t exe_dir[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe_dir, MAX_PATH);
    for (int i = static_cast<int>(wcslen(exe_dir)) - 1; i >= 0; --i) {
      if (exe_dir[i] == L'\\' || exe_dir[i] == L'/') {
        exe_dir[i + 1] = L'\0';
        break;
      }
    }
    wcscat_s(exe_dir, L"pixelgrab.cfg");

    char path_utf8[MAX_PATH * 3] = {};
    WideCharToMultiByte(CP_UTF8, 0, exe_dir, -1, path_utf8, sizeof(path_utf8),
                        nullptr, nullptr);

    std::string provider = "baidu", app_id, secret_key;
    std::ifstream cfg(path_utf8);
    if (cfg.is_open()) {
      std::string line;
      while (std::getline(cfg, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        while (!key.empty() && key.back() == ' ') key.pop_back();
        while (!val.empty() && val.front() == ' ') val.erase(val.begin());
        if (key == "provider")   provider = val;
        else if (key == "app_id")     app_id = val;
        else if (key == "secret_key") secret_key = val;
      }
    }

    if (!app_id.empty() && !secret_key.empty()) {
      pixelgrab_translate_set_config(ctx_, provider.c_str(),
                                     app_id.c_str(), secret_key.c_str());
    }
  }

  overlay_.RestoreSystemCursors();

  HINSTANCE hInst = GetModuleHandleW(nullptr);
  RegisterWindowClasses(hInst);

  overlay_.SetOverlay(CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
      WS_EX_LAYERED | WS_EX_TRANSPARENT,
      kOverlayClass, nullptr, WS_POPUP,
      0, 0, 0, 0,
      nullptr, nullptr, hInst, nullptr));

  menu_host_ = CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
      L"STATIC", nullptr, WS_POPUP,
      0, 0, 1, 1,
      nullptr, nullptr, hInst, nullptr);

  tray_.SetTrayHwnd(CreateWindowExW(
      0, kTrayClass, nullptr, 0,
      0, 0, 0, 0,
      HWND_MESSAGE, nullptr, hInst, nullptr));

  std::memset(&tray_.Nid(), 0, sizeof(NOTIFYICONDATAW));
  tray_.Nid().cbSize           = sizeof(NOTIFYICONDATAW);
  tray_.Nid().hWnd             = tray_.TrayHwnd();
  tray_.Nid().uID              = kTrayId;
  tray_.Nid().uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  tray_.Nid().uCallbackMessage = kTrayMsg;
  tray_.Nid().hIcon            = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));
  wcscpy_s(tray_.Nid().szTip, L"PixelGrab");
  Shell_NotifyIconW(NIM_ADD, &tray_.Nid());

  hotkey_ = CreatePlatformHotkey();
  settings_.LoadHotkeySettings();
  hotkey_->Register(kHotkeyF1, settings_.VkCapture());
  hotkey_->Register(kHotkeyF3, settings_.VkPin());

  std::printf("Ready. Capture=%ls, Pin=%ls\n",
              VKToFKeyName(settings_.VkCapture()),
              VKToFKeyName(settings_.VkPin()));

  about_.TriggerUpdateCheck(false);

  return true;
}

int Application::Run() {
  MSG msg;
  while (running_.load()) {
    for (int msg_n = 0;
         PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
         ++msg_n) {
      if (selection_.IsSelecting() && msg_n >= 8) break;
      if (msg.message == WM_QUIT) {
        running_.store(false);
        break;
      }

      switch (msg.message) {
        case WM_HOTKEY:
          if (msg.wParam == kHotkeyF1 &&
              !annotation_.IsAnnotating() &&
              !recording_.IsStandaloneRecording())
            f1_toolbar_.ShowMenu();
          if (msg.wParam == kHotkeyF3 && !annotation_.IsAnnotating())
            pins_.PinCapture();
          break;

        case static_cast<UINT>(kMsgLeftDown):
          if (selection_.IsSelecting()) {
            int mx = static_cast<int>(msg.wParam);
            int my = static_cast<int>(msg.lParam);
            selection_.SetSelectDragging(true);
            POINT sp = {mx, my};
            selection_.SetSelectStart(sp);
            if (color_picker_.PickerWnd())
              ShowWindow(color_picker_.PickerWnd(), SW_HIDE);
          }
          break;

        case static_cast<UINT>(kMsgLeftUp):
          if (selection_.IsSelecting()) {
            int ux = static_cast<int>(msg.wParam);
            int uy = static_cast<int>(msg.lParam);
            int ddx = IntAbs(ux - selection_.SelectStart().x);
            int ddy = IntAbs(uy - selection_.SelectStart().y);
            selection_.SetSelectDragging(false);
            if (ddx > 5 || ddy > 5) {
              selection_.HandleRegionSelect(
                  selection_.SelectStart().x, selection_.SelectStart().y,
                  ux, uy);
            } else {
              selection_.HandleClick(ux, uy);
            }
          }
          break;

        case static_cast<UINT>(kMsgRightClick):
          selection_.HandleCancel();
          break;

        case static_cast<UINT>(kMsgDoubleClick):
          pins_.HandleDoubleClick(static_cast<int>(msg.wParam),
                                    static_cast<int>(msg.lParam));
          break;

        case static_cast<UINT>(kMsgKeyEscape):
          if (selection_.IsSelecting())                   selection_.HandleCancel();
          else if (f1_toolbar_.Toolbar())                  f1_toolbar_.Dismiss();
          else if (annotation_.IsAnnotating())            annotation_.Cancel();
          else if (recording_.IsStandaloneRecording())   recording_.StopStandalone();
          else if (recording_.RecPreviewWnd())           recording_.DismissPreview();
          break;

        case static_cast<UINT>(kMsgCopyColor):
          color_picker_.CopyColor();
          break;

        case static_cast<UINT>(kMsgToggleColor):
          color_picker_.ToggleDisplay();
          break;

        default:
          break;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    if (!running_.load()) break;

    if (selection_.IsSelecting()) {
      POINT cursor;
      GetCursorPos(&cursor);
      if (selection_.IsSelectDragging()) {
        int rx = IntMin(selection_.SelectStart().x, static_cast<int>(cursor.x));
        int ry = IntMin(selection_.SelectStart().y, static_cast<int>(cursor.y));
        int rw = IntAbs(static_cast<int>(cursor.x) - selection_.SelectStart().x);
        int rh = IntAbs(static_cast<int>(cursor.y) - selection_.SelectStart().y);
        selection_.SetHighlightHwnd(nullptr);
        if (rw > kHighlightBorder * 2 || rh > kHighlightBorder * 2) {
          overlay_.ShowHighlight(rx, ry, rw, rh);
        } else {
          overlay_.HideHighlight();
        }
      } else {
        POINT lc = selection_.LastCursor();
        if (cursor.x != lc.x || cursor.y != lc.y) {
          selection_.SetLastCursor(cursor);
          selection_.HandleMove(static_cast<int>(cursor.x),
                                 static_cast<int>(cursor.y));
        }
      }
    }

    if (!selection_.IsSelecting() && (GetAsyncKeyState(VK_ESCAPE) & 0x0001)) {
      if (color_picker_.IsActive())                     color_picker_.Dismiss();
      else if (f1_toolbar_.Toolbar())                   f1_toolbar_.Dismiss();
      else if (annotation_.IsAnnotating())              annotation_.Cancel();
      else if (recording_.IsStandaloneRecording())     recording_.StopStandalone();
      else if (recording_.RecPreviewWnd())             recording_.DismissPreview();
    }

    if (!pins_.Pins().empty()) {
      pixelgrab_pin_process_events(ctx_);
    }

    pins_.SyncBorders();

    Sleep(selection_.IsSelecting() ? 1 : 16);
  }

  return 0;
}

void Application::Shutdown() {
  std::printf("\nExiting...\n");

  overlay_.RestoreSystemCursors();

  hotkey_->UnregisterAll();

  if (selection_.MouseHook()) {
    UnhookWindowsHookEx(selection_.MouseHook());
  }
  if (selection_.KbdHook()) {
    UnhookWindowsHookEx(selection_.KbdHook());
  }

  if (annotation_.IsAnnotating()) annotation_.Cleanup();
  if (recording_.IsStandaloneRecording()) recording_.StopStandalone();
  color_picker_.Dismiss();
  f1_toolbar_.Dismiss();
  recording_.DismissPreview();
  if (recording_.CountdownWnd()) {
    KillTimer(recording_.CountdownWnd(), kCountdownTimerId);
    DestroyWindow(recording_.CountdownWnd());
  }

  for (auto& e : pins_.Pins()) {
    pins_.HideBorderFor(e);
    if (e.pin) pixelgrab_pin_destroy(e.pin);
  }
  pins_.Pins().clear();
  if (captured_) {
    pixelgrab_image_destroy(captured_);
    captured_ = nullptr;
  }
  pixelgrab_pin_destroy_all(ctx_);

  Shell_NotifyIconW(NIM_DELETE, &tray_.Nid());
  if (tray_.TrayHwnd()) {
    DestroyWindow(tray_.TrayHwnd());
    tray_.SetTrayHwnd(nullptr);
  }

  if (menu_host_) {
    DestroyWindow(menu_host_);
    menu_host_ = nullptr;
  }
  if (overlay_.Overlay()) {
    DestroyWindow(overlay_.Overlay());
    overlay_.SetOverlay(nullptr);
  }

  HINSTANCE hInst = GetModuleHandleW(nullptr);
  unRegisterWindowClasses(hInst);

  pixelgrab_context_destroy(ctx_);
  ctx_ = nullptr;

  if (instance_mutex_) {
    ReleaseMutex(instance_mutex_);
    CloseHandle(instance_mutex_);
    instance_mutex_ = nullptr;
  }

  std::printf("Done.\n");
}

#endif  // _WIN32
