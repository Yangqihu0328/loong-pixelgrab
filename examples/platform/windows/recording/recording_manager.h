// Copyright 2026 The PixelGrab Authors
// Screen recording: standalone recording, recording border, settings, countdown, preview.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_RECORDING_RECORDING_MANAGER_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_RECORDING_RECORDING_MANAGER_H_

#ifdef _WIN32

#include "platform/windows/win_app_defs.h"

class RecordingManager {
 public:
  static LRESULT CALLBACK RecCtrlWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static LRESULT CALLBACK RecSettingsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static LRESULT CALLBACK CountdownWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static LRESULT CALLBACK RecPreviewWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  void StopRecording();
  void StartStandalone(RECT rc);
  void StopStandalone();
  void ShowRecBorder(RECT rc);
  void HideRecBorder();
  void ShowSettings(RECT rc);
  void ShowCountdown(RECT rc);
  void ShowPreview();
  void DismissPreview();
  void CopyFileToClipboard(const char* filepath);
  bool ApplyUserWatermark(PixelGrabImage* image);

  // Accessors
  PixelGrabRecorder* Recorder() const { return recorder_; }
  bool IsRecording() const { return recording_; }
  RECT RecordRect() const { return record_rect_; }
  PixelGrabRecorder* StandaloneRecorder() const { return standalone_recorder_; }
  bool IsStandaloneRecording() const { return standalone_recording_; }
  HWND RecCtrlWnd() const { return rec_ctrl_wnd_; }
  HWND RecCtrlLabel() const { return rec_ctrl_label_; }
  HWND RecBorder() const { return rec_border_; }
  HWND RecSettingsWnd() const { return rec_settings_wnd_; }
  HWND CountdownWnd() const { return countdown_wnd_; }
  HWND RecPreviewWnd() const { return rec_preview_wnd_; }
  bool RecAudioSpeaker() const { return rec_audio_speaker_; }
  void SetRecAudioSpeaker(bool v) { rec_audio_speaker_ = v; }
  bool RecAudioMic() const { return rec_audio_mic_; }
  void SetRecAudioMic(bool v) { rec_audio_mic_ = v; }
  bool RecUserWmEnabled() const { return rec_user_wm_enabled_; }
  void SetRecUserWmEnabled(bool v) { rec_user_wm_enabled_ = v; }
  const char* RecUserWmText() const { return rec_user_wm_text_; }
  char* RecUserWmTextBuf() { return rec_user_wm_text_; }
  int RecUserWmFontSize() const { return rec_user_wm_font_size_; }
  void SetRecUserWmFontSize(int v) { rec_user_wm_font_size_ = v; }
  int RecUserWmOpacity() const { return rec_user_wm_opacity_; }
  void SetRecUserWmOpacity(int v) { rec_user_wm_opacity_ = v; }
  const char* RecOutputPath() const { return rec_output_path_; }
  int64_t RecFinalDurationMs() const { return rec_final_duration_ms_; }

 private:
  PixelGrabRecorder* recorder_ = nullptr;
  bool               recording_ = false;
  RECT               record_rect_ = {};

  PixelGrabRecorder* standalone_recorder_ = nullptr;
  bool               standalone_recording_ = false;
  HWND               rec_ctrl_wnd_ = nullptr;
  HWND               rec_ctrl_label_ = nullptr;
  HWND               rec_ctrl_stop_btn_ = nullptr;
  HWND               rec_ctrl_pause_btn_ = nullptr;
  ULONGLONG          standalone_rec_start_ = 0;
  RECT               standalone_rec_rect_ = {};

  HWND rec_border_ = nullptr;

  bool    rec_audio_speaker_ = false;
  bool    rec_audio_mic_ = false;
  char    rec_audio_device_id_[256] = {};
  bool    rec_user_wm_enabled_ = false;
  char    rec_user_wm_text_[256] = {};
  int     rec_user_wm_font_size_ = 14;
  int     rec_user_wm_opacity_ = 75;
  char    rec_output_path_[MAX_PATH] = {};
  int64_t rec_final_duration_ms_ = 0;

  bool  rec_settings_done_ = false;
  bool  rec_settings_ok_ = false;
  HWND  rec_settings_wnd_ = nullptr;
  RECT  rec_pending_rect_ = {};

  HWND  countdown_wnd_ = nullptr;
  int   countdown_value_ = 3;
  RECT  countdown_rec_rect_ = {};

  HWND  rec_preview_wnd_ = nullptr;
};

#endif  // _WIN32
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_RECORDING_RECORDING_MANAGER_H_
