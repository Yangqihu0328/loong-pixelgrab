// Copyright 2026 The PixelGrab Authors
//
// Update checker implementation.
// Platform-specific HTTP and callback dispatch are handled per-platform.
// The JSON parsing and version comparison logic is cross-platform.

#include "core/update_checker.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

// ===================================================================
// Lightweight JSON helpers (cross-platform, no external dependency)
// ===================================================================

static std::string JsonGetString(const std::string& json, const char* key) {
  std::string pattern = std::string("\"") + key + "\"";
  auto pos = json.find(pattern);
  if (pos == std::string::npos) return {};

  pos += pattern.size();
  pos = json.find(':', pos);
  if (pos == std::string::npos) return {};
  ++pos;

  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
                                json[pos] == '\n' || json[pos] == '\r'))
    ++pos;

  if (pos >= json.size() || json[pos] != '"') return {};
  ++pos;

  std::string result;
  result.reserve(256);
  while (pos < json.size() && json[pos] != '"') {
    if (json[pos] == '\\' && pos + 1 < json.size()) {
      ++pos;
      switch (json[pos]) {
        case '"':  result += '"';  break;
        case '\\': result += '\\'; break;
        case 'n':  result += '\n'; break;
        case 'r':  result += '\r'; break;
        case 't':  result += '\t'; break;
        default:   result += json[pos]; break;
      }
    } else {
      result += json[pos];
    }
    ++pos;
  }
  return result;
}

static std::string JsonGetFirstAssetUrl(const std::string& json) {
  auto pos = json.find("\"assets\"");
  if (pos == std::string::npos) return {};
  pos = json.find("\"browser_download_url\"", pos);
  if (pos == std::string::npos) return {};
  std::string sub = json.substr(pos);
  return JsonGetString("{" + sub, "browser_download_url");
}

// ===================================================================
// Semantic version comparison (cross-platform)
// ===================================================================

struct SemVer {
  int major = 0, minor = 0, patch = 0;
};

static SemVer ParseVersion(const char* s) {
  SemVer v;
  if (s && (*s == 'v' || *s == 'V')) ++s;
#ifdef _MSC_VER
  if (s) sscanf_s(s, "%d.%d.%d", &v.major, &v.minor, &v.patch);
#else
  if (s) std::sscanf(s, "%d.%d.%d", &v.major, &v.minor, &v.patch);
#endif
  return v;
}

static bool IsNewer(const SemVer& latest, const SemVer& current) {
  if (latest.major != current.major) return latest.major > current.major;
  if (latest.minor != current.minor) return latest.minor > current.minor;
  return latest.patch > current.patch;
}

// ===================================================================
// Platform-specific: Windows implementation
// ===================================================================

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include "core/platform_http.h"

static constexpr UINT WM_UPDATE_RESULT = WM_APP + 100;

struct UpdateContext {
  UpdateCallback callback;
  UpdateInfo     info;
};

static LRESULT CALLBACK UpdateWndProc(HWND hwnd, UINT msg,
                                       WPARAM wp, LPARAM lp) {
  if (msg == WM_UPDATE_RESULT) {
    auto* ctx = reinterpret_cast<UpdateContext*>(lp);
    if (ctx) {
      if (ctx->callback) ctx->callback(ctx->info);
      delete ctx;
    }
    DestroyWindow(hwnd);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

void StartUpdateCheckAsync(const char* github_repo,
                           const char* current_version,
                           UpdateCallback cb) {
  static bool class_registered = false;
  static const wchar_t kClassName[] = L"PGUpdateChecker";

  if (!class_registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize      = sizeof(wc);
    wc.lpfnWndProc = UpdateWndProc;
    wc.hInstance   = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
    class_registered = true;
  }

  HWND hwnd = CreateWindowExW(0, kClassName, nullptr, 0,
                               0, 0, 0, 0,
                               HWND_MESSAGE, nullptr,
                               GetModuleHandleW(nullptr), nullptr);

  std::string repo(github_repo);
  std::string api_path = "/repos/" + repo + "/releases/latest";
  std::string cur_ver(current_version);

  std::thread([hwnd, api_path, cur_ver, cb]() {
    auto* ctx = new UpdateContext();
    ctx->callback = cb;
    std::memset(&ctx->info, 0, sizeof(ctx->info));

    auto http = CreatePlatformHttp();
    std::string json = http->HttpsGet("api.github.com", api_path.c_str());

    if (!json.empty()) {
      std::string tag = JsonGetString(json, "tag_name");
      std::string body = JsonGetString(json, "body");
      std::string asset_url = JsonGetFirstAssetUrl(json);
      std::string html_url = JsonGetString(json, "html_url");

      if (!tag.empty()) {
        SemVer latest  = ParseVersion(tag.c_str());
        SemVer current = ParseVersion(cur_ver.c_str());

        ctx->info.available = IsNewer(latest, current);

        const char* ver_str = tag.c_str();
        if (*ver_str == 'v' || *ver_str == 'V') ++ver_str;
        strncpy_s(ctx->info.latest_version, ver_str, _TRUNCATE);

        const std::string& url = asset_url.empty() ? html_url : asset_url;
        strncpy_s(ctx->info.download_url, url.c_str(), _TRUNCATE);

        if (body.size() > sizeof(ctx->info.release_notes) - 1)
          body.resize(sizeof(ctx->info.release_notes) - 1);
        strncpy_s(ctx->info.release_notes, body.c_str(), _TRUNCATE);
      }
    }

    PostMessage(hwnd, WM_UPDATE_RESULT, 0,
                reinterpret_cast<LPARAM>(ctx));
  }).detach();
}

void OpenUrlInBrowser(const char* url) {
  if (!url || !url[0]) return;
  wchar_t wurl[512] = {};
  for (int i = 0; i < 511 && url[i]; ++i)
    wurl[i] = static_cast<wchar_t>(url[i]);
  ShellExecuteW(nullptr, L"open", wurl, nullptr, nullptr, SW_SHOWNORMAL);
}

#else
// Stub for non-Windows platforms (to be implemented)
void StartUpdateCheckAsync(const char*, const char*, UpdateCallback) {
  std::printf("Update check not implemented on this platform.\n");
}
void OpenUrlInBrowser(const char*) {
  std::printf("OpenUrlInBrowser not implemented on this platform.\n");
}
#endif  // _WIN32
