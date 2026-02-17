// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_CORE_CALLBACK_SINK_H_
#define PIXELGRAB_CORE_CALLBACK_SINK_H_

#include <mutex>
#include <string>

#include "spdlog/sinks/base_sink.h"

#include "pixelgrab/pixelgrab.h"

namespace pixelgrab {
namespace internal {

/// Custom spdlog sink that forwards log messages to a user-defined C callback.
///
/// Thread safety: inherits from base_sink which is guarded by Mutex.
class CallbackSink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  CallbackSink() = default;

  /// Set the user callback and optional userdata pointer.
  /// Passing nullptr as callback disables forwarding.
  void SetCallback(pixelgrab_log_callback_t callback, void* userdata) {
    std::lock_guard<std::mutex> lock(spdlog::sinks::base_sink<std::mutex>::mutex_);
    callback_ = callback;
    userdata_ = userdata;
  }

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    if (!callback_) return;

    // Format the message payload (without level/time prefix — the callback
    // receives raw message + level separately).
    spdlog::memory_buf_t formatted;
    spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);
    std::string text(formatted.data(), formatted.size());

    // Map spdlog level → PixelGrabLogLevel.
    PixelGrabLogLevel level = MapLevel(msg.level);

    callback_(level, text.c_str(), userdata_);
  }

  void flush_() override {
    // Nothing to flush for a callback sink.
  }

 private:
  static PixelGrabLogLevel MapLevel(spdlog::level::level_enum lvl) {
    switch (lvl) {
      case spdlog::level::trace:    return kPixelGrabLogTrace;
      case spdlog::level::debug:    return kPixelGrabLogDebug;
      case spdlog::level::info:     return kPixelGrabLogInfo;
      case spdlog::level::warn:     return kPixelGrabLogWarn;
      case spdlog::level::err:      return kPixelGrabLogError;
      case spdlog::level::critical: return kPixelGrabLogFatal;
      case spdlog::level::off:      return kPixelGrabLogFatal;
      default:                      return kPixelGrabLogInfo;
    }
  }

  pixelgrab_log_callback_t callback_ = nullptr;
  void* userdata_ = nullptr;
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_CORE_CALLBACK_SINK_H_
