// Copyright 2026 The PixelGrab Authors
//
// Application singleton — owns all manager instances and core state.
// Windows-specific implementation using Win32 API.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_APPLICATION_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_APPLICATION_H_

#ifdef _WIN32

#include "platform/windows/win_app_defs.h"
#include "core/platform_hotkey.h"
#include "core/update_checker.h"
#include "platform/windows/capture/overlay_manager.h"
#include "platform/windows/capture/selection_manager.h"
#include "platform/windows/capture/annotation_manager.h"
#include "platform/windows/capture/pin_manager.h"
#include "platform/windows/capture/f1_toolbar.h"
#include "platform/windows/capture/color_picker.h"
#include "platform/windows/recording/recording_manager.h"
#include "platform/windows/tray/tray_manager.h"
#include "platform/windows/tray/settings_dialog.h"
#include "platform/windows/tray/about_dialog.h"

class Application {
 public:
  static Application& instance();

  // Lifecycle
  bool Init();
  int  Run();
  void Shutdown();

  // Core state accessors
  PixelGrabContext* Ctx() const { return ctx_; }
  PixelGrabImage* Captured() const { return captured_; }
  void SetCaptured(PixelGrabImage* img) { captured_ = img; }
  bool IsRunning() const { return running_.load(); }
  void Quit() { running_.store(false); }
  DWORD MainThread() const { return main_thread_; }
  HWND MenuHost() const { return menu_host_; }
  SelectPurpose GetSelectPurpose() const { return select_purpose_; }
  void SetSelectPurpose(SelectPurpose p) { select_purpose_ = p; }

  // Manager accessors
  OverlayManager&      Overlay()      { return overlay_; }
  SelectionManager&    Selection()    { return selection_; }
  AnnotationManager&   Annotation()   { return annotation_; }
  PinManager&          Pins()         { return pins_; }
  RecordingManager&    Recording()    { return recording_; }
  TrayManager&         Tray()         { return tray_; }
  SettingsDialog&      Settings()     { return settings_; }
  AboutDialog&         About()        { return about_; }
  F1Toolbar&           GetF1Toolbar()   { return f1_toolbar_; }
  ColorPicker&         GetColorPicker() { return color_picker_; }
  IPlatformHotkey&     Hotkey()       { return *hotkey_; }

 private:
  Application() = default;
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  void RegisterWindowClasses(HINSTANCE hInst);
  void unRegisterWindowClasses(HINSTANCE hInst);

  // Core state
  PixelGrabContext*    ctx_ = nullptr;
  PixelGrabImage*     captured_ = nullptr;
  std::atomic<bool>   running_{true};
  DWORD               main_thread_ = 0;
  HANDLE              instance_mutex_ = nullptr;
  HWND                menu_host_ = nullptr;
  SelectPurpose       select_purpose_ = kSelectForCapture;

  // Platform abstractions
  std::unique_ptr<IPlatformHotkey> hotkey_;

  // Manager instances
  OverlayManager      overlay_;
  SelectionManager    selection_;
  AnnotationManager   annotation_;
  PinManager          pins_;
  F1Toolbar           f1_toolbar_;
  ColorPicker         color_picker_;
  RecordingManager    recording_;
  TrayManager         tray_;
  SettingsDialog      settings_;
  AboutDialog         about_;
};

#endif  // _WIN32
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_WIN_APPLICATION_H_
