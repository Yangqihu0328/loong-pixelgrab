# Test sources — one file per module
set(TEST_SOURCES
  test_version.cpp
  test_context.cpp
  test_color.cpp
  test_logging.cpp
  test_screen_capture.cpp
  test_dpi.cpp
  test_annotation.cpp
  test_detection_history.cpp
  test_pin_clipboard.cpp
  test_recorder_watermark.cpp
  test_audio.cpp
  test_ocr.cpp
  test_translate.cpp
  test_validation.cpp
)

# Single test executable containing all test files
add_executable(pixelgrab_tests ${TEST_SOURCES})

target_link_libraries(pixelgrab_tests
  PRIVATE
    pixelgrab
    GTest::gtest
    GTest::gtest_main
)

# Allow includes like "pixelgrab/pixelgrab.h"
target_include_directories(pixelgrab_tests PRIVATE
  ${PROJECT_SOURCE_DIR}/include
)

# Copy pixelgrab DLL next to the test executable so it can be found at runtime.
if(WIN32)
  add_custom_command(TARGET pixelgrab_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:pixelgrab>
      $<TARGET_FILE_DIR:pixelgrab_tests>
    COMMENT "Copying pixelgrab.dll to test output directory"
  )
endif()

include(GoogleTest)
gtest_discover_tests(pixelgrab_tests
  DISCOVERY_MODE PRE_TEST
)

# ---------------------------------------------------------------------------
# Performance benchmark (not part of CTest — run manually)
# ---------------------------------------------------------------------------
add_executable(pixelgrab_bench bench_capture.cpp)
target_link_libraries(pixelgrab_bench PRIVATE pixelgrab)
target_include_directories(pixelgrab_bench PRIVATE ${PROJECT_SOURCE_DIR}/include)

if(WIN32)
  add_custom_command(TARGET pixelgrab_bench POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:pixelgrab>
      $<TARGET_FILE_DIR:pixelgrab_bench>
    COMMENT "Copying pixelgrab.dll to bench output directory"
  )
endif()
