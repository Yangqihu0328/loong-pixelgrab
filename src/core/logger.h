// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_CORE_LOGGER_H_
#define PIXELGRAB_CORE_LOGGER_H_

#include <memory>

#include "spdlog/spdlog.h"

#include "pixelgrab/pixelgrab.h"

namespace pixelgrab {
namespace internal {

class CallbackSink;

/// Initialize the global pixelgrab logger (stderr + optional callback sink).
/// Safe to call multiple times; subsequent calls are no-ops.
void InitLogger();

/// Get the global pixelgrab spdlog logger instance.
std::shared_ptr<spdlog::logger> GetLogger();

/// Get the global callback sink (used to register/unregister user callback).
std::shared_ptr<CallbackSink> GetCallbackSink();

/// Set the global log level.
void SetLogLevel(PixelGrabLogLevel level);

/// Map PixelGrabLogLevel to spdlog::level::level_enum.
spdlog::level::level_enum ToSpdlogLevel(PixelGrabLogLevel level);

}  // namespace internal
}  // namespace pixelgrab

// ---------------------------------------------------------------------------
// Convenience macros (internal use only).
// ---------------------------------------------------------------------------

#define PIXELGRAB_LOG_TRACE(...)  SPDLOG_LOGGER_TRACE(::pixelgrab::internal::GetLogger(), __VA_ARGS__)
#define PIXELGRAB_LOG_DEBUG(...)  SPDLOG_LOGGER_DEBUG(::pixelgrab::internal::GetLogger(), __VA_ARGS__)
#define PIXELGRAB_LOG_INFO(...)   SPDLOG_LOGGER_INFO(::pixelgrab::internal::GetLogger(), __VA_ARGS__)
#define PIXELGRAB_LOG_WARN(...)   SPDLOG_LOGGER_WARN(::pixelgrab::internal::GetLogger(), __VA_ARGS__)
#define PIXELGRAB_LOG_ERROR(...)  SPDLOG_LOGGER_ERROR(::pixelgrab::internal::GetLogger(), __VA_ARGS__)
#define PIXELGRAB_LOG_FATAL(...)  SPDLOG_LOGGER_CRITICAL(::pixelgrab::internal::GetLogger(), __VA_ARGS__)

#endif  // PIXELGRAB_CORE_LOGGER_H_
