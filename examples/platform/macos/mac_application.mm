// Copyright 2026 The PixelGrab Authors
// macOS application implementation — NSStatusItem menu bar app.

#include "platform/macos/mac_application.h"

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#include <cstdio>

// ========================================================================
// AppDelegate — handles NSApplication lifecycle and status item menu
// ========================================================================

@interface PGAppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) NSStatusItem* statusItem;
@property (nonatomic, strong) NSMenu* statusMenu;
@end

@implementation PGAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  (void)notification;

  // Create status bar item
  self.statusItem = [[NSStatusBar systemStatusBar]
      statusItemWithLength:NSVariableStatusItemLength];
  self.statusItem.button.title = @"PG";
  self.statusItem.button.toolTip = @"PixelGrab";

  // Build menu
  self.statusMenu = [[NSMenu alloc] initWithTitle:@"PixelGrab"];

  NSMenuItem* captureItem =
      [[NSMenuItem alloc] initWithTitle:@"Capture (F1)"
                                 action:@selector(onCapture:)
                          keyEquivalent:@""];
  captureItem.target = self;
  [self.statusMenu addItem:captureItem];

  [self.statusMenu addItem:[NSMenuItem separatorItem]];

  NSMenuItem* aboutItem =
      [[NSMenuItem alloc] initWithTitle:@"About"
                                 action:@selector(onAbout:)
                          keyEquivalent:@""];
  aboutItem.target = self;
  [self.statusMenu addItem:aboutItem];

  NSMenuItem* quitItem =
      [[NSMenuItem alloc] initWithTitle:@"Quit"
                                 action:@selector(onQuit:)
                          keyEquivalent:@"q"];
  quitItem.target = self;
  [self.statusMenu addItem:quitItem];

  self.statusItem.menu = self.statusMenu;

  std::printf("PixelGrab v%s -- macOS\n", pixelgrab_version_string());
  std::printf("Ready. Use status bar menu or hotkey to capture.\n");
}

- (void)onCapture:(id)sender {
  (void)sender;
  auto& app = MacApplication::instance();
  if (!app.GetColorPicker().IsActive())
    app.GetColorPicker().Show();
}

- (void)onAbout:(id)sender {
  (void)sender;
  NSAlert* alert = [[NSAlert alloc] init];
  alert.messageText = @"PixelGrab";
  alert.informativeText = [NSString stringWithFormat:
      @"Version %s\nCross-platform screenshot tool.",
      pixelgrab_version_string()];
  [alert runModal];
}

- (void)onQuit:(id)sender {
  (void)sender;
  MacApplication::instance().GetColorPicker().Dismiss();
  [NSApp terminate:nil];
}

@end

// ========================================================================
// MacApplication singleton
// ========================================================================

MacApplication& MacApplication::instance() {
  static MacApplication app;
  return app;
}

bool MacApplication::Init() {
  ctx_ = pixelgrab_context_create();
  if (!ctx_) {
    std::fprintf(stderr, "FATAL: pixelgrab_context_create failed.\n");
    return false;
  }

  settings_ = CreatePlatformSettings();
  hotkey_   = CreatePlatformHotkey();
  http_     = CreatePlatformHttp();

  // Load language setting
  int lang_val = 0;
  if (settings_->GetInt("Language", &lang_val)) {
    if (lang_val >= 0 && lang_val < static_cast<int>(kLangCount))
      SetLanguage(static_cast<Language>(lang_val));
    else
      SetLanguage(DetectSystemLanguage());
  } else {
    SetLanguage(DetectSystemLanguage());
  }

  return true;
}

int MacApplication::Run() {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

    PGAppDelegate* delegate = [[PGAppDelegate alloc] init];
    [NSApp setDelegate:delegate];

    [NSApp run];
  }
  return 0;
}

void MacApplication::Shutdown() {
  color_picker_.Dismiss();

  if (hotkey_) hotkey_->UnregisterAll();

  if (ctx_) {
    pixelgrab_context_destroy(ctx_);
    ctx_ = nullptr;
  }
  std::printf("\nExiting...\n");
}

#endif  // __APPLE__
