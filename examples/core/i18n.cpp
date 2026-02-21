// Copyright 2026 The PixelGrab Authors
//
// Internationalization (i18n) — string tables and language switching.

#include "core/i18n.h"

#include <cassert>

static Language g_lang = kLangZhCN;

// ===================================================================
// String tables — kStrings[language][string_id]
//
// IMPORTANT: The order of entries MUST exactly match the StringId enum.
// Add a comment with the enum name for each entry to aid maintenance.
// ===================================================================

static const wchar_t* kStrings[kLangCount][kStringCount] = {

// -----------------------------------------------------------------
// [kLangZhCN] Simplified Chinese
// -----------------------------------------------------------------
{
  // -- Annotation toolbar buttons --
  L"\x77E9\x5F62",                  // kStr_ToolRect:     矩形
  L"\x692D\x5706",                  // kStr_ToolEllipse:  椭圆
  L"\x7BAD\x5934",                  // kStr_ToolArrow:    箭头
  L"\x7EBF\x6761",                  // kStr_ToolLine:     线条
  L"\x6587\x5B57",                  // kStr_ToolText:     文字
  L"\x9A6C\x8D5B\x514B",           // kStr_ToolMosaic:   马赛克
  L"\x6A21\x7CCA",                  // kStr_ToolBlur:     模糊
  L"\x64A4\x9500",                  // kStr_ToolUndo:     撤销
  L"\x91CD\x505A",                  // kStr_ToolRedo:     重做
  L"\x8D34\x56FE",                  // kStr_ToolPin:      贴图
  L"\x590D\x5236",                  // kStr_ToolCopy:     复制
  L"\x53D6\x6D88",                  // kStr_ToolCancel:   取消
  L"\x989C\x8272",                  // kStr_ToolColor:    颜色

  // -- Annotation property buttons --
  L"\x7EC6",                        // kStr_WidthThin:    细
  L"\x4E2D",                        // kStr_WidthMed:     中
  L"\x7C97",                        // kStr_WidthThick:   粗
  L"\x5C0F",                        // kStr_FontSmall:    小
  L"\x4E2D",                        // kStr_FontMed:      中
  L"\x5927",                        // kStr_FontLarge:    大

  // -- F1 toolbar --
  L"\x622A\x56FE",                  // kStr_F1Capture:    截图
  L"\x5F55\x5C4F",                  // kStr_F1Record:     录屏

  // -- Window titles --
  L"\x5F55\x5C4F\x8BBE\x7F6E",     // kStr_TitleRecSettings:    录屏设置
  L"\x5F55\x5236\x5B8C\x6210",     // kStr_TitleRecComplete:    录制完成
  L"\x8F93\x5165\x6587\x5B57",     // kStr_TitleTextInput:      输入文字
  L"\x5173\x4E8E PixelGrab",       // kStr_TitleAbout:          关于 PixelGrab
  L"\x70ED\x952E\x8BBE\x7F6E",     // kStr_TitleHotkeySettings: 热键设置
  L"PixelGrab \x66F4\x65B0",       // kStr_TitleUpdate:         PixelGrab 更新

  // -- Dialog buttons --
  L"\x505C\x6B62",                  // kStr_BtnStop:          停止
  L"\x5F00\x59CB\x5F55\x5236",     // kStr_BtnStartRecord:   开始录制
  L"\x64AD\x653E\x9884\x89C8",     // kStr_BtnPlayPreview:   播放预览
  L"\x590D\x5236\x5230\x526A\x8D34\x677F", // kStr_BtnCopyClipboard: 复制到剪贴板
  L"\x5B8C\x6210",                  // kStr_BtnDone:          完成
  L"\x68C0\x67E5\x66F4\x65B0",     // kStr_BtnCheckUpdate:   检查更新
  L"\x5173\x95ED",                  // kStr_BtnClose:         关闭
  L"\x786E\x5B9A",                  // kStr_BtnOK:            确定

  // -- Recording settings labels --
  L" \x97F3\x9891 ",               // kStr_LabelAudio:           音频
  L"\x626C\x58F0\x5668"            // kStr_LabelSpeakerSystem:   扬声器 (系统声音)
    L" (\x7CFB\x7EDF\x58F0\x97F3)",
  L"\x9EA6\x514B\x98CE",           // kStr_LabelMicrophone:      麦克风
  L" \x6C34\x5370 ",               // kStr_LabelWatermark:       水印
  L"\x542F\x7528",                  // kStr_LabelEnable:          启用
  L"\x8F93\x5165\x6C34\x5370\x6587\x5B57", // kStr_PlaceholderWatermark: 输入水印文字
  L"\x6587\x5B57\x5C06\x659C\x5411\x6162\x901F\x98D8\x8FC7"  // kStr_HintWatermarkDesc
    L"\x5F55\x5C4F\x753B\x9762"
    L"\xFF0C\x6700\x591A\x540C\x65F6\x663E\x793A"
    L" 5 \x4E2A",                   // 文字将斜向慢速飘过录屏画面，最多同时显示 5 个
  L"\x5B57\x53F7",                  // kStr_LabelFontSize:       字号
  L"\x900F\x660E\x5EA6",            // kStr_LabelOpacity:        透明度

  // -- Recording preview (format strings) --
  L"\x5F55\x5236\x65F6\x957F: %02d:%02d",  // kStr_FmtDuration:    录制时长: MM:SS
  L"\x6587\x4EF6: %s",                      // kStr_FmtFile:        文件: %s
  L"\x683C\x5F0F: MP4  |  \x97F3\x9891: %s", // kStr_FmtFormatAudio: 格式: MP4 | 音频: %s

  // -- Audio source names --
  L"\x626C\x58F0\x5668+\x9EA6\x514B\x98CE", // kStr_AudioSpeakerMic: 扬声器+麦克风
  L"\x626C\x58F0\x5668",            // kStr_AudioSpeaker:    扬声器
  L"\x9EA6\x514B\x98CE",            // kStr_AudioMic:        麦克风
  L"\x65E0",                         // kStr_AudioNone:       无

  // -- MessageBox messages --
  L"\x521B\x5EFA\x5F55\x5236\x5668\x5931\x8D25",      // kStr_MsgCreateRecorderFailed: 创建录制器失败
  L"\x5F55\x5236\x542F\x52A8\x5931\x8D25",             // kStr_MsgStartRecordFailed:    录制启动失败
  L"\x5DF2\x590D\x5236\x5230\x526A\x8D34\x677F",       // kStr_MsgCopiedClipboard:      已复制到剪贴板
  L"OCR \x529F\x80FD\x5F00\x53D1\x4E2D...",            // kStr_MsgOCRDeveloping:        OCR 功能开发中...
  L"PixelGrab \x5DF2\x7ECF\x5728\x8FD0\x884C\x4E2D\x3002", // kStr_MsgAlreadyRunning: PixelGrab 已经在运行中。
  L"\x53D1\x73B0\x65B0\x7248\x672C v%s\xFF01\n\n"      // kStr_MsgNewVersion:
    L"\x70B9\x51FB\x201C\x662F\x201D\x6253\x5F00"      //   发现新版本 v%s！
    L"\x4E0B\x8F7D\x9875\x9762\x3002",                  //   点击"是"打开下载页面。
  L"\x5F53\x524D\x5DF2\x662F\x6700\x65B0\x7248\x672C\x3002", // kStr_MsgUpToDate: 当前已是最新版本。

  // -- Hotkey settings --
  L"\x622A\x56FE\x70ED\x952E:",     // kStr_LabelCaptureHotkey:  截图热键:
  L"\x8D34\x56FE\x70ED\x952E:",     // kStr_LabelPinHotkey:      贴图热键:
  L"\x622A\x56FE\x548C\x8D34\x56FE\x70ED\x952E\x4E0D\x80FD"  // kStr_MsgHotkeyConflict
    L"\x76F8\x540C\xFF01",           // 截图和贴图热键不能相同！
  L"\x63D0\x793A",                   // kStr_MsgHint:             提示

  // -- System tray menu --
  L"\x622A\x56FE (&S)\t%s",         // kStr_TrayCapture:     截图 (&S)\t%s
  L"\x8D34\x56FE (&P)\t%s",         // kStr_TrayPin:         贴图 (&P)\t%s
  L"\x7C98\x8D34\x526A\x8D34\x677F (&V)", // kStr_TrayPasteClip:  粘贴剪贴板 (&V)
  L"\x8BBE\x7F6E (&T)...",          // kStr_TraySettings:    设置 (&T)...
  L"\x5F00\x673A\x81EA\x542F (&A)", // kStr_TrayAutoStart:   开机自启 (&A)
  L"\x5173\x4E8E (&I)",             // kStr_TrayAbout:       关于 (&I)
  L"\x9000\x51FA (&X)",             // kStr_TrayExit:        退出 (&X)
  L"\x5386\x53F2\x8BB0\x5F55",      // kStr_TrayHistory:     历史记录
  L"\x6E05\x9664\x5386\x53F2",      // kStr_TrayClearHistory: 清除历史

  // -- About dialog --
  L"\x622A\x56FE / \x6807\x6CE8 / \x8D34\x56FE / "     // kStr_AboutDesc
    L"\x5F55\x5C4F \x5DE5\x5177",   // 截图 / 标注 / 贴图 / 录屏 工具
  L"\x4E2A\x4EBA\x5FAE\x4FE1",      // kStr_AboutQRWechat:     个人微信
  L"\x5FAE\x4FE1\x6536\x6B3E\x7801",// kStr_AboutQRWechatPay:  微信收款码
  L"\x652F\x4ED8\x5B9D\x6536\x6B3E\x7801",// kStr_AboutQRAlipayPay: 支付宝收款码
  L"\x672A\x627E\x5230\x56FE\x7247", // kStr_AboutImageNotFound: 未找到图片

  // -- Watermark branding --
  L"PixelGrab - "                    // kStr_WatermarkBranding
    L"\x514D\x8D39\x622A\x56FE\x5F55\x5C4F\x5DE5\x5177", // 免费截图录屏工具

  // -- Language menu --
  L"\x8BED\x8A00 (&L)",             // kStr_TrayLanguage:    语言 (&L)
  L"\x4E2D\x6587",                  // kStr_LangChinese:     中文
  L"English",                        // kStr_LangEnglish:     English

  // -- Color picker --
  L"\x5750\x6807\uFF1A%d, %d",       // kStr_PkCoordFmt:  坐标：%d, %d
  L"RGB\uFF1A%d, %d, %d",            // kStr_PkRGBFmt:    RGB：%d, %d, %d
  L"HEX\uFF1A%s",                    // kStr_PkHEXFmt:    HEX：%s
  L"Ctrl+C \x590D\x5236 | Shift \x5207\x6362",  // kStr_PkHint: Ctrl+C 复制 | Shift 切换

  // -- OCR --
  L"\x672A\x8BC6\x522B\x5230\x6587\x5B57",       // kStr_MsgOCRNoText:  未识别到文字
  L"OCR \x8BC6\x522B\x5931\x8D25",                // kStr_MsgOCRFailed:  OCR 识别失败
  L"\x8BC6\x522B\x7ED3\x679C\x5DF2\x590D\x5236"  // kStr_MsgOCRCopied:  识别结果已复制到剪贴板
    L"\x5230\x526A\x8D34\x677F",

  // -- Translation --
  L"\x7FFB\x8BD1",                                 // kStr_BtnTranslate:              翻译
  L"\x6B63\x5728\x7FFB\x8BD1...",                  // kStr_MsgTranslating:            正在翻译...
  L"\x7FFB\x8BD1\x5931\x8D25",                     // kStr_MsgTranslateFailed:        翻译失败
  L"\x7FFB\x8BD1\x672A\x914D\x7F6E\xFF0C"          // kStr_MsgTranslateNotConfigured: 翻译未配置，
    L"\x8BF7\x5728\x8BBE\x7F6E\x4E2D\x586B\x5199"  //   请在设置中填写
    L"\x767E\x5EA6\x7FFB\x8BD1 API \x5BC6\x94A5",   //   百度翻译 API 密钥
},

// -----------------------------------------------------------------
// [kLangEnUS] English
// -----------------------------------------------------------------
{
  // -- Annotation toolbar buttons --
  L"Rect",             // kStr_ToolRect
  L"Ellipse",          // kStr_ToolEllipse
  L"Arrow",            // kStr_ToolArrow
  L"Line",             // kStr_ToolLine
  L"Text",             // kStr_ToolText
  L"Mosaic",           // kStr_ToolMosaic
  L"Blur",             // kStr_ToolBlur
  L"Undo",             // kStr_ToolUndo
  L"Redo",             // kStr_ToolRedo
  L"Pin",              // kStr_ToolPin
  L"Copy",             // kStr_ToolCopy
  L"Cancel",           // kStr_ToolCancel
  L"Color",            // kStr_ToolColor

  // -- Annotation property buttons --
  L"S",                // kStr_WidthThin
  L"M",                // kStr_WidthMed
  L"L",                // kStr_WidthThick
  L"S",                // kStr_FontSmall
  L"M",                // kStr_FontMed
  L"L",                // kStr_FontLarge

  // -- F1 toolbar --
  L"Capture",          // kStr_F1Capture
  L"Record",           // kStr_F1Record

  // -- Window titles --
  L"Recording Settings",    // kStr_TitleRecSettings
  L"Recording Complete",    // kStr_TitleRecComplete
  L"Enter Text",            // kStr_TitleTextInput
  L"About PixelGrab",       // kStr_TitleAbout
  L"Hotkey Settings",       // kStr_TitleHotkeySettings
  L"PixelGrab Update",      // kStr_TitleUpdate

  // -- Dialog buttons --
  L"Stop",                   // kStr_BtnStop
  L"Start Recording",       // kStr_BtnStartRecord
  L"Play Preview",          // kStr_BtnPlayPreview
  L"Copy to Clipboard",     // kStr_BtnCopyClipboard
  L"Done",                   // kStr_BtnDone
  L"Check Update",          // kStr_BtnCheckUpdate
  L"Close",                  // kStr_BtnClose
  L"OK",                     // kStr_BtnOK

  // -- Recording settings labels --
  L" Audio ",                // kStr_LabelAudio
  L"Speaker (System Sound)", // kStr_LabelSpeakerSystem
  L"Microphone",             // kStr_LabelMicrophone
  L" Watermark ",            // kStr_LabelWatermark
  L"Enable",                 // kStr_LabelEnable
  L"Enter watermark text",   // kStr_PlaceholderWatermark
  L"Text will slowly drift across the recording, "  // kStr_HintWatermarkDesc
    L"max 5 at once",
  L"Size",                         // kStr_LabelFontSize
  L"Opacity",                      // kStr_LabelOpacity

  // -- Recording preview (format strings) --
  L"Duration: %02d:%02d",         // kStr_FmtDuration
  L"File: %s",                     // kStr_FmtFile
  L"Format: MP4  |  Audio: %s",   // kStr_FmtFormatAudio

  // -- Audio source names --
  L"Speaker+Mic",       // kStr_AudioSpeakerMic
  L"Speaker",           // kStr_AudioSpeaker
  L"Microphone",        // kStr_AudioMic
  L"None",              // kStr_AudioNone

  // -- MessageBox messages --
  L"Failed to create recorder",    // kStr_MsgCreateRecorderFailed
  L"Failed to start recording",    // kStr_MsgStartRecordFailed
  L"Copied to clipboard",          // kStr_MsgCopiedClipboard
  L"OCR is under development...",  // kStr_MsgOCRDeveloping
  L"PixelGrab is already running.",// kStr_MsgAlreadyRunning
  L"New version v%s found!\n\n"    // kStr_MsgNewVersion
    L"Click \"Yes\" to open the download page.",
  L"You are already up to date.",  // kStr_MsgUpToDate

  // -- Hotkey settings --
  L"Capture hotkey:",              // kStr_LabelCaptureHotkey
  L"Pin hotkey:",                  // kStr_LabelPinHotkey
  L"Capture and Pin hotkeys cannot be the same!",  // kStr_MsgHotkeyConflict
  L"Hint",                         // kStr_MsgHint

  // -- System tray menu --
  L"Capture (&S)\t%s",        // kStr_TrayCapture
  L"Pin (&P)\t%s",            // kStr_TrayPin
  L"Paste Clipboard (&V)",    // kStr_TrayPasteClip
  L"Settings (&T)...",        // kStr_TraySettings
  L"Auto Start (&A)",         // kStr_TrayAutoStart
  L"About (&I)",              // kStr_TrayAbout
  L"Exit (&X)",               // kStr_TrayExit
  L"History",                  // kStr_TrayHistory
  L"Clear History",            // kStr_TrayClearHistory

  // -- About dialog --
  L"Screenshot / Annotation / Pin / Record Tool",  // kStr_AboutDesc
  L"WeChat",                 // kStr_AboutQRWechat
  L"WeChat Pay",             // kStr_AboutQRWechatPay
  L"Alipay Pay",             // kStr_AboutQRAlipayPay
  L"Image not found",        // kStr_AboutImageNotFound

  // -- Watermark branding --
  L"PixelGrab - Free Screenshot & Recording Tool",  // kStr_WatermarkBranding

  // -- Language menu --
  L"Language (&L)",          // kStr_TrayLanguage
  L"\x4E2D\x6587",          // kStr_LangChinese: 中文 (always shown in Chinese)
  L"English",                // kStr_LangEnglish

  // -- Color picker --
  L"Pos: %d, %d",           // kStr_PkCoordFmt
  L"RGB: %d, %d, %d",       // kStr_PkRGBFmt
  L"HEX: %s",               // kStr_PkHEXFmt
  L"Ctrl+C Copy | Shift Toggle",  // kStr_PkHint

  // -- OCR --
  L"No text recognized",           // kStr_MsgOCRNoText
  L"OCR recognition failed",       // kStr_MsgOCRFailed
  L"OCR result copied to clipboard",  // kStr_MsgOCRCopied

  // -- Translation --
  L"Translate",                    // kStr_BtnTranslate
  L"Translating...",               // kStr_MsgTranslating
  L"Translation failed",           // kStr_MsgTranslateFailed
  L"Translation not configured. Please set Baidu Translate API keys in Settings.",  // kStr_MsgTranslateNotConfigured
},

};  // end kStrings

// Compile-time verification: ensure array dimensions match enums.
static_assert(sizeof(kStrings) / sizeof(kStrings[0]) == kLangCount,
              "kStrings language count != kLangCount");
static_assert(sizeof(kStrings[0]) / sizeof(kStrings[0][0]) == kStringCount,
              "kStrings string count != kStringCount");

// ===================================================================
// API implementation
// ===================================================================

const wchar_t* T(StringId id) {
  assert(id >= 0 && id < kStringCount);
  if (id < 0 || id >= kStringCount) return L"";
  return kStrings[g_lang][id];
}

void SetLanguage(Language lang) {
  if (lang >= 0 && lang < kLangCount) g_lang = lang;
}

Language GetLanguage() {
  return g_lang;
}

// ===================================================================
// Platform-specific: DetectSystemLanguage()
// ===================================================================

// DetectSystemLanguage() is implemented per-platform:
//   Windows: here (uses GetUserDefaultUILanguage)
//   macOS:   platform/macos/mac_i18n.mm (uses NSLocale)
//   Linux:   platform/linux/linux_i18n.cpp (uses LANG env)

#ifdef _WIN32

#include <windows.h>

Language DetectSystemLanguage() {
  LANGID lid = GetUserDefaultUILanguage();
  WORD primary = PRIMARYLANGID(lid);
  if (primary == LANG_CHINESE) return kLangZhCN;
  return kLangEnUS;
}

#endif  // _WIN32
