// Copyright 2026 The PixelGrab Authors
//
// Internationalization (i18n) — StringId enum, Language enum, and API.

#ifndef PIXELGRAB_EXAMPLES_CORE_I18N_H_
#define PIXELGRAB_EXAMPLES_CORE_I18N_H_

enum StringId {
  // Annotation toolbar buttons
  kStr_ToolRect = 0,
  kStr_ToolEllipse,
  kStr_ToolArrow,
  kStr_ToolLine,
  kStr_ToolText,
  kStr_ToolMosaic,
  kStr_ToolBlur,
  kStr_ToolUndo,
  kStr_ToolRedo,
  kStr_ToolPin,
  kStr_ToolCopy,
  kStr_ToolCancel,
  kStr_ToolColor,

  // Annotation property buttons
  kStr_WidthThin,
  kStr_WidthMed,
  kStr_WidthThick,
  kStr_FontSmall,
  kStr_FontMed,
  kStr_FontLarge,

  // F1 toolbar
  kStr_F1Capture,
  kStr_F1Record,

  // Window titles
  kStr_TitleRecSettings,
  kStr_TitleRecComplete,
  kStr_TitleTextInput,
  kStr_TitleAbout,
  kStr_TitleHotkeySettings,
  kStr_TitleUpdate,

  // Dialog buttons
  kStr_BtnStop,
  kStr_BtnStartRecord,
  kStr_BtnPlayPreview,
  kStr_BtnCopyClipboard,
  kStr_BtnDone,
  kStr_BtnCheckUpdate,
  kStr_BtnClose,
  kStr_BtnOK,

  // Recording settings labels
  kStr_LabelAudio,
  kStr_LabelSpeakerSystem,
  kStr_LabelMicrophone,
  kStr_LabelWatermark,
  kStr_LabelEnable,
  kStr_PlaceholderWatermark,
  kStr_HintWatermarkDesc,
  kStr_LabelFontSize,
  kStr_LabelOpacity,

  // Recording preview (format strings)
  kStr_FmtDuration,
  kStr_FmtFile,
  kStr_FmtFormatAudio,

  // Audio source names
  kStr_AudioSpeakerMic,
  kStr_AudioSpeaker,
  kStr_AudioMic,
  kStr_AudioNone,

  // MessageBox messages
  kStr_MsgCreateRecorderFailed,
  kStr_MsgStartRecordFailed,
  kStr_MsgCopiedClipboard,
  kStr_MsgOCRDeveloping,
  kStr_MsgAlreadyRunning,
  kStr_MsgNewVersion,
  kStr_MsgUpToDate,

  // Hotkey settings
  kStr_LabelCaptureHotkey,
  kStr_LabelPinHotkey,
  kStr_MsgHotkeyConflict,
  kStr_MsgHint,

  // System tray menu
  kStr_TrayCapture,
  kStr_TrayPin,
  kStr_TrayPasteClip,
  kStr_TraySettings,
  kStr_TrayAutoStart,
  kStr_TrayAbout,
  kStr_TrayExit,
  kStr_TrayHistory,
  kStr_TrayClearHistory,

  // About dialog
  kStr_AboutDesc,
  kStr_AboutQRWechat,
  kStr_AboutQRWechatPay,
  kStr_AboutQRAlipayPay,
  kStr_AboutImageNotFound,

  // Watermark branding
  kStr_WatermarkBranding,

  // Language menu
  kStr_TrayLanguage,
  kStr_LangChinese,
  kStr_LangEnglish,

  // Color picker
  kStr_PkCoordFmt,
  kStr_PkRGBFmt,
  kStr_PkHEXFmt,
  kStr_PkHint,

  // OCR
  kStr_MsgOCRNoText,
  kStr_MsgOCRFailed,
  kStr_MsgOCRCopied,

  // Translation
  kStr_BtnTranslate,
  kStr_MsgTranslating,
  kStr_MsgTranslateFailed,
  kStr_MsgTranslateNotConfigured,

  kStringCount  // sentinel — must be last
};

enum Language {
  kLangZhCN = 0,
  kLangEnUS,
  kLangCount  // sentinel
};

/// Get localized string for the current language.
const wchar_t* T(StringId id);

/// Set the active language.
void SetLanguage(Language lang);

/// Get the active language.
Language GetLanguage();

/// Detect the system UI language and return the closest match.
Language DetectSystemLanguage();

#endif  // PIXELGRAB_EXAMPLES_CORE_I18N_H_
