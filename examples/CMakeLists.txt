# PixelGrab — interactive screenshot / annotation / pin tool
# Cross-platform build configuration.

# ===================================================================
# Cross-platform sources (always compiled)
# ===================================================================
set(CORE_SOURCES
  core/i18n.cpp
  core/update_checker.cpp
)

# ===================================================================
# Platform-specific sources
# ===================================================================
if(WIN32)
  set(PLATFORM_SOURCES
    # Platform interface implementations
    platform/windows/win_settings.cpp
    platform/windows/win_hotkey.cpp
    platform/windows/win_http.cpp

    # Windows Application singleton
    platform/windows/win_application.cpp

    # Capture managers
    platform/windows/capture/overlay_manager.cpp
    platform/windows/capture/selection_manager.cpp
    platform/windows/capture/annotation_manager.cpp
    platform/windows/capture/pin_manager.cpp
    platform/windows/capture/f1_toolbar.cpp
    platform/windows/capture/color_picker.cpp

    # Recording manager
    platform/windows/recording/recording_manager.cpp

    # Tray & dialogs
    platform/windows/tray/tray_manager.cpp
    platform/windows/tray/settings_dialog.cpp
    platform/windows/tray/about_dialog.cpp

    # Resources
    platform/windows/resources/app.rc
  )
  set(PLATFORM_LIBS user32 gdi32 gdiplus dwmapi shell32 advapi32 winhttp)
elseif(APPLE)
  set(PLATFORM_SOURCES
    platform/macos/mac_settings.mm
    platform/macos/mac_hotkey.mm
    platform/macos/mac_http.mm
    platform/macos/mac_i18n.mm
    platform/macos/mac_application.mm
    platform/macos/mac_color_picker.mm
  )
  set(PLATFORM_LIBS
    "-framework Cocoa"
    "-framework Carbon"
    "-framework Foundation"
    "-framework AppKit"
    "-framework ServiceManagement"
  )
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  pkg_check_modules(CURL REQUIRED libcurl)
  pkg_check_modules(X11 REQUIRED x11)

  set(PLATFORM_SOURCES
    platform/linux/linux_settings.cpp
    platform/linux/linux_hotkey.cpp
    platform/linux/linux_http.cpp
    platform/linux/linux_i18n.cpp
    platform/linux/linux_application.cpp
    platform/linux/linux_color_picker.cpp
    platform/linux/linux_capture_overlay.cpp
  )
  set(PLATFORM_LIBS ${GTK3_LIBRARIES} ${CURL_LIBRARIES} ${X11_LIBRARIES})
  set(PLATFORM_INCLUDE_DIRS ${GTK3_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${X11_INCLUDE_DIRS})
endif()

# ===================================================================
# Executable target
# ===================================================================
add_executable(PixelGrab
  main.cpp
  ${CORE_SOURCES}
  ${PLATFORM_SOURCES}
)

target_link_libraries(PixelGrab PRIVATE pixelgrab ${PLATFORM_LIBS})

# Include paths:
#   - CMAKE_CURRENT_SOURCE_DIR (examples/) for "core/..." and "platform/..." includes
#   - PROJECT_BINARY_DIR/include for generated version.h
#   - PROJECT_SOURCE_DIR/include for pixelgrab/pixelgrab.h
target_include_directories(PixelGrab PRIVATE
  ${PROJECT_BINARY_DIR}/include
  ${PROJECT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Linux: add GTK/curl/X11 include paths
if(DEFINED PLATFORM_INCLUDE_DIRS)
  target_include_directories(PixelGrab PRIVATE ${PLATFORM_INCLUDE_DIRS})
endif()

# Pass GitHub repo for update checker
if(DEFINED PIXELGRAB_GITHUB_REPO)
  target_compile_definitions(PixelGrab PRIVATE
    PIXELGRAB_GITHUB_REPO="${PIXELGRAB_GITHUB_REPO}"
  )
endif()

# Copy QR code images to build output (qrcode/ next to exe)
set(_qr_files
  "${PROJECT_SOURCE_DIR}/docs/images/wechat.jpg"
  "${PROJECT_SOURCE_DIR}/docs/images/wechat_pay.jpg"
  "${PROJECT_SOURCE_DIR}/docs/images/alipay.jpg"
)
foreach(_qr IN LISTS _qr_files)
  if(EXISTS "${_qr}")
    add_custom_command(TARGET PixelGrab POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
              "$<TARGET_FILE_DIR:PixelGrab>/qrcode"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${_qr}" "$<TARGET_FILE_DIR:PixelGrab>/qrcode/"
      COMMENT "Copying QR code image: ${_qr}"
    )
  endif()
endforeach()

# ===================================================================
# Install rules
# ===================================================================

install(TARGETS PixelGrab RUNTIME DESTINATION bin)

if(WIN32)
  # pixelgrab.dll
  install(FILES $<TARGET_FILE:pixelgrab> DESTINATION bin)

  # Third-party DLLs (Tesseract, Leptonica, OpenSSL, etc.)
  # These are copied to the build output dir by vcpkg at build time.
  set(_pg_bin_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
  install(CODE "
    set(_config \"\${CMAKE_INSTALL_CONFIG_NAME}\")
    if(NOT _config)
      set(_config \"Release\")
    endif()
    file(GLOB _dlls \"${_pg_bin_dir}/\${_config}/*.dll\")
    foreach(_dll \${_dlls})
      get_filename_component(_name \${_dll} NAME)
      if(NOT \${_name} STREQUAL \"pixelgrab.dll\")
        file(INSTALL \${_dll} DESTINATION \"\${CMAKE_INSTALL_PREFIX}/bin\")
      endif()
    endforeach()
  ")

  # tessdata/ (OCR models — chi_sim + eng)
  install(CODE "
    set(_config \"\${CMAKE_INSTALL_CONFIG_NAME}\")
    if(NOT _config)
      set(_config \"Release\")
    endif()
    set(_tessdata \"${_pg_bin_dir}/\${_config}/tessdata\")
    if(IS_DIRECTORY \${_tessdata})
      file(INSTALL \${_tessdata} DESTINATION \"\${CMAKE_INSTALL_PREFIX}/bin\")
    endif()
  ")
endif()

# pixelgrab.cfg.example (translation config template)
if(EXISTS "${PROJECT_SOURCE_DIR}/pixelgrab.cfg.example")
  install(FILES "${PROJECT_SOURCE_DIR}/pixelgrab.cfg.example" DESTINATION bin)
endif()

# QR code images
install(DIRECTORY DESTINATION bin/qrcode)
foreach(_qr IN LISTS _qr_files)
  if(EXISTS "${_qr}")
    install(FILES "${_qr}" DESTINATION bin/qrcode)
  endif()
endforeach()
