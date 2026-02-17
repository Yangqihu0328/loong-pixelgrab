// Copyright 2026 The PixelGrab Authors
//
// PixelGrab -- Interactive screenshot / annotation / pin tool (Snipaste-style).
// Entry point: delegates to platform-specific Application singleton.

#ifdef _WIN32
#include "platform/windows/win_application.h"
#elif defined(__APPLE__)
#include "platform/macos/mac_application.h"
#elif defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))
#include "platform/linux/linux_application.h"
#else
#include <cstdio>
#endif

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__linux__) && !(defined(__unix__) && !defined(__APPLE__))
int main() {
  std::printf("This platform is not yet supported.\n");
  return 0;
}
#endif

#ifdef __APPLE__
int main() {
  auto& app = MacApplication::instance();
  if (!app.Init()) return 1;
  int ret = app.Run();
  app.Shutdown();
  return ret;
}
#endif

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__) && !defined(_WIN32))
int main() {
  auto& app = LinuxApplication::instance();
  if (!app.Init()) return 1;
  int ret = app.Run();
  app.Shutdown();
  return ret;
}
#endif

#ifdef _WIN32

#ifdef _MSC_VER
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif

int main() {
  auto& app = Application::instance();

  if (!app.Init()) return 0;

  int ret = app.Run();

  app.Shutdown();

  return ret;
}

#endif  // _WIN32
