// Copyright 2026 The PixelGrab Authors
//
// WinPlatformSettings — Windows Registry implementation.

#include "platform/windows/win_settings.h"

#ifdef _WIN32

#include <cstdio>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "platform/windows/win_app_defs.h"

// ===================================================================
// IPlatformSettings — Registry-backed key-value store
// ===================================================================

bool WinPlatformSettings::GetInt(const char* key, int* out_value) {
  HKEY hKey = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, KEY_READ, &hKey)
      != ERROR_SUCCESS)
    return false;

  wchar_t wkey[128] = {};
  for (int i = 0; i < 127 && key[i]; ++i)
    wkey[i] = static_cast<wchar_t>(key[i]);

  DWORD val = 0, size = sizeof(val);
  LSTATUS st = RegQueryValueExW(hKey, wkey, nullptr, nullptr,
                                reinterpret_cast<BYTE*>(&val), &size);
  RegCloseKey(hKey);
  if (st == ERROR_SUCCESS) {
    *out_value = static_cast<int>(val);
    return true;
  }
  return false;
}

bool WinPlatformSettings::SetInt(const char* key, int value) {
  HKEY hKey = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, nullptr,
                      0, KEY_SET_VALUE, nullptr, &hKey, nullptr)
      != ERROR_SUCCESS)
    return false;

  wchar_t wkey[128] = {};
  for (int i = 0; i < 127 && key[i]; ++i)
    wkey[i] = static_cast<wchar_t>(key[i]);

  DWORD val = static_cast<DWORD>(value);
  LSTATUS st = RegSetValueExW(hKey, wkey, 0, REG_DWORD,
                              reinterpret_cast<const BYTE*>(&val), sizeof(val));
  RegCloseKey(hKey);
  return st == ERROR_SUCCESS;
}

bool WinPlatformSettings::GetString(const char* key, char* buf, int buf_size) {
  HKEY hKey = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, KEY_READ, &hKey)
      != ERROR_SUCCESS)
    return false;

  wchar_t wkey[128] = {};
  for (int i = 0; i < 127 && key[i]; ++i)
    wkey[i] = static_cast<wchar_t>(key[i]);

  DWORD size = static_cast<DWORD>(buf_size);
  LSTATUS st = RegQueryValueExW(hKey, wkey, nullptr, nullptr,
                                reinterpret_cast<BYTE*>(buf), &size);
  RegCloseKey(hKey);
  return st == ERROR_SUCCESS;
}

bool WinPlatformSettings::SetString(const char* key, const char* value) {
  HKEY hKey = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, nullptr,
                      0, KEY_SET_VALUE, nullptr, &hKey, nullptr)
      != ERROR_SUCCESS)
    return false;

  wchar_t wkey[128] = {};
  for (int i = 0; i < 127 && key[i]; ++i)
    wkey[i] = static_cast<wchar_t>(key[i]);

  DWORD len = static_cast<DWORD>(std::strlen(value) + 1);
  LSTATUS st = RegSetValueExW(hKey, wkey, 0, REG_BINARY,
                              reinterpret_cast<const BYTE*>(value), len);
  RegCloseKey(hKey);
  return st == ERROR_SUCCESS;
}

bool WinPlatformSettings::IsAutoStartEnabled() {
  HKEY hKey = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &hKey)
      != ERROR_SUCCESS)
    return false;
  DWORD type = 0, size = 0;
  LSTATUS st = RegQueryValueExW(hKey, kRunValue, nullptr, &type, nullptr, &size);
  RegCloseKey(hKey);
  return st == ERROR_SUCCESS && type == REG_SZ && size > 0;
}

void WinPlatformSettings::SetAutoStart(bool enable) {
  HKEY hKey = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &hKey)
      != ERROR_SUCCESS)
    return;
  if (enable) {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    DWORD cbData = static_cast<DWORD>((wcslen(path) + 1) * sizeof(wchar_t));
    RegSetValueExW(hKey, kRunValue, 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(path), cbData);
    std::printf("  Auto-start enabled.\n");
  } else {
    RegDeleteValueW(hKey, kRunValue);
    std::printf("  Auto-start disabled.\n");
  }
  RegCloseKey(hKey);
}

// ===================================================================
// Factory
// ===================================================================

std::unique_ptr<IPlatformSettings> CreatePlatformSettings() {
  return std::make_unique<WinPlatformSettings>();
}

#endif  // _WIN32
