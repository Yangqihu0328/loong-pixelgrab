// Copyright 2026 The PixelGrab Authors
// macOS implementation of DetectSystemLanguage using NSLocale.

#ifdef __APPLE__

#import <Foundation/Foundation.h>
#include "core/i18n.h"

Language DetectSystemLanguage() {
  @autoreleasepool {
    NSArray* langs = [NSLocale preferredLanguages];
    if ([langs count] > 0) {
      NSString* lang = langs[0];
      if ([lang hasPrefix:@"zh"]) {
        return kLangZhCN;
      }
    }
    return kLangEnUS;
  }
}

#endif  // __APPLE__
