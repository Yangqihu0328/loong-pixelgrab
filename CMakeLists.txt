cmake_minimum_required(VERSION 3.16)
project(loong-pixelgrab
  VERSION 1.3.0
  DESCRIPTION "Cross-platform screen capture library"
  LANGUAGES C CXX
)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Put all build outputs (exe/dll) in the same directory for easy testing
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler flags
if(MSVC)
  # /utf-8: treat source and execution charset as UTF-8 (eliminates C4819)
  # /W4: high warning level
  # /WX: warnings as errors
  add_compile_options(/utf-8 /W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(PIXELGRAB_BUILD_EXAMPLES "Build example programs" ON)
option(PIXELGRAB_BUILD_TESTS "Build test programs" OFF)
option(PIXELGRAB_ENABLE_OCR "Enable OCR support (requires Tesseract)" ON)
option(PIXELGRAB_ENABLE_TRANSLATE "Enable translation support" ON)

# GitHub repository for update checker (owner/repo format)
set(PIXELGRAB_GITHUB_REPO "Yangqihu0328/loong-pixelgrab" CACHE STRING
    "GitHub owner/repo for update checking")

# Platform detection
if(WIN32)
  message(STATUS "Platform: Windows")
elseif(APPLE)
  message(STATUS "Platform: macOS")
  enable_language(OBJC OBJCXX)
elseif(UNIX)
  message(STATUS "Platform: Linux")
endif()

# External dependencies via FetchContent
include(FetchContent)

# spdlog
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.15.1
  GIT_SHALLOW    TRUE
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# Google Test (only when tests are enabled)
if(PIXELGRAB_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# Library target
add_subdirectory(src)

# Examples
if(PIXELGRAB_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# Tests
if(PIXELGRAB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ===================================================================
# CPack - Installer / Distribution Packaging
# ===================================================================
set(CPACK_PACKAGE_NAME "PixelGrab")
set(CPACK_PACKAGE_VENDOR "PixelGrab")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PixelGrab - Screenshot / Annotation / Pin Tool")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "PixelGrab")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
# Default output directory
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")

if(WIN32)
  # ---- NSIS Installer ----
  set(CPACK_GENERATOR "NSIS;ZIP")
  set(CPACK_NSIS_DISPLAY_NAME "PixelGrab")
  set(CPACK_NSIS_PACKAGE_NAME "PixelGrab")
  set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\PixelGrab.exe")
  set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/examples/platform/windows/resources/app_icon.ico")
  set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/examples/platform/windows/resources/app_icon.ico")
  # Start menu shortcut
  set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\PixelGrab.lnk' '$INSTDIR\\\\bin\\\\PixelGrab.exe'"
  )
  set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\PixelGrab.lnk'"
  )
  # Kill running PixelGrab BEFORE file operations (prevents file-in-use errors)
  set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS "
    nsExec::ExecToLog 'taskkill /f /im PixelGrab.exe'
    Sleep 1000
  ")
  # Desktop shortcut (created after install)
  set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
    "CreateShortCut '$DESKTOP\\\\PixelGrab.lnk' '$INSTDIR\\\\bin\\\\PixelGrab.exe'"
  )
  # Uninstall: kill process first, then clean up shortcuts
  set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
    nsExec::ExecToLog 'taskkill /f /im PixelGrab.exe'
    Sleep 1000
    Delete '$DESKTOP\\\\PixelGrab.lnk'
  ")
  # Finish page: "Run PixelGrab" checkbox (checked by default)
  # NOTE: NSIS template prepends $INSTDIR\<install_subdir>\ automatically
  set(CPACK_NSIS_MUI_FINISHPAGE_RUN "PixelGrab.exe")
  # Uninstaller in Add/Remove Programs
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MODIFY_PATH OFF)
elseif(APPLE)
  set(CPACK_GENERATOR "DragNDrop;ZIP")
elseif(UNIX)
  set(CPACK_GENERATOR "TGZ;DEB")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "PixelGrab Authors")
  set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
endif()

include(CPack)
