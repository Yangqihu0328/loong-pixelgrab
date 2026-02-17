// Copyright 2026 The loong-pixelgrab Authors

#include "core/logger.h"

#include <mutex>

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "core/callback_sink.h"

namespace pixelgrab {
namespace internal {

namespace {

std::once_flag g_init_flag;
std::shared_ptr<spdlog::logger> g_logger;
std::shared_ptr<CallbackSink> g_callback_sink;

}  // namespace

void InitLogger() {
  std::call_once(g_init_flag, []() {
    // Create sinks: stderr (colored) + callback.
    auto stderr_sink =
        std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    g_callback_sink = std::make_shared<CallbackSink>();

    // Combine sinks into a single logger.
    spdlog::sinks_init_list sinks = {stderr_sink, g_callback_sink};
    g_logger =
        std::make_shared<spdlog::logger>("pixelgrab", sinks);

    // Default pattern: [pixelgrab][level] message
    g_logger->set_pattern("[pixelgrab][%l] %v");

    // Default level: Info.
    g_logger->set_level(spdlog::level::info);

    // Flush on warn and above.
    g_logger->flush_on(spdlog::level::warn);
  });
}

std::shared_ptr<spdlog::logger> GetLogger() {
  InitLogger();
  return g_logger;
}

std::shared_ptr<CallbackSink> GetCallbackSink() {
  InitLogger();
  return g_callback_sink;
}

void SetLogLevel(PixelGrabLogLevel level) {
  InitLogger();
  g_logger->set_level(ToSpdlogLevel(level));
}

spdlog::level::level_enum ToSpdlogLevel(PixelGrabLogLevel level) {
  switch (level) {
    case kPixelGrabLogTrace: return spdlog::level::trace;
    case kPixelGrabLogDebug: return spdlog::level::debug;
    case kPixelGrabLogInfo:  return spdlog::level::info;
    case kPixelGrabLogWarn:  return spdlog::level::warn;
    case kPixelGrabLogError: return spdlog::level::err;
    case kPixelGrabLogFatal: return spdlog::level::critical;
    default:                 return spdlog::level::info;
  }
}

}  // namespace internal
}  // namespace pixelgrab
