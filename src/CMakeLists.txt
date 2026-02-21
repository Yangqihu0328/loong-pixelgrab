# Core sources (always compiled)
set(CORE_SOURCES
  core/image.cpp
  core/image_export.cpp
  core/color_utils.cpp
  core/capture_history.cpp
  core/logger.cpp
  core/pixelgrab_context.cpp
  core/pixelgrab_api.cpp
  annotation/annotation_session.cpp
  detection/snap_engine.cpp
  pin/pin_window_manager.cpp
)

# Optional: OCR module
if(PIXELGRAB_ENABLE_OCR)
  list(APPEND CORE_SOURCES ocr/tesseract_ocr_backend.cpp)
else()
  list(APPEND CORE_SOURCES ocr/ocr_stub.cpp)
endif()

# Optional: Translation module
if(PIXELGRAB_ENABLE_TRANSLATE)
  list(APPEND CORE_SOURCES translate/translate_backend.cpp)
else()
  list(APPEND CORE_SOURCES translate/translate_stub.cpp)
endif()

# Platform-specific sources
if(WIN32)
  set(PLATFORM_SOURCES
    platform/windows/win_capture_backend.cpp
    platform/windows/win_annotation_renderer.cpp
    platform/windows/win_element_detector.cpp
    platform/windows/win_pin_window.cpp
    platform/windows/win_clipboard.cpp
    platform/windows/win_recorder_backend.cpp
    platform/windows/win_watermark_renderer.cpp
    platform/windows/win_audio_backend.cpp
    platform/windows/d3d11_device_manager.cpp
  )
  if(PIXELGRAB_ENABLE_TRANSLATE)
    list(APPEND PLATFORM_SOURCES platform/windows/win_translate_backend.cpp)
  endif()
  set(PLATFORM_LIBS user32 gdi32 shcore gdiplus ole32 oleaut32
                    mfplat mfreadwrite mf mfuuid
                    d3d11 dxgi d2d1 dwrite)
  if(PIXELGRAB_ENABLE_TRANSLATE)
    list(APPEND PLATFORM_LIBS winhttp advapi32)
  endif()
elseif(APPLE)
  set(PLATFORM_SOURCES
    platform/macos/mac_capture_backend.mm
    platform/macos/mac_annotation_renderer.mm
    platform/macos/mac_element_detector.mm
    platform/macos/mac_pin_window.mm
    platform/macos/mac_clipboard.mm
    platform/macos/mac_recorder_backend.mm
    platform/macos/mac_watermark_renderer.mm
    platform/macos/mac_audio_backend.mm
  )
  if(PIXELGRAB_ENABLE_TRANSLATE)
    list(APPEND PLATFORM_SOURCES platform/macos/mac_translate_backend.mm)
  endif()
  find_library(COREGRAPHICS_LIB CoreGraphics)
  find_library(APPKIT_LIB AppKit)
  find_library(CORETEXT_LIB CoreText)
  find_library(FOUNDATION_LIB Foundation)
  find_library(AVFOUNDATION_LIB AVFoundation)
  find_library(COREMEDIA_LIB CoreMedia)
  find_library(COREVIDEO_LIB CoreVideo)
  find_library(AUDIOTOOLBOX_LIB AudioToolbox)
  find_library(COREAUDIO_LIB CoreAudio)
  set(PLATFORM_LIBS
    ${COREGRAPHICS_LIB} ${APPKIT_LIB} ${CORETEXT_LIB} ${FOUNDATION_LIB}
    ${AVFOUNDATION_LIB} ${COREMEDIA_LIB} ${COREVIDEO_LIB}
    ${AUDIOTOOLBOX_LIB} ${COREAUDIO_LIB})
  # ScreenCaptureKit is optional (macOS 12.3+)
  find_library(SCREENCAPTUREKIT_LIB ScreenCaptureKit)
  if(SCREENCAPTUREKIT_LIB)
    list(APPEND PLATFORM_LIBS ${SCREENCAPTUREKIT_LIB})
  endif()
elseif(UNIX)
  set(PLATFORM_SOURCES
    platform/linux/x11_capture_backend.cpp
    platform/linux/x11_annotation_renderer.cpp
    platform/linux/x11_element_detector.cpp
    platform/linux/x11_pin_window.cpp
    platform/linux/x11_clipboard.cpp
    platform/linux/x11_recorder_backend.cpp
    platform/linux/x11_watermark_renderer.cpp
    platform/linux/x11_audio_backend.cpp
  )
  if(PIXELGRAB_ENABLE_TRANSLATE)
    list(APPEND PLATFORM_SOURCES platform/linux/x11_translate_backend.cpp)
  endif()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(X11 REQUIRED x11)
  pkg_check_modules(CAIRO REQUIRED cairo)
  pkg_check_modules(PANGO REQUIRED pangocairo)
  pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-app-1.0)
  pkg_check_modules(PULSE REQUIRED libpulse-simple)
  set(PLATFORM_LIBS
    ${X11_LIBRARIES} ${CAIRO_LIBRARIES} ${PANGO_LIBRARIES}
    ${GST_LIBRARIES} ${PULSE_LIBRARIES})
endif()

# Generate version header from CMake version
configure_file(
  ${PROJECT_SOURCE_DIR}/include/pixelgrab/version.h.in
  ${PROJECT_BINARY_DIR}/include/pixelgrab/version.h
  @ONLY
)

# Shared library target
add_library(pixelgrab SHARED
  ${CORE_SOURCES}
  ${PLATFORM_SOURCES}
)

# Public include directory (source + generated headers)
target_include_directories(pixelgrab
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# stb headers (image export)
target_include_directories(pixelgrab PRIVATE ${STB_INCLUDE_DIR})

# Compile definitions
target_compile_definitions(pixelgrab PRIVATE PIXELGRAB_BUILDING)
if(PIXELGRAB_ENABLE_OCR)
  target_compile_definitions(pixelgrab PRIVATE PIXELGRAB_HAS_OCR=1)
endif()
if(PIXELGRAB_ENABLE_TRANSLATE)
  target_compile_definitions(pixelgrab PRIVATE PIXELGRAB_HAS_TRANSLATE=1)
endif()

# spdlog
target_link_libraries(pixelgrab PRIVATE spdlog::spdlog)

# Tesseract OCR (optional)
if(PIXELGRAB_ENABLE_OCR)
  find_package(Tesseract CONFIG QUIET)
  if(Tesseract_FOUND)
    target_link_libraries(pixelgrab PRIVATE Tesseract::libtesseract)
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(TESSERACT REQUIRED tesseract)
    target_include_directories(pixelgrab PRIVATE ${TESSERACT_INCLUDE_DIRS})
    target_link_libraries(pixelgrab PRIVATE ${TESSERACT_LIBRARIES})
  endif()
endif()

# Platform libraries
target_link_libraries(pixelgrab PRIVATE ${PLATFORM_LIBS})

# Linux: include directories from pkg-config
if(UNIX AND NOT APPLE)
  target_include_directories(pixelgrab PRIVATE
    ${X11_INCLUDE_DIRS} ${CAIRO_INCLUDE_DIRS} ${PANGO_INCLUDE_DIRS}
    ${GST_INCLUDE_DIRS} ${PULSE_INCLUDE_DIRS})
endif()

# Symbol visibility (non-Windows)
if(NOT WIN32)
  set_target_properties(pixelgrab PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
  )
endif()

# ABI versioning
set_target_properties(pixelgrab PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  OUTPUT_NAME pixelgrab
)

# Windows-specific: export all needed symbols
if(WIN32)
  set_target_properties(pixelgrab PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
  )
endif()
