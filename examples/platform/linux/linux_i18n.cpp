// Copyright 2026 The PixelGrab Authors
// Linux implementation of DetectSystemLanguage using LANG/LC_ALL environment.

#include "core/i18n.h"

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <cstdlib>
#include <cstring>

Language DetectSystemLanguage() {
  // Check LC_ALL > LC_MESSAGES > LANG
  const char* lang = std::getenv("LC_ALL");
  if (!lang || !lang[0]) lang = std::getenv("LC_MESSAGES");
  if (!lang || !lang[0]) lang = std::getenv("LANG");
  if (!lang || !lang[0]) return kLangEnUS;

  // Check for Chinese locale (zh_CN, zh_TW, zh.UTF-8, etc.)
  if (std::strncmp(lang, "zh", 2) == 0)
    return kLangZhCN;

  return kLangEnUS;
}

#endif  // __linux__
