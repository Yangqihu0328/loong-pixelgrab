// Copyright 2026 The loong-pixelgrab Authors
// Tests for: pixelgrab_set_log_level, pixelgrab_set_log_callback,
//            pixelgrab_log

#include <cstring>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

// ---------------------------------------------------------------------------
// Helper: capture log messages via callback
// ---------------------------------------------------------------------------

struct LogEntry {
  PixelGrabLogLevel level;
  std::string message;
};

static std::vector<LogEntry>* g_log_entries = nullptr;

static void TestLogCallback(PixelGrabLogLevel level, const char* message,
                            void* userdata) {
  auto* entries = static_cast<std::vector<LogEntry>*>(userdata);
  entries->push_back({level, message ? message : ""});
}

class LoggingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    entries_.clear();
    g_log_entries = &entries_;
    pixelgrab_set_log_level(kPixelGrabLogTrace);
    pixelgrab_set_log_callback(TestLogCallback, &entries_);
  }

  void TearDown() override {
    pixelgrab_set_log_callback(nullptr, nullptr);
    pixelgrab_set_log_level(kPixelGrabLogInfo);
    g_log_entries = nullptr;
  }

  std::vector<LogEntry> entries_;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(LoggingTest, SetLogLevelDoesNotCrash) {
  pixelgrab_set_log_level(kPixelGrabLogTrace);
  pixelgrab_set_log_level(kPixelGrabLogDebug);
  pixelgrab_set_log_level(kPixelGrabLogInfo);
  pixelgrab_set_log_level(kPixelGrabLogWarn);
  pixelgrab_set_log_level(kPixelGrabLogError);
  pixelgrab_set_log_level(kPixelGrabLogFatal);
}

TEST_F(LoggingTest, LogCallbackReceivesMessage) {
  entries_.clear();
  pixelgrab_log(kPixelGrabLogInfo, "test message");

  ASSERT_GE(entries_.size(), 1u);
  // The callback receives the formatted message which includes the raw text.
  bool found = false;
  for (const auto& e : entries_) {
    if (e.message.find("test message") != std::string::npos) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Expected to find 'test message' in log entries";
}

TEST_F(LoggingTest, LogCallbackReceivesCorrectLevel) {
  entries_.clear();
  pixelgrab_log(kPixelGrabLogWarn, "warn msg");

  bool found = false;
  for (const auto& e : entries_) {
    if (e.level == kPixelGrabLogWarn &&
        e.message.find("warn msg") != std::string::npos) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(LoggingTest, LogLevelFiltering) {
  // Set level to Warn — Info messages should be filtered out.
  pixelgrab_set_log_level(kPixelGrabLogWarn);
  entries_.clear();

  pixelgrab_log(kPixelGrabLogInfo, "should be filtered");
  pixelgrab_log(kPixelGrabLogWarn, "should appear");

  bool found_info = false;
  bool found_warn = false;
  for (const auto& e : entries_) {
    if (e.message.find("should be filtered") != std::string::npos)
      found_info = true;
    if (e.message.find("should appear") != std::string::npos)
      found_warn = true;
  }
  EXPECT_FALSE(found_info) << "Info message should have been filtered";
  EXPECT_TRUE(found_warn) << "Warn message should have appeared";
}

TEST_F(LoggingTest, UnregisterCallback) {
  pixelgrab_set_log_callback(nullptr, nullptr);
  entries_.clear();
  pixelgrab_log(kPixelGrabLogInfo, "after unregister");
  // After unregistering, no entries should be added via callback.
  bool found = false;
  for (const auto& e : entries_) {
    if (e.message.find("after unregister") != std::string::npos)
      found = true;
  }
  EXPECT_FALSE(found);
}

TEST_F(LoggingTest, LogNullMessage) {
  // Should not crash.
  pixelgrab_log(kPixelGrabLogInfo, nullptr);
}
