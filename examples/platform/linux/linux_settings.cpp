// Copyright 2026 The PixelGrab Authors
// Linux implementation of IPlatformSettings using ~/.config/pixelgrab/settings.ini.

#include "core/platform_settings.h"

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <unordered_map>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

class LinuxPlatformSettings : public IPlatformSettings {
 public:
  LinuxPlatformSettings() { Load(); }

  bool GetInt(const char* key, int* out_value) override {
    auto it = data_.find(key);
    if (it == data_.end()) return false;
    *out_value = std::atoi(it->second.c_str());
    return true;
  }

  bool SetInt(const char* key, int value) override {
    data_[key] = std::to_string(value);
    return Save();
  }

  bool GetString(const char* key, char* buf, int buf_size) override {
    auto it = data_.find(key);
    if (it == data_.end()) return false;
    std::strncpy(buf, it->second.c_str(), static_cast<size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return true;
  }

  bool SetString(const char* key, const char* value) override {
    data_[key] = value;
    return Save();
  }

  bool IsAutoStartEnabled() override {
    std::string autostart_path = GetAutostartPath();
    struct stat st;
    return stat(autostart_path.c_str(), &st) == 0;
  }

  void SetAutoStart(bool enable) override {
    std::string autostart_dir = GetXDGConfigHome() + "/autostart";
    std::string autostart_path = autostart_dir + "/pixelgrab.desktop";

    if (enable) {
      mkdir(autostart_dir.c_str(), 0755);

      char exe_path[PATH_MAX] = {};
      ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
      if (len <= 0) return;
      exe_path[len] = '\0';

      std::ofstream f(autostart_path);
      if (f) {
        f << "[Desktop Entry]\n"
          << "Type=Application\n"
          << "Name=PixelGrab\n"
          << "Exec=" << exe_path << "\n"
          << "X-GNOME-Autostart-enabled=true\n";
      }
      std::printf("  Auto-start enabled.\n");
    } else {
      std::remove(autostart_path.c_str());
      std::printf("  Auto-start disabled.\n");
    }
  }

 private:
  std::string GetXDGConfigHome() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) return xdg;
    const char* home = std::getenv("HOME");
    if (home) return std::string(home) + "/.config";
    return "/tmp";
  }

  std::string GetConfigDir() {
    return GetXDGConfigHome() + "/pixelgrab";
  }

  std::string GetConfigPath() {
    return GetConfigDir() + "/settings.ini";
  }

  std::string GetAutostartPath() {
    return GetXDGConfigHome() + "/autostart/pixelgrab.desktop";
  }

  void Load() {
    data_.clear();
    std::ifstream f(GetConfigPath());
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
      if (line.empty() || line[0] == '#' || line[0] == '[') continue;
      auto eq = line.find('=');
      if (eq == std::string::npos) continue;
      std::string key = line.substr(0, eq);
      std::string val = line.substr(eq + 1);
      // Trim whitespace
      while (!key.empty() && key.back() == ' ') key.pop_back();
      while (!val.empty() && val.front() == ' ') val.erase(val.begin());
      data_[key] = val;
    }
  }

  bool Save() {
    std::string dir = GetConfigDir();
    mkdir(dir.c_str(), 0755);
    std::ofstream f(GetConfigPath());
    if (!f) return false;
    f << "[Settings]\n";
    for (const auto& kv : data_) {
      f << kv.first << "=" << kv.second << "\n";
    }
    return f.good();
  }

  std::unordered_map<std::string, std::string> data_;
};

std::unique_ptr<IPlatformSettings> CreatePlatformSettings() {
  return std::make_unique<LinuxPlatformSettings>();
}

#endif  // __linux__
