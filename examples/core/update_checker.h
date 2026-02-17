// Copyright 2026 The PixelGrab Authors
//
// Update checker — queries GitHub Releases API for new versions.
// Cross-platform: uses IPlatformHttp for the actual HTTP request.

#ifndef PIXELGRAB_EXAMPLES_CORE_UPDATE_CHECKER_H_
#define PIXELGRAB_EXAMPLES_CORE_UPDATE_CHECKER_H_

#include <functional>

// Result of an update check.
struct UpdateInfo {
  bool     available;
  char     latest_version[32];
  char     download_url[512];
  char     release_notes[2048];
};

using UpdateCallback = std::function<void(const UpdateInfo& info)>;

/// Start an asynchronous update check.
/// On Windows the callback is dispatched via a hidden message window, so the
/// caller must have a message loop running.
void StartUpdateCheckAsync(const char* github_repo,
                           const char* current_version,
                           UpdateCallback cb);

/// Open the given URL in the system default browser.
void OpenUrlInBrowser(const char* url);

#endif  // PIXELGRAB_EXAMPLES_CORE_UPDATE_CHECKER_H_
